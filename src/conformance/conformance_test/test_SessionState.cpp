// Copyright (c) 2019-2026 The Khronos Group Inc.
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

#include "conformance_framework.h"
#include "conformance_options.h"
#include "conformance_utils.h"
#include "utilities/types_and_constants.h"
#include "utilities/throw_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

#include <chrono>
#include <thread>

namespace Conformance
{
    static bool tryReadEvent(XrInstance instance, XrEventDataBuffer* evt)
    {
        *evt = {XR_TYPE_EVENT_DATA_BUFFER};
        XrResult res;
        XRC_CHECK_THROW_XRCMD(res = xrPollEvent(instance, evt));
        return res == XR_SUCCESS;
    }

    static bool tryGetNextSessionState(XrInstance instance, XrEventDataSessionStateChanged* evt)
    {
        XrEventDataBuffer buffer;
        while (tryReadEvent(instance, &buffer)) {
            if (buffer.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                *evt = *reinterpret_cast<XrEventDataSessionStateChanged*>(&buffer);
                return true;
            }
        }
        return false;
    }

    static bool waitForNextSessionState(XrInstance instance, XrEventDataSessionStateChanged* evt, std::chrono::nanoseconds duration = 1s)
    {
        CountdownTimer countdown(duration);
        while (!countdown.IsTimeUp()) {
            if (tryGetNextSessionState(instance, evt)) {
                return true;
            }

            std::this_thread::sleep_for(5ms);
        }

        return false;
    }

    static void submitFrame(XrSession session, XrEnvironmentBlendMode ebm)
    {
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        XRC_CHECK_THROW_XRCMD(xrWaitFrame(session, nullptr, &frameState));
        XRC_CHECK_THROW_XRCMD(xrBeginFrame(session, nullptr));

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = ebm;
        XRC_CHECK_THROW_XRCMD(xrEndFrame(session, &frameEndInfo));
    }

    static void submitFramesUntilSessionState(XrInstance instance, XrSession session, XrEnvironmentBlendMode ebm,
                                              XrSessionState expectedSessionState, std::chrono::nanoseconds duration = 30s)
    {
        CAPTURE(expectedSessionState);

        CountdownTimer countdown(duration);
        while (!countdown.IsTimeUp()) {
            XrEventDataSessionStateChanged evt;
            if (tryGetNextSessionState(instance, &evt)) {
                REQUIRE(evt.state == expectedSessionState);
                return;
            }
            submitFrame(session, ebm);
        }

        FAIL("Failed to reach expected session state");
    }

    TEST_CASE("SessionState", "")
    {
        AutoBasicInstance instance;

        SECTION("Cycle through all states")
        {
            AutoBasicSession session(AutoBasicSession::createSession, instance);
            XrEnvironmentBlendMode ebm = session.PreferredEnvironmentBlendMode();

            REQUIRE(session != XR_NULL_HANDLE_CPP);

            XrEventDataSessionStateChanged evt{};
            XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
            beginInfo.primaryViewConfigurationType = Options::Get().viewConfigurationValue;

            SECTION("Normal operation")
            {
                {
                    INFO("Advancing to IDLE");
                    REQUIRE(waitForNextSessionState(instance, &evt) == true);
                    REQUIRE(evt.state == XR_SESSION_STATE_IDLE);
                }

                {
                    INFO("Advancing to READY");
                    REQUIRE(waitForNextSessionState(instance, &evt) == true);
                    REQUIRE(evt.state == XR_SESSION_STATE_READY);
                }

                REQUIRE(XR_SUCCESS == xrBeginSession(session, &beginInfo));

                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_SYNCHRONIZED);
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_VISIBLE);
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_FOCUSED);

                // Runtime should only allow ending a session in the STOPPING state.
                REQUIRE(XR_ERROR_SESSION_NOT_STOPPING == xrEndSession(session));

                REQUIRE(XR_SUCCESS == xrRequestExitSession(session));

                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_VISIBLE);
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_SYNCHRONIZED);
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_STOPPING);

                // Runtime should not transition from STOPPING to IDLE until the session has been ended.
                // This will wait 1 second before assuming no such incorrect event will come.
                REQUIRE_MSG(waitForNextSessionState(instance, &evt) == false, "Premature progression from STOPPING to IDLE state");

                REQUIRE(XR_SUCCESS == xrEndSession(session));

                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_IDLE);

                // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#session-lifecycle
                // If the runtime determines that its use of this XR session has
                // concluded, it will transition the session state from
                // XR_SESSION_STATE_IDLE to XR_SESSION_STATE_EXITING.

                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_EXITING);
            }

            SECTION("Try calls out of turn")
            {

                XrFrameState frameState{XR_TYPE_FRAME_STATE};

                SECTION("xrWaitFrame before polling session state IDLE")
                {
                    INFO("xrWaitFrame only valid when session running");
                    REQUIRE(XR_ERROR_SESSION_NOT_RUNNING == xrWaitFrame(session, nullptr, &frameState));
                }

                // We have not polled state IDLE yet, but the session may have already moved to that state.
                // So, cannot assert anything about behaviour under various XrSessionState here:
                // session might be in mystery unknown state before IDLE, IDLE, or READY.

                INFO("Polling events until receiving IDLE");
                REQUIRE(waitForNextSessionState(instance, &evt) == true);
                REQUIRE(evt.state == XR_SESSION_STATE_IDLE);

                SECTION("xrWaitFrame after polling session state IDLE")
                {
                    INFO("xrWaitFrame only valid when session running");
                    CHECK(XR_ERROR_SESSION_NOT_RUNNING == xrWaitFrame(session, nullptr, &frameState));
                }

                // We have polled state IDLE but not READY yet. However,the session may have already moved to that state.
                // So, cannot assert anything about behaviour under various XrSessionState here, since we may be in either
                // IDLE or READY.

                INFO("Polling events until receiving READY");
                REQUIRE(waitForNextSessionState(instance, &evt) == true);
                REQUIRE(evt.state == XR_SESSION_STATE_READY);

                SECTION("xrWaitFrame in READY")
                {
                    INFO("xrWaitFrame only valid when session running");
                    // If test hangs here, this is an error in the runtime!
                    // It should not actually wait, but error out immediately.
                    REQUIRE(xrWaitFrame(session, nullptr, &frameState) == XR_ERROR_SESSION_NOT_RUNNING);
                }

                INFO("xrBeginSession");
                REQUIRE(XR_SUCCESS == xrBeginSession(session, &beginInfo));

                if (GetGlobalData().IsUsingGraphicsPlugin()) {
                    // Runtime should not transition from READY to SYNCHRONIZED until one or more frames have been submitted.
                    // The exception is if the runtime is transitioning to STOPPING, which should not happen
                    // during conformance testing. This will wait 1 second before assuming no such incorrect event will come.
                    REQUIRE_MSG(waitForNextSessionState(instance, &evt) == false, "Premature progression from READY to SYNCHRONIZED state");
                }
                SECTION("Second call to xrBeginSession in READY")
                {
                    REQUIRE(XR_ERROR_SESSION_RUNNING == xrBeginSession(session, &beginInfo));
                }

                // READY -> SYNCHRONIZED
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_SYNCHRONIZED);
                SECTION("xrBeginSession in SYNCHRONIZED")
                {
                    REQUIRE(XR_ERROR_SESSION_RUNNING == xrBeginSession(session, &beginInfo));
                }

                // SYNCHRONIZED -> VISIBLE
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_VISIBLE);
                SECTION("xrBeginSession in VISIBLE")
                {
                    REQUIRE(XR_ERROR_SESSION_RUNNING == xrBeginSession(session, &beginInfo));
                }

                // VISIBLE -> FOCUSED
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_FOCUSED);
                SECTION("xrBeginSession in FOCUSED")
                {
                    REQUIRE(XR_ERROR_SESSION_RUNNING == xrBeginSession(session, &beginInfo));
                }

                // Runtime should only allow ending a session in the STOPPING state.
                SECTION("xrEndSession in FOCUSED")
                {
                    REQUIRE(XR_ERROR_SESSION_NOT_STOPPING == xrEndSession(session));
                }

                INFO("xrRequestExitSession");
                REQUIRE(XR_SUCCESS == xrRequestExitSession(session));
                SECTION("xrBeginSession in FOCUSED due to xrRequestExitSession")
                {
                    REQUIRE(XR_ERROR_SESSION_RUNNING == xrBeginSession(session, &beginInfo));
                }

                // FOCUSED -> VISIBLE
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_VISIBLE);
                SECTION("xrBeginSession in VISIBLE due to xrRequestExitSession")
                {
                    REQUIRE(XR_ERROR_SESSION_RUNNING == xrBeginSession(session, &beginInfo));
                }

                // VISIBLE -> SYNCHRONIZED
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_SYNCHRONIZED);
                SECTION("xrBeginSession in SYNCHRONIZED due to xrRequestExitSession")
                {
                    REQUIRE(XR_ERROR_SESSION_RUNNING == xrBeginSession(session, &beginInfo));
                }

                // SYNCHRONIZED -> STOPPING
                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_STOPPING);
                SECTION("xrBeginSession in STOPPING due to xrRequestExitSession")
                {
                    REQUIRE(XR_ERROR_SESSION_RUNNING == xrBeginSession(session, &beginInfo));
                }

                {
                    INFO(
                        "Runtime should not transition from STOPPING to IDLE until the session has been ended. Wait 1s for incorrect event.");
                    // Runtime should not transition from STOPPING to IDLE until the session has been ended.
                    // This will wait 1 second before assuming no such incorrect event will come.
                    REQUIRE(waitForNextSessionState(instance, &evt) == false);
                }

                INFO("xrEndSession");
                REQUIRE(XR_SUCCESS == xrEndSession(session));

                SECTION("xrWaitFrame after xrEndSession but before IDLE")
                {
                    CHECK(XR_ERROR_SESSION_NOT_RUNNING == xrWaitFrame(session, nullptr, &frameState));
                }

                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_IDLE);

                SECTION("xrWaitFrame in IDLE while shutting down")
                {
                    CHECK(XR_ERROR_SESSION_NOT_RUNNING == xrWaitFrame(session, nullptr, &frameState));
                }

                submitFramesUntilSessionState(instance, session, ebm, XR_SESSION_STATE_EXITING);

                SECTION("xrWaitFrame in EXITING while shutting down")
                {
                    CHECK(XR_ERROR_SESSION_NOT_RUNNING == xrWaitFrame(session, nullptr, &frameState));
                }
            }
        }
        SECTION("xrRequestExitSession Session Not Running")
        {
            SECTION("Session not running before starting")
            {
                AutoBasicSession session(AutoBasicSession::createSession, instance);

                REQUIRE(XR_ERROR_SESSION_NOT_RUNNING == xrRequestExitSession(session));
            }

            SECTION("Session not running after ending")
            {
                // A session is considered running after a successful call to
                // xrBeginSession and remains running until any call is made to
                // xrEndSession. Certain functions are only valid to call when
                // a session is running, such as xrWaitFrame, or else the
                // XR_ERROR_SESSION_NOT_RUNNING error must be returned by the
                // runtime.

                // If session is not running when xrRequestExitSession is
                // called, XR_ERROR_SESSION_NOT_RUNNING must be returned.

                AutoBasicSession session(
                    AutoBasicSession::beginSession | AutoBasicSession::createSpaces | AutoBasicSession::createSwapchains, instance);
                REQUIRE(XR_SUCCESS == xrRequestExitSession(session));

                FrameIterator frameIterator(&session);
                frameIterator.RunToSessionState(XR_SESSION_STATE_STOPPING);
                REQUIRE(XR_SUCCESS == xrEndSession(session));

                // Actually test what we want to test!
                REQUIRE(XR_ERROR_SESSION_NOT_RUNNING == xrRequestExitSession(session));
            }
        }

        // Runtime should not transition from READY to SYNCHRONIZED until one
        // or more frames have been submitted.
        // The exception is if the runtime is transitioning to STOPPING, which
        // should not happen during conformance testing.
        if (GetGlobalData().IsUsingGraphicsPlugin()) {
            SECTION("Advance without frame submission")
            {
                AutoBasicSession session(AutoBasicSession::createSession, instance);

                FrameIterator frameIterator(&session);
                frameIterator.RunToSessionState(XR_SESSION_STATE_READY);

                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = Options::Get().viewConfigurationValue;
                REQUIRE(XR_SUCCESS == xrBeginSession(session, &beginInfo));

                // This will wait 1 second before assuming no such incorrect event will come.
                INFO("When using graphics, must not move from READY to SYNCHRONIZED without submittting frames.");
                XrEventDataSessionStateChanged evt;
                REQUIRE(waitForNextSessionState(instance, &evt) == false);
            }
        }
    }

}  // namespace Conformance
