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

#include "catch2/catch_approx.hpp"
#include "catch2/catch_message.hpp"
#include "common/xr_linear.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "utilities/ballistics.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <array>
#include <deque>
#include <utility>
#include <vector>
#include <sstream>

using namespace Conformance;

namespace Conformance
{
    constexpr XrVector3f Up{0, 1, 0};

    // Calculate the correct XrSpaceVelocity for a space which is rigidly attached to another space via a known pose offset
    XrSpaceVelocity adjustVelocitiesForPose(XrSpaceLocation locationWithoutOffset, XrSpaceVelocity velocityWithoutOffset,
                                            XrPosef relativePose)
    {
        XrSpaceVelocity adjustedVelocity = {XR_TYPE_SPACE_VELOCITY};

        if (velocityWithoutOffset.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
            adjustedVelocity.velocityFlags |= XR_SPACE_VELOCITY_LINEAR_VALID_BIT;
            XrVector3f_Add(&adjustedVelocity.linearVelocity, &adjustedVelocity.linearVelocity, &velocityWithoutOffset.linearVelocity);
        }

        if (velocityWithoutOffset.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
            adjustedVelocity.velocityFlags |= XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;
            // Can't easily add angular velocities, and there's only one, so apply directly to result.
            adjustedVelocity.angularVelocity = velocityWithoutOffset.angularVelocity;
        }

        if (velocityWithoutOffset.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
            adjustedVelocity.velocityFlags |= XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;
            adjustedVelocity.velocityFlags |= XR_SPACE_VELOCITY_LINEAR_VALID_BIT;
            XrVector3f leverArmVelocity;

            XrVector3f leverArmInBaseSpace;
            XrQuaternionf_RotateVector3f(&leverArmInBaseSpace, &locationWithoutOffset.pose.orientation, &relativePose.position);

            XrVector3f_Cross(&leverArmVelocity, &velocityWithoutOffset.angularVelocity, &leverArmInBaseSpace);
            XrVector3f_Add(&adjustedVelocity.linearVelocity, &adjustedVelocity.linearVelocity, &leverArmVelocity);
        }

        // velocities are in base space reference frame, so they do not need to be rotated based on the pose of the space

        return adjustedVelocity;
    };

    // Purpose: Verify that the linear and angular velocities returned by the runtime are self-consistent,
    // and that spaces offset from pose actions display correct behavior with pose and velocities.
    TEST_CASE("SpaceOffsets", "[scenario][interactive][no_auto]")
    {
        const char* instructions =
            "Wave the controller(s) around. To freeze time, press [select]."
            " The red-tint gnomons (runtime-reported velocities) should match"
            " the green-tint gnomons (calculated by the CTS).\n\n"
            "The test will automatically pass when the following criteria are met:";
        // the remainder of the instructions are populated based on `criteria`.

        const char* failureInstructions =
            "The test has failed. The failing state is shown frozen in time."
            " For debugging, you may press [select] to un-freeze time until another failure is detected."
            " Press [menu] when you are ready to end the test.\n\n"
            "The paths of the space pose that exceeded the failure thresholds are not greyed out:"
            " The red/green/blue gnomons are past poses. The red and cyan tinted trails"
            " are future poses based on the runtime-provided and CTS-calculated velocities"
            " respectively. Failure here suggests that either your reported angular velocities"
            " or your velocity calculations for offset spaces are incorrect.";

        struct SuccessCriterion
        {
            const char* description;
            XrVector3f linearVelocityComponent;
            float linearVelocityMagnitude;
            XrVector3f angularVelocityComponent;
            float angularVelocityMagnitude;
            bool satisfied;
        };

        SuccessCriterion criteria[] = {
            {"X linear velocity", {1.0f, 0.0f, 0.0f}, 0.5f, {}, 0},   //
            {"Y linear velocity", {0.0f, 1.0f, 0.0f}, 0.5f, {}, 0},   //
            {"Z linear velocity", {0.0f, 0.0f, 1.0f}, 0.5f, {}, 0},   //
            {"X angular velocity", {}, 0, {1.0f, 0.0f, 0.0f}, 6.0f},  //
            {"Y angular velocity", {}, 0, {0.0f, 1.0f, 0.0f}, 6.0f},  //
            {"Z angular velocity", {}, 0, {0.0f, 0.0f, 1.0f}, 6.0f},  //
        };

        CompositionHelper compositionHelper("Space Offsets");

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
                swapchains.emplace_back(swapchain);
            }
        }

        const std::vector<XrPath> subactionPaths{StringToPath(compositionHelper.GetInstance(), "/user/hand/left"),
                                                 StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};
        XrActionSet actionSet;
        XrAction freezeAction, failAction, gripPoseAction;
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
            strcpy(actionInfo.actionName, "freeze");
            strcpy(actionInfo.localizedActionName, "Freeze time");
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &freezeAction));

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.actionName, "grip_pose");
            strcpy(actionInfo.localizedActionName, "Grip pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));
        }

        const std::vector<XrActionSuggestedBinding> bindings = {
            {freezeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
            {freezeAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
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

        XrCompositionLayerQuad* instructionsQuad = nullptr;
        auto updateInstructions = [&](bool failed) {
            if (instructionsQuad != nullptr && instructionsQuad->subImage.swapchain != XR_NULL_HANDLE) {
                compositionHelper.DestroySwapchain(instructionsQuad->subImage.swapchain);
            }
            std::ostringstream oss;
            if (failed) {
                oss << failureInstructions << '\n';
            }
            else {
                oss << instructions << '\n';
                for (const SuccessCriterion& criterion : criteria) {
                    oss << "[" << (criterion.satisfied ? "x" : " ") << "] " << criterion.description << '\n';
                }
            }
            instructionsQuad = compositionHelper.CreateQuadLayer(
                compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 780, oss.str().c_str(), 48)), localSpace, 1,
                {Quat::Identity, {-1.5f, 0, -0.3f}});
            XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &Up, 70 * MATH_PI / 180);
        };
        updateInstructions(false);

        // Spaces attached to the hand (subaction).
        struct HandSpace
        {
            XrPosef poseInActionSpace;
            XrSpace space;
            std::deque<XrPosef> pastPosesInLocalSpace;
            XrSpaceLocation lastReportedLocation{XR_TYPE_SPACE_LOCATION};
            XrSpaceVelocity lastReportedVelocity{XR_TYPE_SPACE_VELOCITY};
            XrSpaceLocation lastPredictedLocation{XR_TYPE_SPACE_LOCATION};
            XrSpaceVelocity lastPredictedVelocity{XR_TYPE_SPACE_VELOCITY};

            bool failed;  // for visualisation only

            HandSpace(XrPosef poseInActionSpace, XrSpace space) : poseInActionSpace(poseInActionSpace), space(space){};
        };

        // Spaces attached to the hand (subaction).
        struct HandSpaces
        {
            XrPath subactionPath;
            XrSpace spaceWithoutOffset;
            std::vector<HandSpace> spaces;
        };
        std::vector<HandSpaces> spaces;

        // Create XrSpaces at various spaces around the grip poses.

        XrPosef handRelativePoses[] = {
            XrPosefCPP(),
            {Quat::FromAxisAngle({1, 0, 0}, Math::DegToRad(135)), {0, 0, 0}},
            {Quat::FromAxisAngle({1, 0, 0}, Math::DegToRad(45)), {0.25, 0, 0}},
            {Quat::FromAxisAngle({1, 0, 0}, Math::DegToRad(45)), {-0.25, 0, 0}},
            {Quat::FromAxisAngle({1, 0, 0}, Math::DegToRad(30)), {0, 0, -0.25}},
            {Quat::Identity, {0, 0, -0.5}},
            {Quat::FromAxisAngle({1, 1, 1}, Math::DegToRad(127)), {-0.25, 0, -0.5}},
            {Quat::FromAxisAngle({1, -1, -1}, Math::DegToRad(38)), {0.25, 0, -0.5}},
        };

        for (XrPath subactionPath : subactionPaths) {
            HandSpaces handSpaces;
            handSpaces.subactionPath = subactionPath;

            XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            spaceCreateInfo.action = gripPoseAction;
            spaceCreateInfo.subactionPath = subactionPath;

            spaceCreateInfo.poseInActionSpace = XrPosefCPP();
            XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &handSpaces.spaceWithoutOffset));

            for (XrPosef pose : handRelativePoses) {
                spaceCreateInfo.poseInActionSpace = pose;
                XrSpace handSpace;
                XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &handSpace));
                handSpaces.spaces.emplace_back(pose, handSpace);
            }
            spaces.emplace_back(std::move(handSpaces));
        }

        constexpr XrVector3f gnomonScale{0.025f, 0.025f, 0.025f};
        constexpr XrColor4f reportedGnomonTint{1.0f, 0.0f, 0.0f, 0.5f};
        constexpr XrColor4f predictedGnomonTint{0.0f, 1.0f, 0.0f, 0.5f};
        constexpr XrVector3f liveCubeScale{0.05f, 0.05f, 0.05f};

        MeshHandle pastGnomonMesh = GetGlobalData().graphicsPlugin->MakeGnomonMesh(1.0f, 0.1f);
        MeshHandle predictedGnomonMesh = GetGlobalData().graphicsPlugin->MakeGnomonMesh(0.9f, 0.1f);
        MeshHandle reportedGnomonMesh = GetGlobalData().graphicsPlugin->MakeGnomonMesh(1.0f, 0.08f);

        bool testFailed = false;
        bool frozen = false;
        bool postFailureUnfreeze = false;
        bool updatedSinceLastFailure = false;

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
                    testFailed = true;
                    return false;
                }
            }

            XrActionStateGetInfo freezeActionGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            freezeActionGetInfo.action = freezeAction;
            XrActionStateBoolean freezeActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
            XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(compositionHelper.GetSession(), &freezeActionGetInfo, &freezeActionState));

            if (testFailed) {
                postFailureUnfreeze = freezeActionState.currentState;
                if (freezeActionState.currentState) {
                    frozen = false;
                }
            }
            else if (freezeActionState.changedSinceLastSync) {
                frozen = freezeActionState.currentState;
            }

            if (!frozen) {
                // Locate space without offset and each offset space. Calculate linear and angular velocities based on moment arm,
                // and check that runtime-provided values are close to the ones we calculated ourselves.
                bool frameFailed = false;
                updatedSinceLastFailure = true;

                for (auto& subactionSpaces : spaces) {
                    for (HandSpace& space : subactionSpaces.spaces) {
                        space.failed = false;
                        CAPTURE(space.poseInActionSpace);

                        // xrLocateSpace on the base space every time to get up to date values
                        XrSpaceVelocity velocityWithoutOffset[2] = {{XR_TYPE_SPACE_VELOCITY}, {XR_TYPE_SPACE_VELOCITY}};
                        XrSpaceLocation locationWithoutOffset[2] = {{XR_TYPE_SPACE_LOCATION, &velocityWithoutOffset[0]},
                                                                    {XR_TYPE_SPACE_LOCATION, &velocityWithoutOffset[1]}};
                        XRC_CHECK_THROW_XRCMD(xrLocateSpace(subactionSpaces.spaceWithoutOffset, localSpace, frameState.predictedDisplayTime,
                                                            &locationWithoutOffset[0]));

                        XrSpaceVelocity spaceVelocity{XR_TYPE_SPACE_VELOCITY};
                        XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION, &spaceVelocity};
                        XRC_CHECK_THROW_XRCMD(xrLocateSpace(space.space, localSpace, frameState.predictedDisplayTime, &spaceLocation));

                        XRC_CHECK_THROW_XRCMD(xrLocateSpace(subactionSpaces.spaceWithoutOffset, localSpace, frameState.predictedDisplayTime,
                                                            &locationWithoutOffset[1]));

                        // run the checks once to see if the space fails with both no-offset locates.
                        bool dryRun = true;
                        bool failed[2] = {false, false};
                        const char* withoutOffsetWasCalled[2] = {"before", "after"};
                        for (int i = 0; i < 2; ++i) {
                            CAPTURE(withoutOffsetWasCalled[i]);
                            space.lastReportedLocation = spaceLocation;
                            space.lastReportedVelocity = spaceVelocity;
                            if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                                if (space.pastPosesInLocalSpace.size() >= 8) {
                                    space.pastPosesInLocalSpace.pop_back();
                                }
                                space.pastPosesInLocalSpace.push_front(spaceLocation.pose);
                            }

                            XrSpaceVelocity predictedVelocity =
                                adjustVelocitiesForPose(locationWithoutOffset[i], velocityWithoutOffset[i], space.poseInActionSpace);

                            if (locationWithoutOffset[i].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                                space.lastPredictedLocation = locationWithoutOffset[i];
                                XrPosef_Multiply(&space.lastPredictedLocation.pose, &locationWithoutOffset[i].pose,
                                                 &space.poseInActionSpace);

                                space.lastPredictedVelocity = predictedVelocity;
                            }

                            CAPTURE(locationWithoutOffset[i].pose);
                            CAPTURE(space.lastPredictedLocation.pose);
                            CAPTURE(spaceLocation.pose);

                            CAPTURE(velocityWithoutOffset[i].linearVelocity);
                            CAPTURE(predictedVelocity.linearVelocity);
                            CAPTURE(spaceVelocity.linearVelocity);
                            CAPTURE(velocityWithoutOffset[i].angularVelocity);
                            CAPTURE(predictedVelocity.angularVelocity);
                            CAPTURE(spaceVelocity.angularVelocity);

                            CAPTURE(XrVector3f_Length(&spaceVelocity.linearVelocity));
                            CAPTURE(XrVector3f_Length(&predictedVelocity.linearVelocity));

                            XrVector3f predictedLeverArmVelocity;
                            XrVector3f reportedLeverArmVelocity;
                            XrVector3f_Sub(&predictedLeverArmVelocity,  //
                                           &predictedVelocity.linearVelocity, &velocityWithoutOffset[i].linearVelocity);
                            XrVector3f_Sub(&reportedLeverArmVelocity,  //
                                           &spaceVelocity.linearVelocity, &velocityWithoutOffset[i].linearVelocity);

                            CAPTURE(XrVector3f_Length(&predictedLeverArmVelocity));
                            CAPTURE(XrVector3f_Length(&reportedLeverArmVelocity));

                            CAPTURE(XrVector3f_Length(&spaceVelocity.angularVelocity));
                            CAPTURE(XrVector3f_Length(&predictedVelocity.angularVelocity));

                            bool anyVelocityInvalid = ~(velocityWithoutOffset[i].velocityFlags & spaceVelocity.velocityFlags) &
                                                      (XR_SPACE_VELOCITY_ANGULAR_VALID_BIT | XR_SPACE_VELOCITY_LINEAR_VALID_BIT);
                            if (!anyVelocityInvalid) {
#define CHECK_WITH_SET_FAILED(...)         \
    failed[i] |= !(__VA_ARGS__);           \
    if (!dryRun && !postFailureUnfreeze) { \
        CHECK(__VA_ARGS__);                \
    }
                                // constants subject to adjustment based on errors found in correct runtimes
                                {
                                    constexpr float pm = 0.01f;  // 10mm
                                    constexpr float pe = 0.1f;   // 10% error is always tolerated
                                    CHECK_WITH_SET_FAILED(
                                        spaceLocation.pose.position.x ==
                                        Catch::Approx(space.lastPredictedLocation.pose.position.x).margin(pm).epsilon(pe));
                                    CHECK_WITH_SET_FAILED(
                                        spaceLocation.pose.position.y ==
                                        Catch::Approx(space.lastPredictedLocation.pose.position.y).margin(pm).epsilon(pe));
                                    CHECK_WITH_SET_FAILED(
                                        spaceLocation.pose.position.z ==
                                        Catch::Approx(space.lastPredictedLocation.pose.position.z).margin(pm).epsilon(pe));

                                    constexpr float rm = 0.05f;  // five percentiles
                                    constexpr float re = 0.1f;   // 10% error is always tolerated
                                    // Quaternions that have the same value but opposite sign on all components are considered equal.
                                    // This does prevent the assertion from having a nice message, but relevant data is CAPTUREd above.
                                    CHECK_WITH_SET_FAILED(
                                        ((spaceLocation.pose.orientation.x ==
                                              Catch::Approx(space.lastPredictedLocation.pose.orientation.x).margin(rm).epsilon(re) &&
                                          spaceLocation.pose.orientation.y ==
                                              Catch::Approx(space.lastPredictedLocation.pose.orientation.y).margin(rm).epsilon(re) &&
                                          spaceLocation.pose.orientation.z ==
                                              Catch::Approx(space.lastPredictedLocation.pose.orientation.z).margin(rm).epsilon(re) &&
                                          spaceLocation.pose.orientation.w ==
                                              Catch::Approx(space.lastPredictedLocation.pose.orientation.w).margin(rm).epsilon(re)) ||
                                         (spaceLocation.pose.orientation.x ==
                                              Catch::Approx(-space.lastPredictedLocation.pose.orientation.x).margin(rm).epsilon(re) &&
                                          spaceLocation.pose.orientation.y ==
                                              Catch::Approx(-space.lastPredictedLocation.pose.orientation.y).margin(rm).epsilon(re) &&
                                          spaceLocation.pose.orientation.z ==
                                              Catch::Approx(-space.lastPredictedLocation.pose.orientation.z).margin(rm).epsilon(re) &&
                                          spaceLocation.pose.orientation.w ==
                                              Catch::Approx(-space.lastPredictedLocation.pose.orientation.w).margin(rm).epsilon(re))));
                                }
                                {
                                    constexpr float am = 0.1f;  // 0.1 radians/sec
                                    constexpr float ae = 0.1f;  // 10% error is always tolerated
                                    CHECK_WITH_SET_FAILED(spaceVelocity.angularVelocity.x ==
                                                          Catch::Approx(predictedVelocity.angularVelocity.x).margin(am).epsilon(ae));
                                    CHECK_WITH_SET_FAILED(spaceVelocity.angularVelocity.y ==
                                                          Catch::Approx(predictedVelocity.angularVelocity.y).margin(am).epsilon(ae));
                                    CHECK_WITH_SET_FAILED(spaceVelocity.angularVelocity.z ==
                                                          Catch::Approx(predictedVelocity.angularVelocity.z).margin(am).epsilon(ae));
                                }
                                {
                                    float angularSpeed = XrVector3f_Length(&spaceVelocity.angularVelocity);

                                    float lm = 0.01f                    // 10 mm/s
                                               + angularSpeed * 0.20f;  // + lever arm speed at 20cm (~40% of lever arm effect at 50cm)
                                    constexpr float le = 0.1f;          // 10% error is always tolerated
                                    CHECK_WITH_SET_FAILED(spaceVelocity.linearVelocity.x ==
                                                          Catch::Approx(predictedVelocity.linearVelocity.x).margin(lm).epsilon(le));
                                    CHECK_WITH_SET_FAILED(spaceVelocity.linearVelocity.y ==
                                                          Catch::Approx(predictedVelocity.linearVelocity.y).margin(lm).epsilon(le));
                                    CHECK_WITH_SET_FAILED(spaceVelocity.linearVelocity.z ==
                                                          Catch::Approx(predictedVelocity.linearVelocity.z).margin(lm).epsilon(le));
                                }
#undef CHECK_WITH_SET_FAILED
                                // Only update criteria if predictions were successful, to be safe
                                if (!testFailed && !failed[i]) {
                                    for (SuccessCriterion& criterion : criteria) {
                                        if (criterion.satisfied) {
                                            continue;
                                        }
                                        float linearMagnitude = std::abs(
                                            XrVector3f_Dot(&velocityWithoutOffset->linearVelocity, &criterion.linearVelocityComponent));
                                        bool linearSatisfied = linearMagnitude >= criterion.linearVelocityMagnitude;
                                        float angularMagnitude = std::abs(
                                            XrVector3f_Dot(&velocityWithoutOffset->angularVelocity, &criterion.angularVelocityComponent));
                                        bool angularSatisfied = angularMagnitude >= criterion.angularVelocityMagnitude;
                                        if (linearSatisfied && angularSatisfied) {
                                            criterion.satisfied = true;

                                            bool allSatisfied = true;
                                            for (const SuccessCriterion& otherCriterion : criteria) {
                                                if (!otherCriterion.satisfied) {
                                                    allSatisfied = false;
                                                    break;
                                                }
                                            }
                                            if (allSatisfied) {
                                                // Test has completed successfully
                                                return false;
                                            }
                                            updateInstructions(testFailed);
                                        }
                                    }
                                }
                            }

                            // reset the loop, actually calling CHECK this time
                            if (dryRun && failed[0] && failed[1]) {
                                dryRun = false;
                                i = 0;
                                space.failed = true;
                                frameFailed = true;
                                if (!testFailed) {
                                    updateInstructions(true);
                                }
                                testFailed = true;
                            }
                        }

                        if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                            cubes.emplace_back(spaceLocation.pose, liveCubeScale);
                        }
                    }
                }

                if (frameFailed) {
                    frozen = true;
                    updatedSinceLastFailure = false;
                }
            }

            for (auto& subactionSpaces : spaces) {
                for (HandSpace& space : subactionSpaces.spaces) {
                    auto dimNonFailed = [&](XrColor4f tint = {0, 0, 0, 0}) {
                        if (testFailed && !updatedSinceLastFailure && !space.failed) {
                            return XrColor4f{0.3f, 0.3f, 0.3f, 0.9f};
                        }
                        return tint;
                    };
                    for (const auto& pastPose : space.pastPosesInLocalSpace) {
                        meshes.emplace_back(pastGnomonMesh, pastPose, gnomonScale, dimNonFailed());
                    }

                    struct predictionTrail
                    {
                        MeshHandle mesh;
                        XrSpaceLocation spaceLocation;
                        XrSpaceVelocity spaceVelocity;
                        XrColor4f tint;
                    };
                    predictionTrail trails[] = {
                        {reportedGnomonMesh, space.lastReportedLocation, space.lastReportedVelocity, reportedGnomonTint},
                        {predictedGnomonMesh, space.lastPredictedLocation, space.lastPredictedVelocity, predictedGnomonTint},
                    };
                    for (predictionTrail& trail : trails) {
                        // Draw an instantaneous indication of the linear & angular velocity
                        if (trail.spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT &&
                            trail.spaceVelocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
                            auto gnomonTime = frameState.predictedDisplayTime;
                            BodyInMotion gnomon{trail.spaceVelocity, trail.spaceLocation.pose, gnomonTime, gnomonTime};
                            for (int step = 1; step < 20; ++step) {
                                auto predictedDisplayTimeAtStep =
                                    frameState.predictedDisplayTime + step * frameState.predictedDisplayPeriod;
                                gnomon.doSimulationStep({0.f, 0.f, 0.f}, predictedDisplayTimeAtStep);
                                meshes.emplace_back(trail.mesh, gnomon.pose, gnomonScale, dimNonFailed(trail.tint));
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

                // Render into each of the separate swapchains using the projection layer view fov and pose.
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

        // The render loop will end if the user waves the controller or if the user presses menu.
        if (testFailed) {
            FAIL("User has failed the test");
        }
    }
}  // namespace Conformance
