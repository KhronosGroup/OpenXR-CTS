// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "composition_utils.h"
#include "conformance_framework.h"
#include "graphics_plugin.h"

#include "common/xr_linear.h"
#include "gltf/GltfHelper.h"
#include "utilities/array_size.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <cstdint>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Conformance
{
    using namespace openxr::math_operators;

    TEST_CASE("glTFRendering", "[self_test][composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test glTF rendering without a graphics plugin");
        }

        CompositionHelper compositionHelper("glTF rendering");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();

        // Each test case will configure the layer manager with its own instructions and image
        InteractiveLayerManager interactiveLayerManager(compositionHelper, nullptr, "glTF rendering");

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

        const std::vector<XrPath> subactionPaths{StringToPath(instance, "/user/hand/left"), StringToPath(instance, "/user/hand/right")};

        XrActionSet actionSet;
        XrAction gripPoseAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "gltf_rendering");
            strcpy(actionSetInfo.localizedActionSetName, "glTF rendering");
            XRC_CHECK_THROW_XRCMD(xrCreateActionSet(instance, &actionSetInfo, &actionSet));

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.actionName, "grip_pose");
            strcpy(actionInfo.localizedActionName, "Grip pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));
        }

        interactionManager.AddActionSet(actionSet);
        XrPath simpleInteractionProfile = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
        interactionManager.AddActionBindings(simpleInteractionProfile,
                                             {{
                                                 {gripPoseAction, StringToPath(instance, "/user/hand/left/input/grip/pose")},
                                                 {gripPoseAction, StringToPath(instance, "/user/hand/right/input/grip/pose")},
                                             }});

        interactionManager.AttachActionSets();

        // Do this early to reduce unnecessary hitch during model loading.
        Image::InitKTX2();

        compositionHelper.BeginSession();

        // Spaces where we will draw the active gltf. Default to one on each controller.
        std::vector<XrSpace> gripSpaces;

        // Create XrSpaces for each grip pose
        for (int i = 0; i < 2; i++) {
            XrSpace space;
            if ((i == 0 && globalData.leftHandUnderTest) || (i == 1 && globalData.rightHandUnderTest)) {
                XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                spaceCreateInfo.action = gripPoseAction;
                spaceCreateInfo.subactionPath = subactionPaths[i];
                spaceCreateInfo.poseInActionSpace = Pose::Identity;
                XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(session, &spaceCreateInfo, &space));
                gripSpaces.emplace_back(space);
            }
        }

        struct glTFTestCase
        {
            const char* filePath;
            const char* name;
            const char* description;
            const char* exampleImagePath;
            XrPosef poseInGripSpace;
            float scale;
        };

        glTFTestCase testCases[] = {
            {"VertexColorTest.glb",
             "Vertex Color Test",
             "Ensure that each box in the \"Test\" row matches the \"Sample pass\" box below.",
             "VertexColorTest.png",
             {Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90)), {0, 0, 0}},
             0.15f},
            {"MetalRoughSpheres.glb",
             "Metal Rough Spheres",
             "Ensure that the spheres follow a pattern from rough to shiny along one axis"
             " and from metallic (like a steel ball) to dielectric (like a pool ball) on the other axis"
             " like on the example image provided.",
             "MetalRoughSpheres.png",
             {Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90)), {0, 0, 0}},
             0.03f},
            {"MetalRoughSpheresNoTextures.glb",
             "Metal Rough Spheres (no textures)",
             "Ensure that the spheres follow a pattern from rough to shiny along one axis"
             " and from metallic (like a steel ball) to dielectric (like a pool ball) on the other axis"
             " like on the example image provided.",
             "MetalRoughSpheresNoTextures.png",
             {Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90)), {-0.11f, 0, 0.11f}},
             35.f},
            {"NormalTangentTest.glb",
             "Normal Tangent Test",
             "Ensure that in each column, the squares look identical, and that in each pair of columns,"
             " the lighting moves \"correctly\" (counter to controller rotation) and is consistent"
             " between adjacent squares. The lighting should appear to be coming from diagonally above.",
             "NormalTangentTest.png",
             {Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90)), {0, 0, 0}},
             0.075f},
            {"NormalTangentMirrorTest.glb",
             "Normal Tangent Mirror Test",
             "Ensure that in each column, the squares look identical, and that in each row of four squares,"
             " the lighting moves \"correctly\" (counter to controller rotation) and is consistent"
             " between adjacent squares. The lighting should appear to be coming from diagonally above.",
             "NormalTangentMirrorTest.png",
             {Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90)), {0, 0, 0}},
             0.075f},
            {"TextureSettingsTest.glb",
             "Texture Settings Test",
             "Ensure that the \"Test\" box in each row matches the \"Sample pass\" box.",
             "TextureSettingsTest.png",
             {Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90)), {0, 0, 0}},
             0.025f},
            {"AlphaBlendModeTest.glb",
             "Alpha Blend Mode Test",
             "Ensure that the first rectangle is opaque, the second has a smooth gradient from transparent"
             " at the top to opaque at the bottom, and that the last three are filled up to the green marker.",
             "AlphaBlendModeTest.png",
             {Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90)), {0, 0, 0}},
             0.075f},
            {"AnisotropyBarnLamp.glb",
             "Barn Lamp KTX2 Texture Test",
             "This model uses many unimplemented extensions."
             " To pass, simply ensure that text is visible on the side of the lamp.",
             nullptr,
             {Quat::FromAxisAngle({1, 0, 0}, DegToRad(-90)), {0, 0, 0}},
             1.0f},
        };

        size_t testCaseIdx = 0;
        auto testCase = testCases[testCaseIdx];

        bool testCaseInitialized = false;
        std::future<Gltf::ModelBuilder> makeModelBuilderTask;
        GLTFModelHandle gltfModel;
        std::vector<GLTFModelInstanceHandle> gltfModelInstances(gripSpaces.size(), GLTFModelInstanceHandle{});

        auto makeModelBuilder = [](const glTFTestCase& tCase) -> Gltf::ModelBuilder {
            // Load the model file into memory
            auto modelData = ReadFileBytes(tCase.filePath, "glTF binary");

            // Load the model into an intermediate form
            // This does parsing and tangent generation, which can take a while
            return Gltf::ModelBuilder(LoadGLTF(modelData));
        };
        auto setupTest = [&]() {
            std::fill(gltfModelInstances.begin(), gltfModelInstances.end(), GLTFModelInstanceHandle{});

            makeModelBuilderTask = std::async(std::launch::async, makeModelBuilder, testCase);

            // Configure the interactive layer manager with the corresponding description and image
            std::ostringstream oss;
            oss << "Subtest " << (testCaseIdx + 1) << "/" << ArraySize(testCases) << ": " << testCase.name << std::endl;
            oss << testCase.description << std::endl;
            interactiveLayerManager.Configure(testCase.exampleImagePath, oss.str().c_str());

            testCaseInitialized = true;
        };
        auto loadModelToGPU = [&](Gltf::ModelBuilder&& gltfModelBuilder) {
            // Load the model onto the GPU
            gltfModel = GetGlobalData().graphicsPlugin->LoadGLTF(std::move(gltfModelBuilder));

            for (size_t i = 0; i < gripSpaces.size(); ++i) {
                gltfModelInstances[i] = GetGlobalData().graphicsPlugin->CreateGLTFModelInstance(gltfModel);
            }
        };

        auto updateLayers = [&](const XrFrameState& frameState) {
            // do this first so if models take time to load, xrLocateViews doesn't complain about an old time
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            if (!testCaseInitialized && !makeModelBuilderTask.valid()) {
                testCase = testCases[testCaseIdx];
                setupTest();
            }
            if (makeModelBuilderTask.valid()) {
                if (makeModelBuilderTask.wait_for(0s) == std::future_status::ready) {
                    loadModelToGPU(makeModelBuilderTask.get());
                }
            }

            std::vector<Cube> renderedCubes;
            std::vector<GLTFDrawable> renderedGLTFs;

            for (size_t i = 0; i < gltfModelInstances.size(); ++i) {
                const auto& space = gripSpaces[i];
                XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
                if (XR_SUCCEEDED(xrLocateSpace(space, localSpace, frameState.predictedDisplayTime, &location))) {
                    if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
                        (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {

                        if (gltfModelInstances[i] != GLTFModelInstanceHandle{}) {
                            XrPosef adjustedPose = location.pose * testCase.poseInGripSpace;
                            renderedGLTFs.push_back(
                                GLTFDrawable{gltfModelInstances[i], adjustedPose, {testCase.scale, testCase.scale, testCase.scale}});
                        }
                        else {
                            // loading spinner
                            constexpr int zones = 12;
                            constexpr int darkenCount = 3;
                            auto now = std::chrono::system_clock::now();
                            auto msSinceEpoch = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch();
                            int offset = uint64_t(msSinceEpoch / 250ms) % zones;
                            for (int zone = 0; zone < zones; ++zone) {
                                int darken = std::max(darkenCount - (zone + offset) % zones, 0);
                                float value = 0.5f - 0.125f * darken;
                                auto tintColor = XrColor4f{value, value, value, 1.0f};
                                XrPosef relativePose = {Quat::FromAxisAngle({0, 1, 0}, (2 * MATH_PI / zones) * zone)};
                                XrVector3f radialOffset = {0, 0, 0.1f};
                                XrPosef_TransformVector3f(&relativePose.position, &relativePose, &radialOffset);
                                XrPosef adjustedPose = location.pose * testCase.poseInGripSpace;
                                renderedCubes.push_back(Cube{adjustedPose, {0.02f, 0.02f, 0.1f}, tintColor});
                            }
                        }
                    }
                }
            }
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
                                                                   RenderParams().Draw(renderedCubes).Draw(renderedGLTFs));
                    });
                }

                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
            }

            if (!interactiveLayerManager.EndFrame(frameState, layers)) {
                // user has marked this test as complete
                if (!std::all_of(gltfModelInstances.begin(), gltfModelInstances.end(),
                                 [](auto v) { return v == GLTFModelInstanceHandle{}; })) {
                    // at least one model handle is valid
                    testCaseIdx++;
                    testCaseInitialized = false;
                    return (testCaseIdx < ArraySize(testCases));
                }
            }
            return true;
        };
        RenderLoop(session, updateLayers).Loop();
    }
}  // namespace Conformance
