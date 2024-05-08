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
#include "utilities/feature_availability.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"
#include "utilities/utils.h"
#include "availability_helper.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <sstream>
#include <vector>

using namespace Conformance;

namespace Conformance
{
    namespace
    {
        const auto kExtensionRequirements = FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_EXT_palm_pose};
        const auto kPromotedCoreRequirements = FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_1};

        constexpr XrVector3f Up{0, 1, 0};

        // Purpose: Ensure that the action space for palm can be used for placing a hand representation.
        inline void SharedGripSurface(const FeatureSet& featureSet)
        {
            GlobalData& globalData = GetGlobalData();

            // This test intentionally skips instead of testing that grip_surface is not available in a core OpenXR 1.0 instance
            // because the non-interactive test already tests this case.
            const std::vector<const char*> extensions = SkipOrGetExtensions("Grip Surface", globalData, featureSet);

            // Check whether we should test palm_ext or grip_surface names.
            // TODO test both palm_pose_ext and core OpenXR 1.1 grip_surface in the same test?
            const bool testExtension = featureSet.Get(FeatureBitIndex::BIT_XR_EXT_palm_pose);

            const char* exampleImage = "palm_pose.png";
            const char* poseIdentifier = testExtension ? "palm_ext" : "grip_surface";
            const char* spaceName = testExtension ? "Palm Pose" : "Grip Surface Pose";
            std::ostringstream instructions;
            instructions << "An origin marker is rendered in each hand using the " << poseIdentifier << " action space. ";
            instructions << "A hand in an open pose is rendered in one hand using the " << poseIdentifier << " action space. ";
            instructions << "A hand in a pointing pose is rendered in the other hand using the " << poseIdentifier << " action space. ";
            instructions << "Press select to swap hands. Press menu to complete the validation.";

            CompositionHelper compositionHelper(spaceName, extensions);
            XrInstance instance = compositionHelper.GetInstance();
            XrSession session = compositionHelper.GetSession();

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
            hands[0].subactionPath = StringToPath(instance, "/user/hand/left");
            hands[1].subactionPath = StringToPath(instance, "/user/hand/right");

            // Set up the actions.
            const std::array<XrPath, 2> subactionPaths{hands[0].subactionPath, hands[1].subactionPath};
            XrActionSet actionSet;
            XrAction completeAction, switchHandsAction, gripSurfacePoseAction;
            {
                XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                strcpy(actionSetInfo.actionSetName, "interaction_test");
                strcpy(actionSetInfo.localizedActionSetName, "Interaction Test");
                XRC_CHECK_THROW_XRCMD(xrCreateActionSet(instance, &actionSetInfo, &actionSet));

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
                if (testExtension) {
                    strcpy(actionInfo.actionName, "palm_pose");
                    strcpy(actionInfo.localizedActionName, "Palm Pose");
                }
                else {
                    strcpy(actionInfo.actionName, "grip_surface_pose");
                    strcpy(actionInfo.localizedActionName, "Grip Surface Pose");
                }
                actionInfo.subactionPaths = subactionPaths.data();
                actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
                XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripSurfacePoseAction));
            }

            const std::vector<XrActionSuggestedBinding> bindings = {
                {completeAction, StringToPath(instance, "/user/hand/left/input/menu/click")},
                {completeAction, StringToPath(instance, "/user/hand/right/input/menu/click")},
                {switchHandsAction, StringToPath(instance, "/user/hand/left/input/select/click")},
                {switchHandsAction, StringToPath(instance, "/user/hand/right/input/select/click")},
                {gripSurfacePoseAction,
                 StringToPath(instance, testExtension ? "/user/hand/left/input/palm_ext/pose" : "/user/hand/left/input/grip_surface/pose")},
                {gripSurfacePoseAction, StringToPath(instance, testExtension ? "/user/hand/right/input/palm_ext/pose"
                                                                             : "/user/hand/right/input/grip_surface/pose")},

            };

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            XRC_CHECK_THROW_XRCMD(xrSuggestInteractionProfileBindings(instance, &suggestedBindings));

            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.actionSets = &actionSet;
            attachInfo.countActionSets = 1;
            XRC_CHECK_THROW_XRCMD(xrAttachSessionActionSets(session, &attachInfo));

            compositionHelper.BeginSession();

            // Create the instructional quad layer placed to the left.
            XrCompositionLayerQuad* const instructionsQuad = compositionHelper.CreateQuadLayer(
                compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 512, instructions.str().c_str(), 48)), localSpace, 1.0f,
                {{0, 0, 0, 1}, {-1.5f, 0, -0.3f}});
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

                auto addCube = [&](const Hand& hand, XrAction poseAction, const XrVector3f& poseInSpacePos, const XrVector3f& scale,
                                   std::vector<SpaceCube>& spaceCubes) {
                    SpaceCube spaceCube;
                    spaceCube.scale = scale;

                    XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                    spaceCreateInfo.subactionPath = hand.subactionPath;
                    spaceCreateInfo.action = poseAction;
                    spaceCreateInfo.poseInActionSpace = {Quat::Identity, poseInSpacePos};
                    XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(session, &spaceCreateInfo, &spaceCube.space));
                    spaceCubes.push_back(spaceCube);
                };

                // Left Hand

                // Hand Open
                addCube(hands[0], gripSurfacePoseAction, {0, 0, PointerAxisLength / 2},
                        {PointerThickness, PointerThickness, PointerAxisLength},
                        hands[0].handOpenCubes);  // Forward (ray)
                addCube(hands[0], gripSurfacePoseAction, {0, PointerAxisLength / 2, 0},
                        {PointerThickness, PointerAxisLength, PointerThickness},
                        hands[0].handOpenCubes);  // Up
                addCube(hands[0], gripSurfacePoseAction, {PointerAxisLength / 2, 0, 0},
                        {PointerAxisLength, PointerThickness, PointerThickness},
                        hands[0].handOpenCubes);  // Right
                addCube(hands[0], gripSurfacePoseAction, {-PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                        hands[0].handOpenCubes);  // Palm
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2, PalmWidth / 2 + FingerWidth / 2 + FingerSpacing, -PalmLength / 2},
                        {FingerThickness, FingerWidth, PointerFingerLength},
                        hands[0].handOpenCubes);  // Thumb
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2, PalmWidth / 2 - FingerWidth / 2, -PalmLength / 2 + -PointerFingerLength / 2},
                        {FingerThickness, FingerWidth, PointerFingerLength},
                        hands[0].handOpenCubes);  // Pointer
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2, FingerSpacing + FingerWidth / 2, -PalmLength / 2 + -MiddleFingerLength / 2},
                        {FingerThickness, FingerWidth, MiddleFingerLength},
                        hands[0].handOpenCubes);  // Middle
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2, -FingerSpacing + -FingerWidth / 2, -PalmLength / 2 + -RingFingerLength / 2},
                        {FingerThickness, FingerWidth, RingFingerLength},
                        hands[0].handOpenCubes);  // Ring
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2, -PalmWidth / 2 + FingerWidth / 2, -PalmLength / 2 + -PinkyFingerLength / 2},
                        {FingerThickness, FingerWidth, PinkyFingerLength},
                        hands[0].handOpenCubes);  // Pinky

                // Hand Pointing
                addCube(hands[0], gripSurfacePoseAction, {0, 0, PointerAxisLength / 2},
                        {PointerThickness, PointerThickness, PointerAxisLength},
                        hands[0].handPointingCubes);  // Forward (ray)
                addCube(hands[0], gripSurfacePoseAction, {0, PointerAxisLength / 2, 0},
                        {PointerThickness, PointerAxisLength, PointerThickness},
                        hands[0].handPointingCubes);  // Up
                addCube(hands[0], gripSurfacePoseAction, {PointerAxisLength / 2, 0, 0},
                        {PointerAxisLength, PointerThickness, PointerThickness},
                        hands[0].handPointingCubes);  // Right
                addCube(hands[0], gripSurfacePoseAction, {-PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                        hands[0].handPointingCubes);  // Palm
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2, PalmWidth / 2 + FingerWidth / 2 + FingerSpacing, -PalmLength / 2},
                        {FingerThickness, FingerWidth, PointerFingerLength},
                        hands[0].handPointingCubes);  // Thumb
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2, PalmWidth / 2 - FingerWidth / 2, -PalmLength / 2 + -PointerFingerLength / 2},
                        {FingerThickness, FingerWidth, PointerFingerLength},
                        hands[0].handPointingCubes);  // Pointer
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2 + CurledFingerThickness / 2, FingerSpacing + FingerWidth / 2, -PalmLength / 2},
                        {CurledFingerThickness, FingerWidth, CurledFingerLength},
                        hands[0].handPointingCubes);  // Middle
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2 + CurledFingerThickness / 2, -FingerSpacing + -FingerWidth / 2, -PalmLength / 2},
                        {CurledFingerThickness, FingerWidth, CurledFingerLength},
                        hands[0].handPointingCubes);  // Ring
                addCube(hands[0], gripSurfacePoseAction,
                        {-PalmThickness / 2 + CurledFingerThickness / 2, -PalmWidth / 2 + FingerWidth / 2, -PalmLength / 2},
                        {CurledFingerThickness, FingerWidth, CurledFingerLength},
                        hands[0].handPointingCubes);  // Pinky

                // Right Hand

                // Hand Open
                addCube(hands[1], gripSurfacePoseAction, {0, 0, PointerAxisLength / 2},
                        {PointerThickness, PointerThickness, PointerAxisLength},
                        hands[1].handOpenCubes);  // Forward (ray)
                addCube(hands[1], gripSurfacePoseAction, {0, PointerAxisLength / 2, 0},
                        {PointerThickness, PointerAxisLength, PointerThickness},
                        hands[1].handOpenCubes);  // Up
                addCube(hands[1], gripSurfacePoseAction, {PointerAxisLength / 2, 0, 0},
                        {PointerAxisLength, PointerThickness, PointerThickness},
                        hands[1].handOpenCubes);  // Right
                addCube(hands[1], gripSurfacePoseAction, {PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                        hands[1].handOpenCubes);  // Palm
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2, PalmWidth / 2 + FingerWidth / 2 + FingerSpacing, -PalmLength / 2},
                        {FingerThickness, FingerWidth, PointerFingerLength},
                        hands[1].handOpenCubes);  // Thumb
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2, PalmWidth / 2 - FingerWidth / 2, -PalmLength / 2 + -PointerFingerLength / 2},
                        {FingerThickness, FingerWidth, PointerFingerLength},
                        hands[1].handOpenCubes);  // Pointer
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2, FingerSpacing + FingerWidth / 2, -PalmLength / 2 + -MiddleFingerLength / 2},
                        {FingerThickness, FingerWidth, MiddleFingerLength},
                        hands[1].handOpenCubes);  // Middle
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2, -FingerSpacing + -FingerWidth / 2, -PalmLength / 2 + -RingFingerLength / 2},
                        {FingerThickness, FingerWidth, RingFingerLength},
                        hands[1].handOpenCubes);  // Ring
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2, -PalmWidth / 2 + FingerWidth / 2, -PalmLength / 2 + -PinkyFingerLength / 2},
                        {FingerThickness, FingerWidth, PinkyFingerLength},
                        hands[1].handOpenCubes);  // Pinky

                // Hand Pointing
                addCube(hands[1], gripSurfacePoseAction, {0, 0, PointerAxisLength / 2},
                        {PointerThickness, PointerThickness, PointerAxisLength},
                        hands[1].handPointingCubes);  // Forward (ray)
                addCube(hands[1], gripSurfacePoseAction, {0, PointerAxisLength / 2, 0},
                        {PointerThickness, PointerAxisLength, PointerThickness},
                        hands[1].handPointingCubes);  // Up
                addCube(hands[1], gripSurfacePoseAction, {PointerAxisLength / 2, 0, 0},
                        {PointerAxisLength, PointerThickness, PointerThickness},
                        hands[1].handPointingCubes);  // Right
                addCube(hands[1], gripSurfacePoseAction, {PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                        hands[1].handPointingCubes);  // Palm
                addCube(hands[1], gripSurfacePoseAction, {PalmThickness / 2, 0, 0}, {PalmThickness, PalmWidth, PalmLength},
                        hands[1].handPointingCubes);  // Palm
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2, PalmWidth / 2 + FingerWidth / 2 + FingerSpacing, -PalmLength / 2},
                        {FingerThickness, FingerWidth, PointerFingerLength},
                        hands[1].handPointingCubes);  // Thumb
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2, PalmWidth / 2 - FingerWidth / 2, -PalmLength / 2 + -PointerFingerLength / 2},
                        {FingerThickness, FingerWidth, PointerFingerLength},
                        hands[1].handPointingCubes);  // Pointer
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2 - CurledFingerThickness / 2, FingerSpacing + FingerWidth / 2, -PalmLength / 2},
                        {CurledFingerThickness, FingerWidth, CurledFingerLength},
                        hands[1].handPointingCubes);  // Middle
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2 - CurledFingerThickness / 2, -FingerSpacing + -FingerWidth / 2, -PalmLength / 2},
                        {CurledFingerThickness, FingerWidth, CurledFingerLength},
                        hands[1].handPointingCubes);  // Ring
                addCube(hands[1], gripSurfacePoseAction,
                        {PalmThickness / 2 - CurledFingerThickness / 2, -PalmWidth / 2 + FingerWidth / 2, -PalmLength / 2},
                        {CurledFingerThickness, FingerWidth, CurledFingerLength},
                        hands[1].handPointingCubes);  // Pinky
            }

            // Initially the pointer is on the 0th hand (left) but it changes to whichever hand last pressed select.
            XrPath pointerHand = hands[0].subactionPath;

            auto update = [&](const XrFrameState& frameState) {
                std::vector<Cube> renderedCubes;

                const std::array<XrActiveActionSet, 1> activeActionSets = {{{actionSet, XR_NULL_PATH}}};
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                syncInfo.activeActionSets = activeActionSets.data();
                syncInfo.countActiveActionSets = (uint32_t)activeActionSets.size();
                XRC_CHECK_THROW_XRCMD(xrSyncActions(session, &syncInfo));

                // Check if user has requested to complete the test.
                {
                    XrActionStateGetInfo completeActionGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    completeActionGetInfo.action = completeAction;
                    XrActionStateBoolean completeActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
                    XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(session, &completeActionGetInfo, &completeActionState));
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
                            renderedCubes.emplace_back(spaceLocation.pose, spaceCube.scale);
                        }
                    }
                };

                for (const Hand& hand : hands) {
                    // Check if user has requested to swap hands
                    XrActionStateGetInfo swapActionGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    swapActionGetInfo.action = switchHandsAction;
                    swapActionGetInfo.subactionPath = hand.subactionPath;
                    XrActionStateBoolean swapActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
                    XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(session, &swapActionGetInfo, &swapActionState));
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

            RenderLoop(session, update).Loop();
        }

        double angleDeg(XrVector3f& a, XrVector3f& b)
        {
            double dotProd = XrVector3f_Dot(&a, &b);

            double magA = XrVector3f_Length(&a);
            double magB = XrVector3f_Length(&b);

            double angleInRad = std::acos(dotProd / (magA * magB));
            double angleInDeg = angleInRad * (180.0 / MATH_PI);

            return angleInDeg;
        }

        void SharedGripSurfaceAutomated(const FeatureSet& featureSet)
        {
            GlobalData& globalData = GetGlobalData();

            // Check whether we should test palm_pose_ext or grip_surface names.
            // TODO test both palm_pose_ext and core OpenXR 1.1 grip_surface in the same test?
            const bool testExtension = featureSet.Get(FeatureBitIndex::BIT_XR_EXT_palm_pose);

            // See if it is explicitly enabled by default
            FeatureSet enabled;
            globalData.PopulateVersionAndEnabledExtensions(enabled);

            XrActionSet actionSet;
            XrAction gripPoseAction, gripSurfacePoseAction;
            XrSpace gripPoseSpace[2], gripSurfacePoseSpace[2];

            auto suggestActions = [&](XrInstance instance, bool testExtension, bool expectSupported = true) {
                // Set up the actions.
                const std::array<XrPath, 2> subactionPaths{StringToPath(instance, "/user/hand/left"),
                                                           StringToPath(instance, "/user/hand/right")};

                {
                    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                    strcpy(actionSetInfo.actionSetName, "conformance_test");
                    strcpy(actionSetInfo.localizedActionSetName, "Conformance Test");
                    XRC_CHECK_THROW_XRCMD(xrCreateActionSet(instance, &actionSetInfo, &actionSet));

                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                    actionInfo.subactionPaths = subactionPaths.data();
                    actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();

                    strcpy(actionInfo.actionName, "grip_pose");
                    strcpy(actionInfo.localizedActionName, "grip pose");
                    actionInfo.subactionPaths = subactionPaths.data();
                    actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
                    XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));

                    if (testExtension) {
                        strcpy(actionInfo.actionName, "palm_pose");
                        strcpy(actionInfo.localizedActionName, "palm pose");
                    }
                    else {
                        strcpy(actionInfo.actionName, "grip_surface_pose");
                        strcpy(actionInfo.localizedActionName, "grip surface pose");
                    }
                    actionInfo.subactionPaths = subactionPaths.data();
                    actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
                    XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripSurfacePoseAction));
                }

                const std::vector<XrActionSuggestedBinding> bindings = {
                    {gripPoseAction, StringToPath(instance, "/user/hand/left/input/grip/pose")},
                    {gripPoseAction, StringToPath(instance, "/user/hand/right/input/grip/pose")},
                    {gripSurfacePoseAction, StringToPath(instance, testExtension ? "/user/hand/left/input/palm_ext/pose"
                                                                                 : "/user/hand/left/input/grip_surface/pose")},
                    {gripSurfacePoseAction, StringToPath(instance, testExtension ? "/user/hand/right/input/palm_ext/pose"
                                                                                 : "/user/hand/right/input/grip_surface/pose")},
                };

                XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                suggestedBindings.interactionProfile = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
                suggestedBindings.suggestedBindings = bindings.data();
                suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();

                if (expectSupported) {
                    XRC_CHECK_THROW_XRCMD(xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
                }
                else {
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &suggestedBindings), XR_ERROR_PATH_UNSUPPORTED);
                }
            };

            // If we test the extension and the extension has not been force-enabled, we can test the extension-not-enabled case.
            if (testExtension && !enabled.get_XR_EXT_palm_pose()) {
                SECTION("Requirements not enabled")
                {
                    AutoBasicInstance instance;
                    AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);
                    suggestActions(instance, true, false);
                }
            }
            else if (testExtension && enabled.get_XR_EXT_palm_pose()) {
                WARN(XR_EXT_PALM_POSE_EXTENSION_NAME " force-enabled, cannot test behavior when extension is disabled.");
            }

            // If we test the Core 1.1 grip_surface and are on an OpenXR 1.0 instance, we can test that grip_surface should not be available.
            if (!testExtension && !enabled.get_XR_VERSION_1_1()) {
                SECTION("Requirements not enabled")
                {
                    AutoBasicInstance instance;
                    AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);
                    suggestActions(instance, false, false);
                }
            }

            // Skip after the "Requirements not enabled" tests, so that unavailability of e.g. grip_surface paths on OpenXR 1.0 is tested before the skip.
            const std::vector<const char*> extensions = SkipOrGetExtensions("Grip Surface", globalData, featureSet);

            AutoBasicInstance instance{extensions};

            suggestActions(instance, testExtension);

            AutoBasicSession session(AutoBasicSession::beginSession | AutoBasicSession::createActions | AutoBasicSession::createSpaces |
                                         AutoBasicSession::createSwapchains,
                                     instance);

            XrSpace localSpace{XR_NULL_HANDLE};
            XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            createSpaceInfo.poseInReferenceSpace = XrPosefCPP();
            REQUIRE_RESULT(xrCreateReferenceSpace(session, &createSpaceInfo, &localSpace), XR_SUCCESS);

            XrPath handPaths[2];
            handPaths[0] = StringToPath(instance, "/user/hand/left");
            handPaths[1] = StringToPath(instance, "/user/hand/right");

            for (uint32_t i = 0; i < 2; i++) {
                XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                spaceCreateInfo.subactionPath = handPaths[i];
                spaceCreateInfo.action = gripPoseAction;
                spaceCreateInfo.poseInActionSpace = XrPosefCPP();
                XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(session, &spaceCreateInfo, &gripPoseSpace[i]));

                spaceCreateInfo.action = gripSurfacePoseAction;
                XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(session, &spaceCreateInfo, &gripSurfacePoseSpace[i]));
            }

            // Get frames iterating to the point of app focused state. This will draw frames along the way.
            FrameIterator frameIterator(&session);
            frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.actionSets = &actionSet;
            attachInfo.countActionSets = 1;
            XRC_CHECK_THROW_XRCMD(xrAttachSessionActionSets(session, &attachInfo));

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            XrActiveActionSet activeActionSet{actionSet};
            syncInfo.activeActionSets = &activeActionSet;
            syncInfo.countActiveActionSets = 1;

            REQUIRE_RESULT(xrSyncActions(session, &syncInfo), XR_SUCCESS);

            enum class TestState
            {
                Untested,
                Inactive,
                Invalid,
                Passed,
            };

            // Test keeps running until all grip surface spaces that are tested have VALID location flags.
            TestState maxTestStates[2] = {TestState::Untested, TestState::Untested};
            bool testPassed = WaitUntilPredicateWithTimeout(
                [&]() -> bool {
                    frameIterator.SubmitFrame();
                    REQUIRE_RESULT_SUCCEEDED(xrSyncActions(session, &syncInfo));

                    for (uint32_t i = 0; i < 2; i++) {
                        if ((i == 0 && !globalData.leftHandUnderTest) || (i == 1 && !globalData.rightHandUnderTest)) {
                            continue;
                        }

                        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                        getInfo.action = gripPoseAction;
                        getInfo.subactionPath = handPaths[i];

                        XrActionStatePose gripState{XR_TYPE_ACTION_STATE_POSE};
                        REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &gripState), XR_SUCCESS);

                        getInfo.action = gripSurfacePoseAction;

                        XrActionStatePose gripSurfaceState{XR_TYPE_ACTION_STATE_POSE};
                        REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &gripSurfaceState), XR_SUCCESS);

                        // grip pose is not actually required to be provided (e.g. wrist controller)
                        if (!gripState.isActive && gripSurfaceState.isActive) {
                            SKIP(
                                "Grip Surface pose without Grip pose detected. Skipping pose relation tests between Grip Surface and Grip pose");
                        }

                        if (!gripState.isActive || !gripSurfaceState.isActive) {
                            maxTestStates[i] = std::max(maxTestStates[i], TestState::Inactive);
                            continue;
                        }

                        XrSpaceVelocity gripVelocity{XR_TYPE_SPACE_VELOCITY};
                        XrSpaceLocation gripLocation{XR_TYPE_SPACE_LOCATION, &gripVelocity};
                        XRC_CHECK_THROW_XRCMD(
                            xrLocateSpace(gripPoseSpace[i], localSpace, frameIterator.frameState.predictedDisplayTime, &gripLocation));

                        // VALID is usually enough here because the palm pose / grip surface is a usually a static offset that should be available for non TRACKED grip pose.
                        if (!(gripLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT &&
                              gripLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
                            maxTestStates[i] = std::max(maxTestStates[i], TestState::Invalid);
                            continue;
                        }

                        // Locate grip surface space in grip space to make checks simpler
                        XrSpaceVelocity gripSurfaceVelocity{XR_TYPE_SPACE_VELOCITY};
                        XrSpaceLocation gripSurfaceLocation{XR_TYPE_SPACE_LOCATION, &gripSurfaceVelocity};
                        XRC_CHECK_THROW_XRCMD(xrLocateSpace(gripSurfacePoseSpace[i], gripPoseSpace[i],
                                                            frameIterator.frameState.predictedDisplayTime, &gripSurfaceLocation));

                        // grip is valid, which means grip surface should be valid too as a static offset
                        REQUIRE((gripSurfaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT &&
                                 gripSurfaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT));

                        const float epsilon = 0.0001f;
                        XrVector3f position = gripSurfaceLocation.pose.position;

                        // The following test test the offsets of grip surface to grip for common controllers.
                        // Special configurations such as "fist grips", backhanded grips, push daggers, pens, etc. will require modifications to these tests or a waiver.

                        if (i == 0) {
                            // For tracked hands: grip surface may be arbitrarily close to grip pose
                            // For controllers: grip surface must be to the left of the grip pose, which is inside the controller.
                            REQUIRE(position.x <= epsilon);
                        }
                        else {
                            // For tracked hands: grip surface may be arbitrarily close to grip pose
                            // For controllers: grip surface must be to the right of the grip pose, which is inside the controller.
                            REQUIRE(position.x >= -epsilon);
                        }

                        {
                            // Grip: +X axis: When you completely open your hand to form a flat 5-finger pose, the ray that is normal to the user's palm (away from the palm in the left hand, into the palm in the right hand).
                            // Grip Surface: +X axis: When a user is holding the controller and straightens their index fingers pointing forward, the ray that is normal (perpendicular) to the user's palm (away from the palm in the left hand, into the palm in the right hand).
                            // In other words, the x axis is normal to the palm for both poses and should "roughly" point in the same direction (away from the palm in the left hand, into the palm in the right hand).
                            XrVector3f xAxis = {1, 0, 0};
                            XrVector3f gripSurfaceXDirection{};
                            XrQuaternionf_RotateVector3f(&gripSurfaceXDirection, &gripSurfaceLocation.pose.orientation, &xAxis);
                            double xAngleDiff = angleDeg(xAxis, gripSurfaceXDirection);
                            REQUIRE(xAngleDiff < 45.0f);
                        }

                        {
                            // Grip: -Z axis: When you close your hand partially (as if holding the controller), the ray that goes through the center of the tube formed by your non-thumb fingers, in the direction of little finger to thumb.
                            // Grip Surface: -Z axis: When a user is holding the controller and straightens their index finger, the ray that is parallel to their finger's pointing direction.
                            // In other words, the wrist (according to the grip surface pose) should not be tilted more than 90° away from the grip pose's z axis, i.e. the controller handle.
                            XrVector3f zAxis = {0, 0, 1};
                            XrVector3f gripSurfaceZDirection{};
                            XrQuaternionf_RotateVector3f(&gripSurfaceZDirection, &gripSurfaceLocation.pose.orientation, &zAxis);
                            double zAngleDiff = angleDeg(zAxis, gripSurfaceZDirection);
                            REQUIRE(std::abs(zAngleDiff) < 90.0f);
                        }

                        {
                            // Grip: +Y axis: orthogonal to +Z and +X using the right-hand rule.
                            // Grip Surface: +Y axis: orthogonal to +Z and +X using the right-hand rule.
                            // When the hand grips a cylindrical controller handle, the grip surface y axis pointing from the palm center "up" to the thumb should align roughly with the controller handle's forward (z = -1) axis.
                            XrVector3f yAxis = {0, 1, 0};
                            XrVector3f zMAxis = {0, 0, -1};
                            XrVector3f gripSurfaceYDirection{};
                            XrQuaternionf_RotateVector3f(&gripSurfaceYDirection, &gripSurfaceLocation.pose.orientation, &yAxis);
                            double yAngleDiff = angleDeg(zMAxis, gripSurfaceYDirection);
                            REQUIRE(std::abs(yAngleDiff) < 45.0f);
                        }

                        if (i == 0) {
                            // Test that the z axis (direction from the palm center to the wrist) of grip surface points "to the left" in grip space.
                            // This should be true for all usual controllers. If this is not true for your controller, you may need to adapt or discard this test.
                            XrVector3f zAxis = {0, 0, 1};
                            XrVector3f gripSurfaceZDirection{};
                            XrQuaternionf_RotateVector3f(&gripSurfaceZDirection, &gripSurfaceLocation.pose.orientation, &zAxis);
                            REQUIRE(gripSurfaceZDirection.x < 0);
                        }
                        else {
                            // Test that the z axis (direction from the palm center to the wrist) of grip surface points "to the right" in grip space.
                            // This should be true for all usual controllers. If this is not true for your controller, you may need to adapt or discard this test.
                            XrVector3f zAxis = {0, 0, 1};
                            XrVector3f gripSurfaceZDirection{};
                            XrQuaternionf_RotateVector3f(&gripSurfaceZDirection, &gripSurfaceLocation.pose.orientation, &zAxis);
                            REQUIRE(gripSurfaceZDirection.x > 0);
                        }

                        {
                            // Test that the z axis (direction from the palm center to the wrist) of grip surface points "upwards" in grip space, meaning that the controller cylinder is grabbed with the wrist angled towards the user and not somehow away.
                            // This should be true for all usual controllers. If this is not true for your controller, you may need to adapt or discard this test.
                            XrVector3f zAxis = {0, 0, 1};
                            XrVector3f gripSurfaceZDirection{};
                            XrQuaternionf_RotateVector3f(&gripSurfaceZDirection, &gripSurfaceLocation.pose.orientation, &zAxis);
                            REQUIRE(gripSurfaceZDirection.y > 0);
                        }

                        maxTestStates[i] = std::max(maxTestStates[i], TestState::Passed);
                    }
                    return (maxTestStates[0] == TestState::Passed || !globalData.leftHandUnderTest) &&
                           (maxTestStates[1] == TestState::Passed || !globalData.rightHandUnderTest);
                },
                30s, 50ms);
            auto testStateMessage = [](TestState state, bool testExtension) -> std::string {
                switch (state) {
                case TestState::Untested:
                    return "was not tested";
                case TestState::Inactive:
                    return std::string(testExtension ? "palm pose" : "grip surface") +
                           " was never observed as active at the same time as grip";
                case TestState::Invalid:
                    return "grip was never observed with a fully valid pose";
                case TestState::Passed:
                    return "passed";
                default:
                    FAIL("unexpected enum value " << static_cast<int64_t>(state));
                    return "test state was [unexpected enum value]";
                };
            };
            INFO("left hand " << testStateMessage(maxTestStates[0], testExtension));
            INFO("right hand " << testStateMessage(maxTestStates[1], testExtension));
            REQUIRE(testPassed);
        }
    }  // namespace

    // TODO make these use the specified interaction profile rather than simple controller?
    // TODO is [scenario] the best sub-category of [interactive] for this test?
    TEST_CASE("XR_EXT_palm_pose", "[XR_EXT_palm_pose][scenario][interactive][no_auto]")
    {
        SharedGripSurface(kExtensionRequirements);
    }

    // Purpose: Ensure that the action space for grip_surface can be used for placing a hand representation.
    TEST_CASE("GripSurface", "[XR_VERSION_1_1][scenario][interactive][no_auto]")
    {
        SharedGripSurface(kPromotedCoreRequirements);
    }

    // These two "objective" tests automatically evaluate their results, but because they require controllers,
    // they are marked as "interactive", and they currently lack conformance automation support

    // TODO make these use the specified interaction profile rather than simple controller?
    TEST_CASE("XR_EXT_palm_pose-objective", "[XR_EXT_palm_pose][actions][interactive][no_auto]")
    {
        SharedGripSurfaceAutomated(kExtensionRequirements);
    }

    TEST_CASE("GripSurface-objective", "[XR_VERSION_1_1][actions][interactive][no_auto]")
    {
        SharedGripSurfaceAutomated(kPromotedCoreRequirements);
    }
}  // namespace Conformance
