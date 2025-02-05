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

#include "composition_utils.h"
#include "graphics_plugin.h"
#include "utilities/array_size.h"
#include "utilities/bitmask_to_string.h"
#include "utilities/swapchain_parameters.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"
#include "utilities/xr_math_operators.h"

#include <catch2/catch_test_macros.hpp>

#include <openxr/openxr.h>

#include <cstdint>
#include <vector>

namespace Conformance
{
    using namespace openxr::math_operators;

    static XrSwapchainCreateInfo MakeDefaultSwapchainCreateInfo(int64_t imageFormat, const SwapchainCreateTestParameters& tp)
    {
        XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        createInfo.faceCount = 1;
        createInfo.format = imageFormat;
        XrSwapchainCreateFlagsRefCPP(createInfo.createFlags) = tp.createFlagsVector[0];
        XrSwapchainUsageFlagsRefCPP(createInfo.usageFlags) = tp.usageFlagsVector[0];
        createInfo.sampleCount = 1;
        createInfo.width = 64;
        createInfo.height = 64;
        createInfo.arraySize = tp.arrayCountVector[0];
        createInfo.mipCount = tp.mipCountVector[0];
        return createInfo;
    }

    namespace
    {

        constexpr int32_t labelImageWidth = 512;
        constexpr int32_t labelImageHeight = 32;
        constexpr int32_t labelFontSize = 18;  // some format names are long!
        constexpr float labelWidth = 2.0f;
        constexpr float labelHeight = labelWidth * labelImageHeight / labelImageWidth;
        constexpr float labelMargin = 0.2f;
        inline XrCompositionLayerQuad* MakeFormatLabel(CompositionHelper& compositionHelper, XrSpace viewSpace, const std::string& label,
                                                       const XrVector3f& position)
        {

            return compositionHelper.CreateQuadLayer(
                compositionHelper.CreateStaticSwapchainImage(
                    CreateTextImage(labelImageWidth, labelImageHeight, label.c_str(), labelFontSize, WordWrap::Disabled)),
                viewSpace, labelWidth, {Quat::Identity, position});
        }

        constexpr int32_t aspectRatio = 8;
        constexpr int32_t gradientImageHeight = 32;
        constexpr int32_t gradientImageWidth = gradientImageHeight * aspectRatio;
        constexpr float gradientWidth = 1.0f;
        constexpr float gradientHeight = gradientWidth / aspectRatio;
        constexpr float quadZ = -3;      // How far away quads are placed.
        constexpr float margin = 0.02f;  // the gap between quads
        constexpr float yOffset = (gradientHeight + margin) / 2;

        inline MeshHandle MakeGradientMesh(IGraphicsPlugin& graphicsPlugin, SwapchainFormat::RawColorComponents referenceComponents)
        {
            // 0-2
            // |/|
            // 1-3
            std::vector<Geometry::Vertex> vertices;
            vertices.reserve(2 * gradientImageWidth);
            for (int col = 0; col < gradientImageWidth; col++) {
                float value = col / (float)gradientImageWidth;

                // if any of the other format's color channels aren't present,
                // they should be sampled as zero, so also zero them here to match
                using Components = SwapchainFormat::RawColorComponents;
                XrColor4f color{
                    referenceComponents & Components::r ? value : 0.0f,
                    referenceComponents & Components::g ? value : 0.0f,
                    referenceComponents & Components::b ? value : 0.0f,
                    1.0,
                };

                color = ColorUtils::FromSRGB(color);  // perceptual gradient instead of linear
                float x = -(gradientWidth / 2) + gradientWidth * value;
                vertices.push_back(Geometry::Vertex{{x, gradientHeight / 2, 0}, {color.r, color.g, color.b}});
                vertices.push_back(Geometry::Vertex{{x, -gradientHeight / 2, 0}, {color.r, color.g, color.b}});
            }
            const uint16_t quadIndices[] = {1, 0, 2, 2, 3, 1};
            std::vector<uint16_t> indices;
            indices.reserve((gradientImageWidth - 1) * ArraySize(quadIndices));
            for (int col = 0; col < gradientImageWidth - 1; col++) {
                for (const uint16_t index : quadIndices) {
                    indices.push_back((uint16_t)(col * 2 + index));
                }
            }

            return graphicsPlugin.MakeSimpleMesh(indices, vertices);
        }
    }  // namespace

    TEST_CASE("GradientFormatsLinearVsNonLinear", "[composition][interactive]")
    {
        const GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            // Nothing to check - no graphics plugin means no swapchain
            SKIP("Cannot test swapchains without a graphics plugin");
        }

        CompositionHelper compositionHelper("Linear vs Non-Linear");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "gradient_linearity.png",
                                                        "Both gradients should match both each other"
                                                        " and the example image, except for banding artifacts,"
                                                        " and should appear perceptually linear."
                                                        " Banding may introduce small color artifacts, but"
                                                        " the gradients should be broadly the same color.");

        XrSession session = compositionHelper.GetSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        // Enumerate formats
        std::vector<int64_t> imageFormatArray;
        {
            uint32_t countOutput = 0;
            XRC_CHECK_THROW_XRCMD_UNQUALIFIED_SUCCESS(xrEnumerateSwapchainFormats(session, 0, &countOutput, nullptr));
            if (countOutput > 0) {
                imageFormatArray.resize(countOutput, XRC_INVALID_IMAGE_FORMAT);
                XRC_CHECK_THROW_XRCMD_UNQUALIFIED_SUCCESS(
                    xrEnumerateSwapchainFormats(session, countOutput, &countOutput, imageFormatArray.data()));
            }
        }

        int64_t defaultFormat = globalData.graphicsPlugin->GetSRGBA8Format();
        SwapchainCreateTestParameters defaultTestParameters;
        REQUIRE(globalData.graphicsPlugin->GetSwapchainCreateTestParameters(defaultFormat, &defaultTestParameters));

        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        auto doInteractiveRenderTest = [&](int64_t projFormat, const SwapchainCreateTestParameters& projTestParameters,  //
                                           int64_t quadFormat, const SwapchainCreateTestParameters& quadTestParameters) {
            // Set up composition projection layer and swapchains (one swapchain per view).
            std::vector<XrSwapchain> swapchains;
            XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(viewSpace);
            {
                const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();
                XrSwapchainCreateInfo createInfo = MakeDefaultSwapchainCreateInfo(projFormat, projTestParameters);
                for (uint32_t j = 0; j < projLayer->viewCount; j++) {
                    // this may need to be rounded up for compressed formats (maybe compensated for with a subimage?)
                    createInfo.width = viewProperties[j].recommendedImageRectWidth;
                    createInfo.height = viewProperties[j].recommendedImageRectHeight;
                    const XrSwapchain swapchain = compositionHelper.CreateSwapchain(createInfo);
                    const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = compositionHelper.MakeDefaultSubImage(swapchain, 0);
                    swapchains.push_back(swapchain);
                }
            }

            // Set up swapchain for composition quad layer
            XrSwapchain gradientQuadSwapchain;
            {
                XrSwapchainCreateInfo createInfo = MakeDefaultSwapchainCreateInfo(quadFormat, quadTestParameters);
                // this may need to be rounded up for compressed formats (maybe compensated for with a subimage?)
                createInfo.width = gradientImageWidth;
                createInfo.height = gradientImageHeight;
                gradientQuadSwapchain = compositionHelper.CreateSwapchain(createInfo);
            }

            const float labelYOffset = gradientHeight + (labelHeight / 2) + labelMargin;

            XrCompositionLayerQuad* const projFormatLabelQuad = MakeFormatLabel(
                compositionHelper, viewSpace, "Projection: " + projTestParameters.imageFormatName, {0, labelYOffset, quadZ});
            interactiveLayerManager.AddLayer(projFormatLabelQuad);

            XrCompositionLayerQuad* const quadFormatLabelQuad =
                MakeFormatLabel(compositionHelper, viewSpace, "Quad: " + quadTestParameters.imageFormatName, {0, -labelYOffset, quadZ});
            interactiveLayerManager.AddLayer(quadFormatLabelQuad);

            XrCompositionLayerQuad* gradientQuad = compositionHelper.CreateQuadLayer(gradientQuadSwapchain, viewSpace, gradientWidth,
                                                                                     XrPosef{Quat::Identity, {0, -yOffset, quadZ}});
            interactiveLayerManager.AddLayer(gradientQuad);

            // Each mesh should only write non-zero to the color components that the **other** format supports, which is why the below looks backwards.
            // We could do an intersection of the two format components, but e.g. writing all white to a format with fewer channels
            // may expose errors that writing something more limited would not.
            MeshHandle quadMesh = MakeGradientMesh(*globalData.graphicsPlugin, projTestParameters.colorComponents);
            MeshHandle projMesh = MakeGradientMesh(*globalData.graphicsPlugin, quadTestParameters.colorComponents);

            auto updateLayers = [&](const XrFrameState& frameState) {
                auto viewData = compositionHelper.LocateViews(viewSpace, frameState.predictedDisplayTime);
                const auto& viewState = std::get<XrViewState>(viewData);

                std::vector<XrCompositionLayerBaseHeader*> layers;
                if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                    viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                    const auto& views = std::get<std::vector<XrView>>(viewData);

                    const uint32_t imageArrayIndex = 0;
                    // Render the gradient into the quad's swapchain
                    compositionHelper.AcquireWaitReleaseImage(gradientQuadSwapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, imageArrayIndex, Colors::Black);

                        XrCompositionLayerProjectionView view = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                        const float quadRenderDistance = 1.0;
                        const float halfAngleX = std::atan2(gradientWidth / 2, quadRenderDistance);
                        const float halfAngleY = std::atan2(gradientHeight / 2, quadRenderDistance);
                        view.fov = {-halfAngleX, halfAngleX, halfAngleY, -halfAngleY};
                        view.pose = Pose::Identity;
                        view.subImage = compositionHelper.MakeDefaultSubImage(gradientQuadSwapchain, imageArrayIndex);

                        auto quadMeshList = {MeshDrawable{quadMesh, {Quat::Identity, {0, 0, -quadRenderDistance}}}};
                        GetGlobalData().graphicsPlugin->RenderView(view, swapchainImage, RenderParams().Draw(quadMeshList));
                    });

                    auto projMeshList = {MeshDrawable{projMesh, {Quat::Identity, {0, yOffset, quadZ}}}};
                    // Render into each of the separate swapchains using the projection layer view fov and pose.
                    for (size_t view = 0; view < views.size(); view++) {
                        compositionHelper.AcquireWaitReleaseImage(swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, imageArrayIndex, Colors::Black);
                            const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                            const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                            GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage,
                                                                       RenderParams().Draw(projMeshList));
                        });
                    }

                    layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
                }
                return interactiveLayerManager.EndFrame(frameState, layers);
            };

            RenderLoop(session, updateLayers).Loop();
        };

        for (int64_t imageFormat : imageFormatArray) {

            SwapchainCreateTestParameters tp;
            REQUIRE(globalData.graphicsPlugin->GetSwapchainCreateTestParameters(imageFormat, &tp));

            if (!tp.supportsRendering) {
                // currently, we render to the format,
                // but we could generate the image
                // or render to another format and copy.
                continue;
            }
            if (!tp.colorFormat) {
                // we are testing by visual inspection
                continue;
            }
            if (tp.colorIntegerRange != SwapchainFormat::ColorIntegerRange::NoIntegerColor) {
                // unsure whether and how non-normalized
                // integer formats map to the screen
                continue;
            }

            DYNAMIC_SECTION(tp.imageFormatName)
            {
                CAPTURE(tp.imageFormatName);

                SECTION("Custom projection layer format")
                {
                    INFO("Formats: projection: " << tp.imageFormatName << ", quad: " << defaultTestParameters.imageFormatName
                                                 << " (default)");
                    doInteractiveRenderTest(imageFormat, tp, defaultFormat, defaultTestParameters);
                }
                if (imageFormat == defaultFormat) {
                    // compare proj to quad, but no point doing it twice
                    continue;
                }
                SECTION("Custom quad layer format")
                {
                    INFO("Formats: projection: " << defaultTestParameters.imageFormatName << " (default), quad: " << tp.imageFormatName);
                    doInteractiveRenderTest(defaultFormat, defaultTestParameters, imageFormat, tp);
                }
            }

            // SwapchainScoped will have xrDestroySwapchain
            // now we need to flush
            GetGlobalData().graphicsPlugin->ClearSwapchainCache();
            GetGlobalData().graphicsPlugin->Flush();
        }
    }
}  // namespace Conformance
