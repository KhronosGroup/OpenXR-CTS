// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#include "utils.h"
#include "conformance_utils.h"
#include "composition_utils.h"
#include <catch2/catch.hpp>
#include <openxr/openxr.h>
#include <xr_linear.h>

using namespace Conformance;

namespace Conformance
{
    constexpr XrVector3f Up{0, 1, 0};
    constexpr int LEFT_HAND = 0;
    constexpr int RIGHT_HAND = 1;
    constexpr int HAND_COUNT = 2;

    static bool SystemSupportsHandTracking(XrInstance instance)
    {
        GlobalData& globalData = GetGlobalData();
        XrSystemHandTrackingPropertiesEXT handTrackingSystemProperties{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
        systemProperties.next = &handTrackingSystemProperties;

        XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemGetInfo.formFactor = globalData.options.formFactorValue;

        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        REQUIRE(XR_SUCCESS == xrGetSystem(instance, &systemGetInfo, &systemId));
        REQUIRE(XR_SUCCESS == xrGetSystemProperties(instance, systemId, &systemProperties));

        return handTrackingSystemProperties.supportsHandTracking == XR_TRUE;
    }

    TEST_CASE("XR_EXT_hand_tracking", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
            // Runtime does not support extension - it should not be possible to get function pointers.
            AutoBasicInstance instance;
            ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreateHandTrackerEXT");
            return;
        }

        SECTION("Extension not enabled")
        {
            if (!globalData.enabledInstanceExtensionNames.contains(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
                AutoBasicInstance instance;
                ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreateHandTrackerEXT");
            }
        }

        SECTION("Create and Destroy trackers")
        {
            AutoBasicInstance instance({XR_EXT_HAND_TRACKING_EXTENSION_NAME});

            auto xrCreateHandTrackerEXT = GetInstanceExtensionFunction<PFN_xrCreateHandTrackerEXT>(instance, "xrCreateHandTrackerEXT");
            auto xrDestroyHandTrackerEXT = GetInstanceExtensionFunction<PFN_xrDestroyHandTrackerEXT>(instance, "xrDestroyHandTrackerEXT");

            if (!SystemSupportsHandTracking(instance)) {
                // This runtime does support hand tracking, but this headset does not
                // support hand tracking, which is fine.
                WARN("Device does not support hand tracking");
                return;
            }

            AutoBasicSession session(AutoBasicSession::beginSession, instance);

            XrHandTrackerEXT handTracker[HAND_COUNT];
            for (size_t i = 0; i < HAND_COUNT; ++i) {
                XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                createInfo.hand = (i == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);
                REQUIRE(XR_SUCCESS == xrCreateHandTrackerEXT(session, &createInfo, &handTracker[i]));
                REQUIRE(XR_SUCCESS == xrDestroyHandTrackerEXT(handTracker[i]));
            }
        }
    }

    // Purpose: Ensure that if the hand tracking extension is enabled, you can see some hands!
    TEST_CASE("XR_EXT_hand_tracking_interactive", "[scenario][interactive][no_auto]")
    {
        const char* instructions =
            "Small cubes are rendered to represent the joints of each hand. "
            "Bring index finger of both hands together to complete the validation.";

        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
            return;
        }

        CompositionHelper compositionHelper("XR_EXT_hand_tracking", {"XR_EXT_hand_tracking"});

        if (!SystemSupportsHandTracking(compositionHelper.GetInstance())) {
            // This runtime does support hand tracking, but this headset does not
            // support hand tracking, which is fine.
            WARN("Device does not support hand tracking");
            return;
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
                    compositionHelper.AcquireWaitReleaseImage(
                        swapchains[view],  //
                        [&](const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) {
                            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, format);
                            const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                            const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                            GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage, format,
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
