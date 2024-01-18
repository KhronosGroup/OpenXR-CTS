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

#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/bitmask_generator.h"
#include "utilities/bitmask_to_string.h"
#include "utilities/generator.h"
#include "utilities/xrduration_literals.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <vector>

namespace Conformance
{
    struct ProjectionLayerWithViews
    {
        ProjectionLayerWithViews(ProjectionLayerWithViews&&) = delete;
        ProjectionLayerWithViews& operator=(ProjectionLayerWithViews&&) = delete;

        ProjectionLayerWithViews(const ProjectionLayerWithViews& other) : Layer(other.Layer), ProjectionViews(other.ProjectionViews)
        {
            Layer.views = ProjectionViews.data();  // New address must be updated.
        }

        ProjectionLayerWithViews(std::vector<XrView> views, XrSpace space, const std::function<XrSwapchainSubImage(uint32_t)>& getSubImage)
        {
            for (size_t viewIndex = 0; viewIndex < views.size(); viewIndex++) {
                XrCompositionLayerProjectionView projectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                projectionView.pose = views[viewIndex].pose;
                projectionView.fov = views[viewIndex].fov;
                projectionView.subImage = getSubImage((uint32_t)viewIndex);
                ProjectionViews.push_back(projectionView);
            }

            Layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            Layer.viewCount = (uint32_t)ProjectionViews.size();
            Layer.views = ProjectionViews.data();
            Layer.space = space;
        }

        XrCompositionLayerProjection Layer;
        std::vector<XrCompositionLayerProjectionView> ProjectionViews;
    };

    TEST_CASE("XrCompositionLayerProjection", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            // Nothing to check - no graphics plugin means no frame submission
            SKIP("Cannot test using frame submission without a graphics plugin");
        }

        AutoBasicSession session(AutoBasicSession::beginSession | AutoBasicSession::createSpaces);

        // viewConfigurationViewVector is based on the global view configuration.
        uint32_t viewCount = (uint32_t)session.viewConfigurationViewVector.size();

        XrSwapchain colorSwapchain{XR_NULL_HANDLE}, unreleasedColorSwapchain{XR_NULL_HANDLE}, arrayColorSwapchain{XR_NULL_HANDLE};
        XrExtent2Di colorSwapchainExtent{256, 256}, unreleasedColorSwapchainExtent{256, 256}, arrayColorSwapchainExtent{256, 256};
        REQUIRE(XR_SUCCESS == CreateColorSwapchain(session, globalData.GetGraphicsPlugin().get(), &colorSwapchain, &colorSwapchainExtent));
        REQUIRE(XR_SUCCESS == CreateColorSwapchain(session, globalData.GetGraphicsPlugin().get(), &arrayColorSwapchain,
                                                   &arrayColorSwapchainExtent, viewCount));
        REQUIRE(XR_SUCCESS == CreateColorSwapchain(session, globalData.GetGraphicsPlugin().get(), &unreleasedColorSwapchain,
                                                   &unreleasedColorSwapchainExtent));

        // Acquire+Wait+Release swapchains so that they are in a valid state but leave
        // unreleasedColorSwapchain in an unused state for a test case.
        {
            std::array<XrSwapchain, 2> swapchains{colorSwapchain, arrayColorSwapchain};
            REQUIRE(XR_SUCCESS == CycleToNextSwapchainImage(swapchains.data(), swapchains.size(), 3_xrSeconds));
        }

        auto waitAndBeginFrame = [&] {
            XrFrameState frameState{XR_TYPE_FRAME_STATE};
            REQUIRE(XR_SUCCESS == xrWaitFrame(session, nullptr, &frameState));
            REQUIRE_RESULT_SUCCEEDED(xrBeginFrame(session, nullptr));
            return frameState;
        };

        auto locateViews = [&](const XrFrameState& frameState) {
            XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
            locateInfo.space = session.spaceVector.front();
            locateInfo.displayTime = frameState.predictedDisplayTime;
            locateInfo.viewConfigurationType = globalData.GetOptions().viewConfigurationValue;

            XrViewState viewState{XR_TYPE_VIEW_STATE};
            std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
            uint32_t viewCountOut = viewCount;
            REQUIRE(xrLocateViews(session, &locateInfo, &viewState, viewCount, &viewCountOut, views.data()) == XR_SUCCESS);
            CAPTURE(XrViewStateFlagsCPP(viewState.viewStateFlags));
            // Must have a pose in order to submit projection layers.
            REQUIRE_MSG((viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0,
                        "Projection layer tests require view orientation tracking");
            REQUIRE_MSG((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0,
                        "Projection layer tests require view position tracking");

            return views;
        };

        auto endFrame = [&](const XrFrameState& frameState, std::vector<void*> layers) {
            XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
            frameEndInfo.environmentBlendMode = globalData.GetOptions().environmentBlendModeValue;
            frameEndInfo.displayTime = frameState.predictedDisplayTime;
            frameEndInfo.layerCount = (uint32_t)layers.size();
            frameEndInfo.layers = reinterpret_cast<const XrCompositionLayerBaseHeader* const*>(layers.data());
            // If the below xrEndFrame fails rely on the next xrBeginFrame's discard to recover.
            return xrEndFrame(session, &frameEndInfo);
        };

        auto createColorSwapchainSubImage = [&](uint32_t /*viewIndex*/) {
            XrSwapchainSubImage subImage;
            subImage.swapchain = colorSwapchain;
            subImage.imageRect = {{0, 0}, colorSwapchainExtent};
            subImage.imageArrayIndex = 0;
            return subImage;
        };

        {
            INFO("Valid projection tests");

            {
                INFO("Basic layer");
                XrFrameState frameState = waitAndBeginFrame();
                std::vector<XrView> views = locateViews(frameState);
                ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                CHECK(XR_SUCCESS == endFrame(frameState, {&projectionLayerWithViews.Layer}));
            }

            {
                INFO("Texture array layer");
                XrFrameState frameState = waitAndBeginFrame();
                std::vector<XrView> views = locateViews(frameState);
                ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), [&](uint32_t viewIndex) {
                    XrSwapchainSubImage subImage;
                    subImage.swapchain = arrayColorSwapchain;
                    subImage.imageRect = {{0, 0}, arrayColorSwapchainExtent};
                    subImage.imageArrayIndex = viewIndex;
                    return subImage;
                });

                CHECK(XR_SUCCESS == endFrame(frameState, {&projectionLayerWithViews.Layer}));
            }

            {
                INFO("Layer flags");
                auto&& layerFlagsGenerator = bitmaskGeneratorIncluding0({XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
                                                                         XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
                                                                         XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT});
                while (layerFlagsGenerator.next()) {
                    CAPTURE(XrCompositionLayerFlagsCPP(layerFlagsGenerator.get()));
                    XrFrameState frameState = waitAndBeginFrame();
                    std::vector<XrView> views = locateViews(frameState);
                    ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                    projectionLayerWithViews.Layer.layerFlags = layerFlagsGenerator.get();
                    CHECK(XR_SUCCESS == endFrame(frameState, {&projectionLayerWithViews.Layer}));
                }
            }

            {
                INFO("Spaces");
                for (XrSpace space : session.spaceVector) {
                    XrFrameState frameState = waitAndBeginFrame();
                    std::vector<XrView> views = locateViews(frameState);
                    ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                    projectionLayerWithViews.Layer.space = space;
                    CHECK(XR_SUCCESS == endFrame(frameState, {&projectionLayerWithViews.Layer}));
                }
            }

            {
                INFO("XR_MIN_COMPOSITION_LAYERS_SUPPORTED layers");
                XrFrameState frameState = waitAndBeginFrame();
                std::vector<XrView> views = locateViews(frameState);
                std::vector<ProjectionLayerWithViews> minQuadLayers(XR_MIN_COMPOSITION_LAYERS_SUPPORTED,
                                                                    {views, session.spaceVector.front(), createColorSwapchainSubImage});
                std::vector<void*> minLayers;  // Convert into an array of pointers (needed by xrEndFrame).
                std::transform(minQuadLayers.begin(), minQuadLayers.end(), std::back_inserter(minLayers),
                               [](ProjectionLayerWithViews& q) { return (void*)&q.Layer; });
                CHECK(XR_SUCCESS == endFrame(frameState, minLayers));
            }
        }

        {
            INFO("Invalid projection tests");

            for (uint32_t view = 0; view < viewCount; view++) {
                INFO("Testing projection view index " << view);

                {
                    INFO("Invalid unreleased (and also never acquired) swapchain");

                    XrFrameState frameState = waitAndBeginFrame();
                    std::vector<XrView> views = locateViews(frameState);
                    ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                    projectionLayerWithViews.ProjectionViews[view].subImage.swapchain = unreleasedColorSwapchain;
                    CHECK(XR_ERROR_LAYER_INVALID == endFrame(frameState, {&projectionLayerWithViews.Layer}));
                }

                {
                    INFO("Invalid view count");
                    XrFrameState frameState = waitAndBeginFrame();
                    std::vector<XrView> views = locateViews(frameState);
                    ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                    projectionLayerWithViews.Layer.viewCount--;
                    CHECK(XR_ERROR_VALIDATION_FAILURE == endFrame(frameState, {&projectionLayerWithViews.Layer}));
                }

                {
                    INFO("Invalid pose");
                    XrFrameState frameState = waitAndBeginFrame();
                    std::vector<XrView> views = locateViews(frameState);
                    ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                    projectionLayerWithViews.ProjectionViews[view].pose.orientation = {0.1f, 0.1f, 0.1f, 0.1f};
                    CHECK(XR_ERROR_POSE_INVALID == endFrame(frameState, {&projectionLayerWithViews.Layer}));
                }

                {
                    INFO("Invalid imageRect with negative offset");
                    XrFrameState frameState = waitAndBeginFrame();
                    std::vector<XrView> views = locateViews(frameState);
                    ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                    projectionLayerWithViews.ProjectionViews[view].subImage.imageRect.offset = {-1, -1};
                    CHECK(XR_ERROR_SWAPCHAIN_RECT_INVALID == endFrame(frameState, {&projectionLayerWithViews.Layer}));
                }

                {
                    INFO("Invalid imageRect out of bounds");
                    XrFrameState frameState = waitAndBeginFrame();
                    std::vector<XrView> views = locateViews(frameState);
                    ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                    projectionLayerWithViews.ProjectionViews[view].subImage.imageRect.offset = {1, 1};
                    CHECK(XR_ERROR_SWAPCHAIN_RECT_INVALID == endFrame(frameState, {&projectionLayerWithViews.Layer}));
                }

                {
                    INFO("Invalid swapchain array index");
                    XrFrameState frameState = waitAndBeginFrame();
                    std::vector<XrView> views = locateViews(frameState);
                    ProjectionLayerWithViews projectionLayerWithViews(views, session.spaceVector.front(), createColorSwapchainSubImage);
                    projectionLayerWithViews.ProjectionViews[view].subImage.imageArrayIndex = 1;
                    CHECK(XR_ERROR_VALIDATION_FAILURE == endFrame(frameState, {&projectionLayerWithViews.Layer}));
                }
            }
        }
    }
}  // namespace Conformance
