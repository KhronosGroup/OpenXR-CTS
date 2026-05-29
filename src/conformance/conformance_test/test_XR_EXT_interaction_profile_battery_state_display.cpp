// Copyright (c) 2019-2026 The Khronos Group Inc.
// Copyright (c) Meta Platforms, LLC and its affiliates. All rights reserved.
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
#include "conformance_utils.h"
#include "composition_utils.h"
#include "action_utils.h"
#include "input_testinputdevice.h"
#include "interaction_info.h"
#include "matchers.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    TEST_CASE("XR_EXT_interaction_profile_battery_state_display", "[XR_EXT_interaction_profile_battery_state_display]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_INTERACTION_PROFILE_BATTERY_STATE_DISPLAY_EXTENSION_NAME)) {
            SKIP(XR_EXT_INTERACTION_PROFILE_BATTERY_STATE_DISPLAY_EXTENSION_NAME " not supported");
        }

        AutoBasicInstance instance({XR_EXT_INTERACTION_PROFILE_BATTERY_STATE_DISPLAY_EXTENSION_NAME}, AutoBasicInstance::createSystemId);
        AutoBasicSession session(AutoBasicSession::createSession, instance);

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set");
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction selectAction{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.actionName, "test_select");
        strcpy(actionCreateInfo.localizedActionName, "test select");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &selectAction), XR_SUCCESS);

        XrPath simpleProfilePath = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
        XrPath leftSelectPath = StringToPath(instance, "/user/hand/left/input/select/click");
        XrPath rightSelectPath = StringToPath(instance, "/user/hand/right/input/select/click");
        std::vector<XrActionSuggestedBinding> bindings{{selectAction, leftSelectPath}, {selectAction, rightSelectPath}};

        XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBindings.interactionProfile = simpleProfilePath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
        REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &suggestedBindings), XR_SUCCESS);

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.actionSets = &actionSet;
        attachInfo.countActionSets = 1;
        REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

        XrPath leftHandPath = StringToPath(instance, "/user/hand/left");
        XrPath rightHandPath = StringToPath(instance, "/user/hand/right");

        SECTION("Query battery state for left hand")
        {
            XrBatteryStateDisplayEXT batteryState{XR_TYPE_BATTERY_STATE_DISPLAY_EXT};
            batteryState.batteryLevel = -1.f;
            XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
            interactionProfileState.next = &batteryState;
            REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, leftHandPath, &interactionProfileState), XR_SUCCESS);

            if (interactionProfileState.interactionProfile == XR_NULL_PATH) {
                // If interactionProfile is XR_NULL_PATH, the runtime must not set the VALID bit.
                INFO("interactionProfile is XR_NULL_PATH, VALID bit must not be set");
                REQUIRE((batteryState.stateFlags & XR_BATTERY_STATE_DISPLAY_STATE_VALID_BIT_EXT) == 0);
            }
            else {
                // If the VALID bit is set, batteryLevel must be in [0.0, 1.0].
                if ((batteryState.stateFlags & XR_BATTERY_STATE_DISPLAY_STATE_VALID_BIT_EXT) != 0) {
                    INFO("VALID bit set, batteryLevel must be in [0.0, 1.0]");
                    REQUIRE(batteryState.batteryLevel >= 0.0f);
                    REQUIRE(batteryState.batteryLevel <= 1.0f);
                }
            }
        }

        SECTION("Query battery state for right hand")
        {
            XrBatteryStateDisplayEXT batteryState{XR_TYPE_BATTERY_STATE_DISPLAY_EXT};
            XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
            interactionProfileState.next = &batteryState;
            REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, rightHandPath, &interactionProfileState), XR_SUCCESS);

            if (interactionProfileState.interactionProfile == XR_NULL_PATH) {
                INFO("interactionProfile is XR_NULL_PATH, VALID bit must not be set");
                REQUIRE((batteryState.stateFlags & XR_BATTERY_STATE_DISPLAY_STATE_VALID_BIT_EXT) == 0);
            }
            else {
                if ((batteryState.stateFlags & XR_BATTERY_STATE_DISPLAY_STATE_VALID_BIT_EXT) != 0) {
                    INFO("VALID bit set, batteryLevel must be in [0.0, 1.0]");
                    REQUIRE(batteryState.batteryLevel >= 0.0f);
                    REQUIRE(batteryState.batteryLevel <= 1.0f);
                }
            }
        }
    }

    TEST_CASE("XR_EXT_interaction_profile_battery_state_display-interactive",
              "[XR_EXT_interaction_profile_battery_state_display][actions][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_INTERACTION_PROFILE_BATTERY_STATE_DISPLAY_EXTENSION_NAME)) {
            SKIP(XR_EXT_INTERACTION_PROFILE_BATTERY_STATE_DISPLAY_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper("XR_EXT_interaction_profile_battery_state_display",
                                            {XR_EXT_INTERACTION_PROFILE_BATTERY_STATE_DISPLAY_EXTENSION_NAME});
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();
        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set");
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction boolAction{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.actionName, "test_select");
        strcpy(actionCreateInfo.localizedActionName, "test select");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &boolAction), XR_SUCCESS);

        bool leftUnderTest = globalData.leftHandUnderTest;
        const char* topLevelPathString = leftUnderTest ? "/user/hand/left" : "/user/hand/right";
        XrPath topLevelPath = StringToPath(instance, topLevelPathString);

        std::shared_ptr<IInputTestDevice> inputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), topLevelPath,
                             GetSimpleInteractionProfile().BindingPaths);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(
            StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString),
            {{boolAction,
              StringToPath(instance, leftUnderTest ? "/user/hand/left/input/select/click" : "/user/hand/right/input/select/click")}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        inputDevice->SetDeviceActive(/*state=*/true, /*skipInteraction=*/false, boolAction, actionSet);
        actionLayerManager.WaitForSessionFocusWithMessage();

        XrBatteryStateDisplayEXT batteryState{XR_TYPE_BATTERY_STATE_DISPLAY_EXT};
        XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
        interactionProfileState.next = &batteryState;
        REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, topLevelPath, &interactionProfileState), XR_SUCCESS);

        // With an active device, we expect a valid interaction profile.
        REQUIRE(interactionProfileState.interactionProfile != XR_NULL_PATH);

        // Validate battery state flags and level.
        if ((batteryState.stateFlags & XR_BATTERY_STATE_DISPLAY_STATE_VALID_BIT_EXT) != 0) {
            INFO("VALID bit set, batteryLevel must be in [0.0, 1.0]");
            REQUIRE(batteryState.batteryLevel >= 0.0f);
            REQUIRE(batteryState.batteryLevel <= 1.0f);
        }
    }
}  // namespace Conformance
