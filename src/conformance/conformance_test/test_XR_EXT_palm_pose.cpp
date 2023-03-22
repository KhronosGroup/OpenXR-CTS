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
#include "throw_helpers.h"
#include "composition_utils.h"
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>
#include <xr_linear.h>

using namespace Conformance;

// Useful to track down errors when debugging stateful graphics APIs like OpenGL:
#define CHKGR() GetGlobalData().graphicsPlugin->CheckState(XRC_FILE_AND_LINE)

namespace Conformance
{
    constexpr XrVector3f Up{0, 1, 0};

    // Purpose: Ensure that the action space for palm can be used for placing a hand representation.
    TEST_CASE("XR_EXT_palm_pose", "[scenario][interactive][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported("XR_EXT_palm_pose")) {
            return;
        }

        const char* exampleImage = "palm_pose.png";
        const char* instructions =
            "An origin marker is rendered in each hand using the palm action space. "
            "A hand in an open pose is rendered in one hand using the palm action space. "
            "A hand in a pointing pose is rendered in the other hand using the palm action space. "
            "Press select to swap hands. Press menu to complete the validation.";

        CompositionHelper compositionHelper("XR_EXT_palm_pose", {"XR_EXT_palm_pose"});

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
            std::vector<SpaceCube> handOpenCubes;
            std::vector<SpaceCube> handPointingCubes;
        };

        Hand hands[2];
        hands[0].subactionPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left");
        hands[1].subactionPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/right");

        // Set up the actions.
        const std::array<XrPath, 2> subactionPaths{hands[0].subactionPath, hands[1].subactionPath};
        XrActionSet actionSet;
        XrAction completeAction, switchHandsAction, handModelPoseAction;
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
            strcpy(actionInfo.actionName, "palm_pose");
            strcpy(actionInfo.localizedActionName, "palm pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &handModelPoseAction));
        }

        const std::vector<XrActionSuggestedBinding> bindings = {
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click")},
            {completeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click")},
            {switchHandsAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
            {switchHandsAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
            {handModelPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/palm_ext/pose")},
            {handModelPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/palm_ext/pose")},
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

        // const float PointerLength = 4.00f;
        const float PointerThickness = 0.01f;
        const float PointerAxisLength = 0.05f;

        const float PalmLength = 0.08f;
        const float PalmThickness = 0.03f;
        const float PalmWidth = 0.08f;

        const float PointerFingerLength = 0.08f;
        const float MiddleFingerLength = 0.09f;
        const float RingFingerLength = 0.08f;
        const float PinkyFingerLength = 0.07f;
        const float FingerSpacing = 0.0033f;
        const float FingerThickness = 0.015f;
        const float FingerWidth = 0.015f;
        const float CurledFingerLength = 0.04f;
        const float CurledFingerThickness = 0.04f;

        // Create Cubes for poses
        {

            auto addCube = [&](Hand hand, XrAction poseAction, const XrVector3f& poseInSpacePos, const XrVector3f& scale,
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

            // Left Hand

            // Hand Open
            addCube(hands[0], handModelPoseAction, {0, 0, PointerAxisLength / 2}, {PointerThickness, PointerThickness, PointerAxisLength},
                    hands[0].handOpenCubes);  // Forward (ray)
            addCube(hands[0], handModelPoseAction, {0, PointerAxisLength / 2, 0}, {PointerThickness, PointerAxisLength, PointerThickness},
                    hands[0].handOpenCubes);  // Up
            addCube(hands[0], handModelPoseAction, {PointerAxisLength / 2, 0, 0}, {PointerAxisLength, PointerThickness, PointerThickness},
                    hands[0].handOpenCubes);  // Right
            addCube(hands[0], handModelPoseAction, {-PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                    hands[0].handOpenCubes);  // Palm
            addCube(hands[0], handModelPoseAction, {-PalmThickness / 2, PalmWidth / 2 + FingerWidth / 2 + FingerSpacing, -PalmLength / 2},
                    {FingerThickness, FingerWidth, PointerFingerLength},
                    hands[0].handOpenCubes);  // Thumb
            addCube(hands[0], handModelPoseAction,
                    {-PalmThickness / 2, PalmWidth / 2 - FingerWidth / 2, -PalmLength / 2 + -PointerFingerLength / 2},
                    {FingerThickness, FingerWidth, PointerFingerLength},
                    hands[0].handOpenCubes);  // Pointer
            addCube(hands[0], handModelPoseAction,
                    {-PalmThickness / 2, FingerSpacing + FingerWidth / 2, -PalmLength / 2 + -MiddleFingerLength / 2},
                    {FingerThickness, FingerWidth, MiddleFingerLength},
                    hands[0].handOpenCubes);  // Middle
            addCube(hands[0], handModelPoseAction,
                    {-PalmThickness / 2, -FingerSpacing + -FingerWidth / 2, -PalmLength / 2 + -RingFingerLength / 2},
                    {FingerThickness, FingerWidth, RingFingerLength},
                    hands[0].handOpenCubes);  // Ring
            addCube(hands[0], handModelPoseAction,
                    {-PalmThickness / 2, -PalmWidth / 2 + FingerWidth / 2, -PalmLength / 2 + -PinkyFingerLength / 2},
                    {FingerThickness, FingerWidth, PinkyFingerLength},
                    hands[0].handOpenCubes);  // Pinky

            // Hand Pointing
            addCube(hands[0], handModelPoseAction, {0, 0, PointerAxisLength / 2}, {PointerThickness, PointerThickness, PointerAxisLength},
                    hands[0].handPointingCubes);  // Forward (ray)
            addCube(hands[0], handModelPoseAction, {0, PointerAxisLength / 2, 0}, {PointerThickness, PointerAxisLength, PointerThickness},
                    hands[0].handPointingCubes);  // Up
            addCube(hands[0], handModelPoseAction, {PointerAxisLength / 2, 0, 0}, {PointerAxisLength, PointerThickness, PointerThickness},
                    hands[0].handPointingCubes);  // Right
            addCube(hands[0], handModelPoseAction, {-PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                    hands[0].handPointingCubes);  // Palm
            addCube(hands[0], handModelPoseAction, {-PalmThickness / 2, PalmWidth / 2 + FingerWidth / 2 + FingerSpacing, -PalmLength / 2},
                    {FingerThickness, FingerWidth, PointerFingerLength},
                    hands[0].handPointingCubes);  // Thumb
            addCube(hands[0], handModelPoseAction,
                    {-PalmThickness / 2, PalmWidth / 2 - FingerWidth / 2, -PalmLength / 2 + -PointerFingerLength / 2},
                    {FingerThickness, FingerWidth, PointerFingerLength},
                    hands[0].handPointingCubes);  // Pointer
            addCube(hands[0], handModelPoseAction,
                    {-PalmThickness / 2 + CurledFingerThickness / 2, FingerSpacing + FingerWidth / 2, -PalmLength / 2},
                    {CurledFingerThickness, FingerWidth, CurledFingerLength},
                    hands[0].handPointingCubes);  // Middle
            addCube(hands[0], handModelPoseAction,
                    {-PalmThickness / 2 + CurledFingerThickness / 2, -FingerSpacing + -FingerWidth / 2, -PalmLength / 2},
                    {CurledFingerThickness, FingerWidth, CurledFingerLength},
                    hands[0].handPointingCubes);  // Ring
            addCube(hands[0], handModelPoseAction,
                    {-PalmThickness / 2 + CurledFingerThickness / 2, -PalmWidth / 2 + FingerWidth / 2, -PalmLength / 2},
                    {CurledFingerThickness, FingerWidth, CurledFingerLength},
                    hands[0].handPointingCubes);  // Pinky

            // Right Hand

            // Hand Open
            addCube(hands[1], handModelPoseAction, {0, 0, PointerAxisLength / 2}, {PointerThickness, PointerThickness, PointerAxisLength},
                    hands[1].handOpenCubes);  // Forward (ray)
            addCube(hands[1], handModelPoseAction, {0, PointerAxisLength / 2, 0}, {PointerThickness, PointerAxisLength, PointerThickness},
                    hands[1].handOpenCubes);  // Up
            addCube(hands[1], handModelPoseAction, {PointerAxisLength / 2, 0, 0}, {PointerAxisLength, PointerThickness, PointerThickness},
                    hands[1].handOpenCubes);  // Right
            addCube(hands[1], handModelPoseAction, {PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                    hands[1].handOpenCubes);  // Palm
            addCube(hands[1], handModelPoseAction, {PalmThickness / 2, PalmWidth / 2 + FingerWidth / 2 + FingerSpacing, -PalmLength / 2},
                    {FingerThickness, FingerWidth, PointerFingerLength},
                    hands[1].handOpenCubes);  // Thumb
            addCube(hands[1], handModelPoseAction,
                    {PalmThickness / 2, PalmWidth / 2 - FingerWidth / 2, -PalmLength / 2 + -PointerFingerLength / 2},
                    {FingerThickness, FingerWidth, PointerFingerLength},
                    hands[1].handOpenCubes);  // Pointer
            addCube(hands[1], handModelPoseAction,
                    {PalmThickness / 2, FingerSpacing + FingerWidth / 2, -PalmLength / 2 + -MiddleFingerLength / 2},
                    {FingerThickness, FingerWidth, MiddleFingerLength},
                    hands[1].handOpenCubes);  // Middle
            addCube(hands[1], handModelPoseAction,
                    {PalmThickness / 2, -FingerSpacing + -FingerWidth / 2, -PalmLength / 2 + -RingFingerLength / 2},
                    {FingerThickness, FingerWidth, RingFingerLength},
                    hands[1].handOpenCubes);  // Ring
            addCube(hands[1], handModelPoseAction,
                    {PalmThickness / 2, -PalmWidth / 2 + FingerWidth / 2, -PalmLength / 2 + -PinkyFingerLength / 2},
                    {FingerThickness, FingerWidth, PinkyFingerLength},
                    hands[1].handOpenCubes);  // Pinky

            // Hand Pointing
            addCube(hands[1], handModelPoseAction, {0, 0, PointerAxisLength / 2}, {PointerThickness, PointerThickness, PointerAxisLength},
                    hands[1].handPointingCubes);  // Forward (ray)
            addCube(hands[1], handModelPoseAction, {0, PointerAxisLength / 2, 0}, {PointerThickness, PointerAxisLength, PointerThickness},
                    hands[1].handPointingCubes);  // Up
            addCube(hands[1], handModelPoseAction, {PointerAxisLength / 2, 0, 0}, {PointerAxisLength, PointerThickness, PointerThickness},
                    hands[1].handPointingCubes);  // Right
            addCube(hands[1], handModelPoseAction, {PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                    hands[1].handPointingCubes);  // Palm
            addCube(hands[1], handModelPoseAction, {PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                    hands[1].handPointingCubes);  // Palm
            addCube(hands[1], handModelPoseAction, {PalmThickness / 2, PalmWidth / 2 + FingerWidth / 2 + FingerSpacing, -PalmLength / 2},
                    {FingerThickness, FingerWidth, PointerFingerLength},
                    hands[1].handPointingCubes);  // Thumb
            addCube(hands[1], handModelPoseAction,
                    {PalmThickness / 2, PalmWidth / 2 - FingerWidth / 2, -PalmLength / 2 + -PointerFingerLength / 2},
                    {FingerThickness, FingerWidth, PointerFingerLength},
                    hands[1].handPointingCubes);  // Pointer
            addCube(hands[1], handModelPoseAction,
                    {PalmThickness / 2 - CurledFingerThickness / 2, FingerSpacing + FingerWidth / 2, -PalmLength / 2},
                    {CurledFingerThickness, FingerWidth, CurledFingerLength},
                    hands[1].handPointingCubes);  // Middle
            addCube(hands[1], handModelPoseAction,
                    {PalmThickness / 2 - CurledFingerThickness / 2, -FingerSpacing + -FingerWidth / 2, -PalmLength / 2},
                    {CurledFingerThickness, FingerWidth, CurledFingerLength},
                    hands[1].handPointingCubes);  // Ring
            addCube(hands[1], handModelPoseAction,
                    {PalmThickness / 2 - CurledFingerThickness / 2, -PalmWidth / 2 + FingerWidth / 2, -PalmLength / 2},
                    {CurledFingerThickness, FingerWidth, CurledFingerLength},
                    hands[1].handPointingCubes);  // Pinky
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
                    if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT &&
                        spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
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
                    locateCubes(hand.handPointingCubes);
                }
                else {
                    locateCubes(hand.handOpenCubes);
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
                    compositionHelper.AcquireWaitReleaseImage(swapchains[view],  //
                                                              [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                                                                  GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                                                                  const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                                                                  const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                                                                  GetGlobalData().graphicsPlugin->RenderView(
                                                                      projLayer->views[view], swapchainImage,
                                                                      RenderParams().Draw(renderedCubes));
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
