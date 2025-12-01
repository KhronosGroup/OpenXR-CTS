// Copyright (c) 2019-2025 The Khronos Group Inc.
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

#include "common/xr_linear.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "utilities/ballistics.h"
#include "utilities/throw_helpers.h"
#include "utilities/xr_math_operators.h"
#include "utilities/colors.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <array>

using namespace Conformance;

namespace Conformance
{
    using namespace openxr::math_operators;

    TEST_CASE("VisualAccessibility", "[composition][interactive][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test visual accessibility without a graphics plugin");
        }

        const char* instructions =
            "This test evaluates visual accessibility features.\n"
            "Section 1: Contrast ratios - verify text is readable against backgrounds.\n"
            "Section 2: Text sizes - check readability at different scales.\n"
            "Section 3: Distance scaling - validate UI elements at various distances.\n"
            "Press SELECT to pass if all sections are accessible, MENU to fail if issues occur.";
        constexpr XrColor4f White = {1.0f, 1.0f, 1.0f, 1.0f};
        constexpr XrColor4f LightGray = {0.6f, 0.6f, 0.6f, 1.0f};

        CompositionHelper compositionHelper("Visual Accessibility");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "contrast_ratio.png", instructions);

        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        // Section 1: Contrast Ratio Tests
        SECTION("Contrast Ratios")
        {

            interactiveLayerManager.Configure("contrast_ratio.png",
                                              "Section 1: Contrast Ratios\nVerify text is readable against different backgrounds.");

            constexpr int ImageColCount = 2;
            constexpr int ImageRowCount = 2;
            constexpr int CellWidth = 512;
            constexpr int CellHeight = 512;
            constexpr int ImageWidth = CellWidth * ImageColCount;
            constexpr int ImageHeight = CellHeight * ImageRowCount;

            auto swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(
                ImageWidth, ImageHeight, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, GetGlobalData().graphicsPlugin->GetSRGBA8Format());
            swapchainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            const XrSwapchain contrastSwapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

            compositionHelper.AcquireWaitReleaseImage(contrastSwapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                RGBAImage image(ImageWidth, ImageHeight);

                auto drawContrastBox = [&](int col, int row, const char* text, XrColor4f bgColor, XrColor4f textColor) {
                    XrRect2Di cellRect{{col * CellWidth, row * CellHeight}, {CellWidth, CellHeight}};
                    image.DrawRect(cellRect.offset.x, cellRect.offset.y, cellRect.extent.width, cellRect.extent.height, bgColor);
                    image.PutText(cellRect, text, 64, textColor);
                };

                drawContrastBox(0, 0, "High Contrast", Colors::Black, White);
                drawContrastBox(1, 0, "Low Contrast", Colors::Gray, LightGray);
                drawContrastBox(0, 1, "Good Contrast", Colors::Blue, Colors::Yellow);
                drawContrastBox(1, 1, "High Contrast", White, Colors::Black);

                image.ConvertToSRGB();
                GetGlobalData().graphicsPlugin->CopyRGBAImage(swapchainImage, 0, image);
            });

            XrCompositionLayerQuad* contrastQuad =
                compositionHelper.CreateQuadLayer(contrastSwapchain, viewSpace, 2.0f, XrPosef{Quat::Identity, {0, -0.25f, -2.0f}});
            interactiveLayerManager.AddLayer(contrastQuad);
        }

        // Section 2: Text Size Tests
        SECTION("Text Sizes")
        {

            interactiveLayerManager.Configure("text_sizes.png",
                                              "Section 2: Text Sizes\nCheck readability of text at different font sizes.");

            constexpr int ImageColCount = 2;
            constexpr int ImageRowCount = 2;
            constexpr int CellWidth = 512;
            constexpr int CellHeight = 256;
            constexpr int ImageWidth = CellWidth * ImageColCount;
            constexpr int ImageHeight = CellHeight * ImageRowCount;

            auto swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(
                ImageWidth, ImageHeight, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, GetGlobalData().graphicsPlugin->GetSRGBA8Format());
            swapchainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            const XrSwapchain textSizeSwapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

            compositionHelper.AcquireWaitReleaseImage(textSizeSwapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                RGBAImage image(ImageWidth, ImageHeight);
                image.DrawRect(0, 0, ImageWidth, ImageHeight, White);

                auto drawTextSize = [&](int col, int row, const char* text, int fontSize) {
                    XrRect2Di cellRect{{col * CellWidth, row * CellHeight}, {CellWidth, CellHeight}};
                    image.PutText(cellRect, text, fontSize, Colors::Black);
                };

                drawTextSize(0, 0, "Small Text", 48);
                drawTextSize(1, 0, "Medium Text", 64);
                drawTextSize(0, 1, "Large Text", 96);
                drawTextSize(1, 1, "Extra Large", 112);

                image.ConvertToSRGB();
                GetGlobalData().graphicsPlugin->CopyRGBAImage(swapchainImage, 0, image);
            });

            XrCompositionLayerQuad* textSizeQuad =
                compositionHelper.CreateQuadLayer(textSizeSwapchain, viewSpace, 1.5f, XrPosef{Quat::Identity, {0, -0.2f, -2.0f}});
            interactiveLayerManager.AddLayer(textSizeQuad);
        }
        // Section 3: Distance Scaling Tests
        SECTION("Distance Scaling")
        {

            interactiveLayerManager.Configure("distance_scaling.png",
                                              "Section 3: Distance Scaling\nValidate UI elements are legible at various distances.");

            // Create UI elements at different distances
            const char* distanceTexts[] = {"X", "2X", "3X", "4X"};
            const float scales[] = {0.5f, 0.45f, 0.4f, 0.35f};
            const float depths[] = {-1.5f, -2.0f, -2.5f, -3.0f};
            const float x_positions[] = {-0.75f, -0.25f, 0.25f, 0.75f};
            const float y_positions[] = {0.4f, 0.1f, -0.2f, -0.5f};

            for (int i = 0; i < 4; ++i) {
                constexpr int imageWidth = 256;
                constexpr int imageHeight = 256;
                RGBAImage distanceImage(imageWidth, imageHeight);
                distanceImage.DrawRect(0, 0, imageWidth, imageHeight, White);
                distanceImage.PutText(XrRect2Di{{0, 0}, {imageWidth, imageHeight}}, distanceTexts[i], 100, Colors::Black);

                const XrSwapchain distanceSwapchain = compositionHelper.CreateStaticSwapchainImage(distanceImage);
                XrPosef pose{Quat::Identity, {x_positions[i], y_positions[i], depths[i]}};
                XrCompositionLayerQuad* distanceQuad = compositionHelper.CreateQuadLayer(distanceSwapchain, viewSpace, scales[i], pose);
                interactiveLayerManager.AddLayer(distanceQuad);
            }
        }

        // Run the interactive test loop
        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

}  // namespace Conformance