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

#include "utilities/utils.h"
#include "conformance_utils.h"
#include "composition_utils.h"
#include "mesh_projection_layer.h"

#include <catch2/catch_test_macros.hpp>
#include <earcut.hpp>
#include <openxr/openxr.h>
#include "common/xr_linear.h"

#include <future>

using namespace Conformance;

namespace mapbox
{
    namespace util
    {

        template <>
        struct nth<0, XrVector2f>
        {
            inline static auto get(const XrVector2f& t)
            {
                return t.x;
            };
        };
        template <>
        struct nth<1, XrVector2f>
        {
            inline static auto get(const XrVector2f& t)
            {
                return t.y;
            };
        };

    }  // namespace util
}  // namespace mapbox

namespace Conformance
{
    static constexpr XrVector3f Up{0, 1, 0};
    static XrPlaneDetectionCapabilityFlagsEXT SystemPlaneDetectionCapabilities(XrInstance instance, XrSystemId systemId)
    {
        XrSystemPlaneDetectionPropertiesEXT planeDetectionSystemProperties{XR_TYPE_SYSTEM_PLANE_DETECTION_PROPERTIES_EXT};
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
        systemProperties.next = &planeDetectionSystemProperties;

        REQUIRE(XR_SUCCESS == xrGetSystemProperties(instance, systemId, &systemProperties));
        return planeDetectionSystemProperties.supportedFeatures;
    }

    static bool SystemSupportsEXTPlaneDetection(XrInstance instance, XrSystemId systemId)
    {
        return SystemPlaneDetectionCapabilities(instance, systemId) & XR_PLANE_DETECTION_CAPABILITY_PLANE_DETECTION_BIT_EXT;
    }

    TEST_CASE("XR_EXT_plane_detection", "[XR_EXT_plane_detection]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_PLANE_DETECTION_EXTENSION_NAME)) {
            // Runtime does not support extension - it should not be possible to get function pointers.
            AutoBasicInstance instance;
            ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreatePlaneDetectorEXT");
            SKIP(XR_EXT_PLANE_DETECTION_EXTENSION_NAME " not supported");
        }

        SECTION("Extension not enabled")
        {
            if (!globalData.IsInstanceExtensionEnabled(XR_EXT_PLANE_DETECTION_EXTENSION_NAME)) {
                AutoBasicInstance instance;
                ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreatePlaneDetectorEXT");
            }
            else {
                WARN(XR_EXT_PLANE_DETECTION_EXTENSION_NAME " force-enabled, cannot test behavior when extension is disabled.");
            }
        }

        SECTION("Create and Destroy")
        {
            AutoBasicInstance instance({XR_EXT_PLANE_DETECTION_EXTENSION_NAME}, AutoBasicInstance::createSystemId);
            XrSystemId systemId = instance.systemId;
            auto xrCreatePlaneDetectorEXT =
                GetInstanceExtensionFunction<PFN_xrCreatePlaneDetectorEXT>(instance, "xrCreatePlaneDetectorEXT");
            auto xrDestroyPlaneDetectorEXT =
                GetInstanceExtensionFunction<PFN_xrDestroyPlaneDetectorEXT>(instance, "xrDestroyPlaneDetectorEXT");

            if (!SystemSupportsEXTPlaneDetection(instance, systemId)) {
                // This runtime does support plane detection tracking, but this system does not, that is fine.
                SKIP("System does not support plane detection");
            }

            AutoBasicSession session(AutoBasicSession::beginSession, instance);

            // pass not initialized structure
            XrPlaneDetectorCreateInfoEXT createInfo{};
            XrPlaneDetectorEXT detection = XR_NULL_HANDLE;
            REQUIRE(XR_ERROR_VALIDATION_FAILURE == xrCreatePlaneDetectorEXT(session, &createInfo, &detection));

            createInfo.type = XR_TYPE_PLANE_DETECTOR_CREATE_INFO_EXT;
            createInfo.flags = XR_PLANE_DETECTOR_ENABLE_CONTOUR_BIT_EXT;

            REQUIRE(XR_SUCCESS == xrCreatePlaneDetectorEXT(session, &createInfo, &detection));
            REQUIRE(XR_SUCCESS == xrDestroyPlaneDetectorEXT(detection));
        }
    }

    static void RunPlaneTest(const std::vector<XrPlaneDetectorOrientationEXT>& orientations, const char* instructions,
                             XrPlaneDetectorSemanticTypeEXT autoCompleteSemanticType = XR_PLANE_DETECTOR_SEMANTIC_TYPE_UNDEFINED_EXT,
                             bool forceOrientationNullptr = false)
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_PLANE_DETECTION_EXTENSION_NAME)) {
            SKIP(XR_EXT_PLANE_DETECTION_EXTENSION_NAME " not supported");
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Not using graphics, which the test requires");
        }

        CompositionHelper compositionHelper("XR_EXT_plane_detection", {XR_EXT_PLANE_DETECTION_EXTENSION_NAME});
        XrPlaneDetectionCapabilityFlagsEXT flags =
            SystemPlaneDetectionCapabilities(compositionHelper.GetInstance(), compositionHelper.GetSystemId());

        if ((flags & XR_PLANE_DETECTION_CAPABILITY_PLANE_DETECTION_BIT_EXT) == 0) {
            SKIP("System does not support plane detection");
        }

        switch (autoCompleteSemanticType) {
        case XR_PLANE_DETECTOR_SEMANTIC_TYPE_UNDEFINED_EXT:
            break;
        case XR_PLANE_DETECTOR_SEMANTIC_TYPE_CEILING_EXT:
            if ((flags & XR_PLANE_DETECTION_CAPABILITY_SEMANTIC_CEILING_BIT_EXT) == 0) {
                // TODO convert to SKIP? compare ctsxml output in both cases
                INFO("Semantic ceiling not supported");
                return;
            }
            break;
        case XR_PLANE_DETECTOR_SEMANTIC_TYPE_FLOOR_EXT:
            if ((flags & XR_PLANE_DETECTION_CAPABILITY_SEMANTIC_FLOOR_BIT_EXT) == 0) {
                // TODO convert to SKIP? compare ctsxml output in both cases
                INFO("Semantic floor not supported");
                return;
            }
            break;
        case XR_PLANE_DETECTOR_SEMANTIC_TYPE_WALL_EXT:
            if ((flags & XR_PLANE_DETECTION_CAPABILITY_SEMANTIC_WALL_BIT_EXT) == 0) {
                // TODO convert to SKIP? compare ctsxml output in both cases
                INFO("Semantic wall not supported");
                return;
            }
            break;
        case XR_PLANE_DETECTOR_SEMANTIC_TYPE_PLATFORM_EXT:
            if ((flags & XR_PLANE_DETECTION_CAPABILITY_SEMANTIC_PLATFORM_BIT_EXT) == 0) {
                // TODO convert to SKIP? compare ctsxml output in both cases
                INFO("Semantic platform not supported");
                return;
            }
            break;
        default:
            WARN("Unexpected Semantic Type requested");
            return;
        }

        XrInstance instance = compositionHelper.GetInstance();

        auto xrCreatePlaneDetectorEXT = GetInstanceExtensionFunction<PFN_xrCreatePlaneDetectorEXT>(instance, "xrCreatePlaneDetectorEXT");
        auto xrDestroyPlaneDetectorEXT = GetInstanceExtensionFunction<PFN_xrDestroyPlaneDetectorEXT>(instance, "xrDestroyPlaneDetectorEXT");
        auto xrBeginPlaneDetectionEXT = GetInstanceExtensionFunction<PFN_xrBeginPlaneDetectionEXT>(instance, "xrBeginPlaneDetectionEXT");
        auto xrGetPlaneDetectionStateEXT =
            GetInstanceExtensionFunction<PFN_xrGetPlaneDetectionStateEXT>(instance, "xrGetPlaneDetectionStateEXT");
        auto xrGetPlaneDetectionsEXT = GetInstanceExtensionFunction<PFN_xrGetPlaneDetectionsEXT>(instance, "xrGetPlaneDetectionsEXT");

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosefCPP{});
        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosefCPP{});

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
        XrAction completeAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "plane_detection_test");
            strcpy(actionSetInfo.localizedActionSetName, "Plane Detection Test");
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

        XrPlaneDetectorCreateInfoEXT createInfo{XR_TYPE_PLANE_DETECTOR_CREATE_INFO_EXT};
        createInfo.flags = XR_PLANE_DETECTOR_ENABLE_CONTOUR_BIT_EXT;
        XrPlaneDetectorEXT detection = XR_NULL_HANDLE;
        REQUIRE(XR_SUCCESS == xrCreatePlaneDetectorEXT(compositionHelper.GetSession(), &createInfo, &detection));

        // Create the instructional quad layer placed to the left.
        XrCompositionLayerQuad* const instructionsQuad =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 512, instructions, 48)),
                                              localSpace, 1.0f, {{0, 0, 0, 1}, {-0.2f, 0, -1.0f}});
        XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &Up, 10 * MATH_PI / 180);

        enum DetectState
        {
            IDLE,
            WAITING,
            PROCESSING
        };
        DetectState detect_state = IDLE;
        std::vector<Cube> renderedCubes;

        auto update = [&](const XrFrameState& frameState) {
            const std::array<XrActiveActionSet, 1> activeActionSets = {{{actionSet, XR_NULL_PATH}}};
            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.activeActionSets = activeActionSets.data();
            syncInfo.countActiveActionSets = (uint32_t)activeActionSets.size();
            XRC_CHECK_THROW_XRCMD(xrSyncActions(compositionHelper.GetSession(), &syncInfo))

            // if an autoCompleteSemanticType is specified it will be used to complete the
            // test.
            if (autoCompleteSemanticType == XR_PLANE_DETECTOR_SEMANTIC_TYPE_UNDEFINED_EXT) {
                XrActionStateGetInfo completeActionGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                completeActionGetInfo.action = completeAction;
                XrActionStateBoolean completeActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(compositionHelper.GetSession(), &completeActionGetInfo, &completeActionState))
                if (completeActionState.currentState == XR_TRUE && completeActionState.changedSinceLastSync) {
                    return false;
                }
            }

            switch (detect_state) {
            case IDLE: {
                XrPosef pose{};
                pose.position = XrVector3f{0.0f, 0.0f, 0.0f};
                pose.orientation = XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f};

                XrPlaneDetectorBeginInfoEXT beginInfo{XR_TYPE_PLANE_DETECTOR_BEGIN_INFO_EXT};
                if (forceOrientationNullptr) {
                    beginInfo.orientationCount = 0;
                    beginInfo.orientations = nullptr;
                }
                else {
                    beginInfo.orientationCount = (uint32_t)orientations.size();
                    beginInfo.orientations = orientations.data();
                }
                beginInfo.minArea = 0.1f;
                beginInfo.maxPlanes = 100;
                beginInfo.boundingBoxPose = pose;
                beginInfo.boundingBoxExtent = XrExtent3DfEXT{10.0f, 10.0f, 10.0f};
                beginInfo.time = frameState.predictedDisplayTime;
                beginInfo.baseSpace = viewSpace;
                REQUIRE(XR_SUCCESS == xrBeginPlaneDetectionEXT(detection, &beginInfo));
                detect_state = WAITING;
            } break;
            case WAITING: {

                // If GetPlaneDetectionStateEXT has not yet returned XR_PLANE_DETECTION_STATE_DONE_EXT
                // calling xrGetPlaneDetectionsEXT must return XR_ERROR_CALL_ORDER_INVALID
                XrPlaneDetectorGetInfoEXT getInfo{XR_TYPE_PLANE_DETECTOR_GET_INFO_EXT};
                getInfo.baseSpace = localSpace;
                getInfo.time = frameState.predictedDisplayTime;
                XrPlaneDetectorLocationsEXT locations{XR_TYPE_PLANE_DETECTOR_LOCATIONS_EXT};
                REQUIRE(XR_ERROR_CALL_ORDER_INVALID == xrGetPlaneDetectionsEXT(detection, &getInfo, &locations));

                XrPlaneDetectionStateEXT state;
                REQUIRE(XR_SUCCESS == xrGetPlaneDetectionStateEXT(detection, &state));
                REQUIRE((state == XR_PLANE_DETECTION_STATE_PENDING_EXT || state == XR_PLANE_DETECTION_STATE_DONE_EXT ||
                         state == XR_PLANE_DETECTION_STATE_ERROR_EXT));
                switch (state) {
                case XR_PLANE_DETECTION_STATE_DONE_EXT:
                    detect_state = PROCESSING;
                    break;
                case XR_PLANE_DETECTION_STATE_ERROR_EXT:
                    detect_state = IDLE;
                    break;
                case XR_PLANE_DETECTION_STATE_PENDING_EXT:
                    break;
                default:
                    break;
                }
            } break;
            case PROCESSING: {
                XrPlaneDetectorGetInfoEXT getInfo{XR_TYPE_PLANE_DETECTOR_GET_INFO_EXT};
                getInfo.baseSpace = localSpace;
                getInfo.time = frameState.predictedDisplayTime;
                XrPlaneDetectorLocationsEXT locations{XR_TYPE_PLANE_DETECTOR_LOCATIONS_EXT};
                REQUIRE(XR_SUCCESS == xrGetPlaneDetectionsEXT(detection, &getInfo, &locations));

                if (locations.planeLocationCountOutput == 0) {
                    break;
                }

                renderedCubes.clear();
                std::vector<XrPlaneDetectorLocationEXT> location_vector;
                location_vector.resize(locations.planeLocationCountOutput);
                for (auto& location : location_vector) {
                    location.type = XR_TYPE_PLANE_DETECTOR_LOCATION_EXT;
                    location.next = nullptr;
                }
                locations.planeLocations = location_vector.data();
                locations.planeLocationCapacityInput = (uint32_t)location_vector.size();

                REQUIRE(XR_SUCCESS == xrGetPlaneDetectionsEXT(detection, &getInfo, &locations));
                for (auto& location : location_vector) {

                    if (autoCompleteSemanticType != XR_PLANE_DETECTOR_SEMANTIC_TYPE_UNDEFINED_EXT) {
                        if (location.semanticType == autoCompleteSemanticType) {
                            // DONE!
                            return false;
                        }
                    }

                    renderedCubes.push_back(Cube{/* pose */ {location.pose.orientation, location.pose.position},
                                                 /* scale: */ {location.extents.width, location.extents.height, 0.01f}});
                }

                detect_state = IDLE;

            } break;
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

        REQUIRE(XR_SUCCESS == xrDestroyPlaneDetectorEXT(detection));
    }

    TEST_CASE("XR_EXT_plane_detection-V", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({XR_PLANE_DETECTOR_ORIENTATION_VERTICAL_EXT},
                     "Planes should be rendered at the vertical surfaces, "
                     "the blue faces should face inward. "
                     "Press the select button on either controller to pass the test.");
    }

    TEST_CASE("XR_EXT_plane_detection-HU", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_UPWARD_EXT},
                     "Planes should be rendered at the horizontal surfaces with upward normals, "
                     "the blue faces should face upward (e.g. floors). "
                     "Press the select button on either controller to pass the test.");
    }

    TEST_CASE("XR_EXT_plane_detection-HD", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_DOWNWARD_EXT},
                     "Planes should be rendered at the horizontal surfaces with downward normals, "
                     "the blue faces should face downward (e.g. ceilings). "
                     "Press the select button on either controller to pass the test.");
    }

    TEST_CASE("XR_EXT_plane_detection-A", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({XR_PLANE_DETECTOR_ORIENTATION_ARBITRARY_EXT},
                     "Planes should be rendered at the non horizontal/vertical surfaces. "
                     "Press the select button on either controller to pass the test.");
    }

    TEST_CASE("XR_EXT_plane_detection-empty-list", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({},
                     "All planes should be rendered. "
                     "Press the select button on either controller to pass the test.");
    }

    TEST_CASE("XR_EXT_plane_detection-nullptr", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({},
                     "All planes should be rendered. "
                     "Press the select button on either controller to pass the test.",
                     XR_PLANE_DETECTOR_SEMANTIC_TYPE_UNDEFINED_EXT, true);
    }

    TEST_CASE("XR_EXT_plane_detection-ceiling", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_DOWNWARD_EXT}, "Make sure a ceiling is detected in the scene.",
                     XR_PLANE_DETECTOR_SEMANTIC_TYPE_CEILING_EXT);
    }

    TEST_CASE("XR_EXT_plane_detection-floor", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_UPWARD_EXT}, "Make sure a floor is detected in the scene.",
                     XR_PLANE_DETECTOR_SEMANTIC_TYPE_FLOOR_EXT);
    }

    TEST_CASE("XR_EXT_plane_detection-wall", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({XR_PLANE_DETECTOR_ORIENTATION_VERTICAL_EXT}, "Make sure a wall is detected in the scene.",
                     XR_PLANE_DETECTOR_SEMANTIC_TYPE_WALL_EXT);
    }

    TEST_CASE("XR_EXT_plane_detection-platform", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneTest({XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_UPWARD_EXT}, "Make sure a platform is detected in the scene.",
                     XR_PLANE_DETECTOR_SEMANTIC_TYPE_PLATFORM_EXT);
    }

    TEST_CASE("XR_EXT_plane_detection-invalid-arguments", "[XR_EXT_plane_detection]")
    {
        // basic setup stuff
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_PLANE_DETECTION_EXTENSION_NAME)) {
            SKIP(XR_EXT_PLANE_DETECTION_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper("XR_EXT_plane_detection", {XR_EXT_PLANE_DETECTION_EXTENSION_NAME});

        if (!SystemSupportsEXTPlaneDetection(compositionHelper.GetInstance(), compositionHelper.GetSystemId())) {
            SKIP("System does not support plane detection");
        }

        XrInstance instance = compositionHelper.GetInstance();

        auto xrCreatePlaneDetectorEXT = GetInstanceExtensionFunction<PFN_xrCreatePlaneDetectorEXT>(instance, "xrCreatePlaneDetectorEXT");
        auto xrDestroyPlaneDetectorEXT = GetInstanceExtensionFunction<PFN_xrDestroyPlaneDetectorEXT>(instance, "xrDestroyPlaneDetectorEXT");
        auto xrBeginPlaneDetectionEXT = GetInstanceExtensionFunction<PFN_xrBeginPlaneDetectionEXT>(instance, "xrBeginPlaneDetectionEXT");
        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosefCPP{});
        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosefCPP{});

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

        XrPlaneDetectorCreateInfoEXT createInfo{XR_TYPE_PLANE_DETECTOR_CREATE_INFO_EXT};
        createInfo.flags = XR_PLANE_DETECTOR_ENABLE_CONTOUR_BIT_EXT;
        XrPlaneDetectorEXT detection = XR_NULL_HANDLE;
        REQUIRE(XR_SUCCESS == xrCreatePlaneDetectorEXT(compositionHelper.GetSession(), &createInfo, &detection));

        // Lambda to create the instructional quad layer placed to the left.
        auto makeInstructionsQuad = [&](const char* instructions) {
            XrCompositionLayerQuad* const instructionsQuad = compositionHelper.CreateQuadLayer(
                compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 512, instructions, 48)), localSpace, 1.0f,
                {{0, 0, 0, 1}, {-0.2f, 0, -1.0f}});
            XrQuaternionf_CreateFromAxisAngle(&instructionsQuad->pose.orientation, &Up, 10 * MATH_PI / 180);
        };

        // Configure the XrPlaneDetectorBeginInfoEXT correctly first, before making it invalid in sections.
        XrPosef pose{};
        pose.position = XrVector3f{0.0f, 0.0f, 0.0f};
        pose.orientation = XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f};

        XrPlaneDetectorBeginInfoEXT beginInfo{XR_TYPE_PLANE_DETECTOR_BEGIN_INFO_EXT};
        beginInfo.minArea = 0.1f;
        beginInfo.maxPlanes = 100;

        std::vector<XrPlaneDetectorOrientationEXT> orientations = {XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_UPWARD_EXT};
        beginInfo.orientationCount = (uint32_t)orientations.size();
        beginInfo.orientations = orientations.data();

        beginInfo.boundingBoxPose = pose;
        beginInfo.boundingBoxExtent = XrExtent3DfEXT{10.0f, 10.0f, 10.0f};

        beginInfo.baseSpace = viewSpace;

        beginInfo.time = 0;

        SECTION("invalid-filters")
        {
            // intentionally do this wrong:
            beginInfo.orientationCount = 1;
            beginInfo.orientations = nullptr;

            makeInstructionsQuad("Testing null filters with count");

            RenderLoop(compositionHelper.GetSession(), [&](const XrFrameState& frameState) {
                beginInfo.time = frameState.predictedDisplayTime;
                REQUIRE(XR_ERROR_VALIDATION_FAILURE == xrBeginPlaneDetectionEXT(detection, &beginInfo));
                return false;
            }).Loop();
        }
        SECTION("invalid-time")
        {

            makeInstructionsQuad("Testing invalid time");

            RenderLoop(compositionHelper.GetSession(), [&](const XrFrameState& /* frameState */) {
                beginInfo.time = 0;
                REQUIRE(XR_ERROR_TIME_INVALID == xrBeginPlaneDetectionEXT(detection, &beginInfo));
                return false;
            }).Loop();
        }
        SECTION("invalid-pose")
        {

            pose.orientation = XrQuaternionf{0.0f, 0.0f, 0.0f, 0.0f};

            makeInstructionsQuad("Testing invalid pose");

            RenderLoop(compositionHelper.GetSession(), [&](const XrFrameState& frameState) {
                beginInfo.time = frameState.predictedDisplayTime;
                beginInfo.boundingBoxPose = pose;
                REQUIRE(XR_ERROR_POSE_INVALID == xrBeginPlaneDetectionEXT(detection, &beginInfo));
                return false;
            }).Loop();
        }

        REQUIRE(XR_SUCCESS == xrDestroyPlaneDetectorEXT(detection));
    }

    static bool IsClockWise(const XrVector2f* points, size_t count)
    {
        float area = 0.0f;
        for (size_t i = 0; i < count; i++) {
            const auto& p1 = points[i];
            const auto& p2 = points[(i + 1) % count];
            area += (p2.x - p1.x) * (p2.y + p1.y);
        }
        return area > 0.0f;
    }

    static void RunPlaneContourTest(const std::vector<XrPlaneDetectorOrientationEXT>& orientations, const char* exampleImage,
                                    const char* instructions)
    {

        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_PLANE_DETECTION_EXTENSION_NAME)) {
            SKIP(XR_EXT_PLANE_DETECTION_EXTENSION_NAME " not supported");
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Not using graphics, which the test requires");
        }

        CompositionHelper compositionHelper("XR_EXT_plane_detection", {XR_EXT_PLANE_DETECTION_EXTENSION_NAME});

        if (!SystemSupportsEXTPlaneDetection(compositionHelper.GetInstance(), compositionHelper.GetSystemId())) {
            SKIP("System does not support plane detection");
        }

        InteractiveLayerManager interactiveLayerManager(compositionHelper, exampleImage, instructions);

        compositionHelper.GetInteractionManager().AttachActionSets();

        MeshProjectionLayerHelper meshProjectionLayerHelper(compositionHelper);

        XrInstance instance = compositionHelper.GetInstance();

        auto xrCreatePlaneDetectorEXT = GetInstanceExtensionFunction<PFN_xrCreatePlaneDetectorEXT>(instance, "xrCreatePlaneDetectorEXT");
        auto xrDestroyPlaneDetectorEXT = GetInstanceExtensionFunction<PFN_xrDestroyPlaneDetectorEXT>(instance, "xrDestroyPlaneDetectorEXT");
        auto xrBeginPlaneDetectionEXT = GetInstanceExtensionFunction<PFN_xrBeginPlaneDetectionEXT>(instance, "xrBeginPlaneDetectionEXT");
        auto xrGetPlaneDetectionStateEXT =
            GetInstanceExtensionFunction<PFN_xrGetPlaneDetectionStateEXT>(instance, "xrGetPlaneDetectionStateEXT");
        auto xrGetPlaneDetectionsEXT = GetInstanceExtensionFunction<PFN_xrGetPlaneDetectionsEXT>(instance, "xrGetPlaneDetectionsEXT");
        auto xrGetPlanePolygonBufferEXT =
            GetInstanceExtensionFunction<PFN_xrGetPlanePolygonBufferEXT>(instance, "xrGetPlanePolygonBufferEXT");

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosefCPP{});
        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosefCPP{});

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

        XrPlaneDetectorCreateInfoEXT createInfo{XR_TYPE_PLANE_DETECTOR_CREATE_INFO_EXT};
        createInfo.flags = XR_PLANE_DETECTOR_ENABLE_CONTOUR_BIT_EXT;
        XrPlaneDetectorEXT detection = XR_NULL_HANDLE;
        REQUIRE(XR_SUCCESS == xrCreatePlaneDetectorEXT(compositionHelper.GetSession(), &createInfo, &detection));

        enum DetectState
        {
            IDLE,
            WAITING,
            PROCESSING,
            RETRIEVING,
        };
        DetectState detect_state = IDLE;

        struct MeshData
        {
            std::vector<uint16_t> indices;
            std::vector<Geometry::Vertex> vertices;
            XrPosef pose{};
        };

        std::vector<MeshDrawable> meshes;
        std::future<std::vector<MeshData>> retrieval;

        auto update = [&](const XrFrameState& frameState) {
            switch (detect_state) {
            case IDLE: {
                XrPosef pose{};
                pose.position = XrVector3f{0.0f, 0.0f, 0.0f};
                pose.orientation = XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f};

                XrPlaneDetectorBeginInfoEXT beginInfo{XR_TYPE_PLANE_DETECTOR_BEGIN_INFO_EXT};
                beginInfo.orientationCount = (uint32_t)orientations.size();
                beginInfo.orientations = orientations.data();
                beginInfo.minArea = 0.1f;
                beginInfo.maxPlanes = 100;
                beginInfo.boundingBoxPose = pose;
                beginInfo.boundingBoxExtent = XrExtent3DfEXT{10.0f, 10.0f, 10.0f};
                beginInfo.time = frameState.predictedDisplayTime;
                beginInfo.baseSpace = viewSpace;
                REQUIRE(XR_SUCCESS == xrBeginPlaneDetectionEXT(detection, &beginInfo));
                detect_state = WAITING;
            } break;
            case WAITING: {
                XrPlaneDetectionStateEXT state;
                REQUIRE(XR_SUCCESS == xrGetPlaneDetectionStateEXT(detection, &state));
                REQUIRE((state == XR_PLANE_DETECTION_STATE_PENDING_EXT || state == XR_PLANE_DETECTION_STATE_DONE_EXT ||
                         state == XR_PLANE_DETECTION_STATE_ERROR_EXT));
                switch (state) {
                case XR_PLANE_DETECTION_STATE_DONE_EXT:
                    detect_state = PROCESSING;
                    break;
                case XR_PLANE_DETECTION_STATE_ERROR_EXT:
                    detect_state = IDLE;
                    break;
                case XR_PLANE_DETECTION_STATE_PENDING_EXT:
                    break;
                default:
                    break;
                }
            } break;
            case PROCESSING: {
                XrPlaneDetectorGetInfoEXT getInfo{XR_TYPE_PLANE_DETECTOR_GET_INFO_EXT};
                getInfo.baseSpace = localSpace;
                getInfo.time = frameState.predictedDisplayTime;
                XrPlaneDetectorLocationsEXT locations{XR_TYPE_PLANE_DETECTOR_LOCATIONS_EXT};
                REQUIRE(XR_SUCCESS == xrGetPlaneDetectionsEXT(detection, &getInfo, &locations));

                if (locations.planeLocationCountOutput == 0) {
                    break;
                }

                std::vector<XrPlaneDetectorLocationEXT> location_vector;
                location_vector.resize(locations.planeLocationCountOutput);
                for (auto& location : location_vector) {
                    location.type = XR_TYPE_PLANE_DETECTOR_LOCATION_EXT;
                    location.next = nullptr;
                }
                locations.planeLocations = location_vector.data();
                locations.planeLocationCapacityInput = (uint32_t)location_vector.size();

                REQUIRE(XR_SUCCESS == xrGetPlaneDetectionsEXT(detection, &getInfo, &locations));
                detect_state = RETRIEVING;

                retrieval = std::async(std::launch::async, [detection, location_vector, xrGetPlanePolygonBufferEXT]() {
                    std::vector<MeshData> local_meshes;
                    for (auto& location : location_vector) {

                        std::vector<std::vector<XrVector2f>> polygon;
                        MeshData md;

                        for (uint32_t polygonBufferIndex = 0; polygonBufferIndex < location.polygonBufferCount; polygonBufferIndex++) {

                            XrPlaneDetectorPolygonBufferEXT polygonBuffer{XR_TYPE_PLANE_DETECTOR_POLYGON_BUFFER_EXT};

                            REQUIRE(XR_SUCCESS ==
                                    xrGetPlanePolygonBufferEXT(detection, location.planeId, polygonBufferIndex, &polygonBuffer));
                            REQUIRE(polygonBuffer.vertexCountOutput > 0);

                            std::vector<XrVector2f> vertices;
                            vertices.resize(polygonBuffer.vertexCountOutput);
                            polygonBuffer.vertexCapacityInput = (uint32_t)vertices.size();
                            polygonBuffer.vertices = vertices.data();
                            REQUIRE(XR_SUCCESS ==
                                    xrGetPlanePolygonBufferEXT(detection, location.planeId, polygonBufferIndex, &polygonBuffer));

                            XrMatrix4x4f transform;
                            XrVector3f scale{1.0f, 1.0f, 1.0f};
                            XrMatrix4x4f_CreateTranslationRotationScale(&transform, &location.pose.position, &location.pose.orientation,
                                                                        &scale);
                            CAPTURE(polygonBufferIndex);
                            if (polygonBufferIndex == 0) {
                                // hull is counter clock-wise.
                                REQUIRE(IsClockWise(polygonBuffer.vertices, polygonBuffer.vertexCountOutput) == false);
                            }
                            else {
                                // holes are clock-wise
                                REQUIRE(IsClockWise(polygonBuffer.vertices, polygonBuffer.vertexCountOutput) == true);
                            }

                            polygon.push_back(vertices);

                            for (const XrVector2f& vertex : vertices) {
                                XrVector3f source = {vertex.x, vertex.y, 0.0f};
                                md.vertices.push_back({source, Geometry::DarkBlue});
                            }
                        }

                        md.indices = mapbox::earcut<uint16_t>(polygon);
                        md.pose = location.pose;
                        std::reverse(md.indices.begin(), md.indices.end());
                        local_meshes.push_back(md);
                    }
                    return local_meshes;
                });

            } break;
            case RETRIEVING: {
                using namespace std::chrono_literals;
                if (retrieval.wait_for(0ms) == std::future_status::ready) {
                    meshes.clear();
                    auto source_meshes = retrieval.get();
                    for (const auto& mesh_data : source_meshes) {
                        auto mesh = GetGlobalData().graphicsPlugin->MakeSimpleMesh(mesh_data.indices, mesh_data.vertices);
                        meshes.emplace_back(mesh, mesh_data.pose);
                    }
                    detect_state = IDLE;
                }
            } break;
            }  // case

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
                                                                      projLayer->views[view], swapchainImage, RenderParams().Draw(meshes));
                                                              });
                }

                layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(compositionHelper.GetSession(), update).Loop();

        REQUIRE(XR_SUCCESS == xrDestroyPlaneDetectorEXT(detection));
    }

    TEST_CASE("XR_EXT_plane_detection-contour-HU", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneContourTest({XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_UPWARD_EXT}, "ext_plane_detection_contour.png",
                            "This should show the plane contours of all upward horizontal planes.");
    }

    TEST_CASE("XR_EXT_plane_detection-contour-HD", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneContourTest({XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_DOWNWARD_EXT}, "ext_plane_detection_contour.png",
                            "This should show the plane contours of all downward horizontal planes.");
    }

    TEST_CASE("XR_EXT_plane_detection-contour-V", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneContourTest({XR_PLANE_DETECTOR_ORIENTATION_VERTICAL_EXT}, "ext_plane_detection_contour.png",
                            "This should show the plane contours of all vertical planes.");
    }

    TEST_CASE("XR_EXT_plane_detection-contour-A", "[scenario][interactive][no_auto][XR_EXT_plane_detection]")
    {
        RunPlaneContourTest({XR_PLANE_DETECTOR_ORIENTATION_ARBITRARY_EXT}, "ext_plane_detection_contour.png",
                            "This should show the plane contours of all non vertical / horizontal (arbitrary) planes.");
    }

}  // namespace Conformance
