// Copyright (c) 2019-2020 The Khronos Group Inc.
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

#include "utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "matchers.h"
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <thread>
#include <catch2/catch.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

#define AS_LIST(name, val) name,
constexpr XrViewConfigurationType KnownViewTypes[] = {XR_LIST_ENUM_XrViewConfigurationType(AS_LIST)};
#undef AS_LIST

namespace Conformance
{

    TEST_CASE("Session State", "")
    {
        AutoBasicSession session(AutoBasicSession::createSession);
        REQUIRE_MSG(session != XR_NULL_HANDLE_CPP,
                    "If this (XrSession creation) fails, ensure the runtime is configured and the AR/VR device is present.");

        auto tryReadEvent = [&](XrEventDataBuffer* evt) {
            *evt = {XR_TYPE_EVENT_DATA_BUFFER};
            XrResult res;
            XRC_CHECK_THROW_XRCMD(res = xrPollEvent(session.GetInstance(), evt));
            return res == XR_SUCCESS;
        };

        auto tryGetNextSessionState = [&](XrEventDataSessionStateChanged* evt) {
            XrEventDataBuffer buffer;
            while (tryReadEvent(&buffer)) {
                if (buffer.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    *evt = *reinterpret_cast<XrEventDataSessionStateChanged*>(&buffer);
                    return true;
                }
            }

            return false;
        };

        auto waitForNextSessionState = [&](XrEventDataSessionStateChanged* evt, std::chrono::nanoseconds duration = 1_sec) {
            CountdownTimer countdown(duration);
            while (!countdown.IsTimeUp()) {
                if (tryGetNextSessionState(evt)) {
                    return true;
                }

                std::this_thread::sleep_for(5_ms);
            }

            return false;
        };

        XrEventDataSessionStateChanged evt;
        REQUIRE(waitForNextSessionState(&evt) == true);
        REQUIRE_MSG(evt.state == XR_SESSION_STATE_IDLE, "Unexpected session state " << evt.state);

        REQUIRE(waitForNextSessionState(&evt) == true);
        REQUIRE_MSG(evt.state == XR_SESSION_STATE_READY, "Unexpected session state " << evt.state);

        // Ensure unsupported view configuration types fail.
        {
            // Get the list of supported view configurations
            uint32_t viewCount = 0;
            REQUIRE(XR_SUCCESS == xrEnumerateViewConfigurations(session.instance, session.systemId, 0, &viewCount, nullptr));
            std::vector<XrViewConfigurationType> runtimeViewTypes(viewCount);
            REQUIRE(XR_SUCCESS ==
                    xrEnumerateViewConfigurations(session.instance, session.systemId, viewCount, &viewCount, runtimeViewTypes.data()));

            for (XrViewConfigurationType viewType : KnownViewTypes) {
                CAPTURE(viewType);

                // Is this enum valid, check against enabled extensions.
                bool valid = IsViewConfigurationTypeEnumValid(viewType);

                const bool isSupportedType =
                    std::find(runtimeViewTypes.begin(), runtimeViewTypes.end(), viewType) != runtimeViewTypes.end();

                if (!valid) {
                    CHECK_MSG(valid == isSupportedType, "Can not support invalid view configuration type");
                }

                // Skip this view config.
                if (isSupportedType) {
                    continue;
                }

                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = viewType;
                XrResult result = xrBeginSession(session, &beginInfo);
                REQUIRE_THAT(result, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED}));
                if (!valid && result == XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED) {
                    WARN(
                        "Runtime accepted an invalid enum value as unsupported, which makes it harder for apps to reason about the error.");
                }
            }
        }

        XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
        beginInfo.primaryViewConfigurationType = GetGlobalData().GetOptions().viewConfigurationValue;
        XRC_CHECK_THROW_XRCMD(xrBeginSession(session, &beginInfo));

        if (GetGlobalData().IsUsingGraphicsPlugin()) {
            // Runtime should not transition from READY to SYNCHRONIZED until one or more frames have been submitted.
            // The exception is if the runtime is transitioning to STOPPING, which should not happen
            // during conformance testing. This will wait 1 second before assuming no such incorrect event will come.
            REQUIRE_MSG(tryGetNextSessionState(&evt) == false, "Premature progression from READY to SYNCHRONIZED state");
        }

        auto submitFrame = [&]() {
            XrFrameState frameState{XR_TYPE_FRAME_STATE};
            XRC_CHECK_THROW_XRCMD(xrWaitFrame(session, nullptr, &frameState));
            XRC_CHECK_THROW_XRCMD(xrBeginFrame(session, nullptr));

            XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
            frameEndInfo.displayTime = frameState.predictedDisplayTime;
            frameEndInfo.environmentBlendMode = GetGlobalData().GetOptions().environmentBlendModeValue;
            XRC_CHECK_THROW_XRCMD(xrEndFrame(session, &frameEndInfo));
        };

        auto submitFramesUntilSessionState = [&](XrSessionState expectedSessionState, std::chrono::nanoseconds duration = 30_sec) {
            CAPTURE(expectedSessionState);

            CountdownTimer countdown(duration);
            while (!countdown.IsTimeUp()) {
                if (tryGetNextSessionState(&evt)) {
                    REQUIRE(evt.state == expectedSessionState);
                    return;
                }
                submitFrame();
            }

            FAIL("Failed to reach expected session state");
        };

        submitFramesUntilSessionState(XR_SESSION_STATE_SYNCHRONIZED);
        submitFramesUntilSessionState(XR_SESSION_STATE_VISIBLE);
        submitFramesUntilSessionState(XR_SESSION_STATE_FOCUSED);

        // Runtime should only allow ending a session in the STOPPING state.
        REQUIRE(XR_ERROR_SESSION_NOT_STOPPING == xrEndSession(session));

        XRC_CHECK_THROW_XRCMD(xrRequestExitSession(session));

        submitFramesUntilSessionState(XR_SESSION_STATE_VISIBLE);
        submitFramesUntilSessionState(XR_SESSION_STATE_SYNCHRONIZED);
        submitFramesUntilSessionState(XR_SESSION_STATE_STOPPING);

        // Runtime should not transition from STOPPING to IDLE until the session has been ended.
        REQUIRE_MSG(tryGetNextSessionState(&evt) == false, "Premature progression from STOPPING to IDLE state");

        XRC_CHECK_THROW_XRCMD(xrEndSession(session));

        submitFramesUntilSessionState(XR_SESSION_STATE_IDLE);
        submitFramesUntilSessionState(XR_SESSION_STATE_EXITING);
    }
}  // namespace Conformance
