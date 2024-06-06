// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "action_utils.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "controller_animation_handler.h"
#include "cts_tinygltf.h"
#include "graphics_plugin.h"
#include "input_testinputdevice.h"
#include "report.h"
#include "two_call.h"
#include "two_call_struct_metadata.h"
#include "two_call_struct_tests.h"

#include "common/hex_and_handles.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"
#include "utilities/utils.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Conformance
{
    constexpr XrVector3f Up{0, 1, 0};

    struct ExtensionDataForXR_MSFT_controller_model
    {
        XrInstance instance;
        PFN_xrGetControllerModelKeyMSFT xrGetControllerModelKeyMSFT_;
        PFN_xrGetControllerModelPropertiesMSFT xrGetControllerModelPropertiesMSFT_;
        PFN_xrGetControllerModelStateMSFT xrGetControllerModelStateMSFT_;
        PFN_xrLoadControllerModelMSFT xrLoadControllerModelMSFT_;

        ExtensionDataForXR_MSFT_controller_model(XrInstance instance_)
            : instance(instance_)
            , xrGetControllerModelKeyMSFT_(
                  GetInstanceExtensionFunction<PFN_xrGetControllerModelKeyMSFT, true>(instance, "xrGetControllerModelKeyMSFT"))
            , xrGetControllerModelPropertiesMSFT_(GetInstanceExtensionFunction<PFN_xrGetControllerModelPropertiesMSFT, true>(
                  instance, "xrGetControllerModelPropertiesMSFT"))
            , xrGetControllerModelStateMSFT_(
                  GetInstanceExtensionFunction<PFN_xrGetControllerModelStateMSFT, true>(instance, "xrGetControllerModelStateMSFT"))
            , xrLoadControllerModelMSFT_(
                  GetInstanceExtensionFunction<PFN_xrLoadControllerModelMSFT, true>(instance, "xrLoadControllerModelMSFT"))
        {
        }

        void CheckInvalidModelKey(XrSession session, XrControllerModelKeyMSFT modelKey) const
        {
            INFO("Known-invalid model key: " << Uint64ToHexString(modelKey));
            uint32_t countOutput = 0;
            CHECK(XR_ERROR_CONTROLLER_MODEL_KEY_INVALID_MSFT == xrLoadControllerModelMSFT_(session, modelKey, 0, &countOutput, NULL));

            XrControllerModelPropertiesMSFT modelProperties{XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT};
            CHECK(XR_ERROR_CONTROLLER_MODEL_KEY_INVALID_MSFT == xrGetControllerModelPropertiesMSFT_(session, modelKey, &modelProperties));

            XrControllerModelStateMSFT modelState{XR_TYPE_CONTROLLER_MODEL_STATE_MSFT};
            CHECK(XR_ERROR_CONTROLLER_MODEL_KEY_INVALID_MSFT == xrGetControllerModelStateMSFT_(session, modelKey, &modelState));
        }

        void CheckValidModelKeys(XrSession session, const std::vector<XrControllerModelKeyMSFT>& modelKeys)
        {

            // Check two call struct for controller model properties and states,
            // plus regular two-call for the model itself
            auto modelPropertiesTwoCallData = getTwoCallStructData<XrControllerModelPropertiesMSFT>();
            auto modelStateTwoCallData = getTwoCallStructData<XrControllerModelStateMSFT>();
            for (auto modelKey : modelKeys) {
                INFO("Model key: " << Uint64ToHexString(modelKey));
                CheckTwoCallStructConformance(modelPropertiesTwoCallData, {XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT},
                                              "xrGetControllerModelPropertiesMSFT", false,
                                              [&](XrControllerModelPropertiesMSFT* properties) {
                                                  return xrGetControllerModelPropertiesMSFT_(session, modelKey, properties);
                                              });

                CheckTwoCallStructConformance(
                    modelStateTwoCallData, {XR_TYPE_CONTROLLER_MODEL_STATE_MSFT}, "xrGetControllerModelStateMSFT", false,
                    [&](XrControllerModelStateMSFT* state) { return xrGetControllerModelStateMSFT_(session, modelKey, state); });
                CHECK_TWO_CALL(uint8_t, {}, xrLoadControllerModelMSFT_, session, modelKey);
            }

            // Try inventing some model keys that will be invalid.

            for (auto modelKey : modelKeys) {
                XrControllerModelKeyMSFT fakeModelKey = modelKey + 1234;
                if (std::find(modelKeys.begin(), modelKeys.end(), fakeModelKey) == modelKeys.end()) {
                    // We invented an invalid key
                    INFO("Invented model key: " << Uint64ToHexString(fakeModelKey));
                    CheckInvalidModelKey(session, fakeModelKey);
                }
            }
        }
    };

    TEST_CASE("XR_MSFT_controller_model-simple", "[XR_MSFT_controller_model]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME)) {
            SKIP(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME " not supported");
        }

        AutoBasicInstance instance({"XR_MSFT_controller_model"});
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);
        ExtensionDataForXR_MSFT_controller_model ext(instance);
        ext.CheckInvalidModelKey(session, XR_NULL_CONTROLLER_MODEL_KEY_MSFT);
    }

    TEST_CASE("XR_MSFT_controller_model", "[XR_MSFT_controller_model]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME)) {
            SKIP(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper("XR_MSFT_controller_model", {"XR_MSFT_controller_model"});
        XrInstance instance = compositionHelper.GetInstance();

        ExtensionDataForXR_MSFT_controller_model ext(instance);

        ActionLayerManager actionLayerManager(compositionHelper);
        XrPath simpleKHR = StringToPath(instance, "/interaction_profiles/microsoft/motion_controller");
        XrPath leftHandPath{StringToPath(instance, "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), instance, compositionHelper.GetSession(), simpleKHR,
            leftHandPath, GetInteractionProfile(InteractionProfileIndex::Profile_microsoft_motion_controller).InputSourcePaths);

        XrPath rightHandPath{StringToPath(instance, "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), instance, compositionHelper.GetSession(), simpleKHR,
            rightHandPath, GetInteractionProfile(InteractionProfileIndex::Profile_microsoft_motion_controller).InputSourcePaths);

        const std::vector<XrPath> subactionPaths{leftHandPath, rightHandPath};

        XrActionSet actionSet;
        XrAction gripPoseAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "interaction_test");
            strcpy(actionSetInfo.localizedActionSetName, "Interaction Test");
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrCreateActionSet(instance, &actionSetInfo, &actionSet));

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.actionName, "grip_pose");
            strcpy(actionInfo.localizedActionName, "Grip pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));
        }

        compositionHelper.BeginSession();
        actionLayerManager.WaitForSessionFocusWithMessage();

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(
            StringToPath(instance, "/interaction_profiles/microsoft/motion_controller"),
            {{{gripPoseAction, StringToPath(instance, "/user/hand/left/input/grip")},
              {gripPoseAction, StringToPath(instance, "/user/hand/right/input/grip")}}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        XrActiveActionSet activeActionSet{actionSet};
        syncInfo.activeActionSets = &activeActionSet;
        syncInfo.countActiveActionSets = 1;

        XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceCreateInfo.poseInActionSpace = XrPosefCPP();

        std::vector<XrSpace> gripSpaces;
        for (std::shared_ptr<IInputTestDevice> controller : {leftHandInputDevice, rightHandInputDevice}) {
            actionSpaceCreateInfo.subactionPath = leftHandInputDevice->TopLevelPath();
            actionSpaceCreateInfo.action = gripPoseAction;
            XrSpace gripSpace;
            XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(compositionHelper.GetSession(), &actionSpaceCreateInfo, &gripSpace));
            gripSpaces.push_back(gripSpace);
        }

        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

        XrSession session = compositionHelper.GetSession();

        std::vector<XrControllerModelKeyMSFT> modelKeys;
        std::map<XrPath, XrControllerModelKeyMSFT> pathsAndKeys;
        // std::vector<std::pair<XrPath, XrControllerModelKeyMSFT>> pathsAndKeys;

        bool gotAllKeys = WaitUntilPredicateWithTimeout(
            [&]() {
                actionLayerManager.IterateFrame();

                xrSyncActions(compositionHelper.GetSession(), &syncInfo);

                for (XrPath subactionPath : subactionPaths) {
                    if (pathsAndKeys.count(subactionPath)) {
                        continue;
                    }
                    XrControllerModelKeyStateMSFT modelKeyState{XR_TYPE_CONTROLLER_MODEL_KEY_STATE_MSFT};
                    CHECK_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelKeyMSFT_(session, subactionPath, &modelKeyState));
                    if (modelKeyState.modelKey != XR_NULL_CONTROLLER_MODEL_KEY_MSFT) {
                        // we got one
                        modelKeys.emplace_back(modelKeyState.modelKey);
                        pathsAndKeys.emplace(subactionPath, modelKeyState.modelKey);
                        // pathsAndKeys.emplace_back(subactionPath, modelKeyState.modelKey);
                    }
                    else {
                    }
                }

                return (pathsAndKeys.size() == subactionPaths.size());
            },
            20s, kActionWaitDelay);

        if (pathsAndKeys.empty()) {
            WARN("Cannot do further testing on XR_MSFT_controller_model: no bound subaction paths have controller model keys");
            return;
        }
        else if (!gotAllKeys) {
            WARN("Only some bound subaction paths have controller model keys");
        }

        // Check two call struct for controller model properties and states,
        // plus regular two-call for the model itself
        auto modelPropertiesTwoCallData = getTwoCallStructData<XrControllerModelPropertiesMSFT>();
        auto modelStateTwoCallData = getTwoCallStructData<XrControllerModelStateMSFT>();
        for (auto modelKey : modelKeys) {
            INFO("Model key: " << Uint64ToHexString(modelKey));
            CheckTwoCallStructConformance(modelPropertiesTwoCallData, {XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT},
                                          "xrGetControllerModelPropertiesMSFT", false, [&](XrControllerModelPropertiesMSFT* properties) {
                                              return ext.xrGetControllerModelPropertiesMSFT_(session, modelKey, properties);
                                          });

            CheckTwoCallStructConformance(
                modelStateTwoCallData, {XR_TYPE_CONTROLLER_MODEL_STATE_MSFT}, "xrGetControllerModelStateMSFT", false,
                [&](XrControllerModelStateMSFT* state) { return ext.xrGetControllerModelStateMSFT_(session, modelKey, state); });
            CHECK_TWO_CALL(uint8_t, {}, ext.xrLoadControllerModelMSFT_, session, modelKey);

            CheckTwoCallStructConformance(modelPropertiesTwoCallData, {XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT},
                                          "xrGetControllerModelPropertiesMSFT", false, [&](XrControllerModelPropertiesMSFT* properties) {
                                              return ext.xrGetControllerModelPropertiesMSFT_(session, modelKey, properties);
                                          });

            XrControllerModelPropertiesMSFT modelProperties{XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT};
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelPropertiesMSFT_(session, modelKey, &modelProperties));
            std::vector<XrControllerModelNodePropertiesMSFT> nodeBuffer(modelProperties.nodeCountOutput);
            modelProperties.nodeCapacityInput = (uint32_t)nodeBuffer.size();
            modelProperties.nodeProperties = nodeBuffer.data();
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelPropertiesMSFT_(session, modelKey, &modelProperties));
            XrControllerModelStateMSFT modelState{XR_TYPE_CONTROLLER_MODEL_STATE_MSFT};
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelStateMSFT_(session, modelKey, &modelState));
            uint32_t modelBufferSize;
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrLoadControllerModelMSFT_(session, modelKey, 0, &modelBufferSize, nullptr));
            std::vector<uint8_t> modelBuffer(modelBufferSize);
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(
                ext.xrLoadControllerModelMSFT_(session, modelKey, modelBufferSize, &modelBufferSize, modelBuffer.data()));

            tinygltf::Model model;
            tinygltf::TinyGLTF loader;
            std::string err;
            std::string warn;
            bool loadedModel = loader.LoadBinaryFromMemory(&model, &err, &warn, modelBuffer.data(), (unsigned int)modelBuffer.size());
            if (!warn.empty()) {
                ReportF("glTF WARN: %s", &warn);
            }

            if (!err.empty()) {
                ReportF("glTF ERR: %s", &err);
            }

            if (!loadedModel) {
                FAIL("Failed to load glTF model provided.");
            }

            //! @todo Check that the model is valid, that it contains the nodes mentioned in the properties, and that the properties list is the same length as the state list
        }

        // Try inventing some model keys that will be invalid.

        for (auto modelKey : modelKeys) {
            XrControllerModelKeyMSFT fakeModelKey = modelKey + 1234;
            if (std::find(modelKeys.begin(), modelKeys.end(), fakeModelKey) == modelKeys.end()) {
                // We invented an invalid key
                INFO("Invented model key: " << Uint64ToHexString(fakeModelKey));
                ext.CheckInvalidModelKey(session, fakeModelKey);
            }
        }
    }

    TEST_CASE("XR_MSFT_controller_model-interactive", "[XR_MSFT_controller_model][scenario][interactive][no_auto]")
    {

        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME)) {
            SKIP(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME " not supported");
        }

        const char* instructions =
            "Ensure the controller model is positioned in the same position as the physical controller. "
            "Press menu to complete the validation.";

        CompositionHelper compositionHelper("XR_MSFT_controller_model_inte...", {"XR_MSFT_controller_model"});

        XrInstance instance = compositionHelper.GetInstance();
        ExtensionDataForXR_MSFT_controller_model ext(instance);

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosefCPP{});

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

        struct Hand
        {
            XrPath subactionPath;
            XrSpace space;
            XrControllerModelKeyMSFT modelKey;
            GLTFModelHandle controllerModel;
            GLTFModelInstanceHandle controllerModelInstance;
            ControllerAnimationHandler animationHandler;
        };

        Hand hands[2] = {};
        hands[0].subactionPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left");
        hands[1].subactionPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/right");

        // Set up the actions.
        const std::array<XrPath, 2> subactionPaths{hands[0].subactionPath, hands[1].subactionPath};
        XrActionSet actionSet;
        XrAction completeAction, gripPoseAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "interaction_test");
            strcpy(actionSetInfo.localizedActionSetName, "Interaction Test");
            XRC_CHECK_THROW_XRCMD(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetInfo, &actionSet));

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionInfo.actionName, "complete_test");
            strcpy(actionInfo.localizedActionName, "Complete test");
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &completeAction));

            // Remainder of actions use subaction.
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.actionName, "grip_pose");
            strcpy(actionInfo.localizedActionName, "Grip pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));
        }

        const std::vector<XrActionSuggestedBinding> bindings = {
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click")},
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click")},
            {gripPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/grip/pose")},
            {gripPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/grip/pose")},
        };

        XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBindings.interactionProfile = StringToPath(compositionHelper.GetInstance(), "/interaction_profiles/khr/simple_controller");
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        XRC_CHECK_THROW_XRCMD(xrSuggestInteractionProfileBindings(compositionHelper.GetInstance(), &suggestedBindings));

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.actionSets = &actionSet;
        attachInfo.countActionSets = 1;
        XRC_CHECK_THROW_XRCMD(xrAttachSessionActionSets(compositionHelper.GetSession(), &attachInfo));

        compositionHelper.BeginSession();
        XrSession session = compositionHelper.GetSession();
        auto& graphicsPlugin = GetGlobalData().graphicsPlugin;

        // Create the instructional quad layer placed to the left.
        XrCompositionLayerQuad* const instructionsQuad =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 768, instructions, 48)),
                                              localSpace, 1, {{0, 0, 0, 1}, {-1.5f, 0, -0.3f}});
        XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &Up, 70 * MATH_PI / 180);

        // Initialize an XrSpace for each hand
        for (Hand& hand : hands) {
            XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            spaceCreateInfo.subactionPath = hand.subactionPath;
            spaceCreateInfo.action = gripPoseAction;
            spaceCreateInfo.poseInActionSpace = XrPosefCPP();
            XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &hand.space));
        }

        auto update = [&](const XrFrameState& frameState) {
            std::vector<Cube> renderedCubes;
            std::vector<GLTFDrawable> renderedGLTFs;

            const std::array<XrActiveActionSet, 1> activeActionSets = {{{actionSet, XR_NULL_PATH}}};
            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.activeActionSets = activeActionSets.data();
            syncInfo.countActiveActionSets = (uint32_t)activeActionSets.size();
            XRC_CHECK_THROW_XRCMD(xrSyncActions(compositionHelper.GetSession(), &syncInfo));

            // Check if user has requested to complete the test.
            {
                XrActionStateGetInfo completeActionGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                completeActionGetInfo.action = completeAction;
                XrActionStateBoolean completeActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XRC_CHECK_THROW_XRCMD(
                    xrGetActionStateBoolean(compositionHelper.GetSession(), &completeActionGetInfo, &completeActionState));
                if (completeActionState.currentState == XR_TRUE && completeActionState.changedSinceLastSync) {
                    return false;
                }
            }

            for (Hand& hand : hands) {
                if (hand.modelKey != XR_NULL_CONTROLLER_MODEL_KEY_MSFT) {
                    continue;
                }
                XrControllerModelKeyStateMSFT modelKeyState{XR_TYPE_CONTROLLER_MODEL_KEY_STATE_MSFT};
                CHECK_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelKeyMSFT_(session, hand.subactionPath, &modelKeyState));
                if (modelKeyState.modelKey != XR_NULL_CONTROLLER_MODEL_KEY_MSFT) {
                    ReportF("Loaded model key");
                    hand.modelKey = modelKeyState.modelKey;

                    uint32_t modelBufferSize;
                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(
                        ext.xrLoadControllerModelMSFT_(session, hand.modelKey, 0, &modelBufferSize, nullptr));
                    std::vector<uint8_t> modelBuffer(modelBufferSize);
                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(
                        ext.xrLoadControllerModelMSFT_(session, hand.modelKey, modelBufferSize, &modelBufferSize, modelBuffer.data()));

                    hand.controllerModel = GetGlobalData().graphicsPlugin->LoadGLTF(modelBuffer);
                    hand.controllerModelInstance = GetGlobalData().graphicsPlugin->CreateGLTFModelInstance(hand.controllerModel);

                    XrControllerModelPropertiesMSFT modelProperties{XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT};
                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelPropertiesMSFT_(session, hand.modelKey, &modelProperties));
                    std::vector<XrControllerModelNodePropertiesMSFT> nodePropertiesBuffer(modelProperties.nodeCountOutput);
                    modelProperties.nodeCapacityInput = (uint32_t)nodePropertiesBuffer.size();
                    modelProperties.nodeProperties = nodePropertiesBuffer.data();
                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelPropertiesMSFT_(session, hand.modelKey, &modelProperties));

                    hand.animationHandler = ControllerAnimationHandler{*GetGlobalData().graphicsPlugin->GetPbrModel(hand.controllerModel),
                                                                       std::move(nodePropertiesBuffer)};

                    ReportF("Loaded model for key");
                }
            }

            for (Hand& hand : hands) {
                XrSpaceVelocity spaceVelocity{XR_TYPE_SPACE_VELOCITY};
                XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION, &spaceVelocity};
                XRC_CHECK_THROW_XRCMD(xrLocateSpace(hand.space, localSpace, frameState.predictedDisplayTime, &spaceLocation));
                if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                    if (hand.modelKey == XR_NULL_CONTROLLER_MODEL_KEY_MSFT) {
                        renderedCubes.push_back(Cube{spaceLocation.pose, {0.1f, 0.1f, 0.1f}});
                    }
                    else {
                        XrControllerModelStateMSFT modelState{XR_TYPE_CONTROLLER_MODEL_STATE_MSFT};
                        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelStateMSFT_(session, hand.modelKey, &modelState));
                        std::vector<XrControllerModelNodeStateMSFT> nodeStateBuffer(modelState.nodeCountOutput);
                        modelState.nodeCapacityInput = (uint32_t)nodeStateBuffer.size();
                        modelState.nodeStates = nodeStateBuffer.data();
                        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetControllerModelStateMSFT_(session, hand.modelKey, &modelState));

                        hand.animationHandler.UpdateControllerParts(nodeStateBuffer,
                                                                    graphicsPlugin->GetModelInstance(hand.controllerModelInstance));

                        renderedGLTFs.push_back(GLTFDrawable{hand.controllerModelInstance, spaceLocation.pose});
                    }
                }
            }

            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each of the separate swapchains using the projection layer view fov and pose.
                for (size_t view = 0; view < views.size(); view++) {
                    compositionHelper.AcquireWaitReleaseImage(swapchains[view],  //
                                                              [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                                                                  GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0);
                                                                  const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                                                                  const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                                                                  GetGlobalData().graphicsPlugin->RenderView(
                                                                      projLayer->views[view], swapchainImage,
                                                                      RenderParams().Draw(renderedCubes).Draw(renderedGLTFs));
                                                              });
                }

                layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
            }

            layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});

            compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

            return compositionHelper.PollEvents();
        };

        RenderLoop(compositionHelper.GetSession(), update).Loop();
    }

}  // namespace Conformance
