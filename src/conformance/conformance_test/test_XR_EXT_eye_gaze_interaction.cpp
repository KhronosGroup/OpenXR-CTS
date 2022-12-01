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

#include "action_utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "composition_utils.h"
#include "two_call.h"
#include "utils.h"
#include "xr_linear.h"

using namespace Conformance;

namespace Conformance
{
    static constexpr const char* kEyeGazeInteractionUserPath = "/user/eyes_ext";
    static constexpr const char* kEyeGazeInteractionPoseInputPath = "/user/eyes_ext/input/gaze_ext/pose";
    static constexpr const char* kEyeGazeInteractionProfilePath = "/interaction_profiles/ext/eye_gaze_interaction";

    static constexpr const char* kKhrSimpleControllerProfilePath = "/interaction_profiles/khr/simple_controller";
    static constexpr const char* kLeftHandClickInputPath = "/user/hand/left/input/select/click";

    static constexpr XrPosef kPoseIdentity{{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    static constexpr XrVector3f kVectorUp{0, 1, 0};
    static constexpr XrVector3f kVectorForward{0, 0, -1};

    static bool SystemSupportsEyeGazeInteraction(XrInstance instance)
    {
        GlobalData& globalData = GetGlobalData();
        XrSystemEyeGazeInteractionPropertiesEXT eyeGazeSystemProperties{XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT};
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES, &eyeGazeSystemProperties};

        XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemGetInfo.formFactor = globalData.options.formFactorValue;

        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        REQUIRE_RESULT(xrGetSystem(instance, &systemGetInfo, &systemId), XR_SUCCESS);
        REQUIRE_RESULT(xrGetSystemProperties(instance, systemId, &systemProperties), XR_SUCCESS);

        return eyeGazeSystemProperties.supportsEyeGazeInteraction != XR_FALSE;
    }

    static XrVector3f RotateVectorByQuaternion(const XrQuaternionf& look, const XrVector3f& pose)
    {
        XrMatrix4x4f m{};
        XrMatrix4x4f_CreateFromQuaternion(&m, &look);

        XrVector3f result{};
        XrMatrix4x4f_TransformVector3f(&result, &m, &pose);

        return result;
    };

    TEST_CASE("XR_EXT_eye_gaze_interaction", "[XR_EXT_eye_gaze_interaction][interactive][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME)) {
            return;
        }

        SECTION("Extension not enabled")
        {
            SECTION("Eye gaze interaction extension not enabled")
            {
                // validate that the extension has not been force enabled...
                // system should never set `supportsEyeGazeInteraction` to XR_TRUE unless
                // the extension has been enabled.
                if (!globalData.enabledInstanceExtensionNames.contains(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME)) {
                    AutoBasicInstance instance;
                    REQUIRE(!SystemSupportsEyeGazeInteraction(instance));
                }
            }
        }

        SECTION("Extension enabled")
        {
            AutoBasicInstance instance({XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME});
            if (!SystemSupportsEyeGazeInteraction(instance)) {
                // This runtime does support eye gaze, but this headset does not which is fine.
                WARN("Device does not support eye gaze interaction");
                return;
            }

            SECTION("Create and destroy eye gaze actions")
            {
                SECTION("Create eye gaze paths")
                {
                    XrPath userPath{XR_NULL_PATH};
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionUserPath, &userPath), XR_SUCCESS);
                    REQUIRE(userPath != XR_NULL_PATH);
                    XrPath gazePosePath{XR_NULL_PATH};
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionPoseInputPath, &gazePosePath), XR_SUCCESS);
                    REQUIRE(gazePosePath != XR_NULL_PATH);
                    XrPath interactionProfilePath{XR_NULL_PATH};
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionProfilePath, &interactionProfilePath), XR_SUCCESS);
                    REQUIRE(interactionProfilePath != XR_NULL_PATH);

                    // and verify that repeatedly mapping path returns same results
                    XrPath otherPath{XR_NULL_PATH};
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionUserPath, &otherPath), XR_SUCCESS);
                    REQUIRE(otherPath == userPath);
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionPoseInputPath, &otherPath), XR_SUCCESS);
                    REQUIRE(otherPath == gazePosePath);
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionProfilePath, &otherPath), XR_SUCCESS);
                    REQUIRE(otherPath == interactionProfilePath);
                }

                SECTION("Create an action set and bindings")
                {
                    // The snippet follows the code sample provided by the spec.

                    // Create action set
                    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                    strcpy(actionSetInfo.actionSetName, "gameplay");
                    strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
                    actionSetInfo.priority = 0;
                    XrActionSet gameplayActionSet{XR_NULL_HANDLE};
                    REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetInfo, &gameplayActionSet), XR_SUCCESS);

                    // Create user intent action
                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    strcpy(actionInfo.actionName, "user_intent");
                    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                    strcpy(actionInfo.localizedActionName, "User Intent");
                    XrAction userIntentAction{XR_NULL_HANDLE};
                    REQUIRE_RESULT(xrCreateAction(gameplayActionSet, &actionInfo, &userIntentAction), XR_SUCCESS);

                    // Create suggested bindings
                    XrPath eyeGazeInteractionProfilePath{XR_NULL_PATH};
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionProfilePath, &eyeGazeInteractionProfilePath), XR_SUCCESS);

                    XrPath gazePosePath{XR_NULL_PATH};
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionPoseInputPath, &gazePosePath), XR_SUCCESS);

                    XrActionSuggestedBinding bindings{};
                    bindings.action = userIntentAction;
                    bindings.binding = gazePosePath;

                    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                    suggestedBindings.interactionProfile = eyeGazeInteractionProfilePath;
                    suggestedBindings.suggestedBindings = &bindings;
                    suggestedBindings.countSuggestedBindings = 1;
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &suggestedBindings), XR_SUCCESS);

                    // Now destroy the action set
                    REQUIRE_RESULT(xrDestroyAction(userIntentAction), XR_SUCCESS);
                    REQUIRE_RESULT(xrDestroyActionSet(gameplayActionSet), XR_SUCCESS);
                }

                SECTION("Attach eye gaze actions to session")
                {
                    // The snippet follows the code sample provided by the spec.

                    AutoBasicSession session(AutoBasicSession::beginSession, instance);

                    // Create action set
                    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                    strcpy(actionSetInfo.actionSetName, "gameplay");
                    strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
                    actionSetInfo.priority = 0;
                    XrActionSet gameplayActionSet{XR_NULL_HANDLE};
                    REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetInfo, &gameplayActionSet), XR_SUCCESS);

                    // Create user intent action
                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    strcpy(actionInfo.actionName, "user_intent");
                    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                    strcpy(actionInfo.localizedActionName, "User Intent");
                    XrAction userIntentAction{XR_NULL_HANDLE};
                    REQUIRE_RESULT(xrCreateAction(gameplayActionSet, &actionInfo, &userIntentAction), XR_SUCCESS);

                    // Create suggested bindings
                    XrPath eyeGazeInteractionProfilePath{XR_NULL_PATH};
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionProfilePath, &eyeGazeInteractionProfilePath), XR_SUCCESS);

                    XrPath gazePosePath{XR_NULL_PATH};
                    REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionPoseInputPath, &gazePosePath), XR_SUCCESS);

                    XrActionSuggestedBinding bindings{};
                    bindings.action = userIntentAction;
                    bindings.binding = gazePosePath;

                    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                    suggestedBindings.interactionProfile = eyeGazeInteractionProfilePath;
                    suggestedBindings.suggestedBindings = &bindings;
                    suggestedBindings.countSuggestedBindings = 1;
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &suggestedBindings), XR_SUCCESS);

                    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
                    attachInfo.countActionSets = 1;
                    attachInfo.actionSets = &gameplayActionSet;
                    REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

                    XrActionSpaceCreateInfo createActionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                    createActionSpaceInfo.action = userIntentAction;
                    createActionSpaceInfo.poseInActionSpace = kPoseIdentity;
                    XrSpace gazeActionSpace{XR_NULL_HANDLE};
                    REQUIRE_RESULT(xrCreateActionSpace(session, &createActionSpaceInfo, &gazeActionSpace), XR_SUCCESS);

                    // clean up
                    REQUIRE_RESULT(xrDestroySpace(gazeActionSpace), XR_SUCCESS);
                    REQUIRE_RESULT(xrDestroyAction(userIntentAction), XR_SUCCESS);
                    REQUIRE_RESULT(xrDestroyActionSet(gameplayActionSet), XR_SUCCESS);
                }
            }
        }

        SECTION("Combine eye gaze with another input source - simple controller")
        {
            // Verify that eye gaze interaction input can be combined with other input sources.
            // Use Simple Controller profile as opposed to vendor-specific inputs for broader coverage.

            AutoBasicInstance instance({XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME});
            if (!SystemSupportsEyeGazeInteraction(instance)) {
                // This runtime does support eye gaze, but this headset does not which is fine.
                WARN("Device does not support eye gaze interaction");
                return;
            }

            AutoBasicSession session(AutoBasicSession::beginSession, instance);

            // Create action set
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "gameplay");
            strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
            actionSetInfo.priority = 0;
            XrActionSet gameplayActionSet{XR_NULL_HANDLE};
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetInfo, &gameplayActionSet), XR_SUCCESS);

            // Create user intent action
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            strcpy(actionInfo.actionName, "user_intent");
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.localizedActionName, "User Intent");
            XrAction userIntentAction{XR_NULL_HANDLE};
            REQUIRE_RESULT(xrCreateAction(gameplayActionSet, &actionInfo, &userIntentAction), XR_SUCCESS);

            // Create user confirmation action
            strcpy(actionInfo.actionName, "user_confirm");
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionInfo.localizedActionName, "User Confirm");
            XrAction userConfirmAction{XR_NULL_HANDLE};
            REQUIRE_RESULT(xrCreateAction(gameplayActionSet, &actionInfo, &userConfirmAction), XR_SUCCESS);

            // Create suggested bindings - one for each profile (separately)
            XrPath eyeGazeInteractionProfilePath{XR_NULL_PATH};
            REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionProfilePath, &eyeGazeInteractionProfilePath), XR_SUCCESS);
            XrPath gazePosePath{XR_NULL_PATH};
            REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionPoseInputPath, &gazePosePath), XR_SUCCESS);

            XrPath simpleControllerProfilePath{XR_NULL_PATH};
            REQUIRE_RESULT(xrStringToPath(instance, kKhrSimpleControllerProfilePath, &simpleControllerProfilePath), XR_SUCCESS);
            XrPath controllerClickPath{XR_NULL_PATH};
            REQUIRE_RESULT(xrStringToPath(instance, kLeftHandClickInputPath, &controllerClickPath), XR_SUCCESS);

            XrActionSuggestedBinding bindings{};
            bindings.action = userIntentAction;
            bindings.binding = gazePosePath;

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = eyeGazeInteractionProfilePath;
            suggestedBindings.suggestedBindings = &bindings;
            suggestedBindings.countSuggestedBindings = 1;
            REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &suggestedBindings), XR_SUCCESS);

            bindings.action = userConfirmAction;
            bindings.binding = controllerClickPath;

            suggestedBindings.interactionProfile = simpleControllerProfilePath;
            suggestedBindings.suggestedBindings = &bindings;
            suggestedBindings.countSuggestedBindings = 1;
            REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &suggestedBindings), XR_SUCCESS);

            // Attach the action set with both bindings to the session
            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.countActionSets = 1;
            attachInfo.actionSets = &gameplayActionSet;
            REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

            XrActionSpaceCreateInfo createActionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            createActionSpaceInfo.action = userIntentAction;
            createActionSpaceInfo.poseInActionSpace = kPoseIdentity;
            XrSpace gazeActionSpace{XR_NULL_HANDLE};
            REQUIRE_RESULT(xrCreateActionSpace(session, &createActionSpaceInfo, &gazeActionSpace), XR_SUCCESS);
        }

        SECTION("Localize eye gaze paths")
        {
            CompositionHelper compositionHelper("XR_EXT_eye_gaze_interaction localization", {XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME});

            if (!SystemSupportsEyeGazeInteraction(compositionHelper.GetInstance())) {
                // This runtime does support eye tracking, but this headset does not which is fine.
                WARN("Device does not support eye gaze interaction");
                return;
            }

            ActionLayerManager actionLayerManager(compositionHelper);
            XrInstance instance = compositionHelper.GetInstance();

            // Create action set
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "gameplay");
            strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
            actionSetInfo.priority = 0;
            XrActionSet gameplayActionSet{XR_NULL_HANDLE};
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetInfo, &gameplayActionSet), XR_SUCCESS);

            // Create user intent action
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            strcpy(actionInfo.actionName, "user_intent");
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.localizedActionName, "User Intent");
            XrAction userIntentAction{XR_NULL_HANDLE};
            REQUIRE_RESULT(xrCreateAction(gameplayActionSet, &actionInfo, &userIntentAction), XR_SUCCESS);

            // Create suggested bindings
            XrPath eyeGazeInteractionProfilePath{XR_NULL_PATH};
            REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionProfilePath, &eyeGazeInteractionProfilePath), XR_SUCCESS);

            XrPath gazePosePath{XR_NULL_PATH};
            REQUIRE_RESULT(xrStringToPath(instance, kEyeGazeInteractionPoseInputPath, &gazePosePath), XR_SUCCESS);

            XrActionSuggestedBinding bindings;
            bindings.action = userIntentAction;
            bindings.binding = gazePosePath;

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = eyeGazeInteractionProfilePath;
            suggestedBindings.suggestedBindings = &bindings;
            suggestedBindings.countSuggestedBindings = 1;
            REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &suggestedBindings), XR_SUCCESS);

            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.countActionSets = 1;
            attachInfo.actionSets = &gameplayActionSet;
            REQUIRE_RESULT(xrAttachSessionActionSets(compositionHelper.GetSession(), &attachInfo), XR_SUCCESS);

            // Wait for session to focus
            compositionHelper.BeginSession();

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            XrActiveActionSet activeActionSet{gameplayActionSet};
            syncInfo.activeActionSets = &activeActionSet;
            syncInfo.countActiveActionSets = 1;

            actionLayerManager.WaitWithMessage("Waiting for eye gaze isActive=true", [&] {
                if (XR_UNQUALIFIED_SUCCESS(xrSyncActions(compositionHelper.GetSession(), &syncInfo))) {
                    XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE};
                    XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getActionStateInfo.action = userIntentAction;
                    REQUIRE_RESULT(XR_SUCCESS, xrGetActionStatePose(compositionHelper.GetSession(), &getActionStateInfo, &actionStatePose));
                    return (bool)actionStatePose.isActive;
                }
                return false;
            });

            XrBoundSourcesForActionEnumerateInfo info{XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
            info.action = userIntentAction;
            std::vector<XrPath> enumerateResult =
                REQUIRE_TWO_CALL(XrPath, {}, xrEnumerateBoundSourcesForAction, compositionHelper.GetSession(), &info);
            REQUIRE_MSG(enumerateResult.size() > 0,
                        "user_intent action not bound to any source. Expected to be bound to /user/eyes_ext/input/gaze_ext/pose source");

            // Now obtain localized names for paths
            XrInputSourceLocalizedNameGetInfo localizeInfo = {XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};

            localizeInfo.sourcePath = enumerateResult[0];
            localizeInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT;
            std::string localizedStringResult =
                REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, compositionHelper.GetSession(), &localizeInfo).data();
            REQUIRE_FALSE(localizedStringResult.empty());

            // clean up
            REQUIRE_RESULT(xrDestroyAction(userIntentAction), XR_SUCCESS);
            REQUIRE_RESULT(xrDestroyActionSet(gameplayActionSet), XR_SUCCESS);
        }
    }

    TEST_CASE("XR_EXT_eye_gaze_interaction_interactive_gaze_only", "[XR_EXT_eye_gaze_interaction][scenario][interactive][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME)) {
            return;
        }

        CompositionHelper compositionHelper("XR_EXT_eye_gaze_interaction interactive gaze only",
                                            {XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME});

        if (!SystemSupportsEyeGazeInteraction(compositionHelper.GetInstance())) {
            // This runtime does support eye tracking, but this headset does not which is fine.
            WARN("Device does not support eye gaze interaction");
            return;
        }

        // Actions
        XrActionSet actionSet{XR_NULL_HANDLE};
        XrAction gazeAction{XR_NULL_HANDLE};

        XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetInfo.actionSetName, "eye_gaze_test");
        strcpy(actionSetInfo.localizedActionSetName, "Eye Gaze Interaction Test");
        REQUIRE_RESULT(XR_SUCCESS, xrCreateActionSet(compositionHelper.GetInstance(), &actionSetInfo, &actionSet));

        XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        strcpy(actionInfo.actionName, "eye_gaze_pose");
        strcpy(actionInfo.localizedActionName, "Eye Gaze Pose");
        REQUIRE_RESULT(XR_SUCCESS, xrCreateAction(actionSet, &actionInfo, &gazeAction));

        const XrPath gazePath = StringToPath(compositionHelper.GetInstance(), kEyeGazeInteractionPoseInputPath);
        const XrActionSuggestedBinding binding{gazeAction, gazePath};

        XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBindings.interactionProfile = StringToPath(compositionHelper.GetInstance(), kEyeGazeInteractionProfilePath);
        suggestedBindings.suggestedBindings = &binding;
        suggestedBindings.countSuggestedBindings = 1;
        REQUIRE_RESULT(XR_SUCCESS, xrSuggestInteractionProfileBindings(compositionHelper.GetInstance(), &suggestedBindings));

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.actionSets = &actionSet;
        attachInfo.countActionSets = 1;
        REQUIRE_RESULT(XR_SUCCESS, xrAttachSessionActionSets(compositionHelper.GetSession(), &attachInfo));

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosefCPP{});
        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosefCPP{});

        XrActionSpaceCreateInfo createActionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        createActionSpaceInfo.action = gazeAction;
        createActionSpaceInfo.poseInActionSpace = kPoseIdentity;
        XrSpace gazeActionSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(XR_SUCCESS, xrCreateActionSpace(compositionHelper.GetSession(), &createActionSpaceInfo, &gazeActionSpace));

        SECTION("Gaze display")
        {
            const char* instructions =
                "A ray should point in the direction of eye gaze. "
                "Two small cubes are rendered in the environment. "
                "Bring your head to one of these cubes to complete the validation. ";

            // Set up composition projection layer and swapchains (one swapchain per view).
            std::vector<XrSwapchain> swapchains;

            XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
            {
                const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();
                for (uint32_t j = 0; j < projLayer->viewCount; j++) {
                    const XrSwapchain swapchain = compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(
                        viewProperties[j].recommendedImageRectWidth, viewProperties[j].recommendedImageRectHeight));
                    const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = compositionHelper.MakeDefaultSubImage(swapchain, 0);
                    swapchains.push_back(swapchain);
                }
            }

            compositionHelper.BeginSession();

            // Create the instructional quad layer placed to the left.
            XrCompositionLayerQuad* const instructionsQuad = compositionHelper.CreateQuadLayer(
                compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 512, instructions, 48)), localSpace, 1.0f,
                {{0, 0, 0, 1}, {-1.5f, 0, -0.3f}});
            XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &kVectorUp, 70 * MATH_PI / 180);

            bool eyeGazeSampleTimeFound = false;
            auto update = [&](const XrFrameState& frameState) {
                std::vector<Cube> renderedCubes;

                XrSpaceLocation viewLoc{XR_TYPE_SPACE_LOCATION};
                REQUIRE_RESULT(XR_SUCCESS, xrLocateSpace(viewSpace, localSpace, frameState.predictedDisplayTime, &viewLoc));

                static constexpr std::array<XrVector3f, 2> staticCubeLocs{{
                    {0.f, 0.f, -0.5f},
                    {0.f, 0.f, 0.5f},
                }};
                static constexpr float staticCubeScale = 0.1f;

                for (size_t i = 0; i < staticCubeLocs.size(); ++i) {
                    renderedCubes.push_back(Cube::Make(staticCubeLocs[i], staticCubeScale));
                }

                // Check if user has requested to complete the test.
                {
                    if (viewLoc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                        const XrVector3f& headPosition = viewLoc.pose.position;
                        for (size_t i = 0; i < staticCubeLocs.size(); ++i) {
                            XrVector3f d;
                            XrVector3f_Sub(&d, &headPosition, &staticCubeLocs[i]);
                            float distance = XrVector3f_Length(&d);
                            if (distance < (staticCubeScale / 2)) {
                                // bring your head to the cube
                                return false;
                            }
                        }
                    }
                }

                // Handle actions

                const XrActiveActionSet activeActionSet{actionSet, XR_NULL_PATH};
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                syncInfo.activeActionSets = &activeActionSet;
                syncInfo.countActiveActionSets = 1;
                // xrSyncActions may return XR_SUCCESS or XR_SESSION_NOT_FOCUSED
                REQUIRE(XR_SUCCEEDED(xrSyncActions(compositionHelper.GetSession(), &syncInfo)));

                XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE};
                XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                getActionStateInfo.action = gazeAction;
                REQUIRE_RESULT(XR_SUCCESS, xrGetActionStatePose(compositionHelper.GetSession(), &getActionStateInfo, &actionStatePose));

                if (actionStatePose.isActive) {
                    XrEyeGazeSampleTimeEXT eyeGazeSampleTime{XR_TYPE_EYE_GAZE_SAMPLE_TIME_EXT};
                    XrSpaceLocation gazeLocation{XR_TYPE_SPACE_LOCATION, &eyeGazeSampleTime};
                    REQUIRE_RESULT(XR_SUCCESS, xrLocateSpace(gazeActionSpace, localSpace, frameState.predictedDisplayTime, &gazeLocation));

                    // The runtime must set both XR_SPACE_LOCATION_POSITION_TRACKED_BIT and XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT
                    // or clear both XR_SPACE_LOCATION_POSITION_TRACKED_BIT and XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT.
                    static constexpr XrSpaceLocationFlags spaceTrackedBits =
                        XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
                    if (gazeLocation.locationFlags & spaceTrackedBits) {
                        REQUIRE(spaceTrackedBits == (gazeLocation.locationFlags & spaceTrackedBits));
                    }
                    // If at least orientation is valid, show a ray representing the gaze
                    if (gazeLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
                        // The sample time must be set
                        if (eyeGazeSampleTime.time <= 0) {
                            FAIL("eyeGazeSampleTime.time is not valid");
                        }
                        else {
                            eyeGazeSampleTimeFound = true;
                        }

                        // Make a cube that has a large z scale and small x and y so it looks like a ray.
                        // Position the ray such that it looks like a ray pointing in the direction of the gaze.
                        XrPosef rayPose = kPoseIdentity;
                        rayPose.orientation = gazeLocation.pose.orientation;
                        if (gazeLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                            rayPose.position = gazeLocation.pose.position;
                        }
                        else if (viewLoc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                            // make ray origin the head pose
                            rayPose.position = viewLoc.pose.position;
                        }
                        // Move the ray's position half of ray's length in the direction of the gaze.
                        // That means, forward vector (0, 0, -1) rotated by the orientation of the gaze.
                        static constexpr XrVector3f rayEdgesScale{0.003f, 0.003f, 1.0f};
                        static constexpr float rayOffsetFromHead = 0.2f;  // 20cm from head
                        const XrVector3f gazeDirection = RotateVectorByQuaternion(rayPose.orientation, kVectorForward);
                        const float rayOffsetForward = rayEdgesScale.z / 2 + rayOffsetFromHead;
                        rayPose.position = XrVector3f{rayPose.position.x + (rayOffsetForward)*gazeDirection.x,
                                                      rayPose.position.y + (rayOffsetForward)*gazeDirection.y,
                                                      rayPose.position.z + (rayOffsetForward)*gazeDirection.z};
                        renderedCubes.push_back(Cube{rayPose, rayEdgesScale});
                    }
                }

                auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
                const auto& viewState = std::get<XrViewState>(viewData);

                std::vector<XrCompositionLayerBaseHeader*> layers;

                if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                    viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                    const auto& views = std::get<std::vector<XrView>>(viewData);

                    // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                    for (size_t view = 0; view < views.size(); view++) {
                        compositionHelper.AcquireWaitReleaseImage(
                            swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) {
                                GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, format);
                                const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                                const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                                GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage, format,
                                                                           RenderParams().Draw(renderedCubes));
                            });
                    }

                    layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
                }

                layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});

                compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

                return compositionHelper.PollEvents();
            };

            RenderLoop(compositionHelper.GetSession(), update).Loop();

            REQUIRE_MSG(eyeGazeSampleTimeFound, "Eye gaze sample time never available");
        }
    }

}  // namespace Conformance
