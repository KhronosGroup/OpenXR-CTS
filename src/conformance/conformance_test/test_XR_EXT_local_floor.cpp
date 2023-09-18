// Copyright (c) 2019-2023, The Khronos Group Inc.
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

#include "utilities/utils.h"
#include "conformance_utils.h"
#include "two_call.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <openxr/openxr.h>
#include <algorithm>

using namespace Conformance;

namespace Conformance
{
    TEST_CASE("XR_EXT_local_floor", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_LOCAL_FLOOR_EXTENSION_NAME)) {
            return;
        }

        SECTION("Extension not enabled")
        {
            if (!globalData.enabledInstanceExtensionNames.contains(XR_EXT_LOCAL_FLOOR_EXTENSION_NAME)) {
                AutoBasicInstance instance;
                AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

                std::vector<XrReferenceSpaceType> refSpaceTypes =
                    CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);
                REQUIRE(std::find(refSpaceTypes.begin(), refSpaceTypes.end(), XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT) ==
                        refSpaceTypes.end());
            }
        }

        SECTION("Validate creation")
        {
            AutoBasicInstance instance({XR_EXT_LOCAL_FLOOR_EXTENSION_NAME});
            AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

            std::vector<XrReferenceSpaceType> refSpaceTypes = CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);
            REQUIRE_THAT(refSpaceTypes, Catch::Matchers::Contains(XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT));

            XrReferenceSpaceCreateInfo localFloorCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            localFloorCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
            localFloorCreateInfo.poseInReferenceSpace = XrPosefCPP();

            XrSpace localFloor = XR_NULL_HANDLE;
            REQUIRE_RESULT(xrCreateReferenceSpace(session, &localFloorCreateInfo, &localFloor), XR_SUCCESS);
        }

        SECTION("Validate correctness")
        {
            AutoBasicInstance instance({XR_EXT_LOCAL_FLOOR_EXTENSION_NAME});
            AutoBasicSession session(AutoBasicSession::createInstance | AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                         AutoBasicSession::createSwapchains | AutoBasicSession::createSpaces,
                                     instance);

            // Get frames iterating to the point of app focused state. This will draw frames along the way.
            FrameIterator frameIterator(&session);
            frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

            // Render one frame to get a predicted display time for the xrLocateSpace calls.
            FrameIterator::RunResult runResult = frameIterator.SubmitFrame();
            REQUIRE(runResult == FrameIterator::RunResult::Success);

            // If stage space is defined, then LOCAL_FLOOR height off the floor must match STAGE
            std::vector<XrReferenceSpaceType> refSpaceTypes = CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);
            if (std::find(refSpaceTypes.begin(), refSpaceTypes.end(), XR_REFERENCE_SPACE_TYPE_STAGE) != refSpaceTypes.end()) {
                XrSpace viewSpace = XR_NULL_HANDLE;
                XrSpace localSpace = XR_NULL_HANDLE;
                XrSpace stageSpace = XR_NULL_HANDLE;
                XrSpace localFloorSpace = XR_NULL_HANDLE;
                XrSpace localFloorAltSpace = XR_NULL_HANDLE;

                XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                createInfo.poseInReferenceSpace = XrPosefCPP();

                createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &viewSpace), XR_SUCCESS);

                createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
                REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &localSpace), XR_SUCCESS);

                createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
                REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &stageSpace), XR_SUCCESS);

                createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
                REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &localFloorSpace), XR_SUCCESS);

                XrSpaceLocation stageLoc{XR_TYPE_SPACE_LOCATION};
                REQUIRE_RESULT(xrLocateSpace(stageSpace, localSpace, frameIterator.frameState.predictedDisplayTime, &stageLoc), XR_SUCCESS);

                float floorOffset = stageLoc.pose.position.y;

                XrReferenceSpaceCreateInfo localFloorCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                localFloorCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
                localFloorCreateInfo.poseInReferenceSpace = {{0.f, 0.f, 0.f, 1.f}, {0.f, floorOffset, 0.f}};
                REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &localFloorAltSpace), XR_SUCCESS);

                XrSpaceLocation localFloorViewLoc{XR_TYPE_SPACE_LOCATION};
                REQUIRE_RESULT(xrLocateSpace(localFloorSpace, viewSpace, frameIterator.frameState.predictedDisplayTime, &localFloorViewLoc),
                               XR_SUCCESS);
                XrSpaceLocation localFloorAltViewLoc{XR_TYPE_SPACE_LOCATION};
                REQUIRE_RESULT(
                    xrLocateSpace(localFloorAltSpace, viewSpace, frameIterator.frameState.predictedDisplayTime, &localFloorAltViewLoc),
                    XR_SUCCESS);

                // Definition of local floor space requires these to be the same.

                REQUIRE(localFloorViewLoc.locationFlags == localFloorAltViewLoc.locationFlags);

                constexpr float epsilon = 0.1f;
                REQUIRE(localFloorViewLoc.pose.position.x == Catch::Approx(localFloorAltViewLoc.pose.position.x).margin(epsilon));
                REQUIRE(localFloorViewLoc.pose.position.y == Catch::Approx(localFloorAltViewLoc.pose.position.y).margin(epsilon));
                REQUIRE(localFloorViewLoc.pose.position.z == Catch::Approx(localFloorAltViewLoc.pose.position.z).margin(epsilon));

                REQUIRE(localFloorViewLoc.pose.orientation.x == Catch::Approx(localFloorAltViewLoc.pose.orientation.x).margin(epsilon));
                REQUIRE(localFloorViewLoc.pose.orientation.y == Catch::Approx(localFloorAltViewLoc.pose.orientation.y).margin(epsilon));
                REQUIRE(localFloorViewLoc.pose.orientation.z == Catch::Approx(localFloorAltViewLoc.pose.orientation.z).margin(epsilon));
                REQUIRE(localFloorViewLoc.pose.orientation.w == Catch::Approx(localFloorAltViewLoc.pose.orientation.w).margin(epsilon));
            }
        }
    }
}  // namespace Conformance
