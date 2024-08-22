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

#include "RGBAImage.h"
#include "common/xr_linear.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "graphics_plugin.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"

#include <openxr/openxr.h>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <tuple>
#include <vector>

using namespace Conformance;

namespace Conformance
{
    using namespace openxr::math_operators;

    constexpr XrVector3f Up{0, 1, 0};

    TEST_CASE("HapticInterrupt", "[scenario][interactive][no_auto]")
    {
        const char* instructions =
            "Press the select button on either hand to begin a 2 second haptic output. "
            "Pressing the select button again during a haptic response should immediately interrupt "
            "the current haptic response and begin another with a different amplitude. "
            "Ensure that the new haptic response also lasts 2 seconds. "
            "Press the menu button on either controller to pass the test. ";

        CompositionHelper compositionHelper("Haptic Interrupt");

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

        const std::vector<XrPath> subactionPaths{StringToPath(compositionHelper.GetInstance(), "/user/hand/left"),
                                                 StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};

        XrActionSet actionSet;
        XrAction hapticAction, completeAction, gripPoseAction, applyHapticAction;
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

            actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
            strcpy(actionInfo.actionName, "haptic_output");
            strcpy(actionInfo.localizedActionName, "Haptic Output");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &hapticAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionInfo.actionName, "apply_haptic_input");
            strcpy(actionInfo.localizedActionName, "Apply Haptic Input");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &applyHapticAction));
        }

        const std::vector<XrActionSuggestedBinding> bindings = {
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click")},
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click")},
            {applyHapticAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
            {applyHapticAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
            {gripPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/grip/pose")},
            {gripPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/grip/pose")},
            {hapticAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/output/haptic")},
            {hapticAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/output/haptic")},
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

        // Create the instructional quad layer placed to the left.
        XrCompositionLayerQuad* const instructionsQuad =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 512, instructions, 48)),
                                              localSpace, 1, {{0, 0, 0, 1}, {-1.5f, 0, -0.3f}});
        instructionsQuad->pose.orientation = Quat::FromAxisAngle(Up, DegToRad(70));

        struct Hand
        {
            XrPath subactionPath;
            XrSpace space;
            bool highAmplitude;
        };

        Hand hands[2];
        hands[0].subactionPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left");
        hands[1].subactionPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/right");

        for (Hand& hand : hands) {
            XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            spaceCreateInfo.action = gripPoseAction;
            spaceCreateInfo.subactionPath = hand.subactionPath;
            spaceCreateInfo.poseInActionSpace.orientation.w = 1.0f;
            XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &hand.space));
        }

        constexpr XrVector3f cubeScale{0.1f, 0.1f, 0.1f};
        auto update = [&](const XrFrameState& frameState) {
            std::vector<Cube> renderedCubes;

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

                // Locate hands
                XrSpaceVelocity spaceVelocity{XR_TYPE_SPACE_VELOCITY};
                XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION, &spaceVelocity};
                XRC_CHECK_THROW_XRCMD(xrLocateSpace(hand.space, localSpace, frameState.predictedDisplayTime, &spaceLocation));
                if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                    renderedCubes.emplace_back(spaceLocation.pose, cubeScale);
                }

                // Check squeeze and apply haptic
                XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                actionStateGetInfo.action = applyHapticAction;
                actionStateGetInfo.subactionPath = hand.subactionPath;
                XrActionStateBoolean applyHapticValue{XR_TYPE_ACTION_STATE_BOOLEAN};
                XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(compositionHelper.GetSession(), &actionStateGetInfo, &applyHapticValue));

                if (applyHapticValue.currentState == XR_TRUE && applyHapticValue.changedSinceLastSync) {
                    XrHapticActionInfo hapticInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                    hapticInfo.action = hapticAction;
                    hapticInfo.subactionPath = hand.subactionPath;
                    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
                    vibration.amplitude = hand.highAmplitude ? 0.75f : 0.25f;
                    vibration.duration = 2000000000;
                    vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
                    XRC_CHECK_THROW_XRCMD(
                        xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticInfo, (XrHapticBaseHeader*)&vibration));

                    hand.highAmplitude = !hand.highAmplitude;
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
                    compositionHelper.AcquireWaitReleaseImage(swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0);
                        const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                        const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                        GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage,
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
    }
}  // namespace Conformance
