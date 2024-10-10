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

#include "action_utils.h"
#include "common/xr_linear.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "input_testinputdevice.h"
#include "report.h"
#include "utilities/event_reader.h"
#include "utilities/throw_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <chrono>
#include <string>

namespace Conformance
{
    using namespace std::chrono_literals;

    const std::chrono::seconds inputWaitTime = 20s;
    const std::chrono::seconds stickyWaitTime = 5s;

    enum class eHand
    {
        Left_Hand,
        Right_Hand
    };

    enum class eDirection
    {
        None,
        Up,
        Down,
        Left,
        Right,
        Center
    };

    enum class eControllerComponent
    {
        Thumbstick,
        Trackpad,
        Both
    };

    enum class eTrackpadShape
    {
        None,
        Round,
        Pill_Vertical,
        Pill_Horizontal
    };

    struct ControllerDescription
    {
        XrPath interactionProfile;
        std::string interactionProfileShortname;
        std::string interactionProfilePrintname;
        eControllerComponent controllerComponents;
        eTrackpadShape trackpadShape;
    };

    struct PathPrintnamePair
    {
        XrPath interactionProfile;
        std::string interactionProfilePrintname;
    };

    static inline std::vector<XrBindingModificationBaseHeaderKHR*>
    makeBasePointerVec(std::vector<XrInteractionProfileDpadBindingEXT>& vBindingModifs)
    {
        std::vector<XrBindingModificationBaseHeaderKHR*> vBindingModifsBase;
        vBindingModifsBase.reserve(vBindingModifs.size());
        for (auto& modif : vBindingModifs) {
            vBindingModifsBase.push_back(reinterpret_cast<XrBindingModificationBaseHeaderKHR*>(&modif));
        }
        return vBindingModifsBase;
    }

    XrResult CreateActionSet(XrActionSet* actionSet, const char* actionSetName, uint32_t priority, XrInstance instance)
    {
        XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetInfo.actionSetName, actionSetName);
        strcpy(actionSetInfo.localizedActionSetName, actionSetName);
        actionSetInfo.priority = priority;
        return xrCreateActionSet(instance, &actionSetInfo, actionSet);
    }

    XrResult CreateAction(XrAction* action, const char* actionName, XrActionType actionType, XrActionSet actionSet)
    {
        XrActionCreateInfo actioninfo{XR_TYPE_ACTION_CREATE_INFO};
        strcpy(actioninfo.actionName, actionName);
        actioninfo.actionType = actionType;
        strcpy(actioninfo.localizedActionName, actionName);
        return xrCreateAction(actionSet, &actioninfo, action);
    }

    void SetDefaultModifiers(XrInteractionProfileDpadBindingEXT* xrDpadModification, XrActionSet actionSet)
    {
        xrDpadModification->actionSet = actionSet;
        xrDpadModification->centerRegion = 0.25f;
        xrDpadModification->wedgeAngle = 2.0f;
        xrDpadModification->forceThreshold = 0.8f;
        xrDpadModification->forceThresholdReleased = 0.2f;
    }

    XrResult SuggestBinding(XrInteractionProfileDpadBindingEXT* xrDpadModification, XrActionSuggestedBinding suggestedBinding,
                            XrInstance instance, const char* interactionProfile)
    {
        // Add dpad binding modifiers to binding modifications vector
        std::vector<XrInteractionProfileDpadBindingEXT> vBindingModifs{{*xrDpadModification}};
        std::vector<XrBindingModificationBaseHeaderKHR*> vBindingModifsBase = makeBasePointerVec(vBindingModifs);

        XrBindingModificationsKHR xrBindingModifications{XR_TYPE_BINDING_MODIFICATIONS_KHR};
        xrBindingModifications.bindingModifications = vBindingModifsBase.data();
        xrBindingModifications.bindingModificationCount = (uint32_t)vBindingModifsBase.size();

        std::vector<XrActionSuggestedBinding> vActionBindings;
        vActionBindings.push_back(suggestedBinding);

        // Create interaction profile/controller path
        XrPath xrInteractionProfilePath;
        xrStringToPath(instance, interactionProfile, &xrInteractionProfilePath);

        // Set suggested binding to interaction profile
        XrInteractionProfileSuggestedBinding xrInteractionProfileSuggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        xrInteractionProfileSuggestedBinding.interactionProfile = xrInteractionProfilePath;
        xrInteractionProfileSuggestedBinding.suggestedBindings = vActionBindings.data();
        xrInteractionProfileSuggestedBinding.countSuggestedBindings = (uint32_t)vActionBindings.size();

        // Set binding modifications to interaction profile's suggested binding
        xrInteractionProfileSuggestedBinding.next = &xrBindingModifications;

        // Finally, suggest interaction profile bindings to runtime
        return xrSuggestInteractionProfileBindings(instance, &xrInteractionProfileSuggestedBinding);
    }

    // Assemble controller component path
    void AssembleInputPath(XrPath* outPath, eHand hand, eControllerComponent controllerComponents, eDirection direction,
                           XrInstance instance)
    {
        static const std::string sLeftHand = "/user/hand/left";
        static const std::string sRightHand = "/user/hand/right";
        static const std::string sThumbstick = "/input/thumbstick";
        static const std::string sTrackpad = "/input/trackpad";
        static const std::string sUp = "/dpad_up";
        static const std::string sDown = "/dpad_down";
        static const std::string sLeft = "/dpad_left";
        static const std::string sRight = "/dpad_right";
        static const std::string sCenter = "/dpad_center";

        // Create top level user path
        if (controllerComponents == eControllerComponent::Both) {
            // We're using the "both" value as a signal to just generate the top level path
            if (hand == eHand::Left_Hand) {
                REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, sLeftHand.c_str(), outPath));
            }
            else {
                REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, sRightHand.c_str(), outPath));
            }

            return;
        }

        // Generate binding path
        std::string sPath;

        // Add hand
        switch (hand) {
        case eHand::Left_Hand:
            sPath.append(sLeftHand);
            break;
        default:
            sPath.append(sRightHand);
            break;
        }

        // Add component
        switch (controllerComponents) {
        case eControllerComponent::Trackpad:
            sPath.append(sTrackpad);
            break;
        default:
            sPath.append(sThumbstick);
            break;
        }

        // Add direction (if any)
        switch (direction) {
        case eDirection::Up:
            sPath.append(sUp);
            break;
        case eDirection::Down:
            sPath.append(sDown);
            break;
        case eDirection::Left:
            sPath.append(sLeft);
            break;
        case eDirection::Right:
            sPath.append(sRight);
            break;
        case eDirection::Center:
            // This might make a invalid paths for thunbsticks, but is useful for error checking.
            sPath.append(sCenter);
            break;
        case eDirection::None:
        default:
            break;
        }

        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, sPath.c_str(), outPath));
    }

    // Create dpad paths
    XrPath pathHand_L, pathHand_R;
    XrPath pathThumbstick_L, pathThumbstick_R, pathTrackpad_L, pathTrackpad_R;
    XrPath pathDpad_Thumbstick_Up_L, pathDpad_Thumbstick_Down_L, pathDpad_Thumbstick_Left_L, pathDpad_Thumbstick_Right_L,
        pathDpad_Thumbstick_Center_L;
    XrPath pathDpad_Thumbstick_Up_R, pathDpad_Thumbstick_Down_R, pathDpad_Thumbstick_Left_R, pathDpad_Thumbstick_Right_R,
        pathDpad_Thumbstick_Center_R;
    XrPath pathDpad_Trackpad_Up_L, pathDpad_Trackpad_Down_L, pathDpad_Trackpad_Left_L, pathDpad_Trackpad_Right_L,
        pathDpad_Trackpad_Center_L;
    XrPath pathDpad_Trackpad_Up_R, pathDpad_Trackpad_Down_R, pathDpad_Trackpad_Left_R, pathDpad_Trackpad_Right_R,
        pathDpad_Trackpad_Center_R;
    void InitDpadPaths(XrInstance instance)
    {
        // Top level user path
        AssembleInputPath(&pathHand_L, eHand::Left_Hand, eControllerComponent::Both, eDirection::Center, instance);
        AssembleInputPath(&pathHand_R, eHand::Right_Hand, eControllerComponent::Both, eDirection::Center, instance);

        // Component paths
        AssembleInputPath(&pathThumbstick_L, eHand::Left_Hand, eControllerComponent::Thumbstick, eDirection::None, instance);
        AssembleInputPath(&pathThumbstick_R, eHand::Right_Hand, eControllerComponent::Thumbstick, eDirection::None, instance);
        AssembleInputPath(&pathTrackpad_L, eHand::Left_Hand, eControllerComponent::Trackpad, eDirection::None, instance);
        AssembleInputPath(&pathTrackpad_R, eHand::Right_Hand, eControllerComponent::Trackpad, eDirection::None, instance);

        // Thumbstick - Left Hand
        AssembleInputPath(&pathDpad_Thumbstick_Up_L, eHand::Left_Hand, eControllerComponent::Thumbstick, eDirection::Up, instance);
        AssembleInputPath(&pathDpad_Thumbstick_Down_L, eHand::Left_Hand, eControllerComponent::Thumbstick, eDirection::Down, instance);
        AssembleInputPath(&pathDpad_Thumbstick_Left_L, eHand::Left_Hand, eControllerComponent::Thumbstick, eDirection::Left, instance);
        AssembleInputPath(&pathDpad_Thumbstick_Right_L, eHand::Left_Hand, eControllerComponent::Thumbstick, eDirection::Right, instance);
        AssembleInputPath(&pathDpad_Thumbstick_Center_L, eHand::Left_Hand, eControllerComponent::Thumbstick, eDirection::Center, instance);

        // Thumbstick - Right Hand
        AssembleInputPath(&pathDpad_Thumbstick_Up_R, eHand::Right_Hand, eControllerComponent::Thumbstick, eDirection::Up, instance);
        AssembleInputPath(&pathDpad_Thumbstick_Down_R, eHand::Right_Hand, eControllerComponent::Thumbstick, eDirection::Down, instance);
        AssembleInputPath(&pathDpad_Thumbstick_Left_R, eHand::Right_Hand, eControllerComponent::Thumbstick, eDirection::Left, instance);
        AssembleInputPath(&pathDpad_Thumbstick_Right_R, eHand::Right_Hand, eControllerComponent::Thumbstick, eDirection::Right, instance);
        AssembleInputPath(&pathDpad_Thumbstick_Center_R, eHand::Right_Hand, eControllerComponent::Thumbstick, eDirection::Center, instance);

        // Trackpad - Left Hand
        AssembleInputPath(&pathDpad_Trackpad_Up_L, eHand::Left_Hand, eControllerComponent::Trackpad, eDirection::Up, instance);
        AssembleInputPath(&pathDpad_Trackpad_Down_L, eHand::Left_Hand, eControllerComponent::Trackpad, eDirection::Down, instance);
        AssembleInputPath(&pathDpad_Trackpad_Left_L, eHand::Left_Hand, eControllerComponent::Trackpad, eDirection::Left, instance);
        AssembleInputPath(&pathDpad_Trackpad_Right_L, eHand::Left_Hand, eControllerComponent::Trackpad, eDirection::Right, instance);
        AssembleInputPath(&pathDpad_Trackpad_Center_L, eHand::Left_Hand, eControllerComponent::Trackpad, eDirection::Center, instance);

        // Trackpad - Right Hand
        AssembleInputPath(&pathDpad_Trackpad_Up_R, eHand::Right_Hand, eControllerComponent::Trackpad, eDirection::Up, instance);
        AssembleInputPath(&pathDpad_Trackpad_Down_R, eHand::Right_Hand, eControllerComponent::Trackpad, eDirection::Down, instance);
        AssembleInputPath(&pathDpad_Trackpad_Left_R, eHand::Right_Hand, eControllerComponent::Trackpad, eDirection::Left, instance);
        AssembleInputPath(&pathDpad_Trackpad_Right_R, eHand::Right_Hand, eControllerComponent::Trackpad, eDirection::Right, instance);
        AssembleInputPath(&pathDpad_Trackpad_Center_R, eHand::Right_Hand, eControllerComponent::Trackpad, eDirection::Center, instance);
    }

    // Initialize supported controllers
    static XrPath pathDaydream, pathIndex, pathVive, pathGo, pathTouch, pathMS;
    std::vector<ControllerDescription> vSupportedControllers;
    void InitControllers(XrInstance instance)
    {
        // Generate handles for the supported controllers
        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, "/interaction_profiles/google/daydream_controller", &pathDaydream));
        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, "/interaction_profiles/valve/index_controller", &pathIndex));
        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, "/interaction_profiles/htc/vive_controller", &pathVive));
        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, "/interaction_profiles/oculus/go_controller", &pathGo));
        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", &pathTouch));
        REQUIRE_RESULT_SUCCEEDED(xrStringToPath(instance, "/interaction_profiles/microsoft/motion_controller", &pathMS));

        // Generate controller descriptions
        // clang-format off
        vSupportedControllers.clear(); // Needs to be reset, state is kept around between sections.
        vSupportedControllers.push_back({pathDaydream, "google/daydream_controller", "Daydream Controller", eControllerComponent::Trackpad, eTrackpadShape::Round});
        vSupportedControllers.push_back({pathIndex, "valve/index_controller", "Index Controller", eControllerComponent::Both, eTrackpadShape::Pill_Vertical});
        vSupportedControllers.push_back({pathVive, "htc/vive_controller", "Vive Controller", eControllerComponent::Trackpad, eTrackpadShape::Round});
        vSupportedControllers.push_back({pathGo, "oculus/go_controller", "Go Controller", eControllerComponent::Trackpad, eTrackpadShape::Round});
        vSupportedControllers.push_back({pathTouch, "oculus/touch_controller", "Touch Controller", eControllerComponent::Thumbstick, eTrackpadShape::None});
        vSupportedControllers.push_back({pathMS, "microsoft/motion_controller", "Motion Controller", eControllerComponent::Both, eTrackpadShape::Round});
        // clang-format on
    }

    void InitTestControllersAndDpadPaths(XrInstance instance)
    {
        GlobalData& globalData = GetGlobalData();
        if (globalData.IsInstanceExtensionSupported(XR_EXT_DPAD_BINDING_EXTENSION_NAME) ||
            !globalData.IsInstanceExtensionSupported(XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME)) {
            WARN("XR_EXT_dpad_binding support implies XR_KHR_binding_modification support.");
        }

        // Create supported interaction profile paths
        InitControllers(instance);

        // Create dpad paths
        InitDpadPaths(instance);
    }

    void InitInteractiveInteractionProfiles(std::vector<PathPrintnamePair>& vInteractionProfiles, eControllerComponent controllerComponent)
    {
        // This function will only push one set of actions and shouldn't be called with both.
        XRC_CHECK_THROW(controllerComponent != eControllerComponent::Both);

        // Find interaction profiles that support the provided controller component
        for (auto& supportedController : vSupportedControllers) {
            if (!IsInteractionProfileEnabled(supportedController.interactionProfileShortname.c_str())) {
                continue;
            }

            if (supportedController.controllerComponents != controllerComponent &&
                supportedController.controllerComponents != eControllerComponent::Both) {
                continue;
            }

            vInteractionProfiles.push_back({supportedController.interactionProfile, supportedController.interactionProfilePrintname});
        }
    }

    // Suggest binding
    void SuggestBinding(XrInstance instance, XrPath interactionProfile, std::vector<XrActionSuggestedBinding>& vActionBindings,
                        XrBindingModificationsKHR* xrBindingModifications, XrResult expectedResult)
    {
        // Set suggested binding to interaction profile
        XrInteractionProfileSuggestedBinding xrInteractionProfileSuggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        xrInteractionProfileSuggestedBinding.interactionProfile = interactionProfile;
        xrInteractionProfileSuggestedBinding.suggestedBindings = vActionBindings.data();
        xrInteractionProfileSuggestedBinding.countSuggestedBindings = (uint32_t)vActionBindings.size();

        // Set binding modifications to interaction profile's suggested binding
        xrInteractionProfileSuggestedBinding.next = xrBindingModifications;

        // Suggest interaction profile bindings to runtime
        REQUIRE_RESULT(expectedResult, xrSuggestInteractionProfileBindings(instance, &xrInteractionProfileSuggestedBinding));
    }

    void CreateBindingModifications(std::vector<XrBindingModificationBaseHeaderKHR*>& vBindingModifsBase,
                                    std::vector<XrInteractionProfileDpadBindingEXT>& vBindingModifs,
                                    XrInteractionProfileDpadBindingEXT* xrDpadModification, eControllerComponent controllerComponent)
    {
        if (!xrDpadModification) {
            return;
        }

        // Set component path for this binding modification
        XrPath pathLeft, pathRight;
        if (controllerComponent == eControllerComponent::Thumbstick) {
            pathLeft = pathThumbstick_L;
            pathRight = pathThumbstick_R;
        }
        else {
            pathLeft = pathTrackpad_L;
            pathRight = pathTrackpad_R;
        }

        // Duplicate requested binding modification values to left and right controllers
        XrInteractionProfileDpadBindingEXT dpadModifications_L = *xrDpadModification;
        dpadModifications_L.binding = pathLeft;
        vBindingModifs.push_back(dpadModifications_L);

        XrInteractionProfileDpadBindingEXT dpadModifications_R = *xrDpadModification;
        dpadModifications_R.binding = pathRight;
        vBindingModifs.push_back(dpadModifications_R);

        // Convert dpad binding modification to a khr binding modification struct
        vBindingModifsBase = makeBasePointerVec(vBindingModifs);
    }

    void SuggestBindings(XrInstance instance, std::vector<XrActionSuggestedBinding>& vActionBindingsThumbstick,
                         std::vector<XrActionSuggestedBinding>& vActionBindingsTrackpad,
                         XrInteractionProfileDpadBindingEXT* xrDpadModification, XrResult expectedResult)
    {
        // Combine thumbstick and trackpad action bindings
        std::vector<XrActionSuggestedBinding> vActionBindingsCombined = vActionBindingsThumbstick;
        vActionBindingsCombined.insert(vActionBindingsCombined.end(), vActionBindingsTrackpad.begin(), vActionBindingsTrackpad.end());

        // Setup binding modifications for each controller component
        std::vector<XrBindingModificationBaseHeaderKHR*> vBindingModifsBase_Thumbstick, vBindingModifsBase_Trackpad,
            vBindingModifsBase_Combined;
        std::vector<XrInteractionProfileDpadBindingEXT> vBindingModifs_Thumbstick, vBindingModifs_Trackpad;

        CreateBindingModifications(vBindingModifsBase_Thumbstick, vBindingModifs_Thumbstick, xrDpadModification,
                                   eControllerComponent::Thumbstick);
        XrBindingModificationsKHR xrBindingModifications_Thumbstick{XR_TYPE_BINDING_MODIFICATIONS_KHR};
        xrBindingModifications_Thumbstick.bindingModifications = vBindingModifsBase_Thumbstick.data();
        xrBindingModifications_Thumbstick.bindingModificationCount = (uint32_t)vBindingModifsBase_Thumbstick.size();

        CreateBindingModifications(vBindingModifsBase_Trackpad, vBindingModifs_Trackpad, xrDpadModification,
                                   eControllerComponent::Trackpad);
        XrBindingModificationsKHR xrBindingModifications_Trackpad{XR_TYPE_BINDING_MODIFICATIONS_KHR};
        xrBindingModifications_Trackpad.bindingModifications = vBindingModifsBase_Trackpad.data();
        xrBindingModifications_Trackpad.bindingModificationCount = (uint32_t)vBindingModifsBase_Trackpad.size();

        XrBindingModificationsKHR xrBindingModifications_Combined{XR_TYPE_BINDING_MODIFICATIONS_KHR};
        vBindingModifsBase_Combined = vBindingModifsBase_Thumbstick;
        vBindingModifsBase_Combined.insert(vBindingModifsBase_Combined.end(), vBindingModifsBase_Trackpad.begin(),
                                           vBindingModifsBase_Trackpad.end());
        xrBindingModifications_Combined.bindingModifications = vBindingModifsBase_Combined.data();
        xrBindingModifications_Combined.bindingModificationCount = (uint32_t)vBindingModifsBase_Combined.size();

        // Suggest bindings
        SuggestBinding(instance, pathDaydream, vActionBindingsTrackpad, xrDpadModification ? &xrBindingModifications_Trackpad : nullptr,
                       expectedResult);
        SuggestBinding(instance, pathIndex, vActionBindingsCombined, xrDpadModification ? &xrBindingModifications_Combined : nullptr,
                       expectedResult);
        SuggestBinding(instance, pathVive, vActionBindingsTrackpad, xrDpadModification ? &xrBindingModifications_Trackpad : nullptr,
                       expectedResult);
        SuggestBinding(instance, pathGo, vActionBindingsTrackpad, xrDpadModification ? &xrBindingModifications_Trackpad : nullptr,
                       expectedResult);
        SuggestBinding(instance, pathTouch, vActionBindingsThumbstick, xrDpadModification ? &xrBindingModifications_Thumbstick : nullptr,
                       expectedResult);
        SuggestBinding(instance, pathMS, vActionBindingsCombined, xrDpadModification ? &xrBindingModifications_Combined : nullptr,
                       expectedResult);
    }

    bool EndFrameB(const XrFrameState& frameState, CompositionHelper& compositionHelper, std::vector<XrCompositionLayerBaseHeader*>& layers)
    {
        compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);
        compositionHelper.PollEvents();
        return true;
    }

    bool WaitForDpadInput(XrAction action, XrActionsSyncInfo* syncInfo, ActionLayerManager& actionLayerManager, XrSession session)
    {
        XrActionStateBoolean actionStateBoolean{XR_TYPE_ACTION_STATE_BOOLEAN};
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;

        XrResult res;
        const auto startTime = std::chrono::system_clock::now();
        while (std::chrono::system_clock::now() - startTime < inputWaitTime) {
            {
                actionLayerManager.IterateFrame();
                res = xrSyncActions(session, syncInfo);
                if (XR_UNQUALIFIED_SUCCESS(res)) {
                    res = xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean);

                    if (XR_UNQUALIFIED_SUCCESS(res) && actionStateBoolean.changedSinceLastSync) {
                        if (actionStateBoolean.currentState) {
                            ReportF("Dpad input detected");
                        }

                        return actionStateBoolean.changedSinceLastSync && actionStateBoolean.currentState;
                    }
                }

                REQUIRE_RESULT_SUCCEEDED(res);
            }
        }

        FAIL("Time out waiting for session focus on xrSyncActions");
        return false;
    }

    bool WaitForStickyDpadInput(XrAction action, XrActionsSyncInfo* syncInfo, ActionLayerManager& actionLayerManager, XrSession session)
    {
        XrActionStateBoolean actionStateBoolean{XR_TYPE_ACTION_STATE_BOOLEAN};
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;

        XrResult res;
        bool isStickyStarted = false;
        const auto startTime = std::chrono::system_clock::now();
        while (std::chrono::system_clock::now() - startTime < inputWaitTime) {
            {
                actionLayerManager.IterateFrame();
                res = xrSyncActions(session, syncInfo);
                if (XR_UNQUALIFIED_SUCCESS(res)) {
                    res = xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean);

                    if (XR_UNQUALIFIED_SUCCESS(res) && actionStateBoolean.changedSinceLastSync) {
                        if (actionStateBoolean.currentState) {
                            ReportF("Sticky dpad input detected...");
                            isStickyStarted = true;

                            // Detect hold
                            const auto stickyTime = std::chrono::system_clock::now();
                            while (std::chrono::system_clock::now() - stickyTime < stickyWaitTime) {
                                actionLayerManager.IterateFrame();
                                xrSyncActions(session, syncInfo);
                                xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean);

                                if (actionStateBoolean.changedSinceLastSync && !actionStateBoolean.currentState) {
                                    ReportF("Sticky dpad input prematurely released...");
                                    isStickyStarted = false;
                                }
                            }
                            return isStickyStarted;
                        }
                    }
                }

                REQUIRE_RESULT_SUCCEEDED(res);
            }
        }

        FAIL("Time out waiting for session focus on xrSyncActions");
        return false;
    }

    using fnWaitForDpadInput = std::function<bool(XrAction, XrActionsSyncInfo*, ActionLayerManager&, XrSession)>;

    struct TestSet
    {
        XrAction action;
        std::string sInstruction;
        std::string sTimeoutError;
    };

    XrAction dpadUp_L, dpadDown_L, dpadLeft_L, dpadRight_L, dpadCenter_L, dpadUp_R, dpadDown_R, dpadLeft_R, dpadRight_R, dpadCenter_R;
    XrActionSet InitInteractiveActions(XrInstance instance)
    {
        // Create action set
        XrActionSet dpadActionSet = XR_NULL_HANDLE;
        REQUIRE_RESULT_SUCCEEDED(CreateActionSet(&dpadActionSet, "dpads", 0, instance));

        // Create generic dpad actions
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadUp_L, "dpad_action_up_l", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadDown_L, "dpad_action_down_l", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadLeft_L, "dpad_action_left_l", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadRight_L, "dpad_action_right_l", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadCenter_L, "dpad_action_center_l", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));

        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadUp_R, "dpad_action_up_r", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadDown_R, "dpad_action_down_r", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadLeft_R, "dpad_action_left_r", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadRight_R, "dpad_action_right_r", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadCenter_R, "dpad_action_center_r", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));

        return dpadActionSet;
    }

    void InitInteractiveActionBindings(std::vector<XrActionSuggestedBinding>& vActionBindings, eControllerComponent controllerComponent)
    {
        // This function will only push one set of actions and shouldn't be called with both.
        XRC_CHECK_THROW(controllerComponent != eControllerComponent::Both);

        auto thumbOrPad = [controllerComponent](XrPath thumb, XrPath pad) -> XrPath {
            return (controllerComponent == eControllerComponent::Thumbstick) ? thumb : pad;
        };

        // Create action bindings
        vActionBindings.push_back({dpadUp_L, thumbOrPad(pathDpad_Thumbstick_Up_L, pathDpad_Trackpad_Up_L)});
        vActionBindings.push_back({dpadDown_L, thumbOrPad(pathDpad_Thumbstick_Down_L, pathDpad_Trackpad_Down_L)});
        vActionBindings.push_back({dpadLeft_L, thumbOrPad(pathDpad_Thumbstick_Left_L, pathDpad_Trackpad_Left_L)});
        vActionBindings.push_back({dpadRight_L, thumbOrPad(pathDpad_Thumbstick_Right_L, pathDpad_Trackpad_Right_L)});

        vActionBindings.push_back({dpadUp_R, thumbOrPad(pathDpad_Thumbstick_Up_R, pathDpad_Trackpad_Up_R)});
        vActionBindings.push_back({dpadDown_R, thumbOrPad(pathDpad_Thumbstick_Down_R, pathDpad_Trackpad_Down_R)});
        vActionBindings.push_back({dpadLeft_R, thumbOrPad(pathDpad_Thumbstick_Left_R, pathDpad_Trackpad_Left_R)});
        vActionBindings.push_back({dpadRight_R, thumbOrPad(pathDpad_Thumbstick_Right_R, pathDpad_Trackpad_Right_R)});
        if (controllerComponent == eControllerComponent::Trackpad) {
            vActionBindings.push_back({dpadCenter_L, pathDpad_Trackpad_Center_L});
            vActionBindings.push_back({dpadCenter_R, pathDpad_Trackpad_Center_R});
        }
    }

    void CreateStickyBindings(XrInteractionProfileDpadBindingEXT* xrDpadModification_L,
                              XrInteractionProfileDpadBindingEXT* xrDpadModification_R, XrActionSet dpadActionSet,
                              eControllerComponent controllerComponent)
    {
        // Set dpad binding modifiers
        SetDefaultModifiers(xrDpadModification_L, dpadActionSet);
        xrDpadModification_L->binding = (controllerComponent == eControllerComponent::Thumbstick) ? pathThumbstick_L : pathTrackpad_L;
        xrDpadModification_L->isSticky = XR_TRUE;

        SetDefaultModifiers(xrDpadModification_R, dpadActionSet);
        xrDpadModification_R->binding = (controllerComponent == eControllerComponent::Thumbstick) ? pathThumbstick_R : pathTrackpad_R;
        xrDpadModification_R->isSticky = XR_TRUE;
    }

    void GenerateDirectionalTestSet(std::vector<TestSet>& tests, eControllerComponent controllerComponent)
    {
        GlobalData& globalData = GetGlobalData();
        bool leftUnderTest = globalData.leftHandUnderTest;
        bool rightUnderTest = globalData.rightHandUnderTest;
        std::string sTimeoutError = "Time out waiting for dpad input";
        std::string sComponent =
            (controllerComponent == eControllerComponent::Thumbstick) ? "thumbstick and release." : "trackpad and release.";
        if (leftUnderTest) {
            tests.push_back({dpadUp_L, "(1) With your LEFT controller, push fully UP on your " + sComponent, sTimeoutError});
            tests.push_back({dpadDown_L, "(2) With your LEFT controller, push fully DOWN on your " + sComponent, sTimeoutError});
            tests.push_back({dpadLeft_L, "(3) With your LEFT controller, push fully LEFT on your " + sComponent, sTimeoutError});
            tests.push_back({dpadRight_L, "(4) With your LEFT controller, push fully RIGHT on your " + sComponent, sTimeoutError});
        }
        if (rightUnderTest) {
            tests.push_back({dpadUp_R, "(5) With your RIGHT controller, push fully UP on your " + sComponent, sTimeoutError});
            tests.push_back({dpadDown_R, "(6) With your RIGHT controller, push fully DOWN on your " + sComponent, sTimeoutError});
            tests.push_back({dpadLeft_R, "(7) With your RIGHT controller, push fully LEFT on your " + sComponent, sTimeoutError});
            tests.push_back({dpadRight_R, "(8) With your RIGHT controller, push fully RIGHT on your " + sComponent, sTimeoutError});
        }
        if (controllerComponent == eControllerComponent::Trackpad) {
            if (leftUnderTest) {
                tests.push_back(
                    {dpadCenter_L, "(9) With your LEFT controller, push the CENTER portion of the  " + sComponent, sTimeoutError});
            }
            if (rightUnderTest) {
                tests.push_back(
                    {dpadCenter_R, "(10) With your RIGHT controller, push the CENTER portion of the   " + sComponent, sTimeoutError});
            }
        }
    }

    void GenerateStickyTestSet(std::vector<TestSet>& tests, eControllerComponent controllerComponent)
    {
        std::string sTimeoutError = "Time out waiting for dpad input";
        std::string sComponent = (controllerComponent == eControllerComponent::Thumbstick) ? "thumbstick" : "trackpad";
        std::string sSuffix = ", rotate counter-clockwise until you get to the \nbottom area and hold (do not release).";

        GlobalData& globalData = GetGlobalData();
        if (globalData.leftHandUnderTest) {
            tests.push_back({dpadLeft_L, "(1) With your LEFT controller, push fully LEFT on your " + sComponent + sSuffix, sTimeoutError});
        }
        if (globalData.rightHandUnderTest) {
            tests.push_back(
                {dpadRight_R, "(2) With your RIGHT controller, push fully RIGHT on your " + sComponent + sSuffix, sTimeoutError});
        }
    }

    XrPath GetDpadPath(XrAction action, std::vector<XrActionSuggestedBinding>& vActionBindings)
    {
        for (auto& actionBinding : vActionBindings) {
            if (actionBinding.action == action) {
                return actionBinding.binding;
            }
        }

        return XR_NULL_PATH;
    }

    XrPath GetTopLevelPath(XrAction action)
    {
        XrPath topLevelPath = XR_NULL_PATH;
        if (action == dpadUp_L || action == dpadDown_L || action == dpadLeft_L || action == dpadRight_L || action == dpadCenter_L) {
            topLevelPath = pathHand_L;
        }
        else {
            topLevelPath = pathHand_R;
        }

        return topLevelPath;
    }

    std::unique_ptr<IInputTestDevice> GetTestDevice(ActionLayerManager& actionLayerManager, CompositionHelper& compositionHelper,
                                                    XrPath topLevelPath, XrActionSet actionSet,
                                                    std::vector<XrActionSuggestedBinding>& vActionBindings)
    {
        // Get active interaction profile
        XrInteractionProfileState xrInteractionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
        REQUIRE_RESULT_SUCCEEDED(xrGetCurrentInteractionProfile(compositionHelper.GetSession(), topLevelPath, &xrInteractionProfileState));

        // Create input map for the test device
        std::map<XrPath, XrAction> actionMap;

        if (topLevelPath == pathHand_L) {
            for (auto& actionBinding : vActionBindings) {
                if (actionBinding.action == dpadUp_L || actionBinding.action == dpadDown_L || actionBinding.action == dpadLeft_L ||
                    actionBinding.action == dpadRight_L || actionBinding.action == dpadCenter_L) {
                    actionMap.insert({actionBinding.binding, actionBinding.action});
                }
            }
        }
        else {
            for (auto& actionBinding : vActionBindings) {
                if (actionBinding.action == dpadUp_R || actionBinding.action == dpadDown_R || actionBinding.action == dpadLeft_R ||
                    actionBinding.action == dpadRight_R || actionBinding.action == dpadCenter_R) {
                    actionMap.insert({actionBinding.binding, actionBinding.action});
                }
            }
        }

        return CreateTestDevice(&actionLayerManager, compositionHelper.GetInstance(), compositionHelper.GetSession(),
                                xrInteractionProfileState.interactionProfile, topLevelPath, actionSet,
                                (topLevelPath == pathHand_L) ? dpadUp_L : dpadUp_R, actionMap);
    }

    void Test_Interactive(std::vector<TestSet>& tests, XrPath interactionProfile, XrActionSet dpadActionSet,
                          std::vector<XrActionSuggestedBinding> vActionBindings, XrBindingModificationsKHR* pBindingModifications,
                          fnWaitForDpadInput fnTest, CompositionHelper& compositionHelper, bool bSkipHumanInteraction = true)
    {
        // Get instance
        XrInstance instance = compositionHelper.GetInstance();
        REQUIRE(instance != XR_NULL_HANDLE);

        // Suggest bindings
        SuggestBinding(instance, interactionProfile, vActionBindings, pBindingModifications, XR_SUCCESS);

        // Start session
        compositionHelper.BeginSession();
        XrSession session = compositionHelper.GetSession();
        REQUIRE(session != XR_NULL_HANDLE);

        // Create helper classes
        EventReader eventReader(compositionHelper.GetEventQueue());
        ActionLayerManager actionLayerManager(compositionHelper);

        // Attach action sets
        compositionHelper.GetInteractionManager().AddActionSet(dpadActionSet);
        compositionHelper.GetInteractionManager().AttachActionSets();
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        XrActiveActionSet activeActionSet{dpadActionSet};
        syncInfo.activeActionSets = &activeActionSet;
        syncInfo.countActiveActionSets = 1;

        // Create test input devices (for the conformance automated extension, if available)
        std::unique_ptr<IInputTestDevice> testDevice_L =
            GetTestDevice(actionLayerManager, compositionHelper, pathHand_L, dpadActionSet, vActionBindings);
        std::unique_ptr<IInputTestDevice> testDevice_R =
            GetTestDevice(actionLayerManager, compositionHelper, pathHand_R, dpadActionSet, vActionBindings);

        // Wait for focused state for input
        actionLayerManager.DisplayMessage("Waiting for session focus...");

        // Set test devices to active
        GlobalData& globalData = GetGlobalData();
        if (globalData.leftHandUnderTest) {
            testDevice_L->SetDeviceActive(true);
        }
        if (globalData.rightHandUnderTest) {
            testDevice_R->SetDeviceActive(true);
        }

        actionLayerManager.WaitForSessionFocusWithMessage();

        for (auto& test : tests) {
            actionLayerManager.DisplayMessage(test.sInstruction);

            if (bSkipHumanInteraction) {
                if (GetTopLevelPath(test.action) == pathHand_L) {
                    testDevice_L->SetButtonStateBool(GetDpadPath(test.action, vActionBindings), true, true);
                }
                else {
                    testDevice_R->SetButtonStateBool(GetDpadPath(test.action, vActionBindings), true, true);
                }
            }
            else {
                REQUIRE_MSG(fnTest(test.action, &syncInfo, actionLayerManager, session), test.sTimeoutError.c_str());
            }
        }
    }

    TEST_CASE("XR_EXT_dpad_binding", "[XR_EXT_dpad_binding]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_DPAD_BINDING_EXTENSION_NAME) ||
            !globalData.IsInstanceExtensionSupported(XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME)) {
            SKIP(XR_EXT_DPAD_BINDING_EXTENSION_NAME " or " XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME " not supported");
        }

        AutoBasicInstance instance({XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME, XR_EXT_DPAD_BINDING_EXTENSION_NAME});

        // Create supported interaction profile paths
        InitControllers(instance);

        // Create dpad paths
        InitDpadPaths(instance);

        // Create action set
        XrActionSet dpadActionSet = XR_NULL_HANDLE;
        REQUIRE_RESULT_SUCCEEDED(CreateActionSet(&dpadActionSet, "dpads", 0, instance));

        // Create generic dpad action
        XrAction dpadAction = XR_NULL_HANDLE;
        REQUIRE_RESULT_SUCCEEDED(CreateAction(&dpadAction, "dpad_action", XR_ACTION_TYPE_BOOLEAN_INPUT, dpadActionSet));

        // Set dpad suggested bindings
        std::vector<XrActionSuggestedBinding> vActionBindingsThumbstick;
        vActionBindingsThumbstick.push_back({dpadAction, pathDpad_Thumbstick_Up_L});
        vActionBindingsThumbstick.push_back({dpadAction, pathDpad_Thumbstick_Down_L});
        vActionBindingsThumbstick.push_back({dpadAction, pathDpad_Thumbstick_Left_L});
        vActionBindingsThumbstick.push_back({dpadAction, pathDpad_Thumbstick_Right_L});

        vActionBindingsThumbstick.push_back({dpadAction, pathDpad_Thumbstick_Up_R});
        vActionBindingsThumbstick.push_back({dpadAction, pathDpad_Thumbstick_Down_R});
        vActionBindingsThumbstick.push_back({dpadAction, pathDpad_Thumbstick_Left_R});
        vActionBindingsThumbstick.push_back({dpadAction, pathDpad_Thumbstick_Right_R});

        std::vector<XrActionSuggestedBinding> vActionBindingsTrackpad;
        vActionBindingsTrackpad.push_back({dpadAction, pathDpad_Trackpad_Up_L});
        vActionBindingsTrackpad.push_back({dpadAction, pathDpad_Trackpad_Down_L});
        vActionBindingsTrackpad.push_back({dpadAction, pathDpad_Trackpad_Left_L});
        vActionBindingsTrackpad.push_back({dpadAction, pathDpad_Trackpad_Right_L});

        vActionBindingsTrackpad.push_back({dpadAction, pathDpad_Trackpad_Up_R});
        vActionBindingsTrackpad.push_back({dpadAction, pathDpad_Trackpad_Down_R});
        vActionBindingsTrackpad.push_back({dpadAction, pathDpad_Trackpad_Left_R});
        vActionBindingsTrackpad.push_back({dpadAction, pathDpad_Trackpad_Right_R});

        SECTION("Full bindings")
        {
            // Set dpad binding modifiers
            XrInteractionProfileDpadBindingEXT xrDpadModification{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            SetDefaultModifiers(&xrDpadModification, dpadActionSet);

            // Suggest bindings
            SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification, XR_SUCCESS);
        }

        SECTION("Default bindings")
        {
            // Suggest bindings
            SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, nullptr, XR_SUCCESS);
        }

        SECTION("Invalid binding identifier paths")
        {
            // Set dpad binding modifiers
            XrInteractionProfileDpadBindingEXT xrDpadModification{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            XrBindingModificationsKHR xrBindingModifications{XR_TYPE_BINDING_MODIFICATIONS_KHR};
            std::vector<XrBindingModificationBaseHeaderKHR*> vBindingModifsBase{};

            // Set default values.
            SetDefaultModifiers(&xrDpadModification, dpadActionSet);

            // Create single element list
            vBindingModifsBase.push_back(reinterpret_cast<XrBindingModificationBaseHeaderKHR*>(&xrDpadModification));
            xrBindingModifications.bindingModifications = vBindingModifsBase.data();
            xrBindingModifications.bindingModificationCount = (int32_t)vBindingModifsBase.size();

            std::vector<XrPath> allDpadPaths = {
                pathDpad_Thumbstick_Up_L,     pathDpad_Thumbstick_Down_L,   pathDpad_Thumbstick_Left_L, pathDpad_Thumbstick_Right_L,
                pathDpad_Thumbstick_Center_L, pathDpad_Thumbstick_Up_R,     pathDpad_Thumbstick_Down_R, pathDpad_Thumbstick_Left_R,
                pathDpad_Thumbstick_Right_R,  pathDpad_Thumbstick_Center_R, pathDpad_Trackpad_Up_L,     pathDpad_Trackpad_Down_L,
                pathDpad_Trackpad_Left_L,     pathDpad_Trackpad_Right_L,    pathDpad_Trackpad_Center_L, pathDpad_Trackpad_Up_R,
                pathDpad_Trackpad_Down_R,     pathDpad_Trackpad_Left_R,     pathDpad_Trackpad_Right_R,  pathDpad_Trackpad_Center_L,
            };

            for (XrPath path : allDpadPaths) {
                xrDpadModification.binding = path;

                SuggestBinding(instance, pathDaydream, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                SuggestBinding(instance, pathIndex, vActionBindingsThumbstick, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                SuggestBinding(instance, pathVive, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                SuggestBinding(instance, pathGo, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                SuggestBinding(instance, pathTouch, vActionBindingsThumbstick, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                SuggestBinding(instance, pathMS, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
            }

            // Valid for some but not all.
            for (XrPath path : {pathThumbstick_L, pathThumbstick_R}) {
                xrDpadModification.binding = path;

                SuggestBinding(instance, pathDaydream, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                // Index has thumbsticks
                SuggestBinding(instance, pathVive, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                SuggestBinding(instance, pathGo, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                // Touch has thumbsticks
                // WinMR has thumbsticks
            }

            for (XrPath path : {pathTrackpad_L, pathTrackpad_R}) {
                xrDpadModification.binding = path;

                // Daydream has trackpad
                // Index has trackpad
                // Vive has trackpad
                // Go has trackpad
                SuggestBinding(instance, pathTouch, vActionBindingsThumbstick, &xrBindingModifications, XR_ERROR_PATH_UNSUPPORTED);
                // WinMR has trackpad
            }
        }

        SECTION("Invalid action bindings")
        {
            // Reject trackpad paths for devices that doesn't have them.
            for (XrActionSuggestedBinding binding : vActionBindingsTrackpad) {
                std::vector<XrActionSuggestedBinding> vActionBinding{binding};

                // Daydream has trackpad
                // Index has trackpad
                // Vive has trackpad
                // Go has trackpad
                SuggestBinding(instance, pathTouch, vActionBinding, nullptr, XR_ERROR_PATH_UNSUPPORTED);
                // WinMR has trackpad
            }

            // Reject thumbstick paths for devices that doesn't have them.
            for (XrActionSuggestedBinding binding : vActionBindingsThumbstick) {
                std::vector<XrActionSuggestedBinding> vActionBinding{binding};

                SuggestBinding(instance, pathDaydream, vActionBinding, nullptr, XR_ERROR_PATH_UNSUPPORTED);
                // Index has thumbsticks
                SuggestBinding(instance, pathVive, vActionBinding, nullptr, XR_ERROR_PATH_UNSUPPORTED);
                SuggestBinding(instance, pathGo, vActionBinding, nullptr, XR_ERROR_PATH_UNSUPPORTED);
                // Touch has thumbsticks
                // WinMR has thumbsticks
            }
        }

        SECTION("Reject double bindings")
        {
            // Set dpad binding modifiers
            XrInteractionProfileDpadBindingEXT xrDpadModification1{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            XrInteractionProfileDpadBindingEXT xrDpadModification2{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            XrBindingModificationsKHR xrBindingModifications{XR_TYPE_BINDING_MODIFICATIONS_KHR};
            std::vector<XrBindingModificationBaseHeaderKHR*> vBindingModifsBase{};

            // Set default values.
            SetDefaultModifiers(&xrDpadModification1, dpadActionSet);
            SetDefaultModifiers(&xrDpadModification2, dpadActionSet);

            // Create single element list
            vBindingModifsBase.push_back(reinterpret_cast<XrBindingModificationBaseHeaderKHR*>(&xrDpadModification1));
            vBindingModifsBase.push_back(reinterpret_cast<XrBindingModificationBaseHeaderKHR*>(&xrDpadModification2));
            xrBindingModifications.bindingModifications = vBindingModifsBase.data();
            xrBindingModifications.bindingModificationCount = (int32_t)vBindingModifsBase.size();

            // Check for two of the same thumbsticks.
            for (XrPath path : {pathThumbstick_L, pathThumbstick_R}) {
                xrDpadModification1.binding = path;
                xrDpadModification2.binding = path;

                // Daydream doesn't have thumbstick
                SuggestBinding(instance, pathIndex, vActionBindingsThumbstick, &xrBindingModifications, XR_ERROR_VALIDATION_FAILURE);
                // Vive doesn't have thumbstick.
                // Go doesn't have thumbstick.
                SuggestBinding(instance, pathTouch, vActionBindingsThumbstick, &xrBindingModifications, XR_ERROR_VALIDATION_FAILURE);
                SuggestBinding(instance, pathMS, vActionBindingsThumbstick, &xrBindingModifications, XR_ERROR_VALIDATION_FAILURE);
            }

            // Check for two of the same trackpads.
            for (XrPath path : {pathTrackpad_L, pathTrackpad_R}) {
                xrDpadModification1.binding = path;
                xrDpadModification2.binding = path;

                SuggestBinding(instance, pathDaydream, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_VALIDATION_FAILURE);
                SuggestBinding(instance, pathIndex, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_VALIDATION_FAILURE);
                SuggestBinding(instance, pathVive, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_VALIDATION_FAILURE);
                SuggestBinding(instance, pathGo, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_VALIDATION_FAILURE);
                // Touch doesn't have trackpad
                SuggestBinding(instance, pathMS, vActionBindingsTrackpad, &xrBindingModifications, XR_ERROR_VALIDATION_FAILURE);
            }
        }

        SECTION("forceThreshold min/max")
        {
            // Set dpad binding modifiers
            XrInteractionProfileDpadBindingEXT xrDpadModification{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            SetDefaultModifiers(&xrDpadModification, dpadActionSet);

            // Needed to not error out.
            xrDpadModification.forceThresholdReleased = 0.0001f;
            CAPTURE(xrDpadModification.forceThresholdReleased);

            for (float f : {-0.8f, -0.0001f, 0.0f, 1.0001f, 1.8f}) {
                xrDpadModification.forceThreshold = f;
                CAPTURE(xrDpadModification.forceThreshold);

                // Suggest bindings
                SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification,
                                XR_ERROR_VALIDATION_FAILURE);
            }

            // Specifically check if a low value and a high value are included.
            for (float f : {0.0001f, 1.0f}) {
                xrDpadModification.forceThreshold = f;
                CAPTURE(xrDpadModification.forceThreshold);

                // Suggest bindings
                SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification, XR_SUCCESS);
            }
        }

        SECTION("forceThresholdReleased min/max")
        {
            // Set dpad binding modifiers
            XrInteractionProfileDpadBindingEXT xrDpadModification{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            SetDefaultModifiers(&xrDpadModification, dpadActionSet);

            // Needed to not error out.
            xrDpadModification.forceThreshold = 1.0f;
            CAPTURE(xrDpadModification.forceThreshold);

            for (float f : {-0.8f, -0.0001f, 0.0f, 1.0001f, 1.8f}) {
                xrDpadModification.forceThresholdReleased = f;
                CAPTURE(xrDpadModification.forceThresholdReleased);

                // Suggest bindings
                SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification,
                                XR_ERROR_VALIDATION_FAILURE);
            }

            // Specifically check if a low value and a high value are included.
            for (float f : {0.0001f, 1.0f}) {
                xrDpadModification.forceThresholdReleased = f;
                CAPTURE(xrDpadModification.forceThresholdReleased);

                // Suggest bindings
                SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification, XR_SUCCESS);
            }
        }

        SECTION("forceThresholds")
        {
            // Set dpad binding modifiers
            XrInteractionProfileDpadBindingEXT xrDpadModification{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            SetDefaultModifiers(&xrDpadModification, dpadActionSet);

            // Check validation failure when forceThresholdReleased > forceThreshold
            xrDpadModification.forceThreshold = 0.3f;
            xrDpadModification.forceThresholdReleased = 0.5f;
            CAPTURE(xrDpadModification.forceThreshold);
            CAPTURE(xrDpadModification.forceThresholdReleased);

            // Suggest bindings
            SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification, XR_ERROR_VALIDATION_FAILURE);

            // Check for equality
            xrDpadModification.forceThresholdReleased = xrDpadModification.forceThreshold;
            CAPTURE(xrDpadModification.forceThresholdReleased);

            // Suggest bindings
            SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification, XR_SUCCESS);

            // Check for a valid combination
            xrDpadModification.forceThreshold = 0.5f;
            xrDpadModification.forceThresholdReleased = 0.3f;
            CAPTURE(xrDpadModification.forceThreshold);
            CAPTURE(xrDpadModification.forceThresholdReleased);

            // Suggest bindings
            SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification, XR_SUCCESS);
        }

        SECTION("centerRegion min/max")
        {
            // Set dpad binding modifiers
            XrInteractionProfileDpadBindingEXT xrDpadModification{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            SetDefaultModifiers(&xrDpadModification, dpadActionSet);

            for (float f : {-0.25f, 0.0f, 1.0f, 1.25f}) {
                xrDpadModification.centerRegion = f;
                CAPTURE(xrDpadModification.centerRegion);

                // Suggest bindings
                SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification,
                                XR_ERROR_VALIDATION_FAILURE);
            }
        }

        SECTION("wedgeAngle min/max")
        {
            // Set dpad binding modifiers
            XrInteractionProfileDpadBindingEXT xrDpadModification{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
            SetDefaultModifiers(&xrDpadModification, dpadActionSet);

            for (float v : {-0.25f, -0.0001f, (MATH_PI) + 0.0001f, 4.0f}) {
                xrDpadModification.wedgeAngle = v;
                CAPTURE(xrDpadModification.wedgeAngle);

                // Suggest bindings
                SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification,
                                XR_ERROR_VALIDATION_FAILURE);
            }

            for (float v : {0.0f, (MATH_PI)-0.0001f}) {
                xrDpadModification.wedgeAngle = v;
                CAPTURE(xrDpadModification.wedgeAngle);

                // Suggest bindings
                SuggestBindings(instance, vActionBindingsThumbstick, vActionBindingsTrackpad, &xrDpadModification, XR_SUCCESS);
            }
        }
    }

    TEST_CASE("XR_EXT_dpad_binding-interactive_thumbstick", "[XR_EXT_dpad_binding][actions][interactive][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_DPAD_BINDING_EXTENSION_NAME) ||
            !globalData.IsInstanceExtensionSupported(XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME)) {
            SKIP(XR_EXT_DPAD_BINDING_EXTENSION_NAME " or " XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME " not supported");
        }

        // Initialize test
        CompositionHelper compositionHelper("XR_EXT_dpad_binding_interactive",
                                            {XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME, XR_EXT_DPAD_BINDING_EXTENSION_NAME});
        XrInstance instance = compositionHelper.GetInstance();
        REQUIRE(instance != XR_NULL_HANDLE);

        // Per-instance
        InitTestControllersAndDpadPaths(compositionHelper.GetInstance());

        // Set dpad suggested bindings
        std::vector<PathPrintnamePair> interactionProfiles{};
        InitInteractiveInteractionProfiles(interactionProfiles, eControllerComponent::Thumbstick);

        if (interactionProfiles.size() == 0) {
            SKIP("Enabled interaction profile(s) has no thumbstick, skipping test");
        }

        // Setup ActionSet and Actions.
        XrActionSet dpadActionSet = InitInteractiveActions(instance);

        // Needs to happen after we have called InitInteractiveActions and actions are setup.
        std::vector<XrActionSuggestedBinding> vActionBindings{};
        InitInteractiveActionBindings(vActionBindings, eControllerComponent::Thumbstick);

        for (auto& pair : interactionProfiles) {
            // This needs to be section as Test_Interactive calls suggests bindings without creating new actions.
            // So we need to use catch2 to create a new set of actions, actionset, session and instance.
            DYNAMIC_SECTION(pair.interactionProfilePrintname)
            {

                SECTION("Runtime default dpad directions")
                {
                    // Generate tests
                    std::vector<TestSet> tests;
                    GenerateDirectionalTestSet(tests, eControllerComponent::Thumbstick);

                    // Start test
                    Test_Interactive(tests, pair.interactionProfile, dpadActionSet, vActionBindings, nullptr, WaitForDpadInput,
                                     compositionHelper);
                }

                SECTION("Sticky dpad")
                {
                    XrInteractionProfileDpadBindingEXT xrDpadModification_L{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
                    XrInteractionProfileDpadBindingEXT xrDpadModification_R{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
                    CreateStickyBindings(&xrDpadModification_L, &xrDpadModification_R, dpadActionSet, eControllerComponent::Thumbstick);

                    // Setup binding modifications
                    std::vector<XrInteractionProfileDpadBindingEXT> vBindingModifs{{xrDpadModification_L, xrDpadModification_R}};

                    // Convert dpad binding modifications to khr
                    std::vector<XrBindingModificationBaseHeaderKHR*> vBindingModifsBase = makeBasePointerVec(vBindingModifs);

                    XrBindingModificationsKHR xrBindingModifications{XR_TYPE_BINDING_MODIFICATIONS_KHR};
                    xrBindingModifications.bindingModifications = vBindingModifsBase.data();
                    xrBindingModifications.bindingModificationCount = (uint32_t)vBindingModifsBase.size();

                    // Create test set
                    std::vector<TestSet> tests;
                    GenerateStickyTestSet(tests, eControllerComponent::Thumbstick);

                    // Start test
                    Test_Interactive(tests, pair.interactionProfile, dpadActionSet, vActionBindings, &xrBindingModifications,
                                     WaitForStickyDpadInput, compositionHelper);
                }
            }
        }
    }

    TEST_CASE("XR_EXT_dpad_binding-interactive_trackpad", "[XR_EXT_dpad_binding][actions][interactive][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_DPAD_BINDING_EXTENSION_NAME) ||
            !globalData.IsInstanceExtensionSupported(XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME)) {
            SKIP(XR_EXT_DPAD_BINDING_EXTENSION_NAME " or " XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME " not supported");
        }

        // Initialize test
        CompositionHelper compositionHelper("XR_EXT_dpad_binding_interactive",
                                            {XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME, XR_EXT_DPAD_BINDING_EXTENSION_NAME});
        XrInstance instance = compositionHelper.GetInstance();
        REQUIRE(instance != XR_NULL_HANDLE);

        // Per-instance
        InitTestControllersAndDpadPaths(instance);

        // Set dpad suggested bindings
        std::vector<PathPrintnamePair> interactionProfiles{};
        InitInteractiveInteractionProfiles(interactionProfiles, eControllerComponent::Trackpad);

        if (interactionProfiles.size() == 0) {
            SKIP("Enabled interaction profile(s) has no thumbstick, skipping test");
        }

        // Setup ActionSet and Actions.
        XrActionSet dpadActionSet = InitInteractiveActions(instance);

        // Needs to happen after we have called InitInteractiveActions and actions are setup.
        std::vector<XrActionSuggestedBinding> vActionBindings{};
        InitInteractiveActionBindings(vActionBindings, eControllerComponent::Trackpad);

        for (auto& pair : interactionProfiles) {
            // This needs to be section as Test_Interactive calls suggests bindings without creating new actions.
            // So we need to use catch2 to create a new set of actions, actionset, session and instance.
            DYNAMIC_SECTION(pair.interactionProfilePrintname)
            {
                SECTION("Runtime default dpad directions")
                {
                    // Generate tests
                    std::vector<TestSet> tests;
                    GenerateDirectionalTestSet(tests, eControllerComponent::Trackpad);

                    // Start test
                    Test_Interactive(tests, pair.interactionProfile, dpadActionSet, vActionBindings, nullptr, WaitForDpadInput,
                                     compositionHelper);
                }
                SECTION("Sticky dpad")
                {
                    XrInteractionProfileDpadBindingEXT xrDpadModification_L{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
                    XrInteractionProfileDpadBindingEXT xrDpadModification_R{XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT};
                    CreateStickyBindings(&xrDpadModification_L, &xrDpadModification_R, dpadActionSet, eControllerComponent::Trackpad);

                    // Setup binding modifications
                    std::vector<XrInteractionProfileDpadBindingEXT> vBindingModifs{{xrDpadModification_L, xrDpadModification_R}};

                    // Convert dpad binding modifications to khr
                    std::vector<XrBindingModificationBaseHeaderKHR*> vBindingModifsBase = makeBasePointerVec(vBindingModifs);

                    XrBindingModificationsKHR xrBindingModifications{XR_TYPE_BINDING_MODIFICATIONS_KHR};
                    xrBindingModifications.bindingModifications = vBindingModifsBase.data();
                    xrBindingModifications.bindingModificationCount = (uint32_t)vBindingModifsBase.size();

                    // Create test set
                    std::vector<TestSet> tests;
                    GenerateStickyTestSet(tests, eControllerComponent::Trackpad);

                    // Start test
                    Test_Interactive(tests, pair.interactionProfile, dpadActionSet, vActionBindings, &xrBindingModifications,
                                     WaitForStickyDpadInput, compositionHelper);
                }
            }
        }
    }
}  // namespace Conformance
