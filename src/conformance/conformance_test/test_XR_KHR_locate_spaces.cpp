// Copyright (c) 2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "availability_helper.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/array_size.h"
#include "utilities/bitmask_to_string.h"
#include "utilities/types_and_constants.h"
#include "utilities/xrduration_literals.h"
#include "xr_math_approx.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <cmath>

namespace Conformance
{
    using namespace openxr::math_operators;

    namespace
    {
        const auto kExtensionRequirements = FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_KHR_locate_spaces};
        const auto kPromotedCoreRequirements = FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_1};
    };  // namespace

    static inline void SharedLocateSpaces(const FeatureSet& featureSet)
    {
        GlobalData& globalData = GetGlobalData();

        const std::vector<const char*> extensions = SkipOrGetExtensions("Locate spaces", globalData, featureSet);

        SECTION("Requirements not enabled")
        {
            // See if it is explicitly enabled by default
            FeatureSet enabled;
            globalData.PopulateVersionAndEnabledExtensions(enabled);

            if (featureSet.Get(FeatureBitIndex::BIT_XR_KHR_locate_spaces) && !enabled.Get(FeatureBitIndex::BIT_XR_KHR_locate_spaces)) {
                AutoBasicInstance instance;
                ValidateInstanceExtensionFunctionNotSupported(instance, "xrLocateSpacesKHR");
            }
            else if (featureSet.Get(FeatureBitIndex::BIT_XR_KHR_locate_spaces) && enabled.Get(FeatureBitIndex::BIT_XR_KHR_locate_spaces)) {
                WARN(XR_KHR_LOCATE_SPACES_EXTENSION_NAME " force-enabled, cannot test behavior when extension is disabled.");
            }

            if (!featureSet.Get(FeatureBitIndex::BIT_XR_KHR_locate_spaces) && !enabled.Get(FeatureBitIndex::BIT_XR_VERSION_1_1)) {
                AutoBasicInstance instance;
                ValidateInstanceExtensionFunctionNotSupported(instance, "xrLocateSpaces");
            }
        }

        AutoBasicInstance instance(extensions, AutoBasicInstance::createSystemId);

        PFN_xrLocateSpacesKHR xrLocateSpacesPFN = nullptr;

        // When the extension is enabled, testing the extension takes precedence over core, even on an 1.1 instance.
        // I.e. core is only tested when the extension is not enabled AND 1.1 is enabled.
        // TODO test core AND extension at the same time?
        if (featureSet.Get(FeatureBitIndex::BIT_XR_KHR_locate_spaces)) {
            xrLocateSpacesPFN = GetInstanceExtensionFunction<PFN_xrLocateSpacesKHR>(instance, "xrLocateSpacesKHR");
        }
        else if (featureSet.Get(FeatureBitIndex::BIT_XR_VERSION_1_1)) {
            xrLocateSpacesPFN = &xrLocateSpaces;
        }
        else {
            throw std::logic_error("Invalid feature set");
        }

        // Get a session started.
        AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                     AutoBasicSession::createSpaces,
                                 instance);

        // Get frames iterating to the point of app focused state. This will draw frames along the way.
        FrameIterator frameIterator(&session);
        frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

        // Render one frame to get a predicted display time for the xrLocateSpace calls.
        FrameIterator::RunResult runResult = frameIterator.SubmitFrame();
        REQUIRE(runResult == FrameIterator::RunResult::Success);

        XrResult result;

        // compare the calculated pose with the expected pose
        auto ValidateSpaceLocation = [](XrSpaceLocationDataKHR& spaceLocation, XrPosef& expectedPose) -> void {
            CAPTURE(XrSpaceLocationFlagsCPP(spaceLocation.locationFlags));
            CHECK((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0);
            CHECK((spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0);

            if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                REQUIRE(spaceLocation.pose.position == Vector::Approx(expectedPose.position));
            }
            if (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
                CHECK(spaceLocation.pose.orientation == Quat::Approx(expectedPose.orientation));
            }
        };

        struct SpacesData
        {
            std::vector<XrSpace> spacesVec;
            std::vector<XrSpaceLocationDataKHR> locationDataVec;
            XrSpaceLocationsKHR locations{XR_TYPE_SPACE_LOCATIONS_KHR};

            explicit SpacesData(uint32_t spaceCount)
            {
                spacesVec.insert(spacesVec.end(), spaceCount, XR_NULL_HANDLE_CPP);
                locationDataVec.insert(locationDataVec.end(), spaceCount, {(XrSpaceLocationFlags)0, Pose::Identity});
                locations.locations = locationDataVec.data();
                locations.locationCount = spaceCount;
            }

            ~SpacesData()
            {
                for (auto& space : spacesVec) {
                    if (space != XR_NULL_HANDLE_CPP) {
                        REQUIRE(XR_SUCCESS == xrDestroySpace(space));
                    }
                }
            }
        };

        XrReferenceSpaceCreateInfo spaceCreateInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;  // view has to be supported
        spaceCreateInfo.poseInReferenceSpace = Pose::Identity;

        XrTime time = frameIterator.frameState.predictedDisplayTime;
        CHECK(time != 0);

        SECTION("valid inputs")
        {
            SECTION("baseSpace not in spaces")
            {
                SpacesData spacesData(3);

                // identical spaces:
                for (auto& space : spacesData.spacesVec) {
                    result = xrCreateReferenceSpace(session, &spaceCreateInfo, &space);
                    CHECK(result == XR_SUCCESS);
                }

                // baseSpace and spaces distinct
                XrSpace baseSpace = spacesData.spacesVec[0];
                XrSpace* spaces = &(spacesData.spacesVec.data()[1]);
                uint32_t count = (uint32_t)spacesData.spacesVec.size() - 1;
                spacesData.locations.locationCount = count;

                XrSpacesLocateInfoKHR locateInfo{XR_TYPE_SPACES_LOCATE_INFO_KHR};
                locateInfo.baseSpace = baseSpace;
                locateInfo.spaces = spaces;
                locateInfo.spaceCount = count;
                locateInfo.time = time;

                // Exercise the predicted display time
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_SUCCESS);

                // Exercise 40ms ago (or the earliest possible valid time, whichever is later)
                locateInfo.time = std::max(time - 40_xrMilliseconds, 1_xrNanoseconds);
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_SUCCESS);

                // Exercise 1s ago (or the first valid time, whichever is later)
                locateInfo.time = std::max(time - 1_xrSeconds, 1_xrNanoseconds);
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_SUCCESS);
            }

            SECTION("baseSpace in spaces")
            {
                SpacesData spacesData(3);

                // identical spaces:
                for (auto& space : spacesData.spacesVec) {
                    result = xrCreateReferenceSpace(session, &spaceCreateInfo, &space);
                    CHECK(result == XR_SUCCESS);
                }

                // baseSpace included in located spaces
                XrSpace baseSpace = spacesData.spacesVec[0];
                XrSpace* spaces = spacesData.spacesVec.data();
                uint32_t count = (uint32_t)spacesData.spacesVec.size();

                XrSpacesLocateInfoKHR locateInfo{XR_TYPE_SPACES_LOCATE_INFO_KHR};
                locateInfo.baseSpace = baseSpace;
                locateInfo.spaces = spaces;
                locateInfo.spaceCount = count;
                locateInfo.time = time;

                // Exercise the predicted display time
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_SUCCESS);

                // Exercise 40ms ago (or the earliest possible valid time, whichever is later)
                locateInfo.time = std::max(time - 40_xrMilliseconds, 1_xrNanoseconds);
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_SUCCESS);
                locateInfo.time = time;

                // Exercise 1s ago (or the first valid time, whichever is later)
                locateInfo.time = std::max(time - 1_xrSeconds, 1_xrNanoseconds);
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_SUCCESS);
                locateInfo.time = time;
            }
        }

        SECTION("wrong inputs")
        {
            SpacesData spacesData(3);

            // identical spaces:
            for (auto& space : spacesData.spacesVec) {
                result = xrCreateReferenceSpace(session, &spaceCreateInfo, &space);
                CHECK(result == XR_SUCCESS);
            }

            // baseSpace included in located spaces
            XrSpace baseSpace = spacesData.spacesVec[0];
            XrSpace* spaces = spacesData.spacesVec.data();
            uint32_t count = (uint32_t)spacesData.spacesVec.size();

            XrSpacesLocateInfoKHR locateInfo{XR_TYPE_SPACES_LOCATE_INFO_KHR};
            locateInfo.baseSpace = baseSpace;
            locateInfo.spaces = spaces;
            locateInfo.spaceCount = count;
            locateInfo.time = time;

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                XrSpace spaceTmp = spaces[2];

                // Exercise NULL handle
                spaces[2] = XR_NULL_HANDLE_CPP;
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_ERROR_HANDLE_INVALID);
                spaces[2] = spaceTmp;

                // Exercise another NULL handle.
                locateInfo.baseSpace = XR_NULL_HANDLE_CPP;
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_ERROR_HANDLE_INVALID);
                locateInfo.baseSpace = baseSpace;

                // Exercise invalid handle.
                spaces[2] = GlobalData().invalidSpace;
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_ERROR_HANDLE_INVALID);
                spaces[2] = spaceTmp;

                // Exercise another invalid handle.
                locateInfo.baseSpace = GlobalData().invalidSpace;
                result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
                CHECK(result == XR_ERROR_HANDLE_INVALID);
                locateInfo.baseSpace = baseSpace;
            }

            // Exercise 0 as an invalid time.
            locateInfo.time = 0;
            result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
            CHECK(result == XR_ERROR_TIME_INVALID);
            locateInfo.time = time;

            // Exercise negative values as an invalid time.
            locateInfo.time = (XrTime)-42;
            result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
            CHECK(result == XR_ERROR_TIME_INVALID);
            locateInfo.time = time;

            // Exercise spaceCount = 0, locationCount = 0.
            // Set both to zero to ensure XR_ERROR_VALIDATION_FAILURE is not returned due to difference in value.
            spacesData.locations.locationCount = 0;
            result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
            CHECK(result == XR_ERROR_VALIDATION_FAILURE);
            locateInfo.spaceCount = count;
            spacesData.locations.locationCount = count;

            // Exercise spaceCount > locationCount
            spacesData.locations.locationCount = count - 1;
            result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
            CHECK(result == XR_ERROR_VALIDATION_FAILURE);
            spacesData.locations.locationCount = count;

            // Exercise spaceCount < locationCount
            locateInfo.spaceCount = count - 1;
            result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
            CHECK(result == XR_ERROR_VALIDATION_FAILURE);
            locateInfo.spaceCount = count;
        }

        SECTION("space location math")
        {
            // to capture only the handle and not the full object below
            XrSession sessionHandle = session.GetSession();

            // Creates a space to be used as baseSpace using baseSpacePose and additionally spaceCount spaces with the corresponding spacePose array entry,
            // locates them and compares the result with the expected expectedPoses entry. Intention is to check the math behind xrLocateSpace.
            // All spaces are view spaces - this only tests offset poses.
            auto LocateAndTest = [ValidateSpaceLocation, sessionHandle, time, xrLocateSpacesPFN](
                                     XrPosef baseSpacePose, size_t spaceCountSizeT, XrPosef* spacePoses, XrPosef* expectedPoses) -> void {
                uint32_t spaceCount = static_cast<uint32_t>(spaceCountSizeT);

                // create baseSpace and spaces in one go
                SpacesData spacesData(spaceCount + 1);

                XrSpace& baseSpace = spacesData.spacesVec[0];
                XrSpace* spaces = &(spacesData.spacesVec.data()[1]);

                XrReferenceSpaceCreateInfo spaceCreateInfoWithPose = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                spaceCreateInfoWithPose.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;  // view has to be supported

                spaceCreateInfoWithPose.poseInReferenceSpace = baseSpacePose;
                CHECK(XR_SUCCESS == xrCreateReferenceSpace(sessionHandle, &spaceCreateInfoWithPose, &baseSpace));

                for (uint32_t i = 0; i < spaceCount; i++) {
                    spaceCreateInfoWithPose.poseInReferenceSpace = spacePoses[i];
                    CHECK(XR_SUCCESS == xrCreateReferenceSpace(sessionHandle, &spaceCreateInfoWithPose, &spaces[i]));

                    // The pose in the location is intentionally initialized to garbage as it will be set by the xrLocateSpacesKHR below
                    // If it would just be the identity, it might not catch all runtime errors where the location is not set by the runtime!
                    spacesData.locations.locations[i] = {0, {{3, 2, 1, 0}, {4.2f, 3.1f, 1.4f}}};
                }

                XrSpacesLocateInfoKHR locateInfo{XR_TYPE_SPACES_LOCATE_INFO_KHR};
                locateInfo.baseSpace = baseSpace;
                locateInfo.spaces = spaces;
                locateInfo.spaceCount = spaceCount;
                spacesData.locations.locationCount = spaceCount;
                locateInfo.time = time;

                XrResult result = xrLocateSpacesPFN(sessionHandle, &locateInfo, &spacesData.locations);
                {
                    INFO("xrLocateSpacesKHR");
                    CHECK(XR_SUCCESS == result);
                }

                // the main test:
                for (uint32_t i = 0; i < spaceCount; i++) {
                    // baseSpace given offset, space i given offset, space i expected pose, space i actual located pose
                    CAPTURE(baseSpacePose, spacePoses[i], expectedPoses[i], spacesData.locations.locations[i].pose);
                    ValidateSpaceLocation(spacesData.locations.locations[i], expectedPoses[i]);
                }
            };

            // Independent of tracking, it should be possible to get the relative pose of two
            // Spaces which are in the same reference space.
            XrPosef identity = Pose::Identity;

            {
                // Exercise identical spaces at the reference space origin.
                XrPosef spacePoses[] = {identity, identity};
                XrPosef expectedPoses[] = {identity, identity};
                LocateAndTest(identity, ArraySize(spacePoses), spacePoses, expectedPoses);
            }

            {
                // Exercise baseSpace and spaces created with the same offset from view space origin.
                XrPosef offset = {Quat::Identity, {1, 2, 3}};
                XrPosef spacePoses[] = {offset, offset};
                XrPosef expectedPoses[] = {identity, identity};
                LocateAndTest(offset, ArraySize(spacePoses), spacePoses, expectedPoses);
            }

            {
                // Exercise identical spaces which also have a rotation.
                XrPosef offset = {{Quat::FromAxisAngle({1, 0, 0}, DegToRad(45))}, {7, 8, 9}};
                XrPosef spacePoses[] = {offset, offset};
                XrPosef expectedPoses[] = {identity, identity};
                LocateAndTest(offset, ArraySize(spacePoses), spacePoses, expectedPoses);
            }

            {
                // Exercise different spaces without a rotation.
                XrPosef baseOffset = {Quat::Identity, {-1, -2, -3}};
                XrPosef spacePoses[] = {{Quat::Identity, {1, 2, 3}},  //
                                        {Quat::Identity, {2, 3, 4}}};
                XrPosef expectedPoses[] = {{Quat::Identity, {2, 4, 6}},  //
                                           {Quat::Identity, {3, 5, 7}}};
                LocateAndTest(baseOffset, ArraySize(spacePoses), spacePoses, expectedPoses);
            }

            XrQuaternionf rot_90_x = Quat::FromAxisAngle({1, 0, 0}, DegToRad(90));
            XrQuaternionf rot_m90_x = Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90));

            XrQuaternionf rot_90_y = Quat::FromAxisAngle({0, 1, 0}, DegToRad(90));

            {
                // Different positions, different orientations
                {
                    XrPosef baseOffset = {Quat::Identity, {0, 0, 0}};
                    XrPosef spacePoses[] = {{{rot_90_x}, {5, 0, 0}},  //
                                            {{rot_90_x}, {0, 5, 0}},  //
                                            {{rot_90_x}, {0, 0, 5}}};
                    XrPosef expectedPoses[] = {{{rot_90_x}, {5, 0, 0}},  //
                                               {{rot_90_x}, {0, 5, 0}},  //
                                               {{rot_90_x}, {0, 0, 5}}};
                    LocateAndTest(baseOffset, ArraySize(spacePoses), spacePoses, expectedPoses);
                }

                {
                    XrPosef baseOffset = {Quat::Identity, {-5, -5, -5}};
                    XrPosef spacePoses[] = {{{rot_90_y}, {5, 0, 0}},  //
                                            {{rot_90_y}, {0, 5, 0}},  //
                                            {{rot_90_y}, {0, 0, 5}}};
                    XrPosef expectedPoses[] = {{{rot_90_y}, {10, 5, 5}},  //
                                               {{rot_90_y}, {5, 10, 5}},  //
                                               {{rot_90_y}, {5, 5, 10}}};
                    LocateAndTest(baseOffset, ArraySize(spacePoses), spacePoses, expectedPoses);
                }

                {
                    XrPosef baseOffset = {{rot_90_y}, {7, -13, 17}};
                    XrPosef spacePoses[] = {{rot_m90_x, {2, 3, 5}}};
                    XrPosef expectedPoses[] = {{{-0.5f, -0.5f, -0.5f, 0.5f}, {12, 16, -5}}};
                    LocateAndTest(baseOffset, ArraySize(spacePoses), spacePoses, expectedPoses);
                }
            }
        }

        SECTION("locate all spaces")
        {
            for (XrSpace baseSpace : session.spaceVector) {
                // this test only uses locations from SpacesDat and ignores its other members
                SpacesData spacesData((uint32_t)session.spaceVector.size());

                XrSpacesLocateInfoKHR locateInfo{XR_TYPE_SPACES_LOCATE_INFO_KHR};
                locateInfo.baseSpace = baseSpace;
                locateInfo.spaces = session.spaceVector.data();
                locateInfo.spaceCount = (uint32_t)session.spaceVector.size();
                locateInfo.time = time;

                // here the baseSpace is included in the spaces to located
                CHECK(XR_SUCCESS == xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations));

                // Note: the actual relation between these spaces can be anything as they are based on different
                // reference spaces. So "location" can not be checked.
            }
        }

        SECTION("space velocities valid inputs")
        {
            SpacesData spacesData(3);

            // identical spaces:
            for (auto& space : spacesData.spacesVec) {
                result = xrCreateReferenceSpace(session, &spaceCreateInfo, &space);
                CHECK(result == XR_SUCCESS);
            }

            // baseSpace and spaces distinct
            XrSpace baseSpace = spacesData.spacesVec[0];
            XrSpace* spaces = &spacesData.spacesVec.data()[1];
            uint32_t count = (uint32_t)spacesData.spacesVec.size() - 1;
            spacesData.locations.locationCount = count;

            std::vector<XrSpaceVelocityDataKHR> velocityVec;
            velocityVec.insert(velocityVec.end(), count, {});

            XrSpaceVelocitiesKHR velocities{XR_TYPE_SPACE_VELOCITIES_KHR};
            velocities.velocityCount = count;
            velocities.velocities = velocityVec.data();

            XrSpacesLocateInfoKHR locateInfo{XR_TYPE_SPACES_LOCATE_INFO_KHR};
            locateInfo.baseSpace = baseSpace;
            locateInfo.spaces = spaces;
            locateInfo.spaceCount = count;
            spacesData.locations.next = &velocities;
            locateInfo.time = time;

            result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
            CHECK(result == XR_SUCCESS);

            for (auto& velocity : velocityVec) {
                bool velocitiesValid = ((velocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) != 0) &&
                                       ((velocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) != 0);

                // velocity between identical spaces must be known and zero
                REQUIRE(velocitiesValid);

                REQUIRE(velocity.linearVelocity == Vector::Approx(XrVector3f{0, 0, 0}));
                REQUIRE(velocity.angularVelocity == Vector::Approx(XrVector3f{0, 0, 0}));
            }
        }

        SECTION("space velocities invalid inputs")
        {
            SpacesData spacesData(3);

            // identical spaces:
            for (auto& space : spacesData.spacesVec) {
                result = xrCreateReferenceSpace(session, &spaceCreateInfo, &space);
                CHECK(result == XR_SUCCESS);
            }

            // baseSpace and spaces distinct
            XrSpace baseSpace = spacesData.spacesVec[0];
            XrSpace* spaces = &spacesData.spacesVec.data()[1];
            uint32_t count = (uint32_t)spacesData.spacesVec.size() - 1;
            spacesData.locations.locationCount = count;

            std::vector<XrSpaceVelocityDataKHR> velocityVec;
            velocityVec.insert(velocityVec.end(), count, {});

            XrSpaceVelocitiesKHR velocities{XR_TYPE_SPACE_VELOCITIES_KHR};
            velocities.velocityCount = count;
            velocities.velocities = velocityVec.data();

            XrSpacesLocateInfoKHR locateInfo{XR_TYPE_SPACES_LOCATE_INFO_KHR};
            locateInfo.baseSpace = baseSpace;
            locateInfo.spaces = spaces;
            locateInfo.spaceCount = count;
            spacesData.locations.next = &velocities;
            locateInfo.time = time;

            // Exercise velocityCount < spaceCount
            velocities.velocityCount = count - 1;
            result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
            CHECK(result == XR_ERROR_VALIDATION_FAILURE);
            velocities.velocityCount = count;

            // Exercise velocityCount > spaceCount, locationCount
            // Decrease both spaceCount and locationCount to ensure XR_ERROR_VALIDATION_FAILURE is not returned due to difference in value.
            locateInfo.spaceCount = count - 1;
            spacesData.locations.locationCount = count - 1;
            result = xrLocateSpacesPFN(session, &locateInfo, &spacesData.locations);
            CHECK(result == XR_ERROR_VALIDATION_FAILURE);
            locateInfo.spaceCount = count;
            spacesData.locations.locationCount = count;
        }
    }

    TEST_CASE("xrLocateSpaces", "[XR_VERSION_1_1]")
    {
        SharedLocateSpaces(kPromotedCoreRequirements);
    }

    TEST_CASE("XR_KHR_locate_spaces", "[XR_KHR_locate_spaces]")
    {
        SharedLocateSpaces(kExtensionRequirements);
    }
}  // namespace Conformance
