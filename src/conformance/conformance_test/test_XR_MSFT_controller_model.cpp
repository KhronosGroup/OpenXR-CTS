// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "action_utils.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "report.h"
#include "two_call.h"
#include "two_call_struct_metadata.h"
#include "two_call_struct_tests.h"

#include "common/hex_and_handles.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <array>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace Conformance
{
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

    TEST_CASE("XR_MSFT_controller_model-simple", "")
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

    TEST_CASE("XR_MSFT_controller_model", "")
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
        std::shared_ptr<IInputTestDevice> leftHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, compositionHelper.GetSession(),
                             simpleKHR, leftHandPath, cWMRControllerIPData);

        XrPath rightHandPath{StringToPath(instance, "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, compositionHelper.GetSession(),
                             simpleKHR, rightHandPath, cWMRControllerIPData);

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
        actionSpaceCreateInfo.poseInActionSpace = XrPosef{Quat::Identity, {0, 0, 0}};

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
}  // namespace Conformance
