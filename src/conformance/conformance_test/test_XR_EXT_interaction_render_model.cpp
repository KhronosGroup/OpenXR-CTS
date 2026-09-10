// Copyright (c) 2019-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "action_utils.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "controller_animation_handler.h"
#include "ext_render_model.h"
#include "graphics_plugin.h"
#include "report.h"
#include "two_call.h"
#include "two_call_struct_tests.h"

#include "utilities/event_reader.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Conformance
{
    struct DispatchTable_EXT_interaction_render_model : DispatchTable_EXT_render_model
    {

        PFN_xrEnumerateInteractionRenderModelIdsEXT xrEnumerateInteractionRenderModelIdsEXT_;
        explicit DispatchTable_EXT_interaction_render_model(XrInstance instance_)
            : DispatchTable_EXT_render_model(instance_)
            , xrEnumerateInteractionRenderModelIdsEXT_(GetInstanceExtensionFunction<PFN_xrEnumerateInteractionRenderModelIdsEXT, true>(
                  instance, "xrEnumerateInteractionRenderModelIdsEXT"))
        {
        }
    };

    namespace
    {
        struct ActionSetup
        {
            std::vector<XrPath> subactionPaths;

            XrActionSet actionSet{};
            XrAction gripPoseAction{};
            ActionSetup(XrInstance instance, InteractionManager& interactionManager)
                : subactionPaths{StringToPath(instance, "/user/hand/left"), StringToPath(instance, "/user/hand/right")}
            {
                {
                    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                    strcpy(actionSetInfo.actionSetName, "xr_ext_render_model");
                    strcpy(actionSetInfo.localizedActionSetName, "XR_EXT_render_model");
                    XRC_CHECK_THROW_XRCMD(xrCreateActionSet(instance, &actionSetInfo, &actionSet));

                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                    strcpy(actionInfo.actionName, "grip_pose");
                    strcpy(actionInfo.localizedActionName, "Grip pose");
                    actionInfo.subactionPaths = subactionPaths.data();
                    actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
                    XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));
                }

                interactionManager.AddActionSet(actionSet);
                XrPath simpleInteractionProfile = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
                interactionManager.AddActionBindings(simpleInteractionProfile,
                                                     {{
                                                         {gripPoseAction, StringToPath(instance, "/user/hand/left/input/grip/pose")},
                                                         {gripPoseAction, StringToPath(instance, "/user/hand/right/input/grip/pose")},
                                                     }});

                interactionManager.AttachActionSets();
            }
        };

        bool HasInteractionRenderModelsChangedEvent(EventReader& reader)
        {
            XrEventDataBuffer eventBuffer;
            while (reader.TryReadNext(eventBuffer)) {
                if (eventBuffer.type == XR_TYPE_EVENT_DATA_INTERACTION_RENDER_MODELS_CHANGED_EXT) {
                    return true;
                }
            }
            return false;
        }

        template <typename F>
        bool WaitForInteractionRenderModelsChangedEvent(EventReader& reader, F&& waitingFunctor)
        {
            CountdownTimer countdown(std::chrono::seconds(2));
            bool haveEvent = HasInteractionRenderModelsChangedEvent(reader);
            while (!haveEvent && !countdown.IsTimeUp()) {
                waitingFunctor();
                haveEvent = HasInteractionRenderModelsChangedEvent(reader);
            }
            return haveEvent;
        }
    }  // namespace

    TEST_CASE("XR_EXT_interaction_render_model-auto", "[XR_EXT_interaction_render_model][XR_EXT_render_model]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME)) {
            SKIP(XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME " not supported");
        }
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_RENDER_MODEL_EXTENSION_NAME)) {
            FAIL(XR_EXT_RENDER_MODEL_EXTENSION_NAME
                 " not supported but it is a dependency of " XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME);
        }

        AutoBasicInstance instance(
            {XR_EXT_UUID_EXTENSION_NAME, XR_EXT_RENDER_MODEL_EXTENSION_NAME, XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME});
        DispatchTable_EXT_interaction_render_model ext(instance);

        SECTION("SingleLifeCycle")
        {
            AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);
            InteractionManager interactionManager{instance, session};
            ActionSetup actionSetup{instance, interactionManager};
            EventReader eventReaderForIRMChanged(session.GetEventQueue());
            // CompositionHelper compositionHelper("XR_EXT_render_model", {XR_EXT_UUID_EXTENSION_NAME, XR_EXT_RENDER_MODEL_EXTENSION_NAME,
            //                                                             XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME});

            SECTION("EnumerateTooEarly")
            {
                {
                    INFO("Did not begin the session yet");
                    CHECK(false == HasInteractionRenderModelsChangedEvent(eventReaderForIRMChanged));
                    uint32_t countOutput = 0;
                    REQUIRE(XR_ERROR_SESSION_NOT_RUNNING ==
                            ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, 0, &countOutput, nullptr));
                    REQUIRE(countOutput == 0);
                }
                session.BeginSession();
                {
                    INFO("Did not sync actions yet");
                    CHECK(false == HasInteractionRenderModelsChangedEvent(eventReaderForIRMChanged));
                    uint32_t countOutput = 0;
                    REQUIRE(XR_SUCCESS == ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, 0, &countOutput, nullptr));
                    REQUIRE(countOutput == 0);
                }
                interactionManager.SyncActions(XR_NULL_PATH);

                // Wait 2 seconds for the event
                bool haveEvent = WaitForInteractionRenderModelsChangedEvent(eventReaderForIRMChanged,
                                                                            [&] { interactionManager.SyncActions(XR_NULL_PATH); });

                if (!haveEvent) {
                    WARN("Timed out waiting for interaction render models to be notified");
                    return;
                }
                uint32_t countOutput = 0;
                REQUIRE(XR_SUCCESS == ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, 0, &countOutput, nullptr));
                REQUIRE(countOutput > 0);
            }
            SECTION("NormalOperation")
            {
                session.BeginSession();
                interactionManager.SyncActions(XR_NULL_PATH);

                // Wait 2 seconds for the event
                bool haveEvent = WaitForInteractionRenderModelsChangedEvent(eventReaderForIRMChanged,
                                                                            [&] { interactionManager.SyncActions(XR_NULL_PATH); });
                if (!haveEvent) {
                    WARN("Timed out waiting for interaction render models to be notified");
                    return;
                }
                uint32_t countOutput = 0;
                REQUIRE(XR_SUCCESS == ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, 0, &countOutput, nullptr));
                REQUIRE(countOutput > 0);
                std::vector<XrRenderModelIdEXT> rmids;
                rmids.resize(countOutput);
                REQUIRE(XR_SUCCESS ==
                        ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, (uint32_t)rmids.size(), &countOutput, rmids.data()));
                CHECK_THAT(rmids, !Catch::Matchers::Contains(XR_NULL_RENDER_MODEL_ID_EXT));
                CHECK_THAT(rmids, !Catch::Matchers::Contains(InvalidValues::InvalidAtomValue<XrRenderModelIdEXT>()));
                std::set<XrRenderModelIdEXT> uniqueRMIds(rmids.begin(), rmids.end());
                {
                    INFO("No repeated render model IDs enumerated");
                    CHECK(rmids.size() == uniqueRMIds.size());
                }

                // TODO check we can load all of them
                // TODO check that we cannot shut down the session and load the UUID in a new session
            }
        }

        // If, when attempting to create the handle, the session does not support any
        // render model of the given render model ID requiring only glTF extensions
        // from the supplied glTF extension list (in
        // slink:XrRenderModelCreateInfoEXT::pname:gltfExtensions), the runtime must:
        // return ename:XR_ERROR_RENDER_MODEL_GLTF_EXTENSION_REQUIRED_EXT.
        // --> NOT APPLICABLE IN THIS EXTENSION since a no-ext model is required

        // The runtime must: return ename:XR_ERROR_RENDER_MODEL_GLTF_EXTENSION_REQUIRED_EXT
        // if the runtime is unable to return a glTF asset that only uses extensions found
        // in the application's list of supported glTF extensions.
        // --> NOT APPLICABLE IN THIS EXTENSION since a no-ext model is required

        // Related extensions may: require the runtime to support providing base glTF
        // assets without any required glTF extensions, in which case this error must: not
        // be returned by flink:xrCreateRenderModelAssetEXT in association with such
        // models.
        // --> Tested in testRenderModel in the interactive test
    }

    struct InteractionModelData
    {
        RenderModelEXTScoped rm;
        GLTFModelHandle glTFModelHandle;
        GLTFModelInstanceHandle glTFModelInstanceHandle;
        RenderModelAnimationHandler animationHandler;

        XrSpace rmSpace{XR_NULL_HANDLE};

        InteractionModelData(RenderModelData&& rmd)
            : rm(std::move(rmd.rm))
            , glTFModelHandle(rmd.glTFModelHandle)
            , glTFModelInstanceHandle(rmd.glTFModelInstanceHandle)
            , animationHandler(rmd.animationHandler)
        {
        }
    };
    TEST_CASE("XR_EXT_interaction_render_model-interactive",
              "[XR_EXT_interaction_render_model][XR_EXT_render_model][interactive][scenario][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_EXT_RENDER_MODEL_EXTENSION_NAME)) {
            SKIP(XR_EXT_RENDER_MODEL_EXTENSION_NAME " not supported");
        }

        if (!globalData.IsInstanceExtensionSupported(XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME)) {
            SKIP(XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper(
            "XR_EXT_interaction_render_model",
            {XR_EXT_UUID_EXTENSION_NAME, XR_EXT_RENDER_MODEL_EXTENSION_NAME, XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME},
            CompositionHelper::EnvironmentBlendModePreference::PreferPassthrough);
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "irm_interactive.jpeg",
            "Make sure all devices use to interact in the scene are visible. "
            "The devices should be accurately located and scaled to match their physical counterparts on all axes. "
            "If two controller are used, make sure they appear to collide when brought together physically.");

        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        ActionSetup actions{instance, interactionManager};

        DispatchTable_EXT_interaction_render_model ext(instance);

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, Pose::Identity);

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

        ActionLayerManager actionLayerManager(compositionHelper);

        actionLayerManager.WaitForSessionFocusWithMessage();
        interactionManager.SyncActions(XR_NULL_PATH);
        uint32_t numModels{0};
        CHECK_RESULT_UNQUALIFIED_SUCCESS(ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, 0, &numModels, nullptr));
        std::vector<XrRenderModelIdEXT> interactionModelIds(numModels, XR_NULL_PATH);
        CHECK_RESULT_UNQUALIFIED_SUCCESS(
            ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, numModels, &numModels, interactionModelIds.data()));

        // Create render model handles
        // The names of glTF extensions that the application is capable of supporting.
        // The runtime may return a glTF model with any of these extensions listed in
        // the "extensionsRequired" array.
        std::vector<const char*> appSupportedGltfExtensions{"KHR_texture_basisu", "KHR_materials_specular"};

        std::vector<InteractionModelData> interactionModels;
        for (XrRenderModelIdEXT id : interactionModelIds) {
            // This exercises most functionality of render models and render model assets, and returns the processed model
            InteractionModelData imd = testRenderModel(ext, session, id, true, appSupportedGltfExtensions);

            // Create a space for locating the render model.
            XrRenderModelSpaceCreateInfoEXT spaceCreateInfo{XR_TYPE_RENDER_MODEL_SPACE_CREATE_INFO_EXT};
            spaceCreateInfo.renderModel = imd.rm.get();
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrCreateRenderModelSpaceEXT_(session, &spaceCreateInfo, &imd.rmSpace));

            interactionModels.emplace_back(std::move(imd));
        }

        // TODO this only cares about one model at a time right now
        size_t modelIdx = 0;
        InteractionModelData& modelData = interactionModels[modelIdx];
        XrRenderModelEXT testModel = modelData.rm.get();

        XrSpace modelSpace = modelData.rmSpace;
        RenderModelAnimationHandler& animationHandler = modelData.animationHandler;

        EventReader eventReader(compositionHelper.GetEventQueue());

        auto updateLayers = [&](const XrFrameState& frameState) {
            // need to update session state early so that the GetSessionState check below works
            compositionHelper.PollEvents();

            // do this first so if models take time to load, xrLocateViews doesn't complain about an old time
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            // want our standard action sets on all subaction paths

            std::vector<GLTFModelInstanceHandle> renderedGLTFs;

            // Use xrLocateSpace to locate the model's space
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
            REQUIRE(XR_SUCCESS == xrLocateSpace(modelSpace, localSpace, frameState.predictedDisplayTime, &location));
            const bool modelVisible = ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
                                       (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT));

            if (modelVisible) {
                {
                    INFO("Only allowed to be visible when session state is FOCUSED");
                    CHECK(compositionHelper.GetSessionState() == XR_SESSION_STATE_FOCUSED);
                }
                // DONE: The runtime must: return ename:XR_ERROR_VALIDATION_FAILURE if
                // slink:XrRenderModelStatesListEXT::stateCount is not equal to the number of node
                // names returned by the flink:xrGetRenderModelAssetPropertiesEXT function.
                // --- if the count is wrong, the test case will directly break, can also use assert --

                // The array for reading node states must be the same size as the node
                // properties array.
                std::vector<XrRenderModelNodeStateEXT> nodeStates(animationHandler.GetNumberOfAnimatableNodes());

                XrRenderModelStateGetInfoEXT stateGetInfo{XR_TYPE_RENDER_MODEL_STATE_GET_INFO_EXT};
                stateGetInfo.displayTime = frameState.predictedDisplayTime;
                XrRenderModelStateEXT state{XR_TYPE_RENDER_MODEL_STATE_EXT};

                // TODO: The runtime must: return ename:XR_ERROR_VALIDATION_FAILURE if
                // slink:XrRenderModelStateEXT::stateCount is not equal to the number of nodes
                // returned by the flink:xrGetRenderModelPropertiesEXT function.
                state.nodeStateCount = (uint32_t)nodeStates.size();
                state.nodeStates = nodeStates.data();

                // xrGetRenderModelStateEXT does not use the two-call idiom. The size is
                // determined by xrGetRenderModelPropertiesEXT.
                CHECK_RESULT_UNQUALIFIED_SUCCESS(ext.xrGetRenderModelStateEXT_(testModel, &stateGetInfo, &state));

                Pbr::ModelInstance& modelInstance = GetGlobalData().graphicsPlugin->GetModelInstance(modelData.glTFModelInstanceHandle);
                animationHandler.UpdateNodes(std::move(nodeStates), modelInstance);
                modelInstance.SetModelToWorld(location.pose);
                renderedGLTFs.push_back(modelData.glTFModelInstanceHandle);
            }

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                for (size_t view = 0; view < views.size(); view++) {
                    compositionHelper.AcquireWaitReleaseImage(swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, compositionHelper.GetEnvironmentBlendMode());
                        const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                        const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                        GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage,
                                                                   RenderParams().Draw(renderedGLTFs));
                    });
                }

                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
            }

            if (!interactiveLayerManager.EndFrame(frameState, layers)) {
                // user has marked this test as complete
                modelIdx++;
                return (modelIdx < interactionModels.size());
            }
            return true;
        };

        RenderLoop(session, updateLayers).Loop();
    }

    TEST_CASE("XR_EXT_interaction_render_model-objective", "[XR_EXT_interaction_render_model][interactive][scenario][no_auto]")
    {

        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_EXT_RENDER_MODEL_EXTENSION_NAME)) {
            SKIP(XR_EXT_RENDER_MODEL_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper(
            "XR_EXT_interaction_render_model",
            {XR_EXT_UUID_EXTENSION_NAME, XR_EXT_RENDER_MODEL_EXTENSION_NAME, XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME},
            CompositionHelper::EnvironmentBlendModePreference::PreferPassthrough);
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "irm_interactive.jpeg",
            "Make sure all devices use to interact in the scene are visible. "
            "The devices should be accurately located and scaled to match their physical counterparts on all axes. "
            "If two controller are used, make sure they appear to collide when brought together physically. ");

        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();

        DispatchTable_EXT_interaction_render_model ext(instance);

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, Pose::Identity);

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

        interactionManager.AttachActionSets();

        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        SECTION("Enumerate nothing if focused but before sync actions")
        {
            interactionManager.SyncActions(XR_NULL_PATH);
            actionLayerManager.WaitForSessionFocusWithMessage();
            uint32_t numModels{0};
            CHECK_RESULT_UNQUALIFIED_SUCCESS(ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, 0, &numModels, nullptr));
            CHECK(numModels == 0);
        }
        SECTION("Enumerate nothing if sync actions but not focused")
        {
            interactionManager.SyncActions(XR_NULL_PATH);
            uint32_t numModels{0};
            CHECK_RESULT_UNQUALIFIED_SUCCESS(ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, 0, &numModels, nullptr));
            CHECK(numModels == 0);
        }
        SECTION("Properly enumerate")
        {

            actionLayerManager.WaitForSessionFocusWithMessage();
            interactionManager.SyncActions(XR_NULL_PATH);
            uint32_t numModels{0};
            CHECK_RESULT_UNQUALIFIED_SUCCESS(ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, 0, &numModels, nullptr));
            std::vector<XrRenderModelIdEXT> interactionModelIds(numModels, XR_NULL_PATH);
            CHECK_RESULT_UNQUALIFIED_SUCCESS(
                ext.xrEnumerateInteractionRenderModelIdsEXT_(session, nullptr, numModels, &numModels, interactionModelIds.data()));

            // Create render model handles
            // The names of glTF extensions that the application is capable of supporting.
            // The runtime may return a glTF model with any of these extensions listed in
            // the "extensionsRequired" array.
            std::vector<const char*> appSupportedGltfExtensions{"KHR_texture_basisu", "KHR_materials_specular"};

            std::vector<InteractionModelData> interactionModels;
            for (XrRenderModelIdEXT id : interactionModelIds) {
                // This exercises most functionality of render models and render model assets, and returns the processed model
                InteractionModelData imd = testRenderModel(ext, session, id, true, appSupportedGltfExtensions);

                // Create a space for locating the render model.
                XrRenderModelSpaceCreateInfoEXT spaceCreateInfo{XR_TYPE_RENDER_MODEL_SPACE_CREATE_INFO_EXT};
                spaceCreateInfo.renderModel = imd.rm.get();
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(ext.xrCreateRenderModelSpaceEXT_(session, &spaceCreateInfo, &imd.rmSpace));

                interactionModels.emplace_back(std::move(imd));
            }

            // TODO:
            // The runtime must: return the same render model states and assets to these
            // handles since they are sharing the same underlying render model ID. TEST WITH SAME RENDER MODEL ID TWICE
            // SECTION("Same RenderModel ID")
            // {
            // }

            // TODO: Although any associated slink:XrSpace handles created by
            // flink:xrCreateRenderModelSpaceEXT are not destroyed upon calling
            // flink:xrDestroyRenderModelEXT because the space is a child of the session
            // handle, any render model spaces created from a now-destroyed render model handle
            // must: no longer return any elink:XrSpaceLocationFlagBits or
            // elink:XrSpaceVelocityFlagBits set in slink:XrSpaceLocation::pname:locationFlags
            // or slink:XrSpaceVelocity::pname:velocityFlags, respectively.
            // SECTION("Test destroying render model handle, XrSpace flag not set")
            // {
            // }
        }
    }
}  // namespace Conformance
