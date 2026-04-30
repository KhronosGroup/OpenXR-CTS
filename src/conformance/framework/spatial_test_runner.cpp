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

#include "spatial_test_runner.h"

#include <openxr/openxr.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_options.h"
#include "spatial_conformance_utils.h"
#include "utilities/event_reader.h"
#include "utilities/stringification.h"
#include "utilities/throw_helpers.h"
#include "utilities/xr_math_operators.h"

using Catch::Matchers::VectorContains;

namespace Conformance
{
#define FUNCTION_POINTERS(_)                         \
    _(xrEnumerateSpatialCapabilityComponentTypesEXT) \
    _(xrCreateSpatialContextAsyncEXT)                \
    _(xrCreateSpatialContextCompleteEXT)             \
    _(xrDestroySpatialContextEXT)                    \
    _(xrCreateSpatialDiscoverySnapshotAsyncEXT)      \
    _(xrCreateSpatialDiscoverySnapshotCompleteEXT)   \
    _(xrDestroySpatialSnapshotEXT)                   \
    _(xrQuerySpatialComponentDataEXT)                \
    _(xrPollFutureEXT)

#define DEFINE_FUNCTION_POINTER(name) static PFN_##name name;
    FUNCTION_POINTERS(DEFINE_FUNCTION_POINTER)

#define LOAD_FUNCTION_POINTER(name) name = GetInstanceExtensionFunction<PFN_##name>(instance, #name);

    void SpatialTestRunner::RunTest(const char* testName, const char* instructions, const XrPosef& instructionsPanelPose)
    {
        GlobalData& globalData = GetGlobalData();

        const std::vector<const char*> requiredExtensions = getRequiredExtensions();

        for (const char* extensionName : requiredExtensions) {
            if (!globalData.IsInstanceExtensionSupported(extensionName)) {
                SKIP(extensionName << " not supported");
            }
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Not using graphics, which the test requires");
        }

        // most spatial tests should benefit from preferring passthrough
        CompositionHelper compositionHelper(testName, requiredExtensions,
                                            CompositionHelper::EnvironmentBlendModePreference::PreferPassthrough);

        instance = compositionHelper.GetInstance();
        XrSystemId systemId = compositionHelper.GetSystemId();

        FUNCTION_POINTERS(LOAD_FUNCTION_POINTER)

        std::vector<const XrSpatialCapabilityConfigurationBaseHeaderEXT*> capabilityConfigs;
        onCreateSpatialContext(capabilityConfigs);

        for (const auto* const capabilityConfig : capabilityConfigs) {
            if (!IsSpatialCapabilitySupported(instance, systemId, capabilityConfig->capability)) {
                SKIP(XrEnumStr(capabilityConfig->capability) << " not enumerated as "
                                                                "supported.");
            }

            XrSpatialCapabilityComponentTypesEXT capabilityComponents{XR_TYPE_SPATIAL_CAPABILITY_COMPONENT_TYPES_EXT};
            XRC_CHECK_THROW_XRCMD(
                xrEnumerateSpatialCapabilityComponentTypesEXT(instance, systemId, capabilityConfig->capability, &capabilityComponents));
            std::vector<XrSpatialComponentTypeEXT> supportedComponents(capabilityComponents.componentTypeCountOutput);
            capabilityComponents.componentTypeCapacityInput = static_cast<uint32_t>(supportedComponents.size());
            capabilityComponents.componentTypes = supportedComponents.data();
            XRC_CHECK_THROW_XRCMD(
                xrEnumerateSpatialCapabilityComponentTypesEXT(instance, systemId, capabilityConfig->capability, &capabilityComponents));

            for (uint32_t i = 0; i < capabilityConfig->enabledComponentCount; ++i) {
                if (!VectorContains(capabilityConfig->enabledComponents[i]).match(supportedComponents)) {
                    SKIP(XrEnumStr(capabilityConfig->enabledComponents[i]) << " is required by "
                                                                              "this test but is not supported.");
                }
            }
        }

        localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, Pose::Identity);

        // Set up composition projection layer and swapchains (one swapchain per
        // view).
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

        const std::vector<XrPath> subactionPaths{StringToPath(compositionHelper.GetInstance(), "/user/hand/left"),
                                                 StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};

        XrActionSet actionSet;
        XrAction completeAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "spatial_plane_tracking_test");
            strcpy(actionSetInfo.localizedActionSetName, "Spatial Plane Tracking Test");
            XRC_CHECK_THROW_XRCMD(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetInfo, &actionSet))

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionInfo.actionName, "complete_test");
            strcpy(actionInfo.localizedActionName, "Complete test");
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &completeAction))
        }

        const std::vector<XrActionSuggestedBinding> bindings = {
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
        };

        XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBindings.interactionProfile = StringToPath(compositionHelper.GetInstance(), "/interaction_profiles/khr/simple_controller");
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        XRC_CHECK_THROW_XRCMD(xrSuggestInteractionProfileBindings(compositionHelper.GetInstance(), &suggestedBindings))

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.actionSets = &actionSet;
        attachInfo.countActionSets = 1;
        XRC_CHECK_THROW_XRCMD(xrAttachSessionActionSets(compositionHelper.GetSession(), &attachInfo))

        compositionHelper.BeginSession();

        // Create the instructional quad layer placed to the left.
        XrCompositionLayerQuad* const instructionsQuad =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 1024, instructions, 48)),
                                              localSpace, 1.0f, instructionsPanelPose);
        instructionsQuad->pose.orientation = Quat::FromAxisAngle(UpVector, DegToRad(10));

        gnomonMesh = GetGlobalData().graphicsPlugin->MakeGnomonMesh();

        XrSpatialContextCreateInfoEXT contextCreateInfo{XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT, nullptr,
                                                        static_cast<uint32_t>(capabilityConfigs.size()), capabilityConfigs.data()};

        XrFutureEXT ctxCreationFuture;
        XRC_CHECK_THROW_XRCMD(xrCreateSpatialContextAsyncEXT(compositionHelper.GetSession(), &contextCreateInfo, &ctxCreationFuture));

        state = DiscoveryState::WaitingForCtxCreation;
        XrFuturePollInfoEXT pollInfo{XR_TYPE_FUTURE_POLL_INFO_EXT};
        XrFuturePollResultEXT pollResult{XR_TYPE_FUTURE_POLL_RESULT_EXT};

        EventReader eventReader(compositionHelper.GetEventQueue());
        XrEventDataBuffer eventDataBuffer;

        auto update = [&](const XrFrameState& frameState) {
            const std::array<XrActiveActionSet, 1> activeActionSets = {{{actionSet, XR_NULL_PATH}}};
            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.activeActionSets = activeActionSets.data();
            syncInfo.countActiveActionSets = (uint32_t)activeActionSets.size();
            XRC_CHECK_THROW_XRCMD(xrSyncActions(compositionHelper.GetSession(), &syncInfo))

            XrActionStateGetInfo completeActionGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            completeActionGetInfo.action = completeAction;
            XrActionStateBoolean completeActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
            XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(compositionHelper.GetSession(), &completeActionGetInfo, &completeActionState))
            if (completeActionState.currentState == XR_TRUE && completeActionState.changedSinceLastSync) {
                return false;
            }

            switch (state) {
            case DiscoveryState::WaitingForCtxCreation: {
                pollInfo.future = ctxCreationFuture;
                XRC_CHECK_THROW_XRCMD(xrPollFutureEXT(instance, &pollInfo, &pollResult));
                if (pollResult.state == XR_FUTURE_STATE_READY_EXT) {
                    XrCreateSpatialContextCompletionEXT completion{XR_TYPE_CREATE_SPATIAL_CONTEXT_COMPLETION_EXT};
                    XRC_CHECK_THROW_XRCMD(
                        xrCreateSpatialContextCompleteEXT(compositionHelper.GetSession(), ctxCreationFuture, &completion));
                    XRC_CHECK_THROW_XRCMD(completion.futureResult);
                    spatialContext = completion.spatialContext;
                    state = DiscoveryState::Idle;

                    postCreateSpatialContextCompletion(frameState);
                }
                break;
            }
            case DiscoveryState::Idle: {
                if (eventReader.TryReadUntilEvent(eventDataBuffer, XR_TYPE_EVENT_DATA_SPATIAL_DISCOVERY_RECOMMENDED_EXT)) {
                    state = DiscoveryState::DiscoveryRecommended;
                }
                break;
            }
            case DiscoveryState::DiscoveryRecommended: {
                // create snapshot for all components enabled in the context.
                XrSpatialDiscoverySnapshotCreateInfoEXT discoverySnapshotCreateInfo{
                    XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT,
                };
                XRC_CHECK_THROW_XRCMD(
                    xrCreateSpatialDiscoverySnapshotAsyncEXT(spatialContext, &discoverySnapshotCreateInfo, &discoveryFuture));
                state = DiscoveryState::WaitingForDiscoveryResult;

                break;
            }
            case DiscoveryState::WaitingForDiscoveryResult: {
                pollInfo.future = discoveryFuture;
                XRC_CHECK_THROW_XRCMD(xrPollFutureEXT(instance, &pollInfo, &pollResult));
                if (pollResult.state == XR_FUTURE_STATE_READY_EXT) {
                    XrCreateSpatialDiscoverySnapshotCompletionInfoEXT createSnapshotCompletionInfo{
                        XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_INFO_EXT,
                        nullptr,
                        localSpace,
                        frameState.predictedDisplayTime,
                        discoveryFuture,
                    };
                    XrCreateSpatialDiscoverySnapshotCompletionEXT completion{XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_EXT};
                    XRC_CHECK_THROW_XRCMD(
                        xrCreateSpatialDiscoverySnapshotCompleteEXT(spatialContext, &createSnapshotCompletionInfo, &completion));
                    XRC_CHECK_THROW_XRCMD(completion.futureResult);
                    discoverySnapshot = completion.snapshot;

                    postCreateDiscoverySnapshotCompletion(discoverySnapshot);
                }
                break;
            }
            case DiscoveryState::StopDiscovery: {
                break;
            }
            }

            onUpdate(frameState);

            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each viewport of the wide swapchain using the projection
                // layer view fov and pose.
                for (size_t view = 0; view < views.size(); view++) {
                    compositionHelper.AcquireWaitReleaseImage(
                        swapchains[view],  //
                        [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, compositionHelper.GetEnvironmentBlendMode());
                            const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                            const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                            GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage,
                                                                       RenderParams().Draw(renderedCubes).Draw(renderedMeshes));
                        });
                }

                layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
            }

            layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});

            compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

            compositionHelper.PollEvents();
            return true;
        };

        RenderLoop(compositionHelper.GetSession(), update).Loop();

        if (spatialContext != XR_NULL_HANDLE) {
            XRC_CHECK_THROW_XRCMD(xrDestroySpatialContextEXT(spatialContext));
        }
    }

    void SpatialTestRunner::postCreateDiscoverySnapshotCompletion(XrSpatialSnapshotEXT snapshot)
    {
        querySnapshot(snapshot);
        render();

        state = DiscoveryState::Idle;
        XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(snapshot));
    }

    void SpatialTestRunner::querySnapshot(XrSpatialSnapshotEXT snapshot)
    {
        const std::vector<XrSpatialComponentTypeEXT> queryComponents = getQueryComponents();
        XrSpatialComponentDataQueryConditionEXT queryCond{
            XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT,
            nullptr,
            static_cast<uint32_t>(queryComponents.size()),
            queryComponents.data(),
        };

        XrSpatialComponentDataQueryResultEXT queryResult{XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT};

        XRC_CHECK_THROW_XRCMD(xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult));

        if (queryResult.entityIdCountOutput > 0) {
            entityIds.resize(queryResult.entityIdCountOutput);
            entityStates.resize(queryResult.entityStateCountOutput);

            queryResult.entityIdCapacityInput = static_cast<uint32_t>(entityIds.size());
            queryResult.entityIds = entityIds.data();
            queryResult.entityStateCapacityInput = static_cast<uint32_t>(entityStates.size());
            queryResult.entityStates = entityStates.data();

            std::vector<XrBaseOutStructure*> listStructs = getComponentDataListStructPtrs(queryResult.entityIdCountOutput);
            for (uint32_t i = 0; i < listStructs.size() - 1; ++i) {
                listStructs[i]->next = listStructs[i + 1];
            }
            queryResult.next = !listStructs.empty() ? listStructs[0] : nullptr;
            XRC_CHECK_THROW_XRCMD(xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult));
        }
    }

    void SpatialTestRunner::renderBounded2D(const XrSpatialBounded2DDataEXT& bounded2D, XrColor4f color)
    {
        renderedCubes.push_back(Cube{
            /* pose */ bounded2D.center,
            /* scale: */
            {bounded2D.extents.width, bounded2D.extents.height, 0.001f},
            /* tint */
            color,
        });
        renderGnomon(bounded2D.center);
    }

    void SpatialTestRunner::renderGnomon(const XrPosef& pose)
    {
        renderedMeshes.push_back(MeshDrawable{
            gnomonMesh,
            pose,
            {0.1f, 0.1f, 0.1f},
        });
    }

}  // namespace Conformance
