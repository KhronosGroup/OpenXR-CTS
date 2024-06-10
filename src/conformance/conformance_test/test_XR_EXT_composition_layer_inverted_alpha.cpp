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

#include "common/xr_linear.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <cstring>
#include <limits>
#include <vector>

namespace Conformance
{
    // Purpose: Validates alpha blending (both premultiplied and unpremultiplied), when alpha channel stores transparency instead of opacity.
    TEST_CASE("XR_EXT_composition_layer_inverted_alpha", "[composition][interactive][no_auto]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_COMPOSITION_LAYER_INVERTED_ALPHA_EXTENSION_NAME)) {
            SKIP(XR_EXT_COMPOSITION_LAYER_INVERTED_ALPHA_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper("SrcAlphaBlend (Inverted Alpha)", {XR_EXT_COMPOSITION_LAYER_INVERTED_ALPHA_EXTENSION_NAME});
        // Use the same example image as "SourceAlphaBlending" since the final blending result should be the same.
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "source_alpha_blending.png",
                                                        "All three squares should have an identical blue-green gradient.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        constexpr float QuadZ = -3;  // How far away quads are placed.

        // Creates image with correctly combined green and blue gradient (this is the the source of truth).
        {
            Conformance::RGBAImage blueGradientOverGreen(256, 256);
            for (int y = 0; y < 256; y++) {
                const float t = y / 255.0f;
                const XrColor4f dst = Colors::Green;
                const XrColor4f src{0, 0, t, t};

                // The blended color here has a 1 alpha value to test that the runtime is ignoring the texture alpha when
                // the XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT flag is not set. If the runtime is erroneously
                // reading texture alpha, it is more likely to output black pixels.
                const XrColor4f blended{dst.r * (1 - src.a) + src.r, dst.g * (1 - src.a) + src.g, dst.b * (1 - src.a) + src.b, 1};
                blueGradientOverGreen.DrawRect(0, y, blueGradientOverGreen.width, 1, blended);
            }

            const XrSwapchain answerSwapchain = compositionHelper.CreateStaticSwapchainImage(blueGradientOverGreen);
            XrCompositionLayerQuad* truthQuad =
                compositionHelper.CreateQuadLayer(answerSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {0, 0, QuadZ}});

            // Set the unpremultiplied bit on this quad (and the green ones below) to make it more obvious when a runtime
            // supports the premultiplied flag but not the texture flag. Without this bit set, the final color will be:
            //   ( 1 - inverted_alpha ) * dst + src
            // dst is black, and alpha is 0, so the output is just src.
            // If we use unpremultiplied, the formula becomes:
            //   ( 1 - inverted_alpha ) * dst + alpha * src
            // which results in black pixels and is obviously wrong.
            truthQuad->layerFlags |= XR_COMPOSITION_LAYER_INVERTED_ALPHA_BIT_EXT;
            truthQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;

            interactiveLayerManager.AddLayer(truthQuad);
        }

        auto createGradientTest = [&](bool premultiplied, float x, float y) {
            // A solid green quad layer will be composited under a blue gradient.
            {
                const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::GreenZeroAlpha);
                XrCompositionLayerQuad* greenQuad =
                    compositionHelper.CreateQuadLayer(greenSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {x, y, QuadZ}});
                greenQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
                interactiveLayerManager.AddLayer(greenQuad);
            }

            // Create gradient of blue lines from 0.0 to 1.0.
            {
                Conformance::RGBAImage blueGradient(256, 256);
                for (int row = 0; row < blueGradient.height; row++) {
                    XrColor4f color{0, 0, 1, row / (float)blueGradient.height};
                    if (premultiplied) {
                        color = XrColor4f{color.r * color.a, color.g * color.a, color.b * color.a, color.a};
                    }

                    // invert the alpha channel to make it stores the transparency value, instead of opacity
                    color.a = 1.0f - color.a;

                    blueGradient.DrawRect(0, row, blueGradient.width, 1, color);
                }

                const XrSwapchain gradientSwapchain = compositionHelper.CreateStaticSwapchainImage(blueGradient);
                XrCompositionLayerQuad* gradientQuad =
                    compositionHelper.CreateQuadLayer(gradientSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {x, y, QuadZ}});

                gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_INVERTED_ALPHA_BIT_EXT;
                if (!premultiplied) {
                    gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
                }

                interactiveLayerManager.AddLayer(gradientQuad);
            }
        };

        createGradientTest(true, -1.02f, 0);  // Test premultiplied (left of center "answer")
        createGradientTest(false, 1.02f, 0);  // Test unpremultiplied (right of center "answer")

        RenderLoop(compositionHelper.GetSession(), [&](const XrFrameState& frameState) {
            return interactiveLayerManager.EndFrame(frameState);
        }).Loop();
    }
}  // namespace Conformance
