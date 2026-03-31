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

#include "common/xr_linear.h"
#include "composition_utils.h"
#include "conformance_utils.h"
#include "input_testinputdevice.h"
#include "report.h"
#include "utilities/event_reader.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <chrono>
#include <thread>

namespace Conformance
{
    using namespace std::chrono_literals;
#ifdef XR_USE_PLATFORM_ANDROID
    const std::chrono::nanoseconds waitDelay = 0ms;
#else
    const std::chrono::nanoseconds waitDelay = 5ms;
#endif  // XR_USE_PLATFORM_ANDROID
    const std::chrono::seconds waitTimeBeforeExit = 1000s;

    struct DisplayMessageManager : public ITestMessageDisplay
    {
        DisplayMessageManager(CompositionHelper* compositionHelper, RenderLoop* renderLoop,
                              std::vector<XrCompositionLayerBaseHeader*>& layers, const XrSpace* localSpace)
            : m_compositionHelper(compositionHelper), m_renderLoop(renderLoop), m_localSpace(localSpace), m_layers(layers)
        {
        }

        void IterateFrame() override
        {
            m_renderLoop->IterateFrame();
        }

        void DisplayMessage(const std::string& sMessage) override
        {
            constexpr XrVector3f Up{0, 1, 0};
            m_layers.clear();

            XrCompositionLayerQuad* const instructionsQuad = m_compositionHelper->CreateQuadLayer(
                m_compositionHelper->CreateStaticSwapchainImage(CreateTextImage(1024, 256, sMessage.c_str(), 48)), *m_localSpace, 1,
                {{0, 0, 0, 1}, {-1.5f, 0, -0.3f}});
            XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &Up, 70 * MATH_PI / 180);

            m_layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});
            ReportF("%s", sMessage.c_str());
        }

    private:
        std::mutex m_mutex;

        CompositionHelper* m_compositionHelper;
        RenderLoop* m_renderLoop;
        const XrSpace* m_localSpace;
        std::vector<XrCompositionLayerBaseHeader*>& m_layers;
    };

    static bool EndFrameViewConfig(const XrFrameState& frameState, CompositionHelper& compositionHelper,
                                   std::vector<XrCompositionLayerBaseHeader*>& layers)
    {
        compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);
        compositionHelper.PollEvents();
        return true;
    }

    // Initialize supported controllers
    static std::vector<XrPath> vSupportedControllers_SelectClick, vSupportedControllers_TriggerClick, vSupportedControllers_TriggerValue;
    static void InitPaths(XrInstance instance)
    {
        // Controllers with select/click
        std::vector<const char*> vInteractionProfiles_SelectClick = {"/interaction_profiles/khr/simple_controller",
                                                                     "/interaction_profiles/google/daydream_controller"};

        for (auto interactionProfile : vInteractionProfiles_SelectClick) {
            XrPath profilePath;
            REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, interactionProfile, &profilePath));
            vSupportedControllers_SelectClick.push_back(profilePath);
        }

        // Controllers with trigger/click
        std::vector<const char*> vInteractionProfiles_TriggerClick = {"/interaction_profiles/valve/index_controller",
                                                                      "/interaction_profiles/htc/vive_controller",
                                                                      "/interaction_profiles/oculus/go_controller"};

        for (auto interactionProfile : vInteractionProfiles_TriggerClick) {
            XrPath profilePath;
            REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, interactionProfile, &profilePath));
            vSupportedControllers_TriggerClick.push_back(profilePath);
        }

        // Controllers with trigger/value
        std::vector<const char*> vInteractionProfiles_TriggerValue = {"/interaction_profiles/microsoft/motion_controller",
                                                                      "/interaction_profiles/oculus/touch_controller"};

        for (auto interactionProfile : vInteractionProfiles_TriggerValue) {
            XrPath profilePath;
            REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, interactionProfile, &profilePath));
            vSupportedControllers_TriggerValue.push_back(profilePath);
        }
    };

    // Suggest controller bindings
    static XrResult SuggestControllerBinding(XrInstance instance, XrPath interactionProfile, const char* leftPath, const char* rightPath,
                                             std::vector<XrActionSuggestedBinding>& vActionBindings, XrAction action)
    {
        XrPath pathSelect_L, pathSelect_R;
        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, leftPath, &pathSelect_L));
        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, rightPath, &pathSelect_R));

        vActionBindings.push_back({action, pathSelect_L});
        vActionBindings.push_back({action, pathSelect_R});

        // Set suggested binding to interaction profile
        XrInteractionProfileSuggestedBinding xrInteractionProfileSuggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        xrInteractionProfileSuggestedBinding.next = nullptr;
        xrInteractionProfileSuggestedBinding.interactionProfile = interactionProfile;
        xrInteractionProfileSuggestedBinding.suggestedBindings = vActionBindings.data();
        xrInteractionProfileSuggestedBinding.countSuggestedBindings = (uint32_t)vActionBindings.size();

        return xrSuggestInteractionProfileBindings(instance, &xrInteractionProfileSuggestedBinding);
    }

    static bool WaitForViewSessionFocus(RenderLoop* renderLoop, EventReader* eventReader, XrSession session)
    {
        return WaitUntilPredicateWithTimeout(
            [&]() {
                renderLoop->IterateFrame();
                XrEventDataBuffer eventData;
                while (eventReader->TryReadNext(eventData)) {
                    if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                        auto sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                        if (sessionStateChanged->session == session && sessionStateChanged->state == XR_SESSION_STATE_FOCUSED) {
                            return true;
                        }
                    }
                }
                return false;
            },
            waitTimeBeforeExit, waitDelay);
    }

    static bool WaitForExit(RenderLoop* renderLoop, EventReader* eventReader)
    {
        const auto startTime = std::chrono::system_clock::now();
        while (std::chrono::system_clock::now() - startTime < waitTimeBeforeExit) {
            // Move frame
            renderLoop->IterateFrame();

            // Check for view config event change
            XrEventDataBuffer eventData;
            while (eventReader->TryReadNext(eventData)) {
                if (eventData.type == XR_TYPE_EVENT_DATA_VIEW_CONFIGURATION_VIEWS_CHANGED_EXT) {
                    ReportF("Recommended view configuration settings changed.");
                }
            }
        }

        return true;
    }

    static bool WaitForExit(XrAction action, XrActionsSyncInfo* syncInfo, RenderLoop* renderLoop, EventReader* eventReader,
                            DisplayMessageManager* msgManager, XrSession session)
    {
        XrActionStateBoolean actionStateBoolean{XR_TYPE_ACTION_STATE_BOOLEAN};
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;

        XrResult res;
        const auto startTime = std::chrono::system_clock::now();
        while (std::chrono::system_clock::now() - startTime < waitTimeBeforeExit) {
            // Move frame
            renderLoop->IterateFrame();
            res = xrSyncActions(session, syncInfo);

            // Check for exit
            if (XR_UNQUALIFIED_SUCCESS(res)) {
                res = xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean);

                if (XR_UNQUALIFIED_SUCCESS(res) && actionStateBoolean.changedSinceLastSync) {
                    if (actionStateBoolean.currentState) {
                        ReportF("Test complete. Exiting test.");
                    }

                    return actionStateBoolean.changedSinceLastSync && actionStateBoolean.currentState;
                }
            }

            // Check for view config event change
            XrEventDataBuffer eventData;
            while (eventReader->TryReadNext(eventData)) {
                if (eventData.type == XR_TYPE_EVENT_DATA_VIEW_CONFIGURATION_VIEWS_CHANGED_EXT) {
                    msgManager->DisplayMessage("Recommended view configuration settings changed.");
                }
            }
        }

        FAIL("Test timed out. Press trigger to complete.");
        return false;
    }

    static void SleepWithSinusoidalDelay()
    {
        using namespace std::chrono;
        static auto startTime = steady_clock::now();

        // Duration of one wave cycle in seconds
        constexpr double cycleDuration = 0.5;

        // Min and max sleep periods in milliseconds
        constexpr double minSleepMs = 0.0;
        constexpr double maxSleepMs = 100.0;

        // Calculate elapsed time in seconds since start
        auto now = steady_clock::now();
        double elapsed = duration_cast<duration<double>>(now - startTime).count();

        // Calculate normalized time in [0, 1)
        double t = fmod(elapsed, cycleDuration) / cycleDuration;

        // Use a sine wave to smoothly vary between minSleepMs and maxSleepMs
        // t=0: minSleepMs, t=0.5: maxSleepMs, t=1: minSleepMs
        double sleepMs = minSleepMs + (maxSleepMs - minSleepMs) * 0.5 * (1.0 + cos(2.0 * MATH_PI * t));

        // Clamp sleepMs to [minSleepMs, maxSleepMs]
        if (sleepMs < minSleepMs)
            sleepMs = minSleepMs;
        if (sleepMs > maxSleepMs)
            sleepMs = maxSleepMs;

        // Sleep to simulate the frame time
        std::this_thread::sleep_for(milliseconds(static_cast<int>(sleepMs)));
    }

    TEST_CASE("XR_EXT_view_configuration_views_change", "[XR_EXT_view_configuration_views_change][scenario][interactive][no_auto]")
    {
        // Check if extension is supported by the runtime
        if (!GetGlobalData().IsInstanceExtensionSupported(XR_EXT_VIEW_CONFIGURATION_VIEWS_CHANGE_EXTENSION_NAME)) {
            SKIP(XR_EXT_VIEW_CONFIGURATION_VIEWS_CHANGE_EXTENSION_NAME " not supported");
        }

        // Create instance
        CompositionHelper compositionHelper("XR_EXT_view_configuration_views_change",
                                            {XR_EXT_VIEW_CONFIGURATION_VIEWS_CHANGE_EXTENSION_NAME});
        XrInstance instance = compositionHelper.GetInstance();
        REQUIRE(instance != XR_NULL_HANDLE);

        // Start session
        compositionHelper.BeginSession();
        XrSession session = compositionHelper.GetSession();
        REQUIRE(session != XR_NULL_HANDLE);

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);
        EventReader eventReader(compositionHelper.GetEventQueue());
        std::vector<XrCompositionLayerBaseHeader*> layers;

        SECTION("Manual Viewport Change")
        {
            // Setup rendering
            RenderLoop renderLoop(
                session, [&](const XrFrameState& frameState) { return EndFrameViewConfig(frameState, compositionHelper, layers); });
            DisplayMessageManager msgManager(&compositionHelper, &renderLoop, layers, &localSpace);

            // Setup controller paths
            InitPaths(instance);

            // Create action set
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "main_actionset");
            strcpy(actionSetInfo.localizedActionSetName, "main_actionset");
            actionSetInfo.priority = 0;

            XrActionSet actionSet = XR_NULL_HANDLE;
            REQUIRE_RESULT_SUCCEEDED(xrCreateActionSet(instance, &actionSetInfo, &actionSet));

            // Create exit test action
            XrActionCreateInfo actioninfo{XR_TYPE_ACTION_CREATE_INFO};
            strcpy(actioninfo.actionName, "exit_test");
            actioninfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actioninfo.localizedActionName, "exit_test");

            XrAction exitAction = XR_NULL_HANDLE;
            REQUIRE_RESULT_SUCCEEDED(xrCreateAction(actionSet, &actioninfo, &exitAction));

            // Suggest bindings
            std::vector<XrActionSuggestedBinding> vActionBindings_SelectClick;
            for (auto interactionProfile : vSupportedControllers_SelectClick) {
                REQUIRE_RESULT_SUCCEEDED(SuggestControllerBinding(instance, interactionProfile, "/user/hand/left/input/select/click",
                                                                  "/user/hand/right/input/select/click", vActionBindings_SelectClick,
                                                                  exitAction));
            }

            std::vector<XrActionSuggestedBinding> vActionBindings_TriggerClick;
            for (auto interactionProfile : vSupportedControllers_TriggerClick) {
                REQUIRE_RESULT_SUCCEEDED(SuggestControllerBinding(instance, interactionProfile, "/user/hand/left/input/trigger/click",
                                                                  "/user/hand/right/input/trigger/click", vActionBindings_TriggerClick,
                                                                  exitAction));
            }

            std::vector<XrActionSuggestedBinding> vActionBindings_TriggerValue;
            for (auto interactionProfile : vSupportedControllers_TriggerValue) {
                REQUIRE_RESULT_SUCCEEDED(SuggestControllerBinding(instance, interactionProfile, "/user/hand/left/input/trigger/value",
                                                                  "/user/hand/right/input/trigger/value", vActionBindings_TriggerValue,
                                                                  exitAction));
            }

            // Attach action sets
            compositionHelper.GetInteractionManager().AddActionSet(actionSet);
            compositionHelper.GetInteractionManager().AttachActionSets();
            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            XrActiveActionSet activeActionSet{actionSet};
            syncInfo.activeActionSets = &activeActionSet;
            syncInfo.countActiveActionSets = 1;

            // Wait for focused state for input
            msgManager.DisplayMessage("Waiting for session focus...");
            bool bFocused = WaitForViewSessionFocus(&renderLoop, &eventReader, session);

            if (bFocused) {
                msgManager.DisplayMessage("Make runtime change recommended viewport size.");
                WaitForExit(exitAction, &syncInfo, &renderLoop, &eventReader, &msgManager, session);
            }
        }

        SECTION("Simulate Varying Performance")
        {
            // Setup rendering with varying performance
            RenderLoop renderLoop(session, [&](const XrFrameState& frameState) {
                SleepWithSinusoidalDelay();
                return EndFrameViewConfig(frameState, compositionHelper, layers);
            });
            DisplayMessageManager msgManager(&compositionHelper, &renderLoop, layers, &localSpace);

            // Wait for focused state for input
            msgManager.DisplayMessage("Waiting for session focus...");
            bool bFocused = WaitForViewSessionFocus(&renderLoop, &eventReader, session);

            if (bFocused) {
                msgManager.DisplayMessage("Simulating varying render performance to catch non-conformant view change event raising.");
                WaitForExit(&renderLoop, &eventReader);
            }
        }
    }
}  // namespace Conformance
