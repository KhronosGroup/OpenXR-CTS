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
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/array_size.h"
#include "utilities/bitmask_generator.h"
#include "utilities/bitmask_to_string.h"
#include "utilities/xrduration_literals.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <openxr/openxr.h>

#include <array>
#include <cmath>
#include <ostream>
#include <sstream>
#include <vector>

namespace Conformance
{
    // This implements an automated programmatic test of the equirect layer. However, a separate visual
    // test is required in order to validate that it looks correct.
    TEST_CASE("XR_KHR_composition_layer_equirect", "[XR_KHR_composition_layer_equirect]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME)) {
            SKIP(XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME " not supported");
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Test run not using graphics plugin");
        }

        auto graphicsPlugin = globalData.GetGraphicsPlugin();

        AutoBasicInstance instance({XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME});
        AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                     AutoBasicSession::createSpaces,
                                 instance);

        FrameIterator frameIterator(&session);
        frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

        // At this point we have a session ready for us to generate custom frames for.
        // The current XrSessionState is XR_SESSION_STATE_FOCUSED.

        // Create a stereo equirect.
        XrSwapchain swapchainPair[2];
        XrExtent2Di extents{256, 256};

        XrResult result = CreateColorSwapchain(session.GetSession(), graphicsPlugin.get(), &swapchainPair[0], &extents);
        REQUIRE_RESULT_SUCCEEDED(result);
        SwapchainCHECK swapchainCHECK0(swapchainPair[0]);  // Auto-deletes the swapchain.

        result = CreateColorSwapchain(session.GetSession(), graphicsPlugin.get(), &swapchainPair[1], &extents);
        REQUIRE_RESULT_SUCCEEDED(result);
        SwapchainCHECK swapchainCHECK1(swapchainPair[1]);  // Auto-deletes the swapchain.

        result = CycleToNextSwapchainImage(swapchainPair, 2, 3_xrSeconds);
        REQUIRE_RESULT_SUCCEEDED(result);

        // typedef struct XrCompositionLayerEquirectKHR {
        //     XrStructureType             type;
        //     const void* XR_MAY_ALIAS    next;
        //     XrCompositionLayerFlags     layerFlags;
        //     XrSpace                     space;
        //     XrEyeVisibility             eyeVisibility;
        //     XrSwapchainSubImage         subImage;
        //     XrPosef                     pose;
        //     float                       radius;
        //     XrVector2f                  scale;
        //     XrVector2f                  bias;
        // } XrCompositionLayerEquirectKHR;

        auto&& layerFlagsGenerator = bitmaskGeneratorIncluding0({XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
                                                                 XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
                                                                 XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT});
        std::array<XrEyeVisibility, 3> eyeVisibilityArray{XR_EYE_VISIBILITY_BOTH, XR_EYE_VISIBILITY_LEFT /* just these two */};

        while (layerFlagsGenerator.next()) {
            CAPTURE(XrCompositionLayerFlagsCPP(layerFlagsGenerator.get()));
            for (XrSpace space : session.spaceVector) {
                for (XrEyeVisibility eyeVisibility : eyeVisibilityArray) {
                    std::array<float, 3> radiusTestArray{0, 1.f, INFINITY};  // Spec explicitly supports radius 0 and +infinity

                    for (float radius : radiusTestArray) {
                        std::array<XrQuaternionf, 4> orientationTestArray{
                            Quat::Identity,                                   // No rotation; looking down the +x axis
                            XrQuaternionf{0, 0.7071f, 0, 0.7071f},            // 90 degree rotation around y axis; looking down the -z axis.
                            XrQuaternionf{0, 0, 0.7071f, 0.7071f},            // 90 degree rotation around z axis; looking down the +y axis.
                            XrQuaternionf{-0.709f, 0.383f, -0.381f, -0.454f}  // Misc value.
                        };

                        for (const XrQuaternionf& orientation : orientationTestArray) {
                            FrameIterator::RunResult runResult = frameIterator.PrepareSubmitFrame();
                            REQUIRE(runResult == FrameIterator::RunResult::Success);

                            // Set up our equirect layer. We always make two, and some of the time we
                            // split them into left and right eye layers. If we have a left eye then
                            // there must be a following layer which is the right eye.
                            std::vector<XrCompositionLayerEquirectKHR> equirectLayerArray(2, {XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR});

                            equirectLayerArray[0].layerFlags = layerFlagsGenerator.get();
                            equirectLayerArray[1].layerFlags = layerFlagsGenerator.get();

                            equirectLayerArray[0].space = space;
                            equirectLayerArray[1].space = space;

                            equirectLayerArray[0].eyeVisibility = eyeVisibility;
                            equirectLayerArray[1].eyeVisibility =
                                ((eyeVisibility == XR_EYE_VISIBILITY_LEFT) ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH);

                            equirectLayerArray[0].subImage.swapchain = swapchainPair[0];
                            equirectLayerArray[1].subImage.swapchain = swapchainPair[1];
                            equirectLayerArray[0].subImage.imageRect = {{0, 0}, extents};
                            equirectLayerArray[1].subImage.imageRect = {{0, 0}, extents};
                            equirectLayerArray[0].subImage.imageArrayIndex = 0;  // We're not using index
                            equirectLayerArray[1].subImage.imageArrayIndex = 0;  // arrays here.

                            // pose is an XrPosef defining the position and orientation of the center point of the sphere onto
                            // which the equirect image data is mapped, relative to the reference frame of the space.
                            equirectLayerArray[0].pose = {orientation, XrVector3f{0, 0, 0}};
                            equirectLayerArray[1].pose = {orientation, XrVector3f{0, 0, 0}};

                            // "radius is the non-negative radius of the sphere onto which the equirect image data is mapped.
                            //  Values of zero or floating point positive infinity are treated as an infinite sphere."
                            equirectLayerArray[0].radius = radius;
                            equirectLayerArray[1].radius = radius;

                            // scale is an XrVector2f indicating a scale of the texture coordinates after the mapping to 2D.
                            equirectLayerArray[0].scale = XrVector2f{1.f, 1.f};
                            equirectLayerArray[1].scale = XrVector2f{1.f, 1.f};

                            // bias is an XrVector2f indicating a bias of the texture coordinates after the mapping to 2D.
                            equirectLayerArray[0].bias = XrVector2f{0.f, 0.f};
                            equirectLayerArray[1].bias = XrVector2f{0.f, 0.f};

                            const XrCompositionLayerBaseHeader* headerPtrArray[3] = {
                                reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection),
                                reinterpret_cast<const XrCompositionLayerBaseHeader*>(&equirectLayerArray[0]),
                                reinterpret_cast<const XrCompositionLayerBaseHeader*>(&equirectLayerArray[1])};
                            frameIterator.frameEndInfo.layerCount = 3;
                            frameIterator.frameEndInfo.layers = headerPtrArray;

                            // xrEndFrame requires the XR_KHR_composition_layer_equirect extension to be enabled or else
                            // it will return XR_ERROR_LAYER_INVALID.
                            result = xrEndFrame(session.GetSession(), &frameIterator.frameEndInfo);
                            CHECK(result == XR_SUCCESS);
                        }
                    }
                }
            }
        }

        // Leave
        result = xrRequestExitSession(session.GetSession());
        CHECK(result == XR_SUCCESS);

        frameIterator.RunToSessionState(XR_SESSION_STATE_STOPPING);
    }

    struct EquirectTestCase
    {

        const char* name;
        const char* description;
        XrReferenceSpaceType spaceType;
        XrPosef pose;
        float radius;
        XrVector2f scale;
        XrVector2f bias;
        const char* imagePath;
        XrRect2Df crop;  // normalised
        const char* exampleImagePath;
    };

    const EquirectTestCase equirectTestCases[] = {
        {
            "Full sphere at infinity",
            "A 360 view of the inside of a cube at infinity",
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            {Quat::Identity, {0, 0, 0}},
            0.0,  // infinity
            {1.0, 1.0},
            {0.0, 0.0},
            "equirect_8k.png",
            {{0, 0}, {1, 1}},
            "equirect_local_space.jpg",
        },
        {
            "Full sphere at infinity (view space)",
            "A 360 view of the inside of a cube at infinity, rendered in view space",
            XR_REFERENCE_SPACE_TYPE_VIEW,
            {Quat::Identity, {0, 0, 0}},
            0.0,  // infinity
            {1.0, 1.0},
            {0.0, 0.0},
            "equirect_8k.png",
            {{0, 0}, {1, 1}},
            "equirect_view_space.jpg",
        },
        {
            "Full sphere at 2m",
            "A 2m sphere with the same cube test image. "
            "Example is shown from above and to the left of the origin to make the perspective effect clear.",
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            {Quat::Identity, {0, 0, 0}},
            2.0,
            {1.0, 1.0},
            {0.0, 0.0},
            "equirect_8k.png",
            {{0, 0}, {1, 1}},
            "equirect_finite.jpg",
        },
        {
            "Full sphere at 2m with pose",
            "A 2m sphere with the same cube test image, forward by 1.5m and rotated downward",
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            {Quat::FromAxisAngle({1, 0, 0}, DegToRad(45)), {0, 0, -1.5}},
            2.0,
            {1.0, 1.0},
            {0.0, 0.0},
            "equirect_8k.png",
            {{0, 0}, {1, 1}},
            "equirect_finite_pose.jpg",
        },
        {
            "90 degree section at infinity (cropped file)",
            "A 90 degree section in both latitude and longitude, rendered at infinity",
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            {Quat::Identity, {0, 0, 0}},
            0.0,  // infinity
            {0.25, 0.5},
            {0.0, 0.0},
            "equirect_central_90.png",
            {{0, 0}, {1, 1}},
            "equirect_central_90.jpg",
        },
        {
            "90 degree section at infinity (cropped image extents)",
            "A 90 degree section in both latitude and longitude, rendered at infinity",
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            {Quat::Identity, {0, 0, 0}},
            0.0,  // infinity
            {0.25, 0.5},
            {0.0, 0.0},
            "equirect_8k.png",
            {{3 / 8., 2 / 8.}, {1 / 4., 2 / 4.}},
            "equirect_central_90.jpg",
        },
    };

    static RGBAImageCache& EquirectImageCache()
    {
        static RGBAImageCache imageCache{};
        imageCache.Init();
        return imageCache;
    }
    TEST_CASE("XR_KHR_composition_layer_equirect-interactive", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Test run not using graphics plugin");
        }

        if (!globalData.IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME)) {
            SKIP(XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME " not supported");
        }

        auto testCaseIdx = GENERATE(Catch::Generators::range<size_t>(0, ArraySize(equirectTestCases)));
        auto testCase = equirectTestCases[testCaseIdx];
        // technically redundant because GENERATE() makes a "section", but this makes the test more usable.
        DYNAMIC_SECTION("Test condition name: " << testCase.name)
        {
            INFO("Test condition description: " << testCase.description);
            std::string testTitle = SubtestTitle("Equirect layer", testCaseIdx, equirectTestCases);
            CompositionHelper compositionHelper(testTitle.c_str(), {XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME});

            std::ostringstream oss;
            oss << testTitle << ": " << testCase.name << '\n';
            oss << testCase.description << '\n';

            InteractiveLayerManager interactiveLayerManager(compositionHelper, testCase.exampleImagePath, oss.str().c_str());
            compositionHelper.GetInteractionManager().AttachActionSets();
            compositionHelper.BeginSession();

            const XrSpace space = compositionHelper.CreateReferenceSpace(testCase.spaceType);

            std::shared_ptr<RGBAImage> image = EquirectImageCache().Load(testCase.imagePath);
            int32_t imageWidth = image->width;
            int32_t imageHeight = image->height;

            XrSwapchainCreateInfo createInfo = compositionHelper.DefaultColorSwapchainCreateInfo(
                imageWidth, imageHeight, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, GetGlobalData().graphicsPlugin->GetSRGBA8Format());

            // copying to this swapchain, not rendering to it
            createInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;

            XrSwapchain swapchain = compositionHelper.CreateSwapchain(createInfo);

            compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                GetGlobalData().graphicsPlugin->CopyRGBAImage(swapchainImage, 0, *image);
            });

            XrCompositionLayerEquirectKHR equirectLayer{XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR};
            equirectLayer.space = space;
            equirectLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            equirectLayer.subImage.swapchain = swapchain;
            equirectLayer.subImage.imageRect = CropImage(imageWidth, imageHeight, testCase.crop);
            equirectLayer.subImage.imageArrayIndex = 0;
            equirectLayer.pose = testCase.pose;
            equirectLayer.radius = testCase.radius;
            equirectLayer.scale = testCase.scale;
            equirectLayer.bias = testCase.bias;

            interactiveLayerManager.AddBackgroundLayer(&equirectLayer);

            RenderLoop(compositionHelper.GetSession(), [&](const XrFrameState& frameState) {
                if (!interactiveLayerManager.EndFrame(frameState)) {
                    // user has marked this test as complete
                    SUCCEED("User has marked this test as passed");
                    return false;
                }
                return true;
            }).Loop();
        }
    }

}  // namespace Conformance
