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

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <openxr/openxr.h>

#include <cmath>
#include <ostream>
#include <sstream>
#include <vector>

namespace Conformance
{
    namespace
    {
        static const float pi = std::acos(-1.0f);
    }
    struct Equirect2TestCase
    {

        const char* name;
        const char* description;
        XrReferenceSpaceType spaceType;
        XrPosef pose;
        float radius;
        float centralHorizontalAngle;
        float upperVerticalAngle;
        float lowerVerticalAngle;
        const char* imagePath;
        XrRect2Df crop;  // normalised
        const char* exampleImagePath;
    };

    const Equirect2TestCase equirect2TestCases[] = {
        {
            "Full sphere at infinity",
            "A 360 view of the inside of a cube at infinity",
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            {Quat::Identity, {0, 0, 0}},
            0.0f,  // infinity
            pi * 2.f,
            pi / 2.f,
            -pi / 2.f,
            "equirect_8k.png",
            {{0, 0}, {1, 1}},
            "equirect_local_space.jpg",
        },
        {
            "Full sphere at infinity (view space)",
            "A 360 view of the inside of a cube at infinity, rendered in view space",
            XR_REFERENCE_SPACE_TYPE_VIEW,
            {Quat::Identity, {0, 0, 0}},
            0.0f,  // infinity
            pi * 2.f,
            pi / 2.f,
            -pi / 2.f,
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
            2.0f,
            pi * 2.f,
            pi / 2.f,
            -pi / 2.f,
            "equirect_8k.png",
            {{0, 0}, {1, 1}},
            "equirect_finite.jpg",
        },
        {
            "Full sphere at 2m with pose",
            "A 2m sphere with the same cube test image, forward by 1.5m and rotated downward",
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            {Quat::FromAxisAngle({1, 0, 0}, DegToRad(45)), {0, 0, -1.5}},
            2.0f,
            pi * 2.f,
            pi / 2.f,
            -pi / 2.f,
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
            pi / 2.f,
            pi / 4.f,
            -pi / 4.f,
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
            pi / 2.f,
            pi / 4.f,
            -pi / 4.f,
            "equirect_8k.png",
            {{3 / 8., 2 / 8.}, {1 / 4., 2 / 4.}},
            "equirect_central_90.jpg",
        },
    };

    static RGBAImageCache& Equirect2ImageCache()
    {
        static RGBAImageCache imageCache{};
        imageCache.Init();
        return imageCache;
    }
    TEST_CASE("XR_KHR_composition_layer_equirect2-interactive", "[composition][interactive][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME)) {
            SKIP(XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME " not supported");
        }

        auto testCaseIdx = GENERATE(Catch::Generators::range<size_t>(0, ArraySize(equirect2TestCases)));
        auto testCase = equirect2TestCases[testCaseIdx];
        // technically redundant because GENERATE() makes a "section", but this makes the test more usable.
        DYNAMIC_SECTION("Test condition name: " << testCase.name)
        {
            INFO("Test condition description: " << testCase.description);

            std::string testTitle = SubtestTitle("Equirect2 layer", testCaseIdx, equirect2TestCases);
            CompositionHelper compositionHelper(testTitle.c_str(), {XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME});

            std::ostringstream oss;
            oss << testTitle << ": " << testCase.name << '\n';
            oss << testCase.description << '\n';

            InteractiveLayerManager interactiveLayerManager(compositionHelper, testCase.exampleImagePath, oss.str().c_str());
            compositionHelper.GetInteractionManager().AttachActionSets();
            compositionHelper.BeginSession();

            const XrSpace space = compositionHelper.CreateReferenceSpace(testCase.spaceType);

            std::shared_ptr<RGBAImage> image = Equirect2ImageCache().Load(testCase.imagePath);
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

            XrCompositionLayerEquirect2KHR equirect2Layer{XR_TYPE_COMPOSITION_LAYER_EQUIRECT2_KHR};
            equirect2Layer.space = space;
            equirect2Layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            equirect2Layer.subImage.swapchain = swapchain;
            equirect2Layer.subImage.imageRect = CropImage(imageWidth, imageHeight, testCase.crop);
            equirect2Layer.subImage.imageArrayIndex = 0;
            equirect2Layer.pose = testCase.pose;
            equirect2Layer.radius = testCase.radius;
            equirect2Layer.centralHorizontalAngle = testCase.centralHorizontalAngle;
            equirect2Layer.upperVerticalAngle = testCase.upperVerticalAngle;
            equirect2Layer.lowerVerticalAngle = testCase.lowerVerticalAngle;

            interactiveLayerManager.AddBackgroundLayer(&equirect2Layer);

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
