// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#include "conformance_utils.h"
#include "conformance_framework.h"
#include "matchers.h"
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

#define AS_LIST(name, val) {name, #name},
static constexpr std::pair<XrViewConfigurationType, const char*> KnownViewTypes[] = {XR_LIST_ENUM_XrViewConfigurationType(AS_LIST)};
#undef AS_LIST

namespace Conformance
{
    TEST_CASE("xrLocateViews", "")
    {
        GlobalData& globalData = GetGlobalData();

        // how long the test should wait for the app to get focus: 10 seconds in release, 1hr in debug builds.
        auto timeout = (globalData.options.debugMode ? 3600s : 10s);
        CAPTURE(timeout);

        // Get a session started.
        AutoBasicSession session(AutoBasicSession::createInstance | AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                 AutoBasicSession::createSwapchains | AutoBasicSession::createSpaces);

        // Get frames iterating to the point of app focused state. This will draw frames along the way.
        FrameIterator frameIterator(&session);
        FrameIterator::RunResult runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED, timeout);
        REQUIRE(runResult == FrameIterator::RunResult::Success);

        // Render one frame to get a predicted display time for the xrLocateViews calls.
        runResult = frameIterator.SubmitFrame();
        REQUIRE(runResult == FrameIterator::RunResult::Success);

        XrResult result;

        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.space = session.spaceVector.front();
        XrTime time = frameIterator.frameState.predictedDisplayTime;
        CHECK(time != 0);
        locateInfo.displayTime = time;
        locateInfo.viewConfigurationType = globalData.GetOptions().viewConfigurationValue;

        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCount = (uint32_t)session.viewConfigurationViewVector.size();

        CAPTURE(viewCount);
        SECTION("valid inputs")
        {
            std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
            uint32_t viewCountOut = 0;

            CAPTURE(locateInfo.displayTime);
            CHECK(XR_SUCCESS == xrLocateViews(session, &locateInfo, &viewState, viewCount, &viewCountOut, views.data()));

            CHECK(viewCountOut == viewCount);
        }
        SECTION("invalid inputs")
        {
            std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
            uint32_t viewCountOut = 0;

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // Exercise NULL session handle.
                CHECK(xrLocateViews(XR_NULL_HANDLE_CPP, &locateInfo, &viewState, viewCount, &viewCountOut, views.data()) ==
                      XR_ERROR_HANDLE_INVALID);

                // Exercise invalid session handle.
                CHECK(xrLocateViews(GlobalData().invalidSession, &locateInfo, &viewState, viewCount, &viewCountOut, views.data()) ==
                      XR_ERROR_HANDLE_INVALID);
            }

            {
                INFO("Exercise 0 as an invalid time.");
                locateInfo.displayTime = 0;
                CAPTURE(locateInfo.displayTime);
                CHECK(XR_ERROR_TIME_INVALID == xrLocateViews(session, &locateInfo, &viewState, viewCount, &viewCountOut, views.data()));
            }

            {
                INFO("Exercise negative values as an invalid time.");
                locateInfo.displayTime = (XrTime)-42;
                CAPTURE(locateInfo.displayTime);
                CHECK(XR_ERROR_TIME_INVALID == xrLocateViews(session, &locateInfo, &viewState, viewCount, &viewCountOut, views.data()));
            }
        }
        SECTION("all known view types")
        {
            // Ensure unsupported view configuration types fail and supported types pass

            // Get the list of supported view configurations
            uint32_t viewConfigCount = 0;
            REQUIRE(XR_SUCCESS == xrEnumerateViewConfigurations(session.instance, session.systemId, 0, &viewConfigCount, nullptr));
            std::vector<XrViewConfigurationType> runtimeViewTypes(viewConfigCount);
            REQUIRE(XR_SUCCESS == xrEnumerateViewConfigurations(session.instance, session.systemId, viewConfigCount, &viewConfigCount,
                                                                runtimeViewTypes.data()));

            CAPTURE(locateInfo.displayTime);

            for (auto viewTypeAndName : KnownViewTypes) {
                XrViewConfigurationType viewType = viewTypeAndName.first;
                CAPTURE(viewType);
                CAPTURE(viewTypeAndName.second);

                // Is this enum valid, check against enabled extensions.
                bool valid = IsViewConfigurationTypeEnumValid(viewType);

                const bool isSupportedType =
                    std::find(runtimeViewTypes.begin(), runtimeViewTypes.end(), viewType) != runtimeViewTypes.end();
                CAPTURE(valid);
                CAPTURE(isSupportedType);

                if (!valid) {
                    INFO("Not a valid view configuration type given the enabled extensions")
                    CHECK_MSG(!isSupportedType, "Cannot support invalid view configuration type");
                }

                locateInfo.viewConfigurationType = viewType;
                if (isSupportedType) {
                    // Supported but we don't have the corresponding view count immediately at hand
                    // So, we look it up.
                    uint32_t expectedViewCount = 0;
                    REQUIRE(XR_SUCCESS == xrEnumerateViewConfigurationViews(session.GetInstance(), session.GetSystemId(), viewType, 0,
                                                                            &expectedViewCount, nullptr));
                    INFO("Calling xrLocateViews with the noted viewType, which is claimed to be supported");
                    uint32_t viewCountOut = 0;
                    std::vector<XrView> views(expectedViewCount, {XR_TYPE_VIEW});
                    CHECK(XR_SUCCESS == xrLocateViews(session, &locateInfo, &viewState, expectedViewCount, &viewCountOut, views.data()));
                }
                else {
                    // Not a supported type, so call should fail, regardless of the array size.
                    INFO("Calling xrLocateViews with the noted viewType, which is claimed to be not supported");
                    uint32_t viewCountOut = 0;
                    std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});

                    result = xrLocateViews(session, &locateInfo, &viewState, viewCount, &viewCountOut, views.data());
                    REQUIRE_THAT(result, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED}));
                    if (!valid && result == XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED) {
                        WARN(
                            "Runtime accepted an invalid enum value as unsupported, which makes it harder for apps to reason about the error.");
                    }
                }
            }
        }

        // Leave
        result = xrRequestExitSession(session);
        CHECK(result == XR_SUCCESS);

        runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_STOPPING, timeout);
        CHECK(runResult == FrameIterator::RunResult::Success);
    }
}  // namespace Conformance
