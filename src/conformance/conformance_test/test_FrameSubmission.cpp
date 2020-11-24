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

#include <thread>
#include <condition_variable>
#include <queue>
#include "utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "composition_utils.h"
#include "report.h"
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <catch2/catch.hpp>

#define ENUM_LIST(name, val) name,
constexpr XrEnvironmentBlendMode SupportedBlendModes[] = {XR_LIST_ENUM_XrEnvironmentBlendMode(ENUM_LIST)};

namespace Conformance
{
    // Tests for xrBeginFrame, xrWaitFrame, xrEndFrame without testing specific composition layer types.
    TEST_CASE("Frame Submission", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            // Nothing to check - no graphics plugin means no frame submission
            return;
        }

        SECTION("Before xrBeginSession")
        {
            AutoBasicSession session(AutoBasicSession::createSession);

            XrFrameState frameState{XR_TYPE_FRAME_STATE};
            CHECK(XR_ERROR_SESSION_NOT_RUNNING == xrWaitFrame(session, nullptr, &frameState));
            CHECK(XR_ERROR_SESSION_NOT_RUNNING == xrBeginFrame(session, nullptr));
        }

        SECTION("Call order")
        {
            AutoBasicSession session(AutoBasicSession::beginSession);

            XrFrameState frameState{XR_TYPE_FRAME_STATE};
            XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
            frameEndInfo.environmentBlendMode = globalData.GetOptions().environmentBlendModeValue;

            {  // Fresh session, test xrBeginFrame with no corresponding xrWaitFrame.
                CHECK(XR_ERROR_CALL_ORDER_INVALID == xrBeginFrame(session, nullptr));
            }

            {  // Test discarded frame.
                REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                REQUIRE_RESULT_SUCCEEDED(xrBeginFrame(session, nullptr));
                REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                CHECK(XR_FRAME_DISCARDED == xrBeginFrame(session, nullptr));
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                REQUIRE(XR_SUCCESS == xrEndFrame(session, &frameEndInfo));
            }

            {  // Successful frame submitted, test xrBeginFrame with no corresponding xrWaitFrame.
                REQUIRE(XR_ERROR_CALL_ORDER_INVALID == xrBeginFrame(session, nullptr));
            }

            {  // Test the xrBeginFrame return code after a failed xrEndFrame
                REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                REQUIRE_RESULT_SUCCEEDED(xrBeginFrame(session, nullptr));
                XrFrameEndInfo badFrameEndInfo = frameEndInfo;
                badFrameEndInfo.displayTime = 0;
                CHECK(XR_ERROR_TIME_INVALID == xrEndFrame(session, &badFrameEndInfo));
                REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                CHECK(XR_FRAME_DISCARDED == xrBeginFrame(session, nullptr));
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                REQUIRE(XR_SUCCESS == xrEndFrame(session, &frameEndInfo));
            }

            {  // Test that bad xrBeginFrame doesn't discard frame.
                REQUIRE(XR_SUCCESS == xrWaitFrame(session, nullptr, &frameState));
                REQUIRE_RESULT_SUCCEEDED(xrBeginFrame(session, nullptr));  // In case of discarded.
                REQUIRE(XR_ERROR_CALL_ORDER_INVALID == xrBeginFrame(session, nullptr));
                REQUIRE(XR_SUCCESS == xrEndFrame(session, &frameEndInfo));
            }

            // Test xrBeginFrame was not called.
            CHECK(XR_ERROR_CALL_ORDER_INVALID == xrEndFrame(session, &frameEndInfo));

            {  // Two calls to xrWaitFrame should succeed once the prior xrBeginFrame is called.
                XrFrameState frameState1{XR_TYPE_FRAME_STATE};
                REQUIRE(XR_SUCCESS == xrWaitFrame(session, nullptr, &frameState1));
                CHECK(XR_SUCCESS == xrBeginFrame(session, nullptr));
                XrFrameState frameState2{XR_TYPE_FRAME_STATE};
                REQUIRE(XR_SUCCESS == xrWaitFrame(session, nullptr, &frameState2));
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                REQUIRE(XR_SUCCESS == xrEndFrame(session, &frameEndInfo));
                CHECK(XR_SUCCESS == xrBeginFrame(session, nullptr));
                frameEndInfo.displayTime = frameState2.predictedDisplayTime;
                REQUIRE(XR_SUCCESS == xrEndFrame(session, &frameEndInfo));
                CHECK(frameState2.predictedDisplayTime > frameState1.predictedDisplayTime);
            }
        }

        SECTION("EndFrameInfo Tests")
        {
            AutoBasicSession session(AutoBasicSession::beginSession | AutoBasicSession::createSpaces);

            XrFrameState frameState{XR_TYPE_FRAME_STATE};

            XrFrameEndInfo defaultFrameEndInfo{XR_TYPE_FRAME_END_INFO};
            defaultFrameEndInfo.environmentBlendMode = globalData.GetOptions().environmentBlendModeValue;

            {
                INFO("No layers");

                // First frame
                REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                REQUIRE_RESULT_SUCCEEDED(xrBeginFrame(session, nullptr));  // May return XR_FRAME_DISCARDED
                XrFrameEndInfo frameEndInfo = defaultFrameEndInfo;
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                CHECK(XR_SUCCESS == xrEndFrame(session, &frameEndInfo));

                // Second frame. Should get XR_SUCCESS on xrBeginFrame rather than XR_FRAME_DISCARDED.
                REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                REQUIRE(XR_SUCCESS == xrBeginFrame(session, nullptr));
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                CHECK(XR_SUCCESS == xrEndFrame(session, &frameEndInfo));
            }

            {
                INFO("Invalid displayTime");

                REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                REQUIRE_RESULT_SUCCEEDED(xrBeginFrame(session, nullptr));
                XrFrameEndInfo frameEndInfo = defaultFrameEndInfo;
                frameEndInfo.displayTime = 0;
                CHECK(XR_ERROR_TIME_INVALID == xrEndFrame(session, &frameEndInfo));
            }

            {
                INFO("Invalid layer");
                REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                REQUIRE_RESULT_SUCCEEDED(xrBeginFrame(session, nullptr));

                std::array<XrCompositionLayerBaseHeader*, 1> layers = {nullptr};

                XrFrameEndInfo frameEndInfo = defaultFrameEndInfo;
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                frameEndInfo.layerCount = (uint32_t)layers.size();
                frameEndInfo.layers = layers.data();
                CHECK(XR_ERROR_LAYER_INVALID == xrEndFrame(session, &frameEndInfo));
            }

            // Valid and invalid environment blend modes.
            {
                INFO("Environment Blend Modes");

                const auto supportedBlendModes = session.SupportedEnvironmentBlendModes();
                CHECK(std::find(supportedBlendModes.begin(), supportedBlendModes.end(), XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM) ==
                      supportedBlendModes.end());

                for (XrEnvironmentBlendMode blendMode : SupportedBlendModes) {
                    CAPTURE(blendMode);

                    REQUIRE_RESULT_SUCCEEDED(xrWaitFrame(session, nullptr, &frameState));
                    REQUIRE_RESULT_SUCCEEDED(xrBeginFrame(session, nullptr));

                    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
                    frameEndInfo.displayTime = frameState.predictedDisplayTime;
                    frameEndInfo.environmentBlendMode = blendMode;

                    if (blendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM) {
                        // The max value is not valid.
                        continue;
                    }
                    else if (std::find(supportedBlendModes.begin(), supportedBlendModes.end(), blendMode) != supportedBlendModes.end()) {
                        // Runtime supports this blend mode and should allow it.
                        CHECK(XR_SUCCESS == xrEndFrame(session, &frameEndInfo));
                    }
                    else {
                        // Runtime does not support this blend mode and should disallow it.
                        CHECK(XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED == xrEndFrame(session, &frameEndInfo));
                    }
                }
            }
        }

        SECTION("After xrEndSession")
        {
            AutoBasicSession session(AutoBasicSession::beginSession | AutoBasicSession::createSpaces | AutoBasicSession::createSwapchains);

            CHECK(XR_SUCCESS == xrRequestExitSession(session));

            REQUIRE(FrameIterator::RunResult::Success == FrameIterator(&session).RunToSessionState(XR_SESSION_STATE_STOPPING, 5_sec));

            CHECK(XR_SUCCESS == xrEndSession(session));

            XrFrameState frameState{XR_TYPE_FRAME_STATE};
            CHECK(XR_ERROR_SESSION_NOT_RUNNING == xrWaitFrame(session, nullptr, &frameState));
            CHECK(XR_ERROR_SESSION_NOT_RUNNING == xrBeginFrame(session, nullptr));
        }
    }

    // Test uses spends 90% of a predictedDisplayPeriod on both the rendering thread and primary thread. Although the total time
    // spent is over 100% of allowable time, the OpenXR frame API calls should be made concurrently allowing full frame rate.
    TEST_CASE("Timed Pipelined Frame Submission", "")
    {
        using ns = std::chrono::nanoseconds;
        using ms = std::chrono::duration<float, std::milli>;

        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            // Nothing to check - no graphics plugin means no frame submission
            return;
        }

        CompositionHelper compositionHelper("Timed Pipeline Frame Submission");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        SimpleProjectionLayerHelper simpleProjectionLayerHelper(compositionHelper);

        // Busy-waiting is more accurate than "sleeping" which can have several milliseconds of additional delay.
        auto YieldSleep = [](const Stopwatch& sw, ns delay) {
            while (sw.Elapsed() < delay) {
                std::this_thread::yield();
            }
        };

        constexpr int warmupFrameCount = 180;           // Prewarm the frame loop for this many frames.
        constexpr int testFrameCount = 200;             // Average this many frames for analysis.
        constexpr double waitBlockPercentage = 0.90;    // Block for 90% of the display period on waitframe thread.
        constexpr double renderBlockPercentage = 0.90;  // Block for 90% of the display period on render thread.

        std::queue<XrFrameState> queuedFramesForRender;
        std::mutex displayMutex;
        std::condition_variable displayCv;
        bool frameSubmissionCompleted = false;

        ns totalFrameDisplayPeriod(0), totalWaitTime(0);
        Stopwatch frameLoopTimer;

        XrResult appThreadResult = XR_SUCCESS;

        auto appThread = std::thread([&]() {
            ATTACH_THREAD;
            auto queueFrameRender = [&](const XrFrameState& frameState) {
                {
                    std::unique_lock<std::mutex> lock(displayMutex);
                    queuedFramesForRender.push(frameState);
                }
                displayCv.notify_one();
            };

            // Initially prime things by submitting 180 frames without measuring performance.
            for (int frame = 0; frame < warmupFrameCount; ++frame) {
                XrFrameState frameState{XR_TYPE_FRAME_STATE};
                appThreadResult = xrWaitFrame(compositionHelper.GetSession(), nullptr, &frameState);
                if (appThreadResult != XR_SUCCESS) {
                    DETACH_THREAD;
                    return;
                }

                // Mimic a lot of time spent in game "simulation" phase.
                YieldSleep(Stopwatch(true), ns((int32_t)(frameState.predictedDisplayPeriod * waitBlockPercentage)));

                queueFrameRender(frameState);
            }

            frameLoopTimer.Restart();

            // Now submit <testFrameCount> frames and measure the total time spent.
            for (int frame = 0; frame < testFrameCount; ++frame) {
                XrFrameState frameState{XR_TYPE_FRAME_STATE};
                {
                    Stopwatch waitTimer(true);
                    appThreadResult = xrWaitFrame(compositionHelper.GetSession(), nullptr, &frameState);
                    if (appThreadResult != XR_SUCCESS) {
                        DETACH_THREAD;
                        return;
                    }

                    totalWaitTime += waitTimer.Elapsed();
                }

                totalFrameDisplayPeriod += ns(frameState.predictedDisplayPeriod);

                // Mimic a lot of time spent in game "simulation" phase.
                YieldSleep(Stopwatch(true), ns((int32_t)(frameState.predictedDisplayPeriod * waitBlockPercentage)));

                queueFrameRender(frameState);
            }

            // Signal that no more frames are coming and wait for the render thread to exit.
            {
                std::unique_lock<std::mutex> lock(displayMutex);
                frameSubmissionCompleted = true;
                displayCv.notify_one();
            }
            DETACH_THREAD;
        });

        while (true) {
            // Dequeue a frame to render.
            XrFrameState frameState;
            {
                std::unique_lock<std::mutex> lock(displayMutex);
                displayCv.wait(lock, [&] { return !queuedFramesForRender.empty() || frameSubmissionCompleted; });
                if (queuedFramesForRender.empty()) {
                    assert(frameSubmissionCompleted);
                    break;
                }
                frameState = queuedFramesForRender.front();
                queuedFramesForRender.pop();
            }

            XRC_CHECK_THROW_XRCMD(xrBeginFrame(compositionHelper.GetSession(), nullptr));

            Stopwatch sw(true);

            simpleProjectionLayerHelper.UpdateProjectionLayer(frameState);
            std::vector<XrCompositionLayerBaseHeader*> layers{simpleProjectionLayerHelper.GetProjectionLayer()};

            // Mimic a lot of time spent in game render phase.
            YieldSleep(sw, ns((int64_t)(frameState.predictedDisplayPeriod * renderBlockPercentage)));

            compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);
        }

        frameLoopTimer.Stop();
        if (appThread.joinable()) {
            appThread.join();
            REQUIRE_RESULT_SUCCEEDED(appThreadResult);
        }

        const ns averageWaitTime = totalWaitTime / testFrameCount;
        ReportF("Average xrWaitFrame wait time    : %.3fms", std::chrono::duration_cast<ms>(averageWaitTime).count());

        const ns averageAppFrameTime = frameLoopTimer.Elapsed() / testFrameCount;
        ReportF("Average time spent per frame     : %.3fms", std::chrono::duration_cast<ms>(averageAppFrameTime).count());

        const ns averageDisplayPeriod = totalFrameDisplayPeriod / testFrameCount;
        ReportF("Average predicted display period : %.3fms", std::chrono::duration_cast<ms>(averageDisplayPeriod).count());

        // Higher is worse. An overhead of 50% means a 16.66ms display period ran with an average of 25ms per frame.
        // Since frames should be discrete multiples of the display period 50% implies that half of the frames
        // took two display periods to complete, 100% implies every frame took two periods.
        const double overheadFactor = (averageAppFrameTime.count() / (double)averageDisplayPeriod.count()) - 1.0;
        ReportF("Overhead score                   : %.1f%%", overheadFactor * 100);

        // Allow up to 10% of frames to miss timing. This is number is arbitrary and open to debate.
        // The point of this test is to fail runtims that get 1.0 (100% overhead) because they are
        // probably serializing the frame calls.
        REQUIRE_MSG(overheadFactor < 0.1, "Frame timing overhead in pipelined frame submission is too high");

        // If the frame loop runs FASTER then the predictedDisplayPeriod is wrong or xrWaitFrame is not throttling correctly.
        REQUIRE_MSG(overheadFactor > -0.1, "Frame timing overhead in pipelined frame submission is too low");
    }
}  // namespace Conformance
