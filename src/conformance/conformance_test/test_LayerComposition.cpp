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
#include "conformance_options.h"
#include "utilities/colors.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"
#include "utilities/xr_math_operators.h"
#include "utilities/xrduration_literals.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <openxr/openxr.h>
#include <nonstd/span.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <sstream>

using namespace Conformance;

namespace Conformance
{
    namespace
    {
        /// Inserts depthInfo into the next chain at nextptr with the standard values we use.
        /// @param depthInfo and @param nextPtr cannot be null, and thus could be references,
        /// but avoid pass-by-reference so the use of pointers is obvious at the call-site.
        void InsertDefaultDepthInfo(XrCompositionLayerDepthInfoKHR* depthInfo, const void** nextPtr, XrSwapchainSubImage subImage)
        {
            assert(depthInfo != nullptr);
            assert(nextPtr != nullptr);  // pointee may be nullptr
            depthInfo->type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
            depthInfo->next = *nextPtr;
            depthInfo->minDepth = 0.0f;
            depthInfo->maxDepth = 1.0f;
            depthInfo->nearZ = kNearClip;
            depthInfo->farZ = kFarClip;
            depthInfo->subImage = subImage;
            *nextPtr = depthInfo;
        }
    }  // namespace

    using namespace openxr::math_operators;

    // Purpose: Verify behavior of quad visibility and occlusion with the expectation that:
    // 1. Quads render with painters algo.
    // 2. Quads which are facing away are not visible.
    TEST_CASE("QuadOcclusion", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test QuadOcclusion without a graphics plugin");
        }

        CompositionHelper compositionHelper("Quad Occlusion");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "quad_occlusion.png",
            "This test includes a blue and green quad at Z=-2 with opposite rotations on Y axis forming X. The green quad should be"
            " fully visible due to painter's algorithm. A red quad is facing away and should not be visible.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        const XrSwapchain redSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Red);

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        // Each quad is rotated on Y axis by 45 degrees to form an X.
        // Green is added second so it should draw over the blue quad.
        const XrQuaternionf blueRot = Quat::FromAxisAngle({0, 1, 0}, DegToRad(-45));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(blueSwapchain, viewSpace, 1.0f, XrPosef{blueRot, {0, 0, -2}}));
        const XrQuaternionf greenRot = Quat::FromAxisAngle({0, 1, 0}, DegToRad(45));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(greenSwapchain, viewSpace, 1.0f, XrPosef{greenRot, {0, 0, -2}}));
        // Red quad is rotated away from the viewer and should not be visible.
        const XrQuaternionf redRot = Quat::FromAxisAngle({0, 1, 0}, DegToRad(180));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(redSwapchain, viewSpace, 1.0f, XrPosef{redRot, {0, 0, -1}}));

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    namespace SimpleTestLayers
    {
        class TestLayer
        {
        public:
            virtual XrCompositionLayerBaseHeader* Update(nonstd::span<const XrView> views) = 0;
            virtual ~TestLayer(){};
        };
        class QuadLayer : public TestLayer
        {
        public:
            QuadLayer(CompositionHelper* compositionHelper, XrColor4f color, XrSpace space, XrPosef pose, XrExtent2Df extent)
                : m_compositionHelper(compositionHelper)
            {
                float geoMean = std::sqrt(extent.width * extent.height);  // stick to 256^2 pixels
                XrExtent2Di size = {(int32_t)(extent.width / geoMean * 256), (int32_t)(extent.height / geoMean * 256)};

                m_swapchain = m_compositionHelper->CreateStaticSwapchainSolidColor(color, size);
                m_layer = m_compositionHelper->CreateQuadLayer(m_swapchain, space, extent.width, pose);
            }
            XrCompositionLayerBaseHeader* Update(nonstd::span<const XrView> /* views */) override
            {
                return reinterpret_cast<XrCompositionLayerBaseHeader*>(m_layer);
            }
            ~QuadLayer() override
            {
                m_compositionHelper->DestroySwapchain(m_swapchain);
            }

        private:
            CompositionHelper* m_compositionHelper;
            XrSwapchain m_swapchain;
            XrCompositionLayerQuad* m_layer;
        };
        class ProjectionLayer : public TestLayer
        {
        public:
            ProjectionLayer(CompositionHelper* compositionHelper, XrSpace space, bool opaque) : m_compositionHelper(compositionHelper)
            {
                // Set up composition projection layer and swapchains (one swapchain per view).
                std::vector<XrSwapchain> swapchains;
                m_layer = m_compositionHelper->CreateProjectionLayer(space);
                if (!opaque) {
                    m_layer->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                }
                const std::vector<XrViewConfigurationView> viewProperties = m_compositionHelper->EnumerateConfigurationViews();
                for (uint32_t j = 0; j < m_layer->viewCount; j++) {
                    const XrSwapchain swapchain = m_compositionHelper->CreateSwapchain(m_compositionHelper->DefaultColorSwapchainCreateInfo(
                        viewProperties[j].recommendedImageRectWidth, viewProperties[j].recommendedImageRectHeight, 0,
                        opaque ? -1 : GetGlobalData().graphicsPlugin->GetSRGBA8Format()));
                    const_cast<XrSwapchainSubImage&>(m_layer->views[j].subImage) = m_compositionHelper->MakeDefaultSubImage(swapchain, 0);
                    m_swapchains.push_back(swapchain);
                }
            }
            XrCompositionLayerBaseHeader* Update(nonstd::span<const XrView> views) override
            {
                // Render into each of the separate swapchains using the projection layer view fov and pose.
                for (size_t view = 0; view < views.size(); view++) {
                    m_compositionHelper->AcquireWaitReleaseImage(m_swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, Colors::Transparent);
                        const_cast<XrFovf&>(m_layer->views[view].fov) = views[view].fov;
                        const_cast<XrPosef&>(m_layer->views[view].pose) = views[view].pose;
                        GetGlobalData().graphicsPlugin->RenderView(m_layer->views[view], swapchainImage, RenderParams().Draw(m_drawables));
                    });
                }
                return reinterpret_cast<XrCompositionLayerBaseHeader*>(m_layer);
            }
            ~ProjectionLayer() override
            {
                for (XrSwapchain swapchain : m_swapchains) {
                    m_compositionHelper->DestroySwapchain(swapchain);
                }
            }

        protected:
            std::vector<MeshDrawable> m_drawables;

        private:
            CompositionHelper* m_compositionHelper;
            std::vector<XrSwapchain> m_swapchains;
            XrCompositionLayerProjection* m_layer;
        };
        class ProjectionQuadLayer : public ProjectionLayer
        {
        public:
            ProjectionQuadLayer(CompositionHelper* compositionHelper, XrColor4f color, XrSpace space, XrPosef pose, XrExtent2Df extent,
                                bool opaque)
                : ProjectionLayer(compositionHelper, space, opaque)
            {
                // 0-2
                // |/|
                // 1-3
                const Geometry::Vertex quadVertices[] = {{{-0.5, 0.5, 0}}, {{-0.5, -0.5, 0}}, {{0.5, 0.5, 0}}, {{0.5, -0.5, 0}}};
                const uint16_t quadIndices[] = {1, 0, 2, 2, 3, 1};
                auto quadMesh = GetGlobalData().graphicsPlugin->MakeSimpleMesh(quadIndices, quadVertices);

                m_drawables = {MeshDrawable{quadMesh, pose, {extent.width, extent.height, 1.0}, color}};
            }
        };
    }  // namespace SimpleTestLayers

    // Purpose: Verify that a pair of quad layers with a projection layer between them
    // is rendered according to painter's algorithm.
    TEST_CASE("QuadProjectionQuad", "[composition][interactive]")
    {
        const GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        CompositionHelper compositionHelper("QuadProjectionQuad");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "quad_projection_sandwich.png",
            "There should appear to be three visible squares - blue, green, yellow - "
            "though each actually continues hidden under the subsequent ones. "
            "The green rectangle is drawn via a transparent projection layer, "
            "and may have some jitter relative to the others, which are drawn using quad layers.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        // Overall plan:
        //   quad   projection  quad
        //     |        v    +--------+
        //     v    +--------| yellow |
        // +--------| green  +--------+
        // |  blue  +-----------------+
        // +--------------------------+
        // (vertical offsets for illustration purposes only)

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);
        constexpr float quadZ = -3;  // How far away quads are placed.

        std::vector<std::unique_ptr<SimpleTestLayers::TestLayer>> testLayers = {};
        testLayers.push_back(std::make_unique<SimpleTestLayers::QuadLayer>(
            &compositionHelper, Colors::Blue, viewSpace, XrPosef{Quat::Identity, {0.0, 0.0, quadZ}}, XrExtent2Df{3.0, 1.0}));
        testLayers.push_back(std::make_unique<SimpleTestLayers::ProjectionQuadLayer>(
            &compositionHelper, Colors::Green, viewSpace, XrPosef{Quat::Identity, {0.5, 0.0, quadZ}}, XrExtent2Df{2.0, 1.0}, false));
        testLayers.push_back(std::make_unique<SimpleTestLayers::QuadLayer>(
            &compositionHelper, Colors::Yellow, viewSpace, XrPosef{Quat::Identity, {1.0, 0.0, quadZ}}, XrExtent2Df{1.0, 1.0}));

        RenderLoop(session, [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(viewSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);
                for (auto& testLayer : testLayers) {
                    layers.push_back(testLayer->Update(views));
                }
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        }).Loop();
    }

    // Purpose: Verify that a pair of projection layers with a quad layer between them
    // is rendered according to painter's algorithm.
    TEST_CASE("ProjectionQuadProjection", "[composition][interactive]")
    {
        const GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        CompositionHelper compositionHelper("ProjectionQuadProjection");
        InteractiveLayerManager interactiveLayerManager(  //
            compositionHelper, "quad_projection_sandwich.png",
            "There should appear to be three visible squares - blue, green, yellow - "
            "though each actually continues hidden under the subsequent ones. "
            "The blue and yellow rectangles are drawn using projection layers, "
            "and may have some jitter relative to the green rectangle, which is a quad layer.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        // Overall plan:
        // projection  quad  projection
        //     |        v    +--------+
        //     v    +--------| yellow |
        // +--------| green  +--------+
        // |  blue  +-----------------+
        // +--------------------------+
        // (vertical offsets for illustration purposes only)

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);
        constexpr float quadZ = -3;  // How far away quads are placed.

        std::vector<std::unique_ptr<SimpleTestLayers::TestLayer>> testLayers = {};
        testLayers.push_back(std::make_unique<SimpleTestLayers::ProjectionQuadLayer>(
            &compositionHelper, Colors::Blue, viewSpace, XrPosef{Quat::Identity, {0.0, 0.0, quadZ}}, XrExtent2Df{3.0, 1.0}, false));
        testLayers.push_back(std::make_unique<SimpleTestLayers::QuadLayer>(
            &compositionHelper, Colors::Green, viewSpace, XrPosef{Quat::Identity, {0.5, 0.0, quadZ}}, XrExtent2Df{2.0, 1.0}));
        testLayers.push_back(std::make_unique<SimpleTestLayers::ProjectionQuadLayer>(
            &compositionHelper, Colors::Yellow, viewSpace, XrPosef{Quat::Identity, {1.0, 0.0, quadZ}}, XrExtent2Df{1.0, 1.0}, false));

        RenderLoop(session, [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(viewSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);
                for (auto& testLayer : testLayers) {
                    layers.push_back(testLayer->Update(views));
                }
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        }).Loop();
    }

    // Purpose: Verify order of transforms by exercising the two ways poses can be specified:
    // 1. A pose offset when creating the space
    // 2. A pose offset when adding the layer
    // If the poses are applied in an incorrect order, the quads will not render in the correct place or orientation.
    TEST_CASE("QuadPoses", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test QuadPoses without a graphics plugin");
        }

        CompositionHelper compositionHelper("Quad Poses");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "quad_poses.png",
            "Render pairs of quads using similar poses to validate order of operations. The blue/green quads apply a"
            " rotation around the Z axis on an XrSpace and then translate the quad out on the Z axis through the quad"
            " layer's pose. The purple/yellow quads apply the same translation on the XrSpace and the rotation on the"
            " quad layer's pose.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        const XrSwapchain orangeSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Orange);
        const XrSwapchain yellowSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Yellow);

        constexpr int RotationCount = 2;
        constexpr float MaxRotationDegrees = 30;
        // For each rotation there are a pair of quads.
        static_assert(RotationCount * 2 <= XR_MIN_COMPOSITION_LAYERS_SUPPORTED, "Too many layers");

        for (int i = 0; i < RotationCount; i++) {
            const float radians = Math::LinearMap(i, 0, RotationCount - 1, DegToRad(-MaxRotationDegrees), DegToRad(MaxRotationDegrees));

            const XrPosef pose1 = XrPosef{Quat::FromAxisAngle({0, 1, 0}, radians), {0, 0, 0}};
            const XrPosef pose2 = XrPosef{Quat::Identity, {0, 0, -1}};

            const XrSpace viewSpacePose1 = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, pose1);
            const XrSpace viewSpacePose2 = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, pose2);

            auto quad1 = compositionHelper.CreateQuadLayer((i % 2) == 0 ? blueSwapchain : greenSwapchain, viewSpacePose1, 0.25f, pose2);
            interactiveLayerManager.AddLayer(quad1);

            auto quad2 = compositionHelper.CreateQuadLayer((i % 2) == 0 ? orangeSwapchain : yellowSwapchain, viewSpacePose2, 0.25f, pose1);
            interactiveLayerManager.AddLayer(quad2);
        }

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    TEST_CASE("MultipleMutableProjections", "[composition][interactive][aaaa]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        CompositionHelper compositionHelper("Multiple projection mutable Field-Of-View");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_mutable_projections.png",
                                                        "Uses mutable field-of-views for each projection layer view.");
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        if (!compositionHelper.GetViewConfigurationProperties().fovMutable) {
            SKIP("View configuration does not support mutable FoV");
        }

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        const auto maxRecommendedWidth = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                          [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                              return l.recommendedImageRectWidth < r.recommendedImageRectWidth;
                                                          })
                                             ->recommendedImageRectWidth;
        const auto maxRecommendedHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                           [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                               return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                           })
                                              ->recommendedImageRectHeight;

        struct LayerInfo
        {
            //In screen space
            XrOffset2Df centerPosition;
            float scale;
            XrColor4f color;
        };

        std::vector<LayerInfo> layerInfos = {{
            {{.50f, .50f}, 1.f, Colors::Gray},  //base
            {{.75f, .25f}, .25f, Colors::Magenta},
            {{.25f, .25f}, .25f, Colors::Blue},
            {{.25f, .75f}, .25f, Colors::Yellow},
            {{.75f, .75f}, .25f, Colors::Green},
        }};

        struct LayerData
        {
            LayerInfo info;

            XrSwapchain swapchain;
            XrExtent2Di swapchainExtent;

            XrCompositionLayerProjection* projLayer;
        };
        std::vector<LayerData> layerDatas = {};

        for (const auto& layerInfo : layerInfos) {
            layerDatas.push_back({layerInfo});
            LayerData& layerData = layerDatas.back();

            layerData.swapchainExtent = {
                static_cast<int32_t>(maxRecommendedWidth),
                static_cast<int32_t>(maxRecommendedHeight),
            };
            layerData.swapchain = compositionHelper.CreateStaticSwapchainSolidColor(layerInfo.color, layerData.swapchainExtent);

            layerData.projLayer = compositionHelper.CreateProjectionLayer(viewSpace);
            for (uint32_t j = 0; j < layerData.projLayer->viewCount; j++) {
                // views field is pointer to const, but views haven't been populated yet
                auto& view = const_cast<XrCompositionLayerProjectionView&>(layerData.projLayer->views[j]);
                view.subImage = compositionHelper.MakeDefaultSubImage(layerData.swapchain, 0);
            }
        }

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(viewSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                for (const auto& layerData : layerDatas) {
                    XrCompositionLayerProjection* projLayer = layerData.projLayer;
                    for (size_t viewIndex = 0; viewIndex < views.size(); viewIndex++) {
                        auto& projView = const_cast<XrCompositionLayerProjectionView&>(projLayer->views[viewIndex]);
                        projView.pose = views[viewIndex].pose;

                        const XrFovf& baseFov = views[viewIndex].fov;

                        const float pxPanelWidth = static_cast<float>(layerData.swapchainExtent.width);
                        const float pxPanelHeight = static_cast<float>(layerData.swapchainExtent.height);

                        const float pxLayerWidth = pxPanelWidth * layerData.info.scale;
                        const float pxLayerHeight = pxPanelHeight * layerData.info.scale;

                        const float pxLayerCenterX = layerData.info.centerPosition.x * pxPanelWidth;
                        const float pxLayerCenterY = layerData.info.centerPosition.y * pxPanelHeight;

                        const float pxLayerOffsetTop = pxLayerCenterY - (pxLayerHeight / 2);
                        const float pxLayerOffsetLeft = pxLayerCenterX - (pxLayerWidth / 2);

                        float tanLeft = tanf(baseFov.angleLeft);
                        float tanRight = tanf(baseFov.angleRight);
                        float tanDown = tanf(baseFov.angleDown);
                        float tanUp = tanf(baseFov.angleUp);

                        float tanWidth = tanRight - tanLeft;
                        float tanHeight = tanUp - tanDown;

                        float offsetTanX = ((pxLayerOffsetLeft + pxLayerWidth / 2) / pxPanelWidth - 0.5f) * tanWidth;
                        float offsetTanY = ((pxLayerOffsetTop + pxLayerHeight / 2) / pxPanelHeight - 0.5f) * tanHeight;

                        float scaledTanWidth = tanWidth * layerData.info.scale;
                        float scaledTanHeight = tanHeight * layerData.info.scale;

                        projView.fov.angleLeft = atanf(offsetTanX - scaledTanWidth / 2);
                        projView.fov.angleRight = atanf(offsetTanX + scaledTanWidth / 2);
                        projView.fov.angleDown = atanf(offsetTanY - scaledTanHeight / 2);
                        projView.fov.angleUp = atanf(offsetTanY + scaledTanHeight / 2);
                    }
                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
                }
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

    // Purpose: Validates alpha blending (both premultiplied and unpremultiplied).
    TEST_CASE("SourceAlphaBlending", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test SourceAlphaBlending without a graphics plugin");
        }

        CompositionHelper compositionHelper("Source Alpha Blending");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "source_alpha_blending.png",
                                                        "All three squares should have an identical blue-green gradient.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
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

                // The blended color here has a 0 alpha value to test that the runtime is ignoring the texture alpha when
                // the XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT flag is not set. If the runtime is erroneously
                // reading texture alpha, it is more likely to output black pixels.
                const XrColor4f blended{dst.r * (1 - src.a) + src.r, dst.g * (1 - src.a) + src.g, dst.b * (1 - src.a) + src.b, 0};
                blueGradientOverGreen.DrawRect(0, y, blueGradientOverGreen.width, 1, blended);
            }

            const XrSwapchain answerSwapchain = compositionHelper.CreateStaticSwapchainImage(blueGradientOverGreen);
            XrCompositionLayerQuad* truthQuad =
                compositionHelper.CreateQuadLayer(answerSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {0, 0, QuadZ}});

            // Set the unpremultiplied bit on this quad (and the green ones below) to make it more obvious when a runtime
            // supports the premultiplied flag but not the texture flag. Without this bit set, the final color will be:
            //   ( 1 - alpha ) * dst + src
            // dst is black, and alpha is 0, so the output is just src.
            // If we use unpremultiplied, the formula becomes:
            //   ( 1 - alpha ) * dst + alpha * src
            // which results in black pixels and is obviously wrong.
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

                    blueGradient.DrawRect(0, row, blueGradient.width, 1, color);
                }

                const XrSwapchain gradientSwapchain = compositionHelper.CreateStaticSwapchainImage(blueGradient);
                XrCompositionLayerQuad* gradientQuad =
                    compositionHelper.CreateQuadLayer(gradientSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {x, y, QuadZ}});

                gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                if (!premultiplied) {
                    gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
                }

                interactiveLayerManager.AddLayer(gradientQuad);
            }
        };

        createGradientTest(true, -1.02f, 0);  // Test premultiplied (left of center "answer")
        createGradientTest(false, 1.02f, 0);  // Test unpremultiplied (right of center "answer")

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    // Purpose: Validate eye visibility flags.
    TEST_CASE("EyeVisibility", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test eyevisibility without a graphics plugin");
        }

        CompositionHelper compositionHelper("Eye Visibility");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "eye_visibility.png",
                                                        "A green quad is shown in the left eye and a blue quad is shown in the right eye.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();

        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        XrCompositionLayerQuad* quad1 =
            compositionHelper.CreateQuadLayer(greenSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {-1, 0, -2}});
        quad1->eyeVisibility = XR_EYE_VISIBILITY_LEFT;
        interactiveLayerManager.AddLayer(quad1);

        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        XrCompositionLayerQuad* quad2 =
            compositionHelper.CreateQuadLayer(blueSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {1, 0, -2}});
        quad2->eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
        interactiveLayerManager.AddLayer(quad2);

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    TEST_CASE("Subimage", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test subimage without a graphics plugin");
        }

        CompositionHelper compositionHelper("Subimage Tests");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "subimage.png",
            "Creates a 6x2 grid of quad layers testing subImage array index and imageRect. Red should not be visible except minor bleed in.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{Quat::Identity, {0, 0, -1}});

        constexpr float QuadZ = -5;  // How far away quads are placed.
        constexpr int ImageColCount = 3;
        constexpr int ImageRowCount = 2;
        constexpr int ImageArrayCount = 2;
        constexpr int ImageWidth = 1024;
        constexpr int ImageHeight = ImageRowCount * (ImageWidth / ImageColCount);
        constexpr int RedZoneBorderSize = 16;
        constexpr int CellWidth = (ImageWidth / ImageColCount);
        constexpr int CellHeight = CellWidth;

        // Create an array swapchain
        auto swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(
            ImageWidth, ImageHeight, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, globalData.graphicsPlugin->GetSRGBA8Format());
        swapchainCreateInfo.arraySize = ImageArrayCount;
        swapchainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        const XrSwapchain swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

        // Render a grid of numbers (1,2,3,4) in slice 0 and (5,6,7,8) in slice 1 of the swapchain
        // Create a quad layer referencing each number cell.
        compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
            int number = 1;

            // Because each arraySlice is one complete image that is copied at once, the number images are organized awkwardly:
            // Write numbers 1-3, and 7-9 into two rows in the first slice. Then write numbers 4-6 and 10-12 into two rows to the second slice.
            // Quad layers will lay out in one quad layer row slice 1 row 1, slice 1 row 2 from left to right.
            // Then in a second quad layer row, slice 2 row 1 and slice 2 row 2 from left to right.
            for (int arraySlice = 0; arraySlice < ImageArrayCount; arraySlice++) {
                Conformance::RGBAImage numberGridImage(ImageWidth, ImageHeight);

                // All unused areas are red (should not be seen). Only clear a slice once, before drawing the first row.
                numberGridImage.DrawRect(0, 0, numberGridImage.width, numberGridImage.height, Colors::Red);

                for (int row = 0; row < ImageRowCount; row++) {
                    int drawYOffset = row * CellHeight;

                    /*
                     * Only on OpenGL and and OpenGL ES:
                     * The OpenXR runtime must interpret the bottom-left corner of the swapchain image as the coordinate origin unless specified otherwise by extension functionality.
                     * The OpenXR runtime must interpret the swapchain images in a clip space of positive Y pointing up, near Z plane at -1, and far Z plane at 1.
                     *
                     * We render numbers 1-3 in the top row on both APIs, but on OpenGL (ES) we modify the quad layer subrect to select the rows in reverse order from bottom up.
                     */
                    bool originTopLeft = (GetGlobalData().graphicsPlugin->DescribeGraphics() != "OpenGL" &&
                                          GetGlobalData().graphicsPlugin->DescribeGraphics() != "OpenGLES");

                    for (int x = 0; x < ImageColCount; x++) {
                        const auto& color = Colors::UniqueColors[number % Colors::UniqueColors.size()];
                        const XrRect2Di numberRect{{x * CellWidth + RedZoneBorderSize, drawYOffset + RedZoneBorderSize},
                                                   {CellWidth - RedZoneBorderSize * 2, CellHeight - RedZoneBorderSize * 2}};
                        numberGridImage.DrawRect(numberRect.offset.x, numberRect.offset.y, numberRect.extent.width,
                                                 numberRect.extent.height, Colors::Transparent);
                        numberGridImage.PutText(numberRect, std::to_string(number).c_str(), static_cast<int>(CellHeight * 0.75f), color);
                        numberGridImage.DrawRectBorder(numberRect.offset.x, numberRect.offset.y, numberRect.extent.width,
                                                       numberRect.extent.height, 4, color);
                        number++;

                        // Each image slice is shifted ImageColCount to the right.
                        int quadXOffset = ImageColCount * row;

                        const float quadX = Math::LinearMap(quadXOffset + x, 0, ImageColCount * ImageArrayCount - 1, -4.0f, 4.0f);
                        const float quadY = Math::LinearMap(arraySlice, 0, ImageArrayCount * ImageRowCount - 1, 1.f, -3.75f);
                        XrCompositionLayerQuad* const quad =
                            compositionHelper.CreateQuadLayer(swapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {quadX, quadY, QuadZ}});
                        quad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                        quad->subImage.imageArrayIndex = arraySlice;
                        quad->subImage.imageRect = numberRect;
                        if (!originTopLeft) {
                            quad->subImage.imageRect.offset.y = (ImageRowCount - 1 - row) * CellHeight + RedZoneBorderSize;
                        }
                        quad->size.height = 1.0f;  // Height needs to be corrected since the imageRect is customized.
                        interactiveLayerManager.AddLayer(quad);
                    }
                }
                numberGridImage.ConvertToSRGB();
                globalData.graphicsPlugin->CopyRGBAImage(swapchainImage, arraySlice, numberGridImage);

#if 0
                // render the complete texture behind the quad layers for debugging purposes
                XrCompositionLayerQuad* const quad = compositionHelper.CreateQuadLayer(
                    swapchain, viewSpace, 4.0f, XrPosef{Quat::Identity, {(-1 + arraySlice) * 4.2f, 4, -6}});
                quad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                quad->subImage.imageArrayIndex = arraySlice;
                quad->subImage.imageRect.extent.width = ImageWidth;
                quad->subImage.imageRect.extent.height = ImageHeight;
                interactiveLayerManager.AddLayer(quad);
#endif
            }
        });

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    TEST_CASE("ProjectionArraySwapchain", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test ProjectionArraySwapchain without a graphics plugin");
        }

        for (bool submitDepthSwapchain : {false, true}) {
            DYNAMIC_SECTION((submitDepthSwapchain ? "With" : "Without") << " depth submission")
            {
                std::vector<const char*> extensions;
                if (submitDepthSwapchain) {
                    if (!GetGlobalData().IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME)) {
                        continue;
                    }
                    extensions.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
                }

                CompositionHelper compositionHelper("Projection Array Swapchain", extensions);
                InteractiveLayerManager interactiveLayerManager(
                    compositionHelper, "projection_array.png",
                    "Uses a single texture array for a projection layer (each view is a different slice and each slice has a unique color).");
                XrSession session = compositionHelper.GetSession();
                InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
                interactionManager.AttachActionSets();
                compositionHelper.BeginSession();

                const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

                const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

                // Because a single swapchain is being used for all views (each view is a slice of the texture array), the maximum dimensions must be used
                // since the dimensions of all slices are the same.
                const auto maxWidth = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                       [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                           return l.recommendedImageRectWidth < r.recommendedImageRectWidth;
                                                       })
                                          ->recommendedImageRectWidth;
                const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                        [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                            return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                        })
                                           ->recommendedImageRectHeight;

                // Create swapchain with array type.
                XrSwapchainCreateInfo swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(maxWidth, maxHeight);
                XrSwapchainCreateInfo depthSwapchainCreateInfo = compositionHelper.DefaultDepthSwapchainCreateInfo(maxWidth, maxHeight);
                if (submitDepthSwapchain && depthSwapchainCreateInfo.format == -1) {
                    // no depth format available for testing
                    continue;
                }
                swapchainCreateInfo.arraySize = (uint32_t)viewProperties.size() * 3;
                depthSwapchainCreateInfo.arraySize = (uint32_t)viewProperties.size() * 3;
                XrSwapchain swapchain{XR_NULL_HANDLE};
                XrSwapchain depthSwapchain{XR_NULL_HANDLE};
                if (submitDepthSwapchain) {
                    std::tie(swapchain, depthSwapchain) =
                        compositionHelper.CreateSwapchainWithDepth(swapchainCreateInfo, depthSwapchainCreateInfo);
                }
                else {
                    swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);
                }

                // Set up the projection layer
                XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
                std::vector<XrCompositionLayerDepthInfoKHR> depthInfo(projLayer->viewCount);
                for (uint32_t j = 0; j < projLayer->viewCount; j++) {
                    // views field is pointer to const, but views haven't been populated yet
                    auto& view = const_cast<XrCompositionLayerProjectionView&>(projLayer->views[j]);
                    // Use non-contiguous array indices to ferret out any assumptions that implementations are making
                    // about array indices. In particular 0 != left and 1 != right, but this should test for other
                    // assumptions too.
                    uint32_t arrayIndex = swapchainCreateInfo.arraySize - (j * 2 + 1);
                    view.subImage = compositionHelper.MakeDefaultSubImage(swapchain, arrayIndex);

                    if (submitDepthSwapchain) {
                        XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(depthSwapchain, arrayIndex);
                        InsertDefaultDepthInfo(&depthInfo[j], &view.next, subImage);
                    }
                }

                const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2}),
                                                 Cube::Make({0, 1, -2})};

                auto updateLayers = [&](const XrFrameState& frameState) {
                    auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
                    const auto& viewState = std::get<XrViewState>(viewData);

                    std::vector<XrCompositionLayerBaseHeader*> layers;
                    if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                        viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                        const auto& views = std::get<std::vector<XrView>>(viewData);

                        // Render into each slice of the array swapchain using the projection layer view fov and pose.
                        compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                            for (uint32_t slice = 0; slice < (uint32_t)views.size(); slice++) {
                                GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage,
                                                                                projLayer->views[slice].subImage.imageArrayIndex);

                                const_cast<XrFovf&>(projLayer->views[slice].fov) = views[slice].fov;
                                const_cast<XrPosef&>(projLayer->views[slice].pose) = views[slice].pose;
                                GetGlobalData().graphicsPlugin->RenderView(projLayer->views[slice], swapchainImage,
                                                                           RenderParams().Draw(cubes));
                            }
                        });

                        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
                    }
                    return interactiveLayerManager.EndFrame(frameState, layers);
                };

                RenderLoop(session, updateLayers).Loop();
            }
        }
    }

    TEST_CASE("ProjectionWideSwapchain", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test ProjectionWideSwapchain without a graphics plugin");
        }

        for (bool submitDepthSwapchain : {false, true}) {
            DYNAMIC_SECTION((submitDepthSwapchain ? "With" : "Without") << " depth submission")
            {
                std::vector<const char*> extensions;
                if (submitDepthSwapchain) {
                    if (!GetGlobalData().IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME)) {
                        continue;
                    }
                    extensions.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
                }
                CompositionHelper compositionHelper("Projection Wide Swapchain", extensions);
                InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_wide.png",
                                                                "Uses a single wide texture for a projection layer.");
                XrSession session = compositionHelper.GetSession();
                InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
                interactionManager.AttachActionSets();
                compositionHelper.BeginSession();

                const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

                const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

                const auto totalWidth =
                    std::accumulate(viewProperties.begin(), viewProperties.end(), 0,
                                    [](uint32_t l, const XrViewConfigurationView& r) { return l + r.recommendedImageRectWidth; });
                // Because a single swapchain is being used for all views the maximum height must be used.
                const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                        [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                            return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                        })
                                           ->recommendedImageRectHeight;

                // Create wide swapchain.
                XrSwapchainCreateInfo swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(totalWidth, maxHeight);
                XrSwapchainCreateInfo depthSwapchainCreateInfo = compositionHelper.DefaultDepthSwapchainCreateInfo(totalWidth, maxHeight);
                if (submitDepthSwapchain && depthSwapchainCreateInfo.format == -1) {
                    // no depth format available for testing
                    continue;
                }

                XrSwapchain swapchain{XR_NULL_HANDLE};
                XrSwapchain depthSwapchain{XR_NULL_HANDLE};
                if (submitDepthSwapchain) {
                    std::tie(swapchain, depthSwapchain) =
                        compositionHelper.CreateSwapchainWithDepth(swapchainCreateInfo, depthSwapchainCreateInfo);
                }
                else {
                    swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);
                }

                XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
                std::vector<XrCompositionLayerDepthInfoKHR> depthInfo(projLayer->viewCount);
                int x = 0;
                for (uint32_t j = 0; j < projLayer->viewCount; j++) {
                    // views field is pointer to const, but views haven't been populated yet
                    auto& view = const_cast<XrCompositionLayerProjectionView&>(projLayer->views[j]);
                    XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);
                    XrSwapchainSubImage depthSubImage =
                        submitDepthSwapchain ? compositionHelper.MakeDefaultSubImage(depthSwapchain, 0) : XrSwapchainSubImage{};
                    for (XrSwapchainSubImage* s : {&subImage, &depthSubImage}) {
                        s->imageRect.offset = {x, 0};
                        s->imageRect.extent = {(int32_t)viewProperties[j].recommendedImageRectWidth,
                                               (int32_t)viewProperties[j].recommendedImageRectHeight};
                    }
                    view.subImage = subImage;
                    if (submitDepthSwapchain) {
                        InsertDefaultDepthInfo(&depthInfo[j], &view.next, depthSubImage);
                    }
                    x += subImage.imageRect.extent.width;  // Each view is to the left of the previous view.
                }

                const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2}),
                                                 Cube::Make({0, 1, -2})};

                auto updateLayers = [&](const XrFrameState& frameState) {
                    auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
                    const auto& viewState = std::get<XrViewState>(viewData);

                    std::vector<XrCompositionLayerBaseHeader*> layers;
                    if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                        viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                        const auto& views = std::get<std::vector<XrView>>(viewData);

                        // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                        compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                            for (size_t view = 0; view < views.size(); view++) {
                                const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                                const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                                GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage,
                                                                           RenderParams().Draw(cubes));
                            }
                        });

                        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
                    }
                    return interactiveLayerManager.EndFrame(frameState, layers);
                };

                RenderLoop(session, updateLayers).Loop();
            }
        }
    }

    TEST_CASE("ProjectionSeparateSwapchains", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test ProjectionSeparateSwapchains without a graphics plugin");
        }

        for (bool submitDepthSwapchain : {false, true}) {
            DYNAMIC_SECTION((submitDepthSwapchain ? "With" : "Without") << " depth submission")
            {
                std::vector<const char*> extensions;
                if (submitDepthSwapchain) {
                    if (!GetGlobalData().IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME)) {
                        continue;
                    }
                    extensions.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
                }
                CompositionHelper compositionHelper("Projection Separate Swapchains", extensions);
                InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_separate.png",
                                                                "Uses separate textures for each projection layer view.");
                XrSession session = compositionHelper.GetSession();
                InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
                interactionManager.AttachActionSets();
                compositionHelper.BeginSession();

                const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

                const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

                std::vector<XrSwapchain> swapchains;
                std::vector<XrSwapchain> depthSwapchains;
                XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
                std::vector<XrCompositionLayerDepthInfoKHR> depthInfo(projLayer->viewCount);

                if (submitDepthSwapchain) {
                    // early check to avoid double loop break
                    XrSwapchainCreateInfo depthSwapchainCreateInfo = compositionHelper.DefaultDepthSwapchainCreateInfo(1, 1);
                    if (depthSwapchainCreateInfo.format == -1) {
                        // no depth format available for testing
                        continue;
                    }
                }
                for (uint32_t j = 0; j < projLayer->viewCount; j++) {
                    // views field is pointer to const, but views haven't been populated yet
                    auto& projView = const_cast<XrCompositionLayerProjectionView&>(projLayer->views[j]);

                    XrSwapchainCreateInfo swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(
                        viewProperties[j].recommendedImageRectWidth, viewProperties[j].recommendedImageRectHeight);
                    XrSwapchainCreateInfo depthSwapchainCreateInfo = compositionHelper.DefaultDepthSwapchainCreateInfo(
                        viewProperties[j].recommendedImageRectWidth, viewProperties[j].recommendedImageRectHeight);

                    swapchains.push_back(XR_NULL_HANDLE_CPP);
                    XrSwapchain& swapchain = swapchains.back();
                    if (submitDepthSwapchain) {
                        depthSwapchains.push_back(XR_NULL_HANDLE_CPP);
                        XrSwapchain& depthSwapchain = depthSwapchains.back();

                        std::tie(swapchain, depthSwapchain) =
                            compositionHelper.CreateSwapchainWithDepth(swapchainCreateInfo, depthSwapchainCreateInfo);

                        XrSwapchainSubImage depthSubImage = compositionHelper.MakeDefaultSubImage(depthSwapchain, 0);
                        InsertDefaultDepthInfo(&depthInfo[j], &projView.next, depthSubImage);
                    }
                    else {
                        swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);
                    }
                    projView.subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);
                }

                const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2}),
                                                 Cube::Make({0, 1, -2})};

                auto updateLayers = [&](const XrFrameState& frameState) {
                    auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
                    const auto& viewState = std::get<XrViewState>(viewData);

                    std::vector<XrCompositionLayerBaseHeader*> layers;
                    if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                        viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                        const auto& views = std::get<std::vector<XrView>>(viewData);

                        // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                        for (size_t view = 0; view < views.size(); view++) {
                            compositionHelper.AcquireWaitReleaseImage(
                                swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                                    GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);

                                    const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                                    const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                                    GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage,
                                                                               RenderParams().Draw(cubes));
                                });
                        }

                        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
                    }

                    return interactiveLayerManager.EndFrame(frameState, layers);
                };

                RenderLoop(session, updateLayers).Loop();
            }
        }
    }

    static uint32_t ComputeTotalWidthSBS(const std::vector<XrViewConfigurationView>& viewProperties)
    {
        return std::accumulate(viewProperties.begin(), viewProperties.end(), 0,
                               [](uint32_t l, const XrViewConfigurationView& r) { return l + r.recommendedImageRectWidth; });
    }

    static uint32_t MaxRecommendedViewHeight(const std::vector<XrViewConfigurationView>& viewProperties)
    {
        return std::max_element(viewProperties.begin(), viewProperties.end(),
                                [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                    return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                })
            ->recommendedImageRectHeight;
    }
    TEST_CASE("MaxLayers-noninteractive", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        CompositionHelper compositionHelper("Max Layers Noninteractive");
        compositionHelper.BeginSession();
        XrInstance instance = compositionHelper.GetInstance();
        XrSystemId systemId = compositionHelper.GetSystemId();
        XrSession session = compositionHelper.GetSession();

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        const auto totalWidth = ComputeTotalWidthSBS(viewProperties);
        // Because a single swapchain is being used for all views the maximum height must be used.
        const auto maxHeight = MaxRecommendedViewHeight(viewProperties);

        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
        XRC_CHECK_THROW_XRCMD(xrGetSystemProperties(instance, systemId, &systemProperties));

        // In the noninteractive test, we do not clamp, we actually test full amount of layers. But we do not create a swapchain for every layer.
        const uint32_t maxLayerCount = systemProperties.graphicsProperties.maxLayerCount;
        const uint32_t maxLayerCountPlus1 = maxLayerCount + 1;

        // Create and initialize max projection layers, swapchains before hand.
        // This assumes slightly more swapchains than layers are supported as CompositionHelper creates a swapchain too.
        std::vector<XrCompositionLayerProjection*> projLayers;
        std::vector<XrSwapchain> swapchains;

        // In the noninteractive test, reuse swapchains for layers, but use 4 layers so there is at least some variability.
        const uint32_t swapchainCount = 4;
        swapchains.reserve(swapchainCount);
        for (size_t i = 0; i < swapchainCount; ++i) {
            swapchains.push_back(
                compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(totalWidth, maxHeight)));
        }
        for (size_t i = 0; i < maxLayerCount; ++i) {
            projLayers.push_back(compositionHelper.CreateProjectionLayer(localSpace));
            XrSwapchain& swapchain = swapchains[i % 4];
            for (uint32_t j = 0; j < projLayers[i]->viewCount; j++) {
                // in the noninteractive test we don't render, no need to define subimage rects
                const_cast<XrSwapchainSubImage&>(projLayers[i]->views[j].subImage) = compositionHelper.MakeDefaultSubImage(swapchain, 0);
                projLayers[i]->layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            }
        }

        std::vector<XrCompositionLayerQuad*> quadLayers;
        quadLayers.reserve(maxLayerCountPlus1);

        // allocate more layers than supported so we can test too many layers
        for (size_t i = 0; i < maxLayerCountPlus1; ++i) {
            projLayers.push_back(compositionHelper.CreateProjectionLayer(localSpace));

            // reuse swapchains from projection layer, in a noninteractive test it doesn't matter what they show
            XrSwapchain& swapchain = swapchains[i % 4];
            quadLayers.push_back(compositionHelper.CreateQuadLayer(swapchain, localSpace, (float)totalWidth));
        }

        for (uint32_t i = 0; i < maxLayerCountPlus1; i++) {
            for (uint32_t j = 0; j < projLayers[i]->viewCount; j++) {
                XrSwapchain& swapchain = swapchains[i % 4];
                XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);
                subImage.imageRect.offset = {0, 0};
                subImage.imageRect.extent = {(int32_t)viewProperties[j].recommendedImageRectWidth,
                                             (int32_t)viewProperties[j].recommendedImageRectHeight};
                const_cast<XrSwapchainSubImage&>(projLayers[i]->views[j].subImage) = subImage;
                projLayers[i]->layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            }
        }

        // now do the noninteractive tests

        SECTION("Exact number of projection layers submitted")
        {
            auto updateLayers = [&](const XrFrameState& frameState) {
                std::vector<XrCompositionLayerBaseHeader*> layers;

                for (uint32_t i = 0; i < maxLayerCount; i++) {
                    compositionHelper.AcquireWaitReleaseImage(projLayers[i]->views[0].subImage.swapchain,
                                                              [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                                                                  GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                                                              });

                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayers[i]));
                }

                XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
                CAPTURE(frameEndInfo.environmentBlendMode = Options::Get().environmentBlendModeValue);
                CAPTURE(frameEndInfo.displayTime = frameState.predictedDisplayTime);
                CAPTURE(frameEndInfo.layerCount = (uint32_t)layers.size());
                frameEndInfo.layers = layers.data();
                XrResult endFrameResult = xrEndFrame(session, &frameEndInfo);
                if (endFrameResult != XR_SUCCESS) {
                    // This tries submitting "maxLayerCount" projection layers, which is not technically required by the spec,
                    // if maxLayerCount is greater than XR_MIN_COMPOSITION_LAYERS_SUPPORTED.
                    // But, it may be a surprise to app developers, hence the warning.
                    WARN("xrEndFrame returned " << ResultToString(endFrameResult) << ", expected XR_SUCCESS. Tried submitting "
                                                << maxLayerCount << " layers.");
                }

                return true;
            };

            RenderLoop loop(session, updateLayers);
            loop.IterateFrame();
        }
        SECTION("Too many projection layers submitted")
        {
            auto updateLayers = [&](const XrFrameState& frameState) {
                std::vector<XrCompositionLayerBaseHeader*> layers;

                for (uint32_t i = 0; i < maxLayerCountPlus1; i++) {
                    compositionHelper.AcquireWaitReleaseImage(projLayers[i]->views[0].subImage.swapchain,
                                                              [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                                                                  GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                                                              });

                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayers[i]));
                }

                XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
                frameEndInfo.environmentBlendMode = Options::Get().environmentBlendModeValue;
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                frameEndInfo.layerCount = (uint32_t)layers.size();
                frameEndInfo.layers = layers.data();
                XrResult result = xrEndFrame(session, &frameEndInfo);
                REQUIRE_RESULT(result, XR_ERROR_LAYER_LIMIT_EXCEEDED);

                return true;
            };

            RenderLoop loop(session, updateLayers);
            loop.IterateFrame();
        }

        SECTION("Too many quad layers submitted")
        {
            auto updateLayers = [&](const XrFrameState& frameState) {
                std::vector<XrCompositionLayerBaseHeader*> layers;

                for (auto& swapchain : swapchains) {
                    compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                    });
                }

                for (uint32_t i = 0; i < maxLayerCountPlus1; i++) {
                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(quadLayers[i]));
                }

                XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
                frameEndInfo.environmentBlendMode = Options::Get().environmentBlendModeValue;
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                frameEndInfo.layerCount = (uint32_t)layers.size();
                frameEndInfo.layers = layers.data();
                REQUIRE(XR_ERROR_LAYER_LIMIT_EXCEEDED == xrEndFrame(session, &frameEndInfo));

                return true;
            };

            RenderLoop loop(session, updateLayers);
            loop.IterateFrame();
        }

        SECTION("Too many quad layers after projection layers submitted")
        {
            auto updateLayers = [&](const XrFrameState& frameState) {
                std::vector<XrCompositionLayerBaseHeader*> layers;

                for (auto& swapchain : swapchains) {
                    compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                    });
                }

                // Half projection layers, just to pick a value
                const uint32_t numProjectionLayers = std::max<uint32_t>(1, maxLayerCount / 2);
                for (uint32_t i = 0; i < numProjectionLayers; i++) {
                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayers[i]));
                }

                // if maxlayerCount = 15 and 10 projection layers are used, we want to add 15 - 10 + 1 = 6 quad layers to exceed maxLayerCount
                uint32_t quadLayersToAdd = maxLayerCount - numProjectionLayers + 1;
                for (uint32_t i = 0; i < quadLayersToAdd; i++) {
                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(quadLayers[i]));
                }

                XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
                frameEndInfo.environmentBlendMode = Options::Get().environmentBlendModeValue;
                frameEndInfo.displayTime = frameState.predictedDisplayTime;
                frameEndInfo.layerCount = (uint32_t)layers.size();
                frameEndInfo.layers = layers.data();
                REQUIRE(XR_ERROR_LAYER_LIMIT_EXCEEDED == xrEndFrame(session, &frameEndInfo));

                return true;
            };

            RenderLoop loop(session, updateLayers);
            loop.IterateFrame();
        }
    }

    TEST_CASE("MinLayers", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        CompositionHelper compositionHelper("Min Layers");
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AddDefaultActions(compositionHelper.GetInstance());
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();
        XrInstance instance = compositionHelper.GetInstance();
        XrSystemId systemId = compositionHelper.GetSystemId();
        XrSession session = compositionHelper.GetSession();

        const float width = 0.15f;
        const float step = width * 1.5f;
        const float zdist = -2.0f;

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);
        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();
        const auto totalWidth = ComputeTotalWidthSBS(viewProperties);
        // Because a single swapchain is being used for all views the maximum height must be used.
        const auto maxHeight = MaxRecommendedViewHeight(viewProperties);

        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
        XRC_CHECK_THROW_XRCMD(xrGetSystemProperties(instance, systemId, &systemProperties));

        // Minimum Visible Layer test:
        // 5.3 System Properties:
        // The runtime must support at least XR_MIN_COMPOSITION_LAYERS_SUPPORTED layers.
        // 10.4 Frame Submission:
        // XR_ERROR_LAYER_LIMIT_EXCEEDED must be returned if XrFrameEndInfo::layerCount exceeds XrSystemGraphicsProperties::maxLayerCount
        // or if the runtime is unable to composite the specified layers due to resource constraints.
        // This means at best we can test for XR_MIN_COMPOSITION_LAYERS_SUPPORTED visible layers.
        const uint32_t minLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;

        // Create and initialize max projection layers, and associated swapchains.
        // Remember CompositionHelper creates a swapchain too.
        std::vector<XrCompositionLayerProjection*> projLayers;
        std::vector<XrSwapchain> swapchains;

        // Some runtimes support exactly as many swapchains as layers.
        // Therefore, use a quarter swapchain per projection layer to fit layersPerSC projection layers on one swapchain.
        // This value cannot easily be changed without code modifications for projection layer imageRect and layer*Offset
        constexpr uint32_t layersPerSC = 4;
        const uint32_t swapchainCount = (minLayerCount + (layersPerSC - 1)) / layersPerSC;
        swapchains.reserve(swapchainCount);
        for (size_t i = 0; i < swapchainCount; ++i) {
            swapchains.push_back(
                compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(totalWidth, maxHeight)));
        }

        for (size_t i = 0; i < minLayerCount; ++i) {
            projLayers.push_back(compositionHelper.CreateProjectionLayer(localSpace));
            XrSwapchain& swapchain = swapchains[i / layersPerSC];
            for (uint32_t j = 0; j < projLayers[i]->viewCount; j++) {
                XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);

                // Total width is the width for N views at full size.
                // Each rect is for one view, and then we half the size in each direction to fit 2 times the N views on one swapchain.
                subImage.imageRect.extent.width = (totalWidth / (int32_t)viewProperties.size()) / 2;
                subImage.imageRect.extent.height = maxHeight / 2;

                // each [] represents one view with size subImage.imageRect.extent.
                // i is the layer, j is the view per layer.
                // [i=0,j=0][i=0,j=1][i=1,j=0][i=1,j=1]
                // [i=2,j=0][i=2,j=1][i=3,j=0][i=3,j=1]

                // One layer*Offset is a full projection layer view with both left and right eye textures
                uint32_t layerXOffset = subImage.imageRect.extent.width * (uint32_t)viewProperties.size();
                uint32_t layerYOffset = subImage.imageRect.extent.height;  // per layer there is only one height occupied.
                uint32_t layerCol = i & 1;
                uint32_t layerRow = (i >> 1) & 1;
                subImage.imageRect.offset.x = layerCol * layerXOffset + j * subImage.imageRect.extent.width;
                subImage.imageRect.offset.y = layerRow * layerYOffset;

                const_cast<XrSwapchainSubImage&>(projLayers[i]->views[j].subImage) = subImage;
                projLayers[i]->layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            }
        }

        std::vector<XrPosef> gridPoses;
        {
            constexpr int itemsPerRow = 10;
            // center the cubes/quads roughly in the center of the view
            float xOffset = -(itemsPerRow * step) / 2.0f;
            uint32_t row = 0;
            uint32_t col = 0;
            for (uint32_t i = 0; i < minLayerCount; i++) {
                gridPoses.push_back(XrPosef{
                    Quat::Identity,
                    XrVector3f{xOffset + col * step,
                               row * step,  // square
                               zdist},
                });

                col++;
                if (col % itemsPerRow == 0) {
                    row++;
                    col = 0;
                }
            }
        }

        std::vector<XrCompositionLayerQuad*> quadLayers;
        {
            // Minimize swapchain use for quad layers by using only one large swapchain.
            // Use a similar method to render the grid as the "Subimage" test case.
            int ImageColCount = minLayerCount;
            int ImageWidth = 2048;
            int ImageHeight = ImageWidth / ImageColCount;
            int CellWidth = (ImageWidth / ImageColCount);
            int CellHeight = CellWidth;

            auto swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(
                ImageWidth, ImageHeight, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, GetGlobalData().graphicsPlugin->GetSRGBA8Format());
            swapchainCreateInfo.arraySize = 1;
            swapchainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            const XrSwapchain quadSwapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

            // Render a grid of numbers (1,2,3,4,...) in slice 0.
            // Create a quad layer referencing each number cell.
            compositionHelper.AcquireWaitReleaseImage(quadSwapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                int number = 1;
                Conformance::RGBAImage numberGridImage(ImageWidth, ImageHeight);

                // All unused areas are red (should not be seen).
                numberGridImage.DrawRect(0, 0, numberGridImage.width, numberGridImage.height, Colors::Red);

                for (int x = 0; x < ImageColCount; x++) {
                    const auto& color = Colors::UniqueColors[number % Colors::UniqueColors.size()];
                    const XrRect2Di numberRect{{x * CellWidth}, {CellWidth, CellHeight}};
                    numberGridImage.DrawRect(numberRect.offset.x + 1, numberRect.offset.y + 1, numberRect.extent.width - 2,
                                             numberRect.extent.height - 2, Colors::Transparent);
                    numberGridImage.PutText(numberRect, std::to_string(number).c_str(), CellHeight / 3, color);

                    number++;

                    XrCompositionLayerQuad* const quad = compositionHelper.CreateQuadLayer(quadSwapchain, localSpace, width, gridPoses[x]);
                    quad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    quad->subImage.imageArrayIndex = 0;
                    quad->subImage.imageRect = numberRect;
                    quad->size.height = width;

                    quadLayers.push_back(quad);
                }
                numberGridImage.ConvertToSRGB();
                GetGlobalData().graphicsPlugin->CopyRGBAImage(swapchainImage, 0, numberGridImage);
            });
        }

        // now do the interactive test

        SECTION("RenderMaxProjectionLayers")
        {

            {
                std::ostringstream oss;
                oss << "In the next scene, <" << minLayerCount
                    << "> projection layers will be rendered with one cube each. Please verify that <" << minLayerCount
                    << "> cubes are visible. In that next scene, press SELECT to pass the test or MENU to fail this test.\nPress SELECT now to dismiss the instructions and proceed to the next scene.";

                XrCompositionLayerQuad* const instructionsQuad = compositionHelper.CreateQuadLayer(
                    compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 768, oss.str().c_str(), 48)), viewSpace, 1,
                    {Quat::Identity, {-0.0f, 0, -1.0f}});
                auto instructionUpdate = [&](const XrFrameState& frameState) {
                    std::vector<XrCompositionLayerBaseHeader*> layers;
                    layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});
                    compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

                    compositionHelper.PollEvents();

                    interactionManager.SyncActions(XR_NULL_PATH);
                    // needs to be checked after each SyncActions, with no other xrSyncActions in between
                    bool keepRunning = !interactionManager.GetDefaultSelectPressed(session);

                    return keepRunning;
                };
                RenderLoop(compositionHelper.GetSession(), instructionUpdate).Loop();
            }

            auto updateLayers = [&](const XrFrameState& frameState) {
                auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
                const auto& viewState = std::get<XrViewState>(viewData);

                std::vector<XrCompositionLayerBaseHeader*> layers;

                const XrSwapchainImageBaseHeader* swapchainImage = nullptr;

                if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                    viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                    for (uint32_t i = 0; i < minLayerCount; i++) {
                        const auto& views = std::get<std::vector<XrView>>(viewData);
                        uint32_t swapchainIndex = i / 4;

                        XrColor4f transparentClearColor{0, 0, 0, 0};

                        // layers 0-3, 4-7, etc. are rendered into the same swapchain image
                        if (i % layersPerSC == 0) {
                            swapchainImage = compositionHelper.AcquireWaitImage(swapchains[swapchainIndex]);
                            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, transparentClearColor);
                        }

                        std::vector<Cube> cubes;

                        cubes.push_back({Cube::Make(gridPoses[i].position, width)});

                        // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                        for (size_t view = 0; view < views.size(); view++) {
                            const_cast<XrFovf&>(projLayers[i]->views[view].fov) = views[view].fov;
                            const_cast<XrPosef&>(projLayers[i]->views[view].pose) = views[view].pose;
                            GetGlobalData().graphicsPlugin->RenderView(projLayers[i]->views[view], swapchainImage,
                                                                       RenderParams().Draw(cubes));
                        }

                        // after layer 3, 7, etc. has been rendered, the swapchain image can be released.
                        // if maxSupportedProjectionLayers is not divisible by 4, we still need to release on the last one.
                        if (i % layersPerSC == layersPerSC - 1 || i == minLayerCount - 1) {
                            compositionHelper.ReleaseImage(swapchains[swapchainIndex]);
                        }

                        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayers[i]));
                    }
                }

                compositionHelper.EndFrame(frameState.predictedDisplayTime, layers, false);

                compositionHelper.PollEvents();

                interactionManager.SyncActions(XR_NULL_PATH);
                REQUIRE_MSG(!interactionManager.GetDefaultMenuPressed(session), "User failed the test by pressing MENU");

                bool keepRunning = !interactionManager.GetDefaultSelectPressed(session);

                return keepRunning;
            };

            RenderLoop(session, updateLayers).Loop();
        }

        SECTION("RenderMaxQuadLayers")
        {

            {
                std::ostringstream oss;
                oss << "In the next scene, <" << minLayerCount << "> quad layers will be rendered. Please verify that <" << minLayerCount
                    << "> quads are visible. In that next scene, press SELECT to pass the test or MENU to fail this test.\nPress SELECT now to dismiss the instructions and proceed to the next scene.";

                XrCompositionLayerQuad* const instructionsQuad = compositionHelper.CreateQuadLayer(
                    compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 768, oss.str().c_str(), 48)), viewSpace, 1,
                    {Quat::Identity, {-0.0f, 0, -1.0f}});
                auto instructionUpdate = [&](const XrFrameState& frameState) {
                    std::vector<XrCompositionLayerBaseHeader*> layers;
                    layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});
                    compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

                    compositionHelper.PollEvents();

                    interactionManager.SyncActions(XR_NULL_PATH);
                    bool keepRunning = !interactionManager.GetDefaultSelectPressed(session);

                    return keepRunning;
                };
                RenderLoop(compositionHelper.GetSession(), instructionUpdate).Loop();
            }

            auto updateLayers = [&](const XrFrameState& frameState) {
                std::vector<XrCompositionLayerBaseHeader*> layers;

                for (uint32_t i = 0; i < minLayerCount; i++) {
                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(quadLayers[i]));
                }

                compositionHelper.EndFrame(frameState.predictedDisplayTime, layers, false);

                compositionHelper.PollEvents();

                interactionManager.SyncActions(XR_NULL_PATH);
                REQUIRE_MSG(!interactionManager.GetDefaultMenuPressed(session), "User failed the test by pressing MENU");

                bool keepRunning = !interactionManager.GetDefaultSelectPressed(session);

                return keepRunning;
            };

            RenderLoop(session, updateLayers).Loop();
        }

        SECTION("RenderMaxProjAndQuadLayers")
        {
            // We require at least XR_MIN_COMPOSITION_LAYERS_SUPPORTED projection layers to be supported, so this is always > 0.
            uint32_t projLayerCount = minLayerCount / 2;

            uint32_t quadLayersToAdd = minLayerCount - projLayerCount;

            {
                std::ostringstream oss;
                oss << "In the next scene, <" << projLayerCount << "> projection and <" << quadLayersToAdd
                    << "> quad layers will be rendered. Please verify that <" << projLayerCount << "> cubes and <" << quadLayersToAdd
                    << "> quads are visible. In that next scene, press SELECT to pass the test or MENU to fail this test.\nPress SELECT now to dismiss the instructions and proceed to the next scene.";

                XrCompositionLayerQuad* const instructionsQuad = compositionHelper.CreateQuadLayer(
                    compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 768, oss.str().c_str(), 48)), viewSpace, 1,
                    {Quat::Identity, {-0.0f, 0, -1.0f}});
                auto instructionUpdate = [&](const XrFrameState& frameState) {
                    std::vector<XrCompositionLayerBaseHeader*> layers;
                    layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});
                    compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

                    compositionHelper.PollEvents();

                    interactionManager.SyncActions(XR_NULL_PATH);
                    bool keepRunning = !interactionManager.GetDefaultSelectPressed(session);

                    return keepRunning;
                };
                RenderLoop(compositionHelper.GetSession(), instructionUpdate).Loop();
            }

            auto updateLayers = [&](const XrFrameState& frameState) {
                auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
                const auto& viewState = std::get<XrViewState>(viewData);

                std::vector<XrCompositionLayerBaseHeader*> layers;

                const XrSwapchainImageBaseHeader* swapchainImage = nullptr;

                if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                    viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                    for (uint32_t i = 0; i < projLayerCount; i++) {
                        const auto& views = std::get<std::vector<XrView>>(viewData);
                        uint32_t swapchainIndex = i / 4;

                        XrColor4f transparentClearColor{0, 0, 0, 0};

                        // layers 0-3, 4-7, etc. are rendered into the same swapchain image
                        if (i % layersPerSC == 0) {
                            swapchainImage = compositionHelper.AcquireWaitImage(swapchains[swapchainIndex]);
                            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, transparentClearColor);
                        }

                        std::vector<Cube> cubes;

                        cubes.push_back({Cube::Make(gridPoses[i].position, width)});

                        // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                        for (size_t view = 0; view < views.size(); view++) {
                            const_cast<XrFovf&>(projLayers[i]->views[view].fov) = views[view].fov;
                            const_cast<XrPosef&>(projLayers[i]->views[view].pose) = views[view].pose;
                            GetGlobalData().graphicsPlugin->RenderView(projLayers[i]->views[view], swapchainImage,
                                                                       RenderParams().Draw(cubes));
                        }

                        // after layer 3, 7, etc. has been rendered, the swapchain image can be released.
                        // if maxSupportedProjectionLayers is not divisible by layersPerSC, we still need to release on the last one.
                        if (i % layersPerSC == layersPerSC - 1 || i == projLayerCount - 1) {
                            compositionHelper.ReleaseImage(swapchains[swapchainIndex]);
                        }

                        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayers[i]));
                    }
                }

                for (uint32_t i = 0; i < quadLayersToAdd; i++) {

                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(quadLayers[projLayerCount + i]));
                }

                compositionHelper.EndFrame(frameState.predictedDisplayTime, layers, false);

                compositionHelper.PollEvents();

                interactionManager.SyncActions(XR_NULL_PATH);
                REQUIRE_MSG(!interactionManager.GetDefaultMenuPressed(session), "User failed the test by pressing MENU");

                bool keepRunning = !interactionManager.GetDefaultSelectPressed(session);

                return keepRunning;
            };

            RenderLoop(session, updateLayers).Loop();
        }
    }

    TEST_CASE("QuadHands", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        CompositionHelper compositionHelper("Quad Hands");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "quad_hands.png",
                                                        "10x10cm Quads labeled \'L\' and \'R\' should appear 10cm along the grip "
                                                        "positive Z in front of the center of 10cm cubes rendered at the controller "
                                                        "grip poses, or at the origin if that controller isn't being tested."
                                                        "The quads should face you and be upright when the controllers are in "
                                                        "a thumbs-up pointing-into-screen pose. "
                                                        "Check that the quads are properly backface-culled, "
                                                        "that \'R\' is always rendered atop \'L\', "
                                                        "and both are atop the cubes when visible.");

        const std::array<XrPath, 2> subactionPaths{
            StringToPath(instance, "/user/hand/left"),
            StringToPath(instance, "/user/hand/right"),
        };

        XrActionSet actionSet;
        XrAction gripPoseAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "quad_hands");
            strcpy(actionSetInfo.localizedActionSetName, "Quad Hands");
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
        compositionHelper.BeginSession();

        SimpleProjectionLayerHelper simpleProjectionLayerHelper(compositionHelper);

        // Spaces attached to the hand (subaction).
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
            }
            else {
                XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
                spaceCreateInfo.poseInReferenceSpace = Pose::Identity;
                XRC_CHECK_THROW_XRCMD(xrCreateReferenceSpace(session, &spaceCreateInfo, &space));
            }
            gripSpaces.push_back(space);
        }

        // Create 10x10cm L and R quads
        XrCompositionLayerQuad* const leftQuadLayer =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(64, 64, "L", 48)), gripSpaces[0],
                                              0.1f, {Quat::Identity, {0, 0, 0.1f}});

        XrCompositionLayerQuad* const rightQuadLayer =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(64, 64, "R", 48)), gripSpaces[1],
                                              0.1f, {Quat::Identity, {0, 0, 0.1f}});

        interactiveLayerManager.AddLayer(leftQuadLayer);
        interactiveLayerManager.AddLayer(rightQuadLayer);

        const XrVector3f cubeSize{0.1f, 0.1f, 0.1f};
        auto updateLayers = [&](const XrFrameState& frameState) {
            std::vector<Cube> cubes;
            for (const auto& space : gripSpaces) {
                XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
                if (XR_SUCCEEDED(
                        xrLocateSpace(space, simpleProjectionLayerHelper.GetLocalSpace(), frameState.predictedDisplayTime, &location))) {
                    if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
                        (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
                        cubes.emplace_back(Cube{location.pose, cubeSize});
                    }
                }
            }
            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (XrCompositionLayerBaseHeader* projLayer = simpleProjectionLayerHelper.TryGetUpdatedProjectionLayer(frameState, cubes)) {
                layers.push_back(projLayer);
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

    TEST_CASE("ProjectionMutableFieldOfView", "[composition][interactive]")
    {
        for (bool submitDepthSwapchain : {false, true}) {
            DYNAMIC_SECTION((submitDepthSwapchain ? "With" : "Without") << " depth submission")
            {
                std::vector<const char*> extensions;
                if (submitDepthSwapchain) {
                    if (!GetGlobalData().IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME)) {
                        continue;
                    }
                    extensions.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
                }
                GlobalData& globalData = GetGlobalData();
                if (!globalData.IsUsingGraphicsPlugin()) {
                    SKIP("Cannot test without a graphics plugin");
                }

                CompositionHelper compositionHelper("Projection Mutable Field-of-View", extensions);
                XrSession session = compositionHelper.GetSession();
                InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
                InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_mutable.png",
                                                                "Uses mutable field-of-views for each projection layer view.");
                interactionManager.AttachActionSets();
                compositionHelper.BeginSession();

                const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

                if (!compositionHelper.GetViewConfigurationProperties().fovMutable) {
                    SKIP("View configuration does not support mutable FoV");
                }

                const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

                const auto totalWidth =
                    std::accumulate(viewProperties.begin(), viewProperties.end(), 0,
                                    [](uint32_t l, const XrViewConfigurationView& r) { return l + r.recommendedImageRectWidth; });
                // Because a single swapchain is being used for all views the maximum height must be used.
                const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                        [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                            return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                        })
                                           ->recommendedImageRectHeight;

                // Create wide swapchain.
                XrSwapchainCreateInfo swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(totalWidth, maxHeight);
                XrSwapchainCreateInfo depthSwapchainCreateInfo = compositionHelper.DefaultDepthSwapchainCreateInfo(totalWidth, maxHeight);
                if (submitDepthSwapchain && depthSwapchainCreateInfo.format == -1) {
                    // no depth format available for testing
                    continue;
                }

                XrSwapchain swapchain{XR_NULL_HANDLE};
                XrSwapchain depthSwapchain{XR_NULL_HANDLE};
                if (submitDepthSwapchain) {
                    std::tie(swapchain, depthSwapchain) =
                        compositionHelper.CreateSwapchainWithDepth(swapchainCreateInfo, depthSwapchainCreateInfo);
                }
                else {
                    swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);
                }

                XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
                std::vector<XrCompositionLayerDepthInfoKHR> depthInfo(projLayer->viewCount);
                int x = 0;
                for (uint32_t j = 0; j < projLayer->viewCount; j++) {
                    // views field is pointer to const, but views haven't been populated yet
                    auto& view = const_cast<XrCompositionLayerProjectionView&>(projLayer->views[j]);
                    XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);
                    XrSwapchainSubImage depthSubImage =
                        submitDepthSwapchain ? compositionHelper.MakeDefaultSubImage(depthSwapchain, 0) : XrSwapchainSubImage{};
                    for (XrSwapchainSubImage* s : {&subImage, &depthSubImage}) {
                        s->imageRect.offset = {x, 0};
                        s->imageRect.extent = {(int32_t)viewProperties[j].recommendedImageRectWidth,
                                               (int32_t)viewProperties[j].recommendedImageRectHeight};
                    }
                    view.subImage = subImage;
                    if (submitDepthSwapchain) {
                        InsertDefaultDepthInfo(&depthInfo[j], &view.next, depthSubImage);
                    }
                    x += subImage.imageRect.extent.width;  // Each view is to the left of the previous view.
                }

                const std::vector<Cube> cubes = {Cube::Make({-.2f, -.2f, -2}), Cube::Make({.2f, -.2f, -2}), Cube::Make({0, .1f, -2})};

                const XrVector3f Forward{0, 0, 1};
                const XrQuaternionf roll180 = Quat::FromAxisAngle(Forward, MATH_PI);

                auto updateLayers = [&](const XrFrameState& frameState) {
                    auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
                    const auto& viewState = std::get<XrViewState>(viewData);

                    std::vector<XrCompositionLayerBaseHeader*> layers;
                    if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                        viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                        const auto& views = std::get<std::vector<XrView>>(viewData);

                        // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                        compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                            for (size_t viewIndex = 0; viewIndex < views.size(); viewIndex++) {
                                // views field is pointer to const, but views haven't been populated yet
                                auto& projView = const_cast<XrCompositionLayerProjectionView&>(projLayer->views[viewIndex]);
                                // Copy over the provided FOV and pose but use 40% of the suggested FOV.
                                projView.fov = views[viewIndex].fov;
                                projView.pose = views[viewIndex].pose;
                                projView.fov.angleUp *= 0.4f;
                                projView.fov.angleDown *= 0.4f;
                                projView.fov.angleLeft *= 0.4f;
                                projView.fov.angleRight *= 0.4f;

                                // Render using a 180 degree roll on Z which effectively creates a flip on both the X and Y axis.
                                XrCompositionLayerProjectionView rolled = projView;
                                rolled.pose.orientation = roll180 * views[viewIndex].pose.orientation;
                                GetGlobalData().graphicsPlugin->RenderView(rolled, swapchainImage, RenderParams().Draw(cubes));

                                // After rendering, report a flipped FOV on X and Y without the 180 degree roll, which has the same
                                // effect. This switcheroo is necessary since rendering with flipped FOV will result in an inverted
                                // winding causing normally hidden triangles to be visible and visible triangles to be hidden.
                                projView.fov.angleUp = -projView.fov.angleUp;
                                projView.fov.angleDown = -projView.fov.angleDown;
                                projView.fov.angleLeft = -projView.fov.angleLeft;
                                projView.fov.angleRight = -projView.fov.angleRight;
                            }
                        });

                        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
                    }
                    return interactiveLayerManager.EndFrame(frameState, layers);
                };

                RenderLoop(session, updateLayers).Loop();
            }
        }
    }

    TEST_CASE("StaleSwapchain", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test stale swapchains without a graphics plugin");
        }

        CompositionHelper compositionHelper("Stale swapchain");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "stale_swapchain.png",
                                                        "Updates swapchain of each square at 1Hz. "
                                                        "Square on left should be constantly green, and square on right "
                                                        "should switch between green and blue every second. "
                                                        "If there is any flicker on the green square, "
                                                        "likely at the same time as the other square changes color, "
                                                        "that is a failure.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{Quat::Identity, {0, 0, -1}});

        constexpr int ImageSize = 1;

        // Create an array swapchain
        auto swapchainCreateInfo =
            compositionHelper.DefaultColorSwapchainCreateInfo(ImageSize, ImageSize, 0, globalData.graphicsPlugin->GetSRGBA8Format());
        swapchainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        const XrSwapchain constantColorSwapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);
        const XrSwapchain alternatingColorSwapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

        RGBAImage images[2] = {RGBAImage(ImageSize, ImageSize), RGBAImage(ImageSize, ImageSize)};
        images[0].DrawRect(0, 0, ImageSize, ImageSize, Colors::Green);
        images[1].DrawRect(0, 0, ImageSize, ImageSize, Colors::Blue);
        images[0].ConvertToSRGB();
        images[1].ConvertToSRGB();

        XrCompositionLayerQuad* const constantQuad =
            compositionHelper.CreateQuadLayer(constantColorSwapchain, viewSpace, 0.02f, XrPosef{Quat::Identity, {-0.1f, 0, -1}});
        interactiveLayerManager.AddLayer(constantQuad);

        XrCompositionLayerQuad* const alternatingQuad =
            compositionHelper.CreateQuadLayer(alternatingColorSwapchain, viewSpace, 0.02f, XrPosef{Quat::Identity, {0.1f, 0, -1}});
        interactiveLayerManager.AddLayer(alternatingQuad);

        XrTime lastUpdate = 0;
        bool alternatingIndex = false;
        RenderLoop(compositionHelper.GetSession(), [&](const XrFrameState& frameState) {
            // Failing this test may create a flashing image. 1Hz is well outside the
            // documented normal range for photosensitive epilepsy (rarely as low as 3Hz).
            // Regardless, failures may e.g. create a black flash every second, so we use a
            // small square to minimise any effects of the failure condition.
            if (lastUpdate == 0 || (frameState.predictedDisplayTime - lastUpdate) >= 1_xrSeconds) {
                lastUpdate = frameState.predictedDisplayTime;
                compositionHelper.AcquireWaitReleaseImage(constantColorSwapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                    globalData.graphicsPlugin->CopyRGBAImage(swapchainImage, 0, images[0]);
                });
                compositionHelper.AcquireWaitReleaseImage(alternatingColorSwapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                    globalData.graphicsPlugin->CopyRGBAImage(swapchainImage, 0, images[(uint32_t)alternatingIndex]);
                    alternatingIndex = !alternatingIndex;
                });
            }
            return interactiveLayerManager.EndFrame(frameState);
        }).Loop();
    }

    TEST_CASE("ProjectionDepth", "[XR_KHR_composition_layer_depth][XR_FB_composition_layer_depth_test][composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        if (!globalData.IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME)) {
            SKIP(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME " not supported");
        }
        if (!globalData.IsInstanceExtensionSupported(XR_FB_COMPOSITION_LAYER_DEPTH_TEST_EXTENSION_NAME)) {
            SKIP(XR_FB_COMPOSITION_LAYER_DEPTH_TEST_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper(
            "Projection Depth", {XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME, XR_FB_COMPOSITION_LAYER_DEPTH_TEST_EXTENSION_NAME});
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_depth.png",
                                                        "Four cubes each are drawn on two different layers, with the front face"
                                                        " appearing darker on the second layer. All eight cubes should be visible,"
                                                        " with the darker blue front face appearing closer on the left and bottom,"
                                                        " and further away on the right and top.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        std::vector<XrSwapchainCreateInfo> colorSwapchainCreateInfo;
        std::vector<XrSwapchainCreateInfo> depthSwapchainCreateInfo;
        for (auto& view : viewProperties) {
            colorSwapchainCreateInfo.push_back(
                compositionHelper.DefaultColorSwapchainCreateInfo(view.recommendedImageRectWidth, view.recommendedImageRectHeight));
            depthSwapchainCreateInfo.push_back(
                compositionHelper.DefaultDepthSwapchainCreateInfo(view.recommendedImageRectWidth, view.recommendedImageRectHeight));
        }

        const int LayerCount = 2;
        XrCompositionLayerProjection* projLayers[LayerCount];
        XrCompositionLayerDepthTestFB depthTestInfo[LayerCount];
        std::vector<std::pair<XrSwapchain, XrSwapchain>> swapchain[LayerCount];
        std::vector<XrCompositionLayerDepthInfoKHR> depthInfo[LayerCount];

        // Set up the projection layers
        for (int layer = 0; layer < LayerCount; layer++) {
            projLayers[layer] = compositionHelper.CreateProjectionLayer(localSpace);

            // Add depth test info to the chain for each projection layer
            depthTestInfo[layer].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_TEST_FB;
            depthTestInfo[layer].next = projLayers[layer]->next;
            depthTestInfo[layer].depthMask = true;
            depthTestInfo[layer].compareOp = XR_COMPARE_OP_LESS_FB;
            const_cast<const void*&>(projLayers[layer]->next) = &depthTestInfo[layer];

            depthInfo[layer].resize(projLayers[layer]->viewCount);
            for (uint32_t j = 0; j < projLayers[layer]->viewCount; j++) {
                // views field is pointer to const, but views haven't been populated yet
                auto& projView = const_cast<XrCompositionLayerProjectionView&>(projLayers[layer]->views[j]);

                // create color and depth swapchains
                swapchain[layer].push_back(
                    compositionHelper.CreateSwapchainWithDepth(colorSwapchainCreateInfo[j], depthSwapchainCreateInfo[j]));

                projView.subImage = compositionHelper.MakeDefaultSubImage(swapchain[layer][j].first);

                // Add depth info to the chain for each projection layer view
                XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain[layer][j].second);
                InsertDefaultDepthInfo(&depthInfo[layer][j], &projView.next, subImage);
            }
        }

        // Alternate which cube should be in front. Rotate every cube in the second layer to tell them apart
        const std::vector<Cube> cubes[LayerCount] = {
            {Cube::Make({-1, 0, -2.5}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2.5}), Cube::Make({0, 1, -2})},
            {Cube::Make({-1, 0, -2}, 0.25f, {0, 1, 0, 0}), Cube::Make({1, 0, -2.5}, 0.25f, {0, 1, 0, 0}),
             Cube::Make({0, -1, -2}, 0.25f, {1, 0, 0, 0}), Cube::Make({0, 1, -2.5}, 0.25f, {1, 0, 0, 0})}};

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                for (int layer = 0; layer < LayerCount; layer++) {
                    for (size_t j = 0; j < views.size(); j++) {
                        // Render into each view's swapchain using the projection layer view fov and pose.
                        compositionHelper.AcquireWaitReleaseImage(
                            swapchain[layer][j].first, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                                GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);

                                const_cast<XrFovf&>(projLayers[layer]->views[j].fov) = views[j].fov;
                                const_cast<XrPosef&>(projLayers[layer]->views[j].pose) = views[j].pose;
                                GetGlobalData().graphicsPlugin->RenderView(projLayers[layer]->views[j], swapchainImage,
                                                                           RenderParams().Draw(cubes[layer]));
                            });
                    }
                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayers[layer]));
                }
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

}  // namespace Conformance
