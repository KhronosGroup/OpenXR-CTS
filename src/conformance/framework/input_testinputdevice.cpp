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

#include "input_testinputdevice.h"

#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "two_call.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"

#include <catch2/catch_test_macros.hpp>

#include <openxr/openxr.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <ratio>
#include <tuple>
#include <utility>

using namespace std::chrono_literals;

typedef XrResult(XRAPI_PTR* PFN_xrSetInputDeviceActiveEXT)(XrSession session, XrPath interactionProfile, XrPath topLevelPath,
                                                           XrBool32 isActive);
typedef XrResult(XRAPI_PTR* PFN_xrSetInputDeviceStateBoolEXT)(XrSession session, XrPath topLevelPath, XrPath inputSourcePath,
                                                              XrBool32 state);
typedef XrResult(XRAPI_PTR* PFN_xrSetInputDeviceStateFloatEXT)(XrSession session, XrPath topLevelPath, XrPath inputSourcePath, float state);
typedef XrResult(XRAPI_PTR* PFN_xrSetInputDeviceStateVector2fEXT)(XrSession session, XrPath topLevelPath, XrPath inputSourcePath,
                                                                  XrVector2f state);
typedef XrResult(XRAPI_PTR* PFN_xrSetInputDeviceLocationEXT)(XrSession session, XrPath topLevelPath, XrPath inputSourcePath, XrSpace space,
                                                             XrPosef pose);
// On android platforms sleeping the main thread stalls the interactive tests
#ifdef XR_USE_PLATFORM_ANDROID
static constexpr std::chrono::nanoseconds waitDelay = 0ms;
#else
static constexpr std::chrono::nanoseconds waitDelay = 5ms;
#endif  // XR_USE_PLATFORM_ANDROID

static constexpr std::chrono::nanoseconds instructionDelay = 250ms;

namespace Conformance
{
    class HumanDrivenInputdevice : public IInputTestDevice
    {
    public:
        HumanDrivenInputdevice(ITestMessageDisplay* const messageDisplay, XrInstance instance, XrSession session, XrPath interactionProfile,
                               XrPath topLevelPath, XrActionSet actionSet, XrAction firstBooleanAction,
                               std::map<XrPath, XrAction>& actionMap)
            : m_messageDisplay(messageDisplay)
            , m_instance(instance)
            , m_session(session)
            , m_interactionProfile(interactionProfile)
            , m_topLevelPath(topLevelPath)
            , m_conformanceAutomationExtensionEnabled(GetGlobalData().IsInstanceExtensionEnabled("XR_EXT_conformance_automation"))
        {
            m_actionSet = actionSet;
            m_actionMap = actionMap;
            m_firstBooleanAction = firstBooleanAction;  // will be used for testing active controller
            m_shouldDestroyActionSet = false;           // actions and action sets are handled by the test, so do not destroy
        }

        HumanDrivenInputdevice(ITestMessageDisplay* const messageDisplay, InteractionManager* const interactionManager, XrInstance instance,
                               XrSession session, XrPath interactionProfile, XrPath topLevelPath,
                               const InputSourcePathAvailCollection& interactionProfilePaths)
            : m_messageDisplay(messageDisplay)
            , m_instance(instance)
            , m_session(session)
            , m_interactionProfile(interactionProfile)
            , m_topLevelPath(topLevelPath)
            , m_conformanceAutomationExtensionEnabled(GetGlobalData().IsInstanceExtensionEnabled("XR_EXT_conformance_automation"))
        {
            std::string actionSetName = "test_device_action_set_" + std::to_string(m_topLevelPath);
            std::string localizedActionSetName = "Test Device Action Set " + std::to_string(m_topLevelPath);

            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.localizedActionSetName, localizedActionSetName.c_str());
            strcpy(actionSetCreateInfo.actionSetName, actionSetName.c_str());
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &m_actionSet), XR_SUCCESS);

            int i = 0;
            auto GetActionNames = [&i]() mutable -> std::tuple<std::string, std::string> {
                i++;
                return std::tuple<std::string, std::string>{"test_device_action_" + std::to_string(i),
                                                            "test device action " + std::to_string(i)};
            };

            std::string topLevelPathString = std::string(CHECK_TWO_CALL(char, {}, xrPathToString, m_instance, m_topLevelPath).data());
            auto PrefixedByTopLevelPath = [&topLevelPathString](std::string binding) {
                return (binding.length() > topLevelPathString.size()) &&
                       (std::mismatch(topLevelPathString.begin(), topLevelPathString.end(), binding.begin()).first ==
                        topLevelPathString.end());
            };

            FeatureSet enabled;
            GetGlobalData().PopulateVersionAndEnabledExtensions(enabled);

            for (const InputSourcePathAvailData& inputSourceData : interactionProfilePaths) {
                if (!PrefixedByTopLevelPath(inputSourceData.Path)) {
                    continue;
                }
                if (!kInteractionAvailabilities[(size_t)inputSourceData.Availability].IsSatisfiedBy(enabled)) {
                    continue;
                }

                auto actionNames = GetActionNames();

                XrAction action{XR_NULL_HANDLE};
                XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
                actionCreateInfo.actionType = inputSourceData.Type;
                strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                REQUIRE_RESULT(xrCreateAction(m_actionSet, &actionCreateInfo, &action), XR_SUCCESS);

                if (m_firstBooleanAction == XR_NULL_PATH && inputSourceData.Type == XR_ACTION_TYPE_BOOLEAN_INPUT) {
                    m_firstBooleanAction = action;
                }

                const XrPath bindingPath = StringToPath(instance, inputSourceData.Path);
                m_actionMap.insert({bindingPath, action});
                interactionManager->AddActionBindings(m_interactionProfile, {{action, bindingPath}});
            }

            interactionManager->AddActionSet(m_actionSet);
        }

        ~HumanDrivenInputdevice() override
        {
            if (m_shouldDestroyActionSet) {
                for (const auto& pair : m_actionMap) {
                    REQUIRE_RESULT(xrDestroyAction(pair.second), XR_SUCCESS);
                }
                REQUIRE_RESULT(xrDestroyActionSet(m_actionSet), XR_SUCCESS);
            }
        }

        XrPath TopLevelPath() const override
        {
            return m_topLevelPath;
        }
        // void SetDeviceActive(bool state, WaitUntilBoolActionIsActiveUpdated waitCondition) override
        // {
        //     SetDeviceActiveWithoutWaiting(state);
        //     Wait(state, waitCondition);
        // }
        // void SetDeviceActive(bool state, WaitUntilLosesOrGainsOrientationValidity waitCondition) override
        // {
        //     SetDeviceActiveWithoutWaiting(state);
        //     Wait(state, waitCondition);
        // }

        void SetDeviceActiveWithoutWaiting(bool state, const char* extraMessage = nullptr) const override
        {
            SetDeviceActiveViaConformanceAutomationIfPossible(state);
            ShowDeviceStateMessage(state, extraMessage);
        }

        void Wait(bool state, const WaitUntilBoolActionIsActiveUpdated& waitCondition) const override
        {

            enum class ControllerState
            {
                NotFocused,
                Active,
                Inactive
            };

            XrActionSet detectionActionSet{waitCondition.detectionActionSet};
            XrAction detectionBoolAction{waitCondition.detectionBoolAction};

            // Checks the isActive on a boolean action to determine if a controller is on
            auto findController = [&]() -> ControllerState {
                XrActiveActionSet activeActionSets[] = {
                    {m_actionSet, XR_NULL_PATH},
                    {detectionActionSet, XR_NULL_PATH},
                };
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                syncInfo.countActiveActionSets = detectionActionSet == XR_NULL_HANDLE ? 1 : 2;
                syncInfo.activeActionSets = activeActionSets;
                {
                    const XrResult res = xrSyncActions(m_session, &syncInfo);
                    if (res == XR_SESSION_NOT_FOCUSED) {
                        return ControllerState::NotFocused;
                    }
                    if (res != XR_SUCCESS) {
                        XRC_THROW_XRRESULT(res, xrSyncActions);
                    }
                }

                XrActionStateBoolean booleanActionData{XR_TYPE_ACTION_STATE_BOOLEAN};
                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                getInfo.action = detectionBoolAction == XR_NULL_HANDLE ? m_firstBooleanAction : detectionBoolAction;
                const XrResult res = xrGetActionStateBoolean(m_session, &getInfo, &booleanActionData);
                if (res != XR_SUCCESS) {
                    XRC_THROW_XRRESULT(res, xrGetActionStateBoolean);
                }

                return booleanActionData.isActive ? ControllerState::Active : ControllerState::Inactive;
            };

            const ControllerState desiredControllerState = state ? ControllerState::Active : ControllerState::Inactive;
            auto timeSinceStateChanged = std::chrono::high_resolution_clock::now();
            REQUIRE_MSG(WaitUntilPredicateWithTimeout(
                            [&] {
                                if (findController() != desiredControllerState) {
                                    timeSinceStateChanged = std::chrono::high_resolution_clock::now();
                                }
                                else if (std::chrono::high_resolution_clock::now() - timeSinceStateChanged > 250ms) {
                                    return true;  // Only return true when the controller has been stably active for 250ms.
                                }
                                m_messageDisplay->IterateFrame();

                                return false;
                            },
                            30s, waitDelay),
                        "Input device activity not detected");

            m_messageDisplay->DisplayMessage("");
        }
        XrTime Wait(bool state, const WaitUntilLosesOrGainsOrientationValidity& waitCondition) const override
        {
            XrSpace space{waitCondition.actionSpace};
            XrSpace baseSpace{waitCondition.baseSpace};

            const auto initialTime = std::chrono::steady_clock::now();
            XrTime lastUsed;
            auto makeTimestamp = [=, &lastUsed] {
                auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - initialTime);
                lastUsed = waitCondition.initialLocateTime + nanos.count();
                return lastUsed;
            };

            auto checkTracking = [&]() -> XrSpaceLocationFlags {
                XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr, 0, XrPosefCPP{}};
                const XrResult res = xrLocateSpace(space, baseSpace, makeTimestamp(), &location);
                if (res != XR_SUCCESS) {
                    XRC_THROW_XRRESULT(res, xrLocateSpace);
                }
                return location.locationFlags;
            };

            XrSpaceLocationFlags desiredFlags = state ? XR_SPACE_LOCATION_ORIENTATION_VALID_BIT : 0;
            auto timeSinceStateChanged = std::chrono::high_resolution_clock::now();
            REQUIRE_MSG(WaitUntilPredicateWithTimeout(
                            [&] {
                                if ((checkTracking() & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != desiredFlags) {
                                    timeSinceStateChanged = std::chrono::high_resolution_clock::now();
                                }
                                else if (std::chrono::high_resolution_clock::now() - timeSinceStateChanged > 250ms) {
                                    return true;  // Only return true when the controller has been stably locatable for 250ms.
                                }
                                m_messageDisplay->IterateFrame();

                                return false;
                            },
                            30s, waitDelay),
                        "Input device activity not detected");

            m_messageDisplay->DisplayMessage("");
            return lastUsed;
        }

        void SetDeviceActiveViaConformanceAutomationIfPossible(bool state) const
        {

            if (m_conformanceAutomationExtensionEnabled) {
                PFN_xrSetInputDeviceActiveEXT xrSetInputDeviceActiveEXT{nullptr};
                XRC_CHECK_THROW_XRCMD_UNQUALIFIED_SUCCESS(
                    xrGetInstanceProcAddr(m_instance, "xrSetInputDeviceActiveEXT", (PFN_xrVoidFunction*)&xrSetInputDeviceActiveEXT));
                XRC_CHECK_THROW_XRCMD_UNQUALIFIED_SUCCESS(
                    xrSetInputDeviceActiveEXT(m_session, m_interactionProfile, m_topLevelPath, (XrBool32)state));
            }
        }

        void ShowDeviceStateMessage(bool state, const char* extraMessage = nullptr) const
        {
            uint32_t cap = 0;
            XRC_CHECK_THROW_XRCMD_UNQUALIFIED_SUCCESS(xrPathToString(m_instance, m_topLevelPath, 0, &cap, nullptr));

            std::vector<char> humanReadableName(cap, '\0');
            XRC_CHECK_THROW_XRCMD_UNQUALIFIED_SUCCESS(xrPathToString(m_instance, m_topLevelPath, cap, &cap, humanReadableName.data()));
            humanReadableName.resize(cap);

            std::string action = state ? "Turn on" : "Turn off";
            std::string extra;
            if (extraMessage) {
                extra = std::string{extraMessage};
            }
            m_messageDisplay->DisplayMessage(action + " " + humanReadableName.data() + extra);
        }

        void SetDeviceActive(bool state, bool skipInteraction = false, XrAction detectionBoolAction = XR_NULL_HANDLE,
                             XrActionSet detectionActionSet = XR_NULL_HANDLE) override
        {

            SetDeviceActiveWithoutWaiting(state);
            if (skipInteraction) {
                // Skip human interaction, this is just a hint to the runtime via the extension
                return;
            }
            Wait(state, WaitUntilBoolActionIsActiveUpdated{detectionBoolAction, detectionActionSet});
        }

        void SetButtonStateBool(XrPath button, bool state, bool skipInteraction, XrActionSet extraActionSet = XR_NULL_HANDLE) override
        {
            if (m_conformanceAutomationExtensionEnabled) {
                PFN_xrSetInputDeviceStateBoolEXT xrSetInputDeviceStateBoolEXT{nullptr};
                REQUIRE_RESULT(
                    xrGetInstanceProcAddr(m_instance, "xrSetInputDeviceStateBoolEXT", (PFN_xrVoidFunction*)&xrSetInputDeviceStateBoolEXT),
                    XR_SUCCESS);
                REQUIRE_RESULT(xrSetInputDeviceStateBoolEXT(m_session, m_topLevelPath, button, (XrBool32)state), XR_SUCCESS);
            }

            if (skipInteraction) {
                // Skip human interaction, this is just a hint to the runtime via the extension
                return;
            }

            // Blank the instructions briefly before showing the new instructions.
            m_messageDisplay->DisplayMessage("");
            WaitUntilPredicateWithTimeout(
                [&] {
                    m_messageDisplay->IterateFrame();
                    return false;
                },
                instructionDelay, waitDelay);

            std::vector<char> humanReadableName = CHECK_TWO_CALL(char, {}, xrPathToString, m_instance, button);

            std::string action = state ? "Press" : "Release";
            m_messageDisplay->DisplayMessage(action + " " + humanReadableName.data());

            XrAction actionToDetect = m_actionMap.at(button);

            auto GetButtonState = [&](XrAction action) -> bool {
                XrActiveActionSet activeActionSet[] = {
                    {m_actionSet, XR_NULL_PATH},
                    {extraActionSet, XR_NULL_PATH},
                };
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                syncInfo.countActiveActionSets = extraActionSet == XR_NULL_HANDLE ? 1 : 2;
                syncInfo.activeActionSets = activeActionSet;
                const XrResult syncRes = xrSyncActions(m_session, &syncInfo);
                if (syncRes == XR_SESSION_NOT_FOCUSED) {
                    return false;
                }

                REQUIRE_RESULT(syncRes, XR_SUCCESS);

                XrActionStateBoolean booleanActionData{XR_TYPE_ACTION_STATE_BOOLEAN};
                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                getInfo.action = action;
                REQUIRE_RESULT(xrGetActionStateBoolean(m_session, &getInfo, &booleanActionData), XR_SUCCESS);

                return booleanActionData.currentState;
            };

            REQUIRE_MSG(WaitUntilPredicateWithTimeout(
                            [&]() {
                                m_messageDisplay->IterateFrame();
                                return GetButtonState(actionToDetect) == state;
                            },
                            30s, waitDelay),
                        "Boolean button state not detected");

            m_messageDisplay->DisplayMessage("");
        }

        void SetButtonStateFloat(XrPath button, float state, float epsilon, bool skipInteraction = false,
                                 XrActionSet extraActionSet = XR_NULL_HANDLE) override
        {
            if (m_conformanceAutomationExtensionEnabled) {
                PFN_xrSetInputDeviceStateFloatEXT xrSetInputDeviceStateFloatEXT{nullptr};
                REQUIRE_RESULT(
                    xrGetInstanceProcAddr(m_instance, "xrSetInputDeviceStateFloatEXT", (PFN_xrVoidFunction*)&xrSetInputDeviceStateFloatEXT),
                    XR_SUCCESS);
                REQUIRE_RESULT(xrSetInputDeviceStateFloatEXT(m_session, m_topLevelPath, button, state), XR_SUCCESS);
            }

            if (skipInteraction) {
                // Skip human interaction, this is just a hint to the runtime via the extension
                return;
            }

            // Blank the instructions briefly before showing the new instructions.
            m_messageDisplay->DisplayMessage("");
            WaitUntilPredicateWithTimeout(
                [&] {
                    m_messageDisplay->IterateFrame();
                    return false;
                },
                instructionDelay, waitDelay);

            std::vector<char> humanReadableName = CHECK_TWO_CALL(char, {}, xrPathToString, m_instance, button);

            auto message = std::string("Set ") + humanReadableName.data() + "\nExpected:  " + std::to_string(state);

            XrAction actionToDetect = m_actionMap.at(button);

            auto FloatStateWithinEpsilon = [&](XrAction action, float target, float epsilon) -> bool {
                XrActiveActionSet activeActionSet[] = {
                    {m_actionSet, XR_NULL_PATH},
                    {extraActionSet, XR_NULL_PATH},
                };
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                syncInfo.countActiveActionSets = extraActionSet == XR_NULL_HANDLE ? 1 : 2;
                syncInfo.activeActionSets = activeActionSet;
                const XrResult syncRes = xrSyncActions(m_session, &syncInfo);
                if (syncRes == XR_SESSION_NOT_FOCUSED) {
                    return false;
                }

                REQUIRE_RESULT(syncRes, XR_SUCCESS);

                XrActionStateFloat floatActionData{XR_TYPE_ACTION_STATE_FLOAT};
                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                getInfo.action = action;
                REQUIRE_RESULT(xrGetActionStateFloat(m_session, &getInfo, &floatActionData), XR_SUCCESS);

                auto currentValueMessage = "Current:  " + std::to_string(floatActionData.currentState);
                m_messageDisplay->DisplayMessage(message + "\n" + currentValueMessage);

                return std::fabs(target - floatActionData.currentState) < epsilon;
            };

            REQUIRE_MSG(WaitUntilPredicateWithTimeout(
                            [&]() {
                                m_messageDisplay->IterateFrame();
                                return FloatStateWithinEpsilon(actionToDetect, state, epsilon);
                            },
                            30s, waitDelay),
                        "Float input state not detected");

            m_messageDisplay->DisplayMessage("");
        }

        void SetButtonStateVector2(XrPath button, XrVector2f state, float epsilon, bool skipInteraction = false,
                                   XrActionSet extraActionSet = XR_NULL_HANDLE) override
        {
            if (m_conformanceAutomationExtensionEnabled) {
                PFN_xrSetInputDeviceStateVector2fEXT xrSetInputDeviceStateVector2fEXT{nullptr};
                REQUIRE_RESULT(xrGetInstanceProcAddr(m_instance, "xrSetInputDeviceStateVector2fEXT",
                                                     (PFN_xrVoidFunction*)&xrSetInputDeviceStateVector2fEXT),
                               XR_SUCCESS);
                REQUIRE_RESULT(xrSetInputDeviceStateVector2fEXT(m_session, m_topLevelPath, button, state), XR_SUCCESS);
            }

            if (skipInteraction) {
                // Skip human interaction, this is just a hint to the runtime via the extension
                return;
            }

            // Blank the instructions briefly before showing the new instructions.
            m_messageDisplay->DisplayMessage("");
            WaitUntilPredicateWithTimeout(
                [&] {
                    m_messageDisplay->IterateFrame();
                    return false;
                },
                instructionDelay, waitDelay);

            std::vector<char> humanReadableName = CHECK_TWO_CALL(char, {}, xrPathToString, m_instance, button);

            auto message = std::string("Set ") + humanReadableName.data() + "\nExpected: (" + std::to_string(state.x) + ", " +
                           std::to_string(state.y) + ")";

            XrAction actionToDetect = m_actionMap.at(button);

            auto VectorStateWithinEpsilon = [&](XrAction action, XrVector2f target, float epsilon) -> bool {
                XrActiveActionSet activeActionSet[] = {
                    {m_actionSet, XR_NULL_PATH},
                    {extraActionSet, XR_NULL_PATH},
                };
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                syncInfo.countActiveActionSets = extraActionSet == XR_NULL_HANDLE ? 1 : 2;
                syncInfo.activeActionSets = activeActionSet;
                const XrResult syncRes = xrSyncActions(m_session, &syncInfo);
                if (syncRes == XR_SESSION_NOT_FOCUSED) {
                    return false;
                }

                REQUIRE_RESULT(syncRes, XR_SUCCESS);

                XrActionStateVector2f vectorActionData{XR_TYPE_ACTION_STATE_VECTOR2F};
                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                getInfo.action = action;
                REQUIRE_RESULT(xrGetActionStateVector2f(m_session, &getInfo, &vectorActionData), XR_SUCCESS);

                auto currentValueMessage = "Current:  (" + std::to_string(vectorActionData.currentState.x) + ", " +
                                           std::to_string(vectorActionData.currentState.y) + ")";
                m_messageDisplay->DisplayMessage(message + "\n " + currentValueMessage);

                return std::fabs(target.x - vectorActionData.currentState.x) < epsilon &&
                       std::fabs(target.y - vectorActionData.currentState.y) < epsilon;
            };

            REQUIRE_MSG(WaitUntilPredicateWithTimeout(
                            [&]() {
                                m_messageDisplay->IterateFrame();
                                return VectorStateWithinEpsilon(actionToDetect, state, epsilon);
                            },
                            30s, waitDelay),
                        "Float input state not detected");

            m_messageDisplay->DisplayMessage("");
        }

    private:
        ITestMessageDisplay* const m_messageDisplay;
        const XrInstance m_instance;
        const XrSession m_session;
        const XrPath m_interactionProfile;
        const XrPath m_topLevelPath;
        const bool m_conformanceAutomationExtensionEnabled{false};
        XrActionSet m_actionSet;
        std::map<XrPath, XrAction> m_actionMap;

        XrAction m_firstBooleanAction{XR_NULL_PATH};  // Used to detect controller state
        bool m_shouldDestroyActionSet = true;         // Don't destroy the action set if the test provided one
    };

    std::unique_ptr<IInputTestDevice> CreateTestDevice(ITestMessageDisplay* const messageDisplay,
                                                       InteractionManager* const interactionManager, XrInstance instance, XrSession session,
                                                       XrPath interactionProfile, XrPath topLevelPath,
                                                       const InputSourcePathAvailCollection& interactionProfilePaths)
    {
        return std::make_unique<HumanDrivenInputdevice>(messageDisplay, interactionManager, instance, session, interactionProfile,
                                                        topLevelPath, interactionProfilePaths);
    }

    std::unique_ptr<IInputTestDevice> CreateTestDevice(ITestMessageDisplay* const messageDisplay, XrInstance instance, XrSession session,
                                                       XrPath interactionProfile, XrPath topLevelPath, XrActionSet actionSet,
                                                       XrAction firstBooleanAction, std::map<XrPath, XrAction>& actionMap)
    {
        return std::make_unique<HumanDrivenInputdevice>(messageDisplay, instance, session, interactionProfile, topLevelPath, actionSet,
                                                        firstBooleanAction, actionMap);
    }
}  // namespace Conformance
