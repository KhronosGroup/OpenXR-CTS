// Copyright (c) 2019-2021, The Khronos Group Inc.
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

#include <array>
#include <thread>
#include <numeric>
#include "utils.h"
#include "report.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "composition_utils.h"
#include <catch2/catch.hpp>
#include <openxr/openxr.h>
#include <xr_linear.h>

using namespace Conformance;

// Useful to track down errors when debugging stateful graphics APIs like OpenGL:
#define CHKGR() GetGlobalData().graphicsPlugin->CheckState(XRC_FILE_AND_LINE)

namespace Conformance
{
    constexpr XrVector3f Up{0, 1, 0};

    // Purpose: Ensure that the action space for grip can be used for a grippable object, in this case a sword, and the action space for aim can be used for comfortable aiming.
    TEST_CASE("Grip and Aim Pose", "[scenario][interactive]")
    {
        const char* exampleImage = "grip_and_aim_pose.png";
        const char* instructions =
            "A sword is rendered in one hand using the grip action space. "
            "A pointing ray is rendered in the other hand using the aim action space with a small axis to show +X and +Y. "
            "Press select to swap hands. Press menu to complete the validation.";

        CompositionHelper compositionHelper("Grip and Aim Pose");

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

        // A cube rendered centered in a space.
        struct SpaceCube
        {
            XrSpace space;
            XrVector3f scale;
        };

        struct Hand
        {
            XrPath subactionPath;
            std::vector<SpaceCube> swordCubes;
            std::vector<SpaceCube> pointerCubes;
        };

        Hand hands[2];
        hands[0].subactionPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left");
        hands[1].subactionPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/right");

        // Set up the actions.
        const std::array<XrPath, 2> subactionPaths{hands[0].subactionPath, hands[1].subactionPath};
        XrActionSet actionSet;
        XrAction completeAction, switchHandsAction, gripPoseAction, aimPoseAction;
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

            strcpy(actionInfo.actionName, "switch_hands");
            strcpy(actionInfo.localizedActionName, "Switch hands");
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &switchHandsAction));

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.actionName, "grip_pose");
            strcpy(actionInfo.localizedActionName, "Grip pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));

            strcpy(actionInfo.actionName, "aim_pose");
            strcpy(actionInfo.localizedActionName, "Aim pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &aimPoseAction));
        }

        const std::vector<XrActionSuggestedBinding> bindings = {
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click")},
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click")},
            {switchHandsAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
            {switchHandsAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
            {gripPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/grip/pose")},
            {gripPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/grip/pose")},
            {aimPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/aim/pose")},
            {aimPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/aim/pose")},
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
                                              localSpace, 1.0f, {{0, 0, 0, 1}, {-1.5f, 0, -0.3f}});
        XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &Up, 70 * MATH_PI / 180);

        // Create a sample image quad layer placed to the right.
        XrCompositionLayerQuad* const exampleQuad =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(RGBAImage::Load(exampleImage)), localSpace,
                                              1.25f, {{0, 0, 0, 1}, {1.5f, 0, -0.3f}});
        XrQuaternionf_CreateFromAxisAngle(&exampleQuad->pose.orientation, &Up, -70 * MATH_PI / 180);

        const float PointerLength = 4.00f;
        const float PointerThickness = 0.01f;
        const float PointerAxisLength = 0.05f;

        const float SwordBladeLength = 0.5f;
        const float SwordBladeWidth = 0.04f;
        const float SwordHandleLength = 0.1f;
        const float SwordHandleWidth = 0.03f;
        const float SwordGuardWidth = 0.12f;
        const float SwordGuardThickness = 0.02f;

        // Create XrSpaces at various spaces around the grip poses.
        for (Hand& hand : hands) {

            auto addCube = [&](XrAction poseAction, const XrVector3f& poseInSpacePos, const XrVector3f& scale,
                               std::vector<SpaceCube>& spaceCubes) {
                SpaceCube spaceCube;
                spaceCube.scale = scale;

                XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                spaceCreateInfo.subactionPath = hand.subactionPath;
                spaceCreateInfo.action = poseAction;
                spaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, poseInSpacePos};
                XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &spaceCube.space));
                spaceCubes.push_back(std::move(spaceCube));
            };

            // Sword cubes: Blade, Handle (centered at grip pose), Guard
            addCube(gripPoseAction, {0, 0, -SwordBladeLength / 2 - (SwordHandleLength / 2)},
                    {SwordBladeWidth / 4, SwordBladeWidth, SwordBladeLength}, hand.swordCubes);
            addCube(gripPoseAction, {0, 0, 0}, {SwordHandleWidth / 2, SwordHandleWidth, SwordHandleLength}, hand.swordCubes);
            addCube(gripPoseAction, {0, 0, -SwordHandleLength / 2}, {SwordGuardThickness, SwordGuardWidth, SwordGuardThickness},
                    hand.swordCubes);

            // Pointer cubes: Tracking cube at grip and pointer ray on aim with an up/right indicator.
            addCube(gripPoseAction, {0, 0, 0}, {0.03f, 0.03f, 0.05f}, hand.pointerCubes);
            addCube(aimPoseAction, {0, 0, -PointerLength / 2}, {PointerThickness, PointerThickness, PointerLength},
                    hand.pointerCubes);  // Forward (ray)
            addCube(aimPoseAction, {0, PointerAxisLength / 2, 0}, {PointerThickness, PointerAxisLength, PointerThickness},
                    hand.pointerCubes);  // Up
            addCube(aimPoseAction, {PointerAxisLength / 2, 0, 0}, {PointerAxisLength, PointerThickness, PointerThickness},
                    hand.pointerCubes);  // Right
        }

        // Initially the pointer is on the 0th hand (left) but it changes to whichever hand last pressed select.
        XrPath pointerHand = hands[0].subactionPath;

        auto update = [&](const XrFrameState& frameState) {
            std::vector<Cube> renderedCubes;

            const std::array<XrActiveActionSet, 1> activeActionSets = {{actionSet, XR_NULL_PATH}};
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

            // Locate and add to list of cubes to render.
            auto locateCubes = [&](const std::vector<SpaceCube>& spaceCubes) {
                for (auto& spaceCube : spaceCubes) {
                    XrSpaceVelocity spaceVelocity{XR_TYPE_SPACE_VELOCITY};
                    XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION, &spaceVelocity};
                    XRC_CHECK_THROW_XRCMD(xrLocateSpace(spaceCube.space, localSpace, frameState.predictedDisplayTime, &spaceLocation));
                    if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                        renderedCubes.push_back(Cube{spaceLocation.pose, spaceCube.scale});
                    }
                }
            };

            for (const Hand& hand : hands) {
                // Check if user has requested to swap hands
                XrActionStateGetInfo swapActionGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                swapActionGetInfo.action = switchHandsAction;
                swapActionGetInfo.subactionPath = hand.subactionPath;
                XrActionStateBoolean swapActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(compositionHelper.GetSession(), &swapActionGetInfo, &swapActionState));
                if (swapActionState.currentState == XR_TRUE && swapActionState.changedSinceLastSync) {
                    pointerHand = hand.subactionPath;
                }

                if (hand.subactionPath == pointerHand) {
                    locateCubes(hand.pointerCubes);
                }
                else {
                    locateCubes(hand.swordCubes);
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
                            GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage, format, renderedCubes);
                        });
                }

                layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
            }

            layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});
            layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(exampleQuad)});

            compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

            return compositionHelper.PollEvents();
        };

        RenderLoop(compositionHelper.GetSession(), update).Loop();
    }
}  // namespace Conformance
