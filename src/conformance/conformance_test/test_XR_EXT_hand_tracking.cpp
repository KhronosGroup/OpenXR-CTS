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

#include "conformance_framework.h"
#include "composition_utils.h"
#include "conformance_utils.h"
#include "utilities/system_properties_helper.h"
#include "utilities/utils.h"
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>
#include "common/xr_linear.h"

using namespace Conformance;

namespace Conformance
{
    constexpr XrVector3f Up{0, 1, 0};
    constexpr int LEFT_HAND = 0;
    constexpr int RIGHT_HAND = 1;
    constexpr int HAND_COUNT = 2;

    static const auto SystemSupportsHandTracking =
        MakeSystemPropertiesBoolChecker(XrSystemHandTrackingPropertiesEXT{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT},
                                        &XrSystemHandTrackingPropertiesEXT::supportsHandTracking);

    TEST_CASE("XR_EXT_hand_tracking-create-destroy", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
            // Runtime does not support extension - it should not be possible to get function pointers.
            AutoBasicInstance instance;
            ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreateHandTrackerEXT");
            SKIP(XR_EXT_HAND_TRACKING_EXTENSION_NAME " not supported");
        }

        SECTION("Extension not enabled")
        {
            if (!globalData.IsInstanceExtensionEnabled(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
                AutoBasicInstance instance;
                ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreateHandTrackerEXT");
            }
            else {
                WARN(XR_EXT_HAND_TRACKING_EXTENSION_NAME " force-enabled, cannot test behavior when extension is disabled.");
            }
        }

        SECTION("Create and Destroy trackers")
        {
            AutoBasicInstance instance({XR_EXT_HAND_TRACKING_EXTENSION_NAME}, AutoBasicInstance::createSystemId);

            auto xrCreateHandTrackerEXT = GetInstanceExtensionFunction<PFN_xrCreateHandTrackerEXT>(instance, "xrCreateHandTrackerEXT");
            auto xrDestroyHandTrackerEXT = GetInstanceExtensionFunction<PFN_xrDestroyHandTrackerEXT>(instance, "xrDestroyHandTrackerEXT");

            XrSystemId systemId = instance.systemId;

            AutoBasicSession session(AutoBasicSession::beginSession, instance);

            if (!SystemSupportsHandTracking(instance, systemId)) {
                // https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#_create_a_hand_tracker_handle
                // If the system does not support hand tracking, runtime must return XR_ERROR_FEATURE_UNSUPPORTED from xrCreateHandTrackerEXT.
                // In this case, the runtime must return XR_FALSE for supportsHandTracking in XrSystemHandTrackingPropertiesEXT when the
                // function xrGetSystemProperties is called, so that the application can avoid creating a hand tracker.

                for (size_t i = 0; i < HAND_COUNT; ++i) {
                    XrHandTrackerEXT tracker{XR_NULL_HANDLE};
                    XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                    createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                    createInfo.hand = (i == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);
                    REQUIRE(XR_ERROR_FEATURE_UNSUPPORTED == xrCreateHandTrackerEXT(session, &createInfo, &tracker));
                }
            }
            else {
                std::array<XrHandTrackerEXT, HAND_COUNT> handTracker;
                for (size_t i = 0; i < HAND_COUNT; ++i) {
                    XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                    createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                    createInfo.hand = (i == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);
                    REQUIRE(XR_SUCCESS == xrCreateHandTrackerEXT(session, &createInfo, &handTracker[i]));
                    REQUIRE(XR_SUCCESS == xrDestroyHandTrackerEXT(handTracker[i]));
                }
            }
        }
    }

    TEST_CASE("XR_EXT_hand_tracking-simple-queries")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
            SKIP(XR_EXT_HAND_TRACKING_EXTENSION_NAME " not supported");
        }

        AutoBasicInstance instance({XR_EXT_HAND_TRACKING_EXTENSION_NAME}, AutoBasicInstance::createSystemId);

        auto xrCreateHandTrackerEXT = GetInstanceExtensionFunction<PFN_xrCreateHandTrackerEXT>(instance, "xrCreateHandTrackerEXT");
        auto xrDestroyHandTrackerEXT = GetInstanceExtensionFunction<PFN_xrDestroyHandTrackerEXT>(instance, "xrDestroyHandTrackerEXT");
        auto xrLocateHandJointsEXT = GetInstanceExtensionFunction<PFN_xrLocateHandJointsEXT>(instance, "xrLocateHandJointsEXT");

        XrSystemId systemId = instance.systemId;
        if (!SystemSupportsHandTracking(instance, systemId)) {
            // This runtime does support hand tracking, but this headset does not
            // support hand tracking, which is fine.
            SKIP("System does not support hand tracking");
        }

        AutoBasicSession session(AutoBasicSession::beginSession | AutoBasicSession::createActions | AutoBasicSession::createSpaces |
                                     AutoBasicSession::createSwapchains,
                                 instance);

        std::array<XrHandTrackerEXT, HAND_COUNT> handTracker;
        for (size_t i = 0; i < HAND_COUNT; ++i) {
            XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
            createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
            createInfo.hand = (i == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);
            REQUIRE(XR_SUCCESS == xrCreateHandTrackerEXT(session, &createInfo, &handTracker[i]));
        }

        SECTION("Query joint locations")
        {
            XrSpace localSpace = XR_NULL_HANDLE;

            XrReferenceSpaceCreateInfo localSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            localSpaceCreateInfo.poseInReferenceSpace = XrPosefCPP();
            localSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            REQUIRE_RESULT(xrCreateReferenceSpace(session, &localSpaceCreateInfo, &localSpace), XR_SUCCESS);

            // Wait until the runtime is ready for us to begin a session
            auto timeout = (GetGlobalData().options.debugMode ? 3600s : 10s);
            FrameIterator frameIterator(&session);
            FrameIterator::RunResult runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_READY, timeout);
            REQUIRE(runResult == FrameIterator::RunResult::Success);

            for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
                std::array<std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT>, HAND_COUNT> jointLocations;
                std::array<std::array<XrHandJointVelocityEXT, XR_HAND_JOINT_COUNT_EXT>, HAND_COUNT> jointVelocities;

                XrHandJointVelocitiesEXT velocities{XR_TYPE_HAND_JOINT_VELOCITIES_EXT};
                velocities.jointCount = XR_HAND_JOINT_COUNT_EXT;
                velocities.jointVelocities = jointVelocities[hand].data();

                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                locations.next = &velocities;
                locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
                locations.jointLocations = jointLocations[hand].data();

                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                locateInfo.baseSpace = localSpace;
                locateInfo.time = frameIterator.frameState.predictedDisplayTime;
                REQUIRE(XR_SUCCESS == xrLocateHandJointsEXT(handTracker[hand], &locateInfo, &locations));

                // https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#_locate_hand_joints
                for (size_t i = 0; i < jointLocations[hand].size(); ++i) {
                    if (locations.isActive == XR_TRUE) {
                        // If the returned isActive is true, the runtime must return all joint locations with both
                        // XR_SPACE_LOCATION_POSITION_VALID_BIT and XR_SPACE_LOCATION_ORIENTATION_VALID_BIT set.
                        REQUIRE((jointLocations[hand][i].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0);
                        REQUIRE((jointLocations[hand][i].locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0);

                        // If the returned locationFlags has XR_SPACE_LOCATION_POSITION_VALID_BIT set,
                        // the returned radius must be a positive value.
                        REQUIRE(jointLocations[hand][i].radius > 0);

                        // If an XrHandJointVelocitiesEXT structure is chained to XrHandJointLocationsEXT::next,
                        // the returned XrHandJointLocationsEXT::isActive is true, and the velocity is observed
                        // or can be calculated by the runtime, the runtime must fill in the linear velocity of
                        // each hand joint within the reference frame of baseSpace and set the
                        // XR_SPACE_VELOCITY_LINEAR_VALID_BIT.
                        // Similarly, if an XrHandJointVelocitiesEXT structure is chained to
                        // XrHandJointLocationsEXT::next, the returned XrHandJointLocationsEXT::isActive is true,
                        // and the angular velocity is observed or can be calculated by the runtime, the runtime
                        // must fill in the angular velocity of each joint within the reference frame of baseSpace
                        // and set the XR_SPACE_VELOCITY_ANGULAR_VALID_BIT.
                        REQUIRE((jointVelocities[hand][i].velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) != 0);
                        REQUIRE((jointVelocities[hand][i].velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) != 0);
                    }
                    else {
                        // If the returned isActive is false, it indicates the hand tracker did not detect the hand
                        // input or the application lost input focus. In this case, the runtime must return all
                        // jointLocations with neither XR_SPACE_LOCATION_POSITION_VALID_BIT nor
                        // XR_SPACE_LOCATION_ORIENTATION_VALID_BIT set.
                        REQUIRE((jointLocations[hand][i].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == 0);
                        REQUIRE((jointLocations[hand][i].locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) == 0);
                    }
                }
            }
        }

        SECTION("Query invalid joint sets")
        {
            XrSpace localSpace = XR_NULL_HANDLE;

            XrReferenceSpaceCreateInfo localSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            localSpaceCreateInfo.poseInReferenceSpace = XrPosefCPP();
            localSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            REQUIRE_RESULT(xrCreateReferenceSpace(session, &localSpaceCreateInfo, &localSpace), XR_SUCCESS);

            // Wait until the runtime is ready for us to begin a session
            auto timeout = (GetGlobalData().options.debugMode ? 3600s : 10s);
            FrameIterator frameIterator(&session);
            FrameIterator::RunResult runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_READY, timeout);
            REQUIRE(runResult == FrameIterator::RunResult::Success);

            // The application must input jointCount as described by the XrHandJointSetEXT when creating the XrHandTrackerEXT.
            // Otherwise, the runtime must return XR_ERROR_VALIDATION_FAILURE.
            static const uint32_t INVALID_JOINT_COUNT = XR_HAND_JOINT_COUNT_EXT - 1;

            // Firstly test without joint velocities.
            for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
                std::array<std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT>, HAND_COUNT> jointLocations;

                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                locations.jointCount = INVALID_JOINT_COUNT;
                locations.jointLocations = jointLocations[hand].data();

                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                locateInfo.baseSpace = localSpace;
                locateInfo.time = frameIterator.frameState.predictedDisplayTime;
                REQUIRE(XR_ERROR_VALIDATION_FAILURE == xrLocateHandJointsEXT(handTracker[hand], &locateInfo, &locations));
            }

            // Same test again but this time with invalid joint velocity count
            for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
                std::array<std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT>, HAND_COUNT> jointLocations;
                std::array<std::array<XrHandJointVelocityEXT, XR_HAND_JOINT_COUNT_EXT>, HAND_COUNT> jointVelocities;

                XrHandJointVelocitiesEXT velocities{XR_TYPE_HAND_JOINT_VELOCITIES_EXT};
                velocities.jointCount = INVALID_JOINT_COUNT;
                velocities.jointVelocities = jointVelocities[hand].data();

                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                locations.next = &velocities;
                locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
                locations.jointLocations = jointLocations[hand].data();

                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                locateInfo.baseSpace = localSpace;
                locateInfo.time = frameIterator.frameState.predictedDisplayTime;
                REQUIRE(XR_ERROR_VALIDATION_FAILURE == xrLocateHandJointsEXT(handTracker[hand], &locateInfo, &locations));
            }
        }

        for (size_t i = 0; i < HAND_COUNT; ++i) {
            REQUIRE(XR_SUCCESS == xrDestroyHandTrackerEXT(handTracker[i]));
        }
    }

    // Purpose: Ensure that if the hand tracking extension is enabled, you can see some hands!
    TEST_CASE("XR_EXT_hand_tracking-interactive", "[scenario][interactive][no_auto]")
    {
        const char* instructions =
            "Small cubes are rendered to represent the joints of each hand. "
            "Bring index finger of both hands together to complete the validation.";

        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
            SKIP(XR_EXT_HAND_TRACKING_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper("XR_EXT_hand_tracking", {"XR_EXT_hand_tracking"});

        if (!SystemSupportsHandTracking(compositionHelper.GetInstance(), compositionHelper.GetSystemId())) {
            // This runtime does support hand tracking, but this headset does not
            // support hand tracking, which is fine.
            SKIP("System does not support hand tracking");
        }

        XrInstance instance = compositionHelper.GetInstance();

        auto xrCreateHandTrackerEXT = GetInstanceExtensionFunction<PFN_xrCreateHandTrackerEXT>(instance, "xrCreateHandTrackerEXT");
        auto xrDestroyHandTrackerEXT = GetInstanceExtensionFunction<PFN_xrDestroyHandTrackerEXT>(instance, "xrDestroyHandTrackerEXT");
        auto xrLocateHandJointsEXT = GetInstanceExtensionFunction<PFN_xrLocateHandJointsEXT>(instance, "xrLocateHandJointsEXT");

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

        compositionHelper.BeginSession();

        XrHandTrackerEXT handTracker[HAND_COUNT];

        for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
            XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
            createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
            createInfo.hand = (hand == LEFT_HAND ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);
            REQUIRE(XR_SUCCESS == xrCreateHandTrackerEXT(compositionHelper.GetSession(), &createInfo, &handTracker[hand]));
        }

        // Create the instructional quad layer placed to the left.
        XrCompositionLayerQuad* const instructionsQuad =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 512, instructions, 48)),
                                              localSpace, 1.0f, {{0, 0, 0, 1}, {-1.5f, 0, -0.3f}});
        XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &Up, 70 * MATH_PI / 180);

        auto update = [&](const XrFrameState& frameState) {
            std::vector<Cube> renderedCubes;

            XrHandJointLocationEXT jointLocations[HAND_COUNT][XR_HAND_JOINT_COUNT_EXT];

            for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
                locations.jointLocations = jointLocations[hand];

                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                locateInfo.baseSpace = localSpace;
                locateInfo.time = frameState.predictedDisplayTime;
                REQUIRE(XR_SUCCESS == xrLocateHandJointsEXT(handTracker[hand], &locateInfo, &locations));
            }

            // Check if user has requested to complete the test.
            {
                XrHandJointLocationEXT& leftIndexTip = jointLocations[LEFT_HAND][XR_HAND_JOINT_INDEX_TIP_EXT];
                XrHandJointLocationEXT& rightIndexTip = jointLocations[RIGHT_HAND][XR_HAND_JOINT_INDEX_TIP_EXT];

                if ((leftIndexTip.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                    (rightIndexTip.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
                    XrVector3f distance;
                    XrVector3f_Sub(&distance, &leftIndexTip.pose.position, &rightIndexTip.pose.position);
                    float len = XrVector3f_Length(&distance);
                    // bring center of index fingers to within 1cm. Probably fine for most humans, unless
                    // they have huge fingers.
                    if (len < 0.01f) {
                        return false;
                    }
                }
            }

            // Locate and add to list of cubes to render.
            for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
                for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++) {
                    XrHandJointLocationEXT& jointLocation = jointLocations[hand][i];
                    if ((jointLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
                        bool tracked = ((jointLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0);

                        // Position is valid but not tracked => low confidence in position; reduce size of cube...
                        float radius = tracked ? jointLocation.radius : (jointLocation.radius / 2);

                        // Fingers joints are not really cubes, but...
                        renderedCubes.push_back(Cube::Make(jointLocation.pose.position, radius, jointLocation.pose.orientation));
                    }
                }
            }

            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each viewport of the wide swapchain using the projection layer view fov and pose.
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

            compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

            return compositionHelper.PollEvents();
        };

        RenderLoop(compositionHelper.GetSession(), update).Loop();

        for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
            REQUIRE(XR_SUCCESS == xrDestroyHandTrackerEXT(handTracker[hand]));
        }
    }
}  // namespace Conformance
