// Copyright (c) 2019-2023, The Khronos Group Inc.
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

    // Purpose: Verify behavior of action timing and action space linear/angular velocity through throwing
    // 1. Use action state changed timestamp to query velocities
    // 2. Use action space velocities at various rigid offsets to verify "lever arm" effect is computed by runtime.
    TEST_CASE("Interactive Throw", "[scenario][interactive][no_auto]")
    {
        const char* instructions =
            "Press and hold 'select' to spawn three rigidly-attached cubes to that controller. "
            "Release 'select' to throw the three cubes. "
            "The cubes should fly in the same direction as your controller motion and should feel natural. "
            "The rotation of the thrown cubes should match that of the controller. "
            "The velocity should match the lever-arm effect of the controller. "
            "Hit the three target cubes to complete the test. Press the menu button to fail the test. ";

        CompositionHelper compositionHelper("Interactive Throw");

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

        const std::vector<XrPath> subactionPaths{StringToPath(compositionHelper.GetInstance(), "/user/hand/left"),
                                                 StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};

        XrActionSet actionSet;
        XrAction throwAction, failAction, gripPoseAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "interaction_test");
            strcpy(actionSetInfo.localizedActionSetName, "Interaction Test");
            XRC_CHECK_THROW_XRCMD(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetInfo, &actionSet));

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionInfo.actionName, "complete_test");
            strcpy(actionInfo.localizedActionName, "Complete test");
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &failAction));

            // Remainder of actions use subaction.
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionInfo.actionName, "throw");
            strcpy(actionInfo.localizedActionName, "Throw");
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &throwAction));

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.actionName, "grip_pose");
            strcpy(actionInfo.localizedActionName, "Grip pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));
        }

        const std::vector<XrActionSuggestedBinding> bindings = {
            {throwAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
            {throwAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
            {failAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click")},
            {failAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click")},
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

        // Create the instructional quad layer placed to the left.
        XrCompositionLayerQuad* const instructionsQuad =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 768, instructions, 48)),
                                              localSpace, 1, {{0, 0, 0, 1}, {-1.5f, 0, -0.3f}});
        XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &Up, 70 * MATH_PI / 180);

        // Spaces attached to the hand (subaction).
        struct HandThrowSpaces
        {
            XrPath subactionPath;
            std::vector<XrSpace> spaces;
        };
        std::vector<HandThrowSpaces> throwSpaces;

        // Create XrSpaces at various spaces around the grip poses.
        for (XrPath subactionPath : subactionPaths) {
            HandThrowSpaces handThrowSpaces;
            handThrowSpaces.subactionPath = subactionPath;
            for (float meterDistance : {0.0f, 0.25f, 0.5f}) {
                XrSpace handSpace;
                XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                spaceCreateInfo.action = gripPoseAction;
                spaceCreateInfo.subactionPath = subactionPath;
                spaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, -meterDistance}};
                XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &handSpace));
                handThrowSpaces.spaces.push_back(handSpace);
            }
            throwSpaces.push_back(std::move(handThrowSpaces));
        }

        struct ThrownCube
        {
            XrSpaceVelocity velocity;  // Velocity of space that was captured when a throw happened.
            XrPosef pose;
            XrTime updateTime;
            XrTime createTime;
        };
        std::vector<ThrownCube> thrownCubes;

        // Three fixed cubes which must be reached by the thrown cubes to pass the test.
        std::vector<XrVector3f> targetCubes{{-1, -1, -3.0f}, {1, -1, -4.0f}, {0, 1.0f, -5.0f}};

        constexpr XrVector3f gnomonScale{0.025f, 0.025f, 0.025f};
        constexpr XrVector3f inactiveCubeScale{0.05f, 0.05f, 0.05f};
        constexpr XrVector3f activateCubeScale{0.1f, 0.1f, 0.1f};
        constexpr XrVector3f targetCubeScale{0.2f, 0.2f, 0.2f};
        constexpr float targetCubeHitThreshold = 0.25f;

        MeshHandle gnomonMesh = GetGlobalData().graphicsPlugin->MakeGnomonMesh();

        auto update = [&](const XrFrameState& frameState) {
            std::vector<Cube> cubes;
            std::vector<MeshDrawable> meshes;

            const std::array<XrActiveActionSet, 1> activeActionSets = {{{actionSet, XR_NULL_PATH}}};
            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.activeActionSets = activeActionSets.data();
            syncInfo.countActiveActionSets = (uint32_t)activeActionSets.size();
            XRC_CHECK_THROW_XRCMD(xrSyncActions(compositionHelper.GetSession(), &syncInfo));

            // Check if user has requested to fail the test.
            {
                XrActionStateGetInfo completeActionGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                completeActionGetInfo.action = failAction;
                XrActionStateBoolean completeActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XRC_CHECK_THROW_XRCMD(
                    xrGetActionStateBoolean(compositionHelper.GetSession(), &completeActionGetInfo, &completeActionState));
                if (completeActionState.currentState == XR_TRUE && completeActionState.changedSinceLastSync) {
                    return false;
                }
            }

            // Remove thrown cubes older than 3s.
            for (auto it = thrownCubes.begin(); it != thrownCubes.end();) {
                const XrDuration age = frameState.predictedDisplayTime - it->createTime;
                if (age > XrDuration(3.0e9)) {
                    it = thrownCubes.erase(it);
                }
                else
                    ++it;
            }

            // Apply velocities to thrown cubes.
            auto simulateThrownCubeAtTime = [](ThrownCube& thrownCube, XrTime predictedDisplayTime) {
                const XrDuration timeSinceLastTick = predictedDisplayTime - thrownCube.updateTime;
                CHECK_MSG(timeSinceLastTick > 0, "Unexpected old frame state predictedDisplayTime or future action state lastChangeTime");
                thrownCube.updateTime = predictedDisplayTime;

                const float secondSinceLastTick = timeSinceLastTick / (float)1'000'000'000;

                // Apply gravity to velocity.
                thrownCube.velocity.linearVelocity.y += -9.8f * secondSinceLastTick;

                // Apply velocity to position.
                XrVector3f deltaVelocity;
                XrVector3f_Scale(&deltaVelocity, &thrownCube.velocity.linearVelocity, secondSinceLastTick);
                XrVector3f_Add(&thrownCube.pose.position, &thrownCube.pose.position, &deltaVelocity);

                // Convert angular velocity to quaternion with the appropriate amount of rotation for the delta time.
                XrQuaternionf angularRotation;
                {
                    const float radiansPerSecond = XrVector3f_Length(&thrownCube.velocity.angularVelocity);
                    XrVector3f angularAxis = thrownCube.velocity.angularVelocity;
                    XrVector3f_Normalize(&angularAxis);
                    XrQuaternionf_CreateFromAxisAngle(&angularRotation, &angularAxis, radiansPerSecond * secondSinceLastTick);
                }

                // Update the orientation given the computed angular rotation.
                XrQuaternionf newOrientation;
                XrQuaternionf_Multiply(&newOrientation, &thrownCube.pose.orientation, &angularRotation);
                thrownCube.pose.orientation = newOrientation;
            };

            for (ThrownCube& thrownCube : thrownCubes) {
                simulateThrownCubeAtTime(thrownCube, frameState.predictedDisplayTime);
                cubes.push_back({thrownCube.pose, activateCubeScale});

                // Remove any target cubes which are hit by the thrown cube.
                for (auto it = targetCubes.begin(); it != targetCubes.end();) {
                    XrVector3f delta;
                    XrVector3f_Sub(&delta, &(*it), &thrownCube.pose.position);
                    if (XrVector3f_Length(&delta) < targetCubeHitThreshold) {
                        it = targetCubes.erase(it);
                    }
                    else {
                        it++;
                    }
                }
            }

            // Once all the targets have been hit and removed, the test is a pass.
            if (targetCubes.empty()) {
                return false;
            }

            // Add the targets.
            for (const XrVector3f& targetCubePosition : targetCubes) {
                cubes.push_back({{{0, 0, 0, 1}, targetCubePosition}, targetCubeScale});
            }

            // Locate throw spaces and add as cubes. Spawn thrown cubes when select released.
            {
                for (auto& subactionSpaces : throwSpaces) {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = throwAction;
                    getInfo.subactionPath = subactionSpaces.subactionPath;
                    XrActionStateBoolean boolState{XR_TYPE_ACTION_STATE_BOOLEAN};
                    XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &boolState));

                    for (XrSpace throwSpace : subactionSpaces.spaces) {
                        XrSpaceVelocity spaceVelocity{XR_TYPE_SPACE_VELOCITY};
                        XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION, &spaceVelocity};
                        XRC_CHECK_THROW_XRCMD(xrLocateSpace(throwSpace, localSpace, frameState.predictedDisplayTime, &spaceLocation));
                        if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                            cubes.push_back(Cube{spaceLocation.pose, boolState.currentState ? activateCubeScale : inactiveCubeScale});

                            // Draw an instantaneous indication of the linear & angular velocity
                            if (spaceVelocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
                                auto gnomonTime = frameState.predictedDisplayTime;
                                ThrownCube gnomon{spaceVelocity, spaceLocation.pose, gnomonTime, gnomonTime};
                                for (int step = 1; step < 20; ++step) {
                                    auto predictedDisplayTimeAtStep =
                                        frameState.predictedDisplayTime + step * frameState.predictedDisplayPeriod;
                                    simulateThrownCubeAtTime(gnomon, predictedDisplayTimeAtStep);
                                    meshes.push_back(MeshDrawable{gnomonMesh, gnomon.pose, gnomonScale});
                                }
                            }

                            // Detect release of throw action.
                            if (boolState.changedSinceLastSync && boolState.currentState == XR_FALSE) {
                                // Locate again, but this time use the action transition timestamp and also get the velocity.
                                XrSpaceVelocity releaseSpaceVelocity{XR_TYPE_SPACE_VELOCITY};
                                XrSpaceLocation releaseSpaceLocation{XR_TYPE_SPACE_LOCATION, &releaseSpaceVelocity};
                                XRC_CHECK_THROW_XRCMD(
                                    xrLocateSpace(throwSpace, localSpace, boolState.lastChangeTime, &releaseSpaceLocation));
                                if (releaseSpaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT &&
                                    releaseSpaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT &&
                                    releaseSpaceVelocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT &&
                                    releaseSpaceVelocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
                                    thrownCubes.emplace_back(ThrownCube{releaseSpaceVelocity, releaseSpaceLocation.pose,
                                                                        boolState.lastChangeTime, boolState.lastChangeTime});
                                }
                            }
                        }
                    }
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
                    compositionHelper.AcquireWaitReleaseImage(swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                        const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                        const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                        GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage,
                                                                   RenderParams().Draw(cubes).Draw(meshes));
                    });
                }

                layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
            }

            layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});

            compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

            return compositionHelper.PollEvents();
        };

        RenderLoop(compositionHelper.GetSession(), update).Loop();

        // The render loop will end if the user hits and removes all three target cubes or if the user presses menu.
        if (!targetCubes.empty()) {
            FAIL("User has failed the test");
        }
    }
}  // namespace Conformance
