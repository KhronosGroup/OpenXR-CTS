// Copyright (c) 2019-2024, The Khronos Group Inc.
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

#include "catch2/catch_approx.hpp"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/bitmask_to_string.h"
#include "utilities/types_and_constants.h"
#include "utilities/xrduration_literals.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <cmath>

namespace Conformance
{
    TEST_CASE("xrLocateSpace", "")
    {
        // Get a session started.
        AutoBasicSession session(AutoBasicSession::createInstance | AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                 AutoBasicSession::createSwapchains | AutoBasicSession::createSpaces);

        // Get frames iterating to the point of app focused state. This will draw frames along the way.
        FrameIterator frameIterator(&session);
        frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

        // Render one frame to get a predicted display time for the xrLocateSpace calls.
        FrameIterator::RunResult runResult = frameIterator.SubmitFrame();
        REQUIRE(runResult == FrameIterator::RunResult::Success);

        XrResult result;

        // ugly magic number:
        // some variance due to numeric inaccuracies is OK
        constexpr float epsilon = 0.001f;

        // compare the calculated pose with the expected pose
        auto ValidateSpaceLocation = [epsilon](XrSpaceLocation& spaceLocation, XrPosef& expectedPose) -> void {
            CAPTURE(XrSpaceLocationFlagsCPP(spaceLocation.locationFlags));
            CHECK((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0);
            CHECK((spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0);

            static auto QuaterionsAreEquivalent = [](const XrQuaternionf& q1, const XrQuaternionf& q2, float epsilon) {
                return std::abs(q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w) == Catch::Approx(1.f).margin(epsilon);
            };

            if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                REQUIRE(spaceLocation.pose.position.x == Catch::Approx(expectedPose.position.x).margin(epsilon));
                REQUIRE(spaceLocation.pose.position.y == Catch::Approx(expectedPose.position.y).margin(epsilon));
                REQUIRE(spaceLocation.pose.position.z == Catch::Approx(expectedPose.position.z).margin(epsilon));
            }
            if (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
                CHECK(QuaterionsAreEquivalent(spaceLocation.pose.orientation, expectedPose.orientation, epsilon));
            }
        };

        // Note both spaces are in the same reference space, so the time should be irrelevant for the location
        // which is important to get the offset between the spaces right.
        XrReferenceSpaceCreateInfo spaceCreateInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;  // view has to be supported
        spaceCreateInfo.poseInReferenceSpace = XrPosefCPP();

        // initialized to NULL to avoid compiler warnings later
        XrSpace spaceA = XR_NULL_HANDLE_CPP;
        XrSpace spaceB = XR_NULL_HANDLE_CPP;
        XrSpaceLocation location = {XR_TYPE_SPACE_LOCATION, nullptr, 0, XrPosefCPP()};
        XrTime time = frameIterator.frameState.predictedDisplayTime;
        CHECK(time != 0);

        SECTION("valid inputs")
        {
            // two identical spaces:
            result = xrCreateReferenceSpace(session, &spaceCreateInfo, &spaceA);
            CHECK(result == XR_SUCCESS);
            result = xrCreateReferenceSpace(session, &spaceCreateInfo, &spaceB);
            CHECK(result == XR_SUCCESS);

            // Exercise the predicted display time
            result = xrLocateSpace(spaceA, spaceB, time, &location);
            CHECK(result == XR_SUCCESS);

            // Exercise 40ms ago (or the first valid time, whichever is later)
            result = xrLocateSpace(spaceA, spaceB, std::max(time - 40_xrMilliseconds, 1_xrNanoseconds), &location);
            CHECK(result == XR_SUCCESS);

            // Exercise 1s ago (or the first valid time, whichever is later)
            result = xrLocateSpace(spaceA, spaceB, std::max(time - 1_xrSeconds, 1_xrNanoseconds), &location);
            CHECK(result == XR_SUCCESS);

            // cleanup
            REQUIRE(XR_SUCCESS == xrDestroySpace(spaceA));
            REQUIRE(XR_SUCCESS == xrDestroySpace(spaceB));
        }

        SECTION("wrong inputs")
        {
            // two identical spaces:
            result = xrCreateReferenceSpace(session, &spaceCreateInfo, &spaceA);
            CHECK(result == XR_SUCCESS);
            result = xrCreateReferenceSpace(session, &spaceCreateInfo, &spaceB);
            CHECK(result == XR_SUCCESS);

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // Exercise NULL handle.
                result = xrLocateSpace(XR_NULL_HANDLE_CPP, spaceB, time, &location);
                CHECK(result == XR_ERROR_HANDLE_INVALID);

                // Exercise another NULL handle.
                result = xrLocateSpace(spaceA, XR_NULL_HANDLE_CPP, time, &location);
                CHECK(result == XR_ERROR_HANDLE_INVALID);

                // Exercise invalid handle.
                result = xrLocateSpace(GlobalData().invalidSpace, spaceB, time, &location);
                CHECK(result == XR_ERROR_HANDLE_INVALID);

                // Exercise another invalid handle.
                result = xrLocateSpace(spaceA, GlobalData().invalidSpace, time, &location);
                CHECK(result == XR_ERROR_HANDLE_INVALID);
            }

            // Exercise 0 as an invalid time.
            result = xrLocateSpace(spaceA, spaceB, (XrTime)0, &location);
            CHECK(result == XR_ERROR_TIME_INVALID);

            // Exercise negative values as an invalid time.
            result = xrLocateSpace(spaceA, spaceB, (XrTime)-42, &location);
            CHECK(result == XR_ERROR_TIME_INVALID);

            // cleanup
            REQUIRE(XR_SUCCESS == xrDestroySpace(spaceA));
            REQUIRE(XR_SUCCESS == xrDestroySpace(spaceB));
        }

        SECTION("space location math")
        {
            // to capture only the handle and not the full object below
            XrSession sessionHandle = session.GetSession();

            // Creates a space for the each of the two input poses, locates them and compares the result
            // with the expected pose. Intention is to check the math behind xrLocateSpace.
            auto LocateAndTest = [ValidateSpaceLocation, sessionHandle, time](XrPosef poseSpaceA, XrPosef poseSpaceB,
                                                                              XrPosef expectedResult) -> void {
                XrSpace spaceA = XR_NULL_HANDLE_CPP;
                XrSpace spaceB = XR_NULL_HANDLE_CPP;

                // The pose in the location is intentionally garbage as it will be set by the xrLocateSpace below
                // If it would just be the identity, it might not catch all runtime errors where the location is not set by the runtime!
                XrSpaceLocation location = {XR_TYPE_SPACE_LOCATION, nullptr, 0, {{3, 2, 1, 0}, {4.2f, 3.1f, 1.4f}}};

                XrReferenceSpaceCreateInfo spaceCreateInfoWithPose = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                spaceCreateInfoWithPose.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;  // view has to be supported

                spaceCreateInfoWithPose.poseInReferenceSpace = poseSpaceA;
                CHECK(XR_SUCCESS == xrCreateReferenceSpace(sessionHandle, &spaceCreateInfoWithPose, &spaceA));

                spaceCreateInfoWithPose.poseInReferenceSpace = poseSpaceB;
                CHECK(XR_SUCCESS == xrCreateReferenceSpace(sessionHandle, &spaceCreateInfoWithPose, &spaceB));

                XrResult result = xrLocateSpace(spaceA, spaceB, time, &location);
                {
                    INFO("xrLocateSpace");
                    CHECK(XR_SUCCESS == result);
                }

                // the main test:
                if (XR_SUCCESS == result) {
                    // Capture the three poses and the result to generate useful error messages in case the result is not
                    // identical to the expected values.
                    CAPTURE(poseSpaceA.orientation, poseSpaceA.position, poseSpaceB.orientation, poseSpaceB.position,
                            expectedResult.orientation, expectedResult.position, location.pose.orientation, location.pose.position);
                    ValidateSpaceLocation(location, expectedResult);
                }

                REQUIRE(XR_SUCCESS == xrDestroySpace(spaceA));
                REQUIRE(XR_SUCCESS == xrDestroySpace(spaceB));
            };

            // Independent on tracking, it should be possible to get the relative pose of two
            // Spaces which are in the same reference space.
            XrPosef identity = XrPosefCPP();

            // Exercise identical spaces at the reference space origin.
            LocateAndTest(identity, identity, identity);

            // Exercise identical spaces which are not located at the origin of the reference space.
            XrPosef space = {Quat::Identity, {1, 2, 3}};
            LocateAndTest(space, space, identity);

            // Exercise identical spaces which also have a rotation.
            space = {{Quat::FromAxisAngle({1, 0, 0}, Math::DegToRad(45))}, {7, 8, 9}};
            LocateAndTest(space, space, identity);

            // Exercise different spaces without a rotation.
            LocateAndTest({Quat::Identity, {1, 2, 3}}, {Quat::Identity, {-1, -2, -3}}, {Quat::Identity, {2, 4, 6}});

            // Another test with different spaces.
            LocateAndTest({Quat::Identity, {-1, -2, -3}}, {Quat::Identity, {1, 2, 3}}, {Quat::Identity, {-2, -4, -6}});

            XrQuaternionf rot_90_x = Quat::FromAxisAngle({1, 0, 0}, Math::DegToRad(90));
            XrQuaternionf rot_m90_x = Quat::FromAxisAngle({1, 0, 0}, Math::DegToRad(-90));

            XrQuaternionf rot_90_y = Quat::FromAxisAngle({0, 1, 0}, Math::DegToRad(90));
            XrQuaternionf rot_m90_y = Quat::FromAxisAngle({0, 1, 0}, Math::DegToRad(-90));

            // Different positions, different orientations
            LocateAndTest({{0, 0, 0, 1}, {0, 0, 0}}, {{rot_90_x}, {5, 0, 0}}, {{rot_m90_x}, {-5, 0, 0}});
            LocateAndTest({{0, 0, 0, 1}, {0, 0, 0}}, {{rot_90_x}, {0, 5, 0}}, {{rot_m90_x}, {0, 0, 5}});
            LocateAndTest({{0, 0, 0, 1}, {0, 0, 0}}, {{rot_90_x}, {0, 0, 5}}, {{rot_m90_x}, {0, -5, 0}});

            LocateAndTest({{0, 0, 0, 1}, {1, 0, 0}}, {{rot_90_y}, {5, 0, 0}}, {{rot_m90_y}, {0, 0, -4}});
            LocateAndTest({{0, 0, 0, 1}, {1, 0, 0}}, {{rot_90_y}, {0, 5, 0}}, {{rot_m90_y}, {0, -5, 1}});
            LocateAndTest({{0, 0, 0, 1}, {1, 0, 0}}, {{rot_90_y}, {0, 0, 5}}, {{rot_m90_y}, {5, 0, 1}});

            LocateAndTest({rot_m90_x, {2, 3, 5}}, {{rot_90_y}, {7, -13, 17}}, {{-0.5f, -0.5f, -0.5f, 0.5f}, {12, 16, -5}});
        }

        SECTION("locate all spaces")
        {
            for (XrSpace space1 : session.spaceVector) {
                for (XrSpace space2 : session.spaceVector) {
                    location = {XR_TYPE_SPACE_LOCATION, nullptr, 0, XrPosefCPP()};
                    CHECK(XR_SUCCESS == xrLocateSpace(space1, space2, time, &location));

                    // Note: the actual relation between these spaces can be anything as they are based on different
                    // reference spaces. So "location" can not be checked.
                }
            }
        }
    }
}  // namespace Conformance
