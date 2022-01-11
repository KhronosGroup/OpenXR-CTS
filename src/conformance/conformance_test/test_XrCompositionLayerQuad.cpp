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
#include "conformance_framework.h"
#include "bitmask_generator.h"
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <catch2/catch.hpp>

namespace Conformance
{
    TEST_CASE("XrCompositionLayerQuad", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            // Nothing to check - no graphics plugin means no frame submission
            return;
        }

        AutoBasicSession session(AutoBasicSession::beginSession | AutoBasicSession::createSpaces);

        XrSwapchain colorSwapchain, unreleasedColorSwapchain;
        XrExtent2Di swapchainExtent{256, 256};
        XRC_CHECK_THROW_XRCMD(CreateColorSwapchain(session, globalData.GetGraphicsPlugin().get(), &colorSwapchain, &swapchainExtent));
        XRC_CHECK_THROW_XRCMD(
            CreateColorSwapchain(session, globalData.GetGraphicsPlugin().get(), &unreleasedColorSwapchain, &swapchainExtent));

        // Acquire+Wait+Release colorSwapchain so that it is in a valid state but leave
        // unreleasedColorSwapchain in an unused state.
        {
            XrSwapchain swapchains[] = {colorSwapchain};
            XRC_CHECK_THROW_XRCMD(CycleToNextSwapchainImage(swapchains, 1, 3_xrSeconds));
        }

        auto makeSimpleQuad = [&] {
            XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
            quad.pose = XrPosefCPP{};
            quad.space = session.spaceVector.front();
            quad.size = {1, 1};
            quad.subImage.imageRect = {{0, 0}, swapchainExtent};
            quad.subImage.swapchain = colorSwapchain;
            return quad;
        };

        auto submitFrame = [&](std::vector<void*> layers) {
            XrFrameState frameState{XR_TYPE_FRAME_STATE};
            XRC_CHECK_THROW_XRCMD(xrWaitFrame(session, nullptr, &frameState));
            XRC_CHECK_THROW_XRCMD(xrBeginFrame(session, nullptr));

            XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
            frameEndInfo.environmentBlendMode = globalData.GetOptions().environmentBlendModeValue;
            frameEndInfo.displayTime = frameState.predictedDisplayTime;
            frameEndInfo.layerCount = (uint32_t)layers.size();
            frameEndInfo.layers = reinterpret_cast<const XrCompositionLayerBaseHeader* const*>(layers.data());
            // If the below xrEndFrame fails rely on the next xrBeginFrame's discard to recover.
            return xrEndFrame(session, &frameEndInfo);
        };

        {
            INFO("Valid quad tests");

            {
                INFO("Basic layer");
                XrCompositionLayerQuad quad = makeSimpleQuad();
                CHECK(XR_SUCCESS == submitFrame({&quad}));
            }

            {
                INFO("Layer flags");
                auto&& layerFlagsGenerator = bitmaskGeneratorIncluding0(
                    {{"XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT", XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT},
                     {"XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT", XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT},
                     {"XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT", XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT}});
                while (layerFlagsGenerator.next()) {
                    CAPTURE(layerFlagsGenerator.get().bitmask);
                    XrCompositionLayerQuad quad = makeSimpleQuad();
                    quad.layerFlags = layerFlagsGenerator.get().bitmask;
                    CHECK(XR_SUCCESS == submitFrame({&quad}));
                }
            }

            {
                INFO("Sizes");
                for (const XrExtent2Df& size : {XrExtent2Df{0, 0}, XrExtent2Df{1, 1}}) {
                    XrCompositionLayerQuad quad = makeSimpleQuad();
                    quad.size = size;
                    CHECK(XR_SUCCESS == submitFrame({&quad}));
                }
            }

            {
                INFO("Eye visibility stereo");
                XrCompositionLayerQuad quadLeft = makeSimpleQuad();
                quadLeft.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
                CAPTURE(quadLeft.eyeVisibility);
                XrCompositionLayerQuad quadRight = makeSimpleQuad();
                quadRight.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
                CAPTURE(quadRight.eyeVisibility);
                CHECK(XR_SUCCESS == submitFrame({&quadLeft, &quadRight}));
            }

            {
                INFO("Eye visibility mono");
                for (XrEyeVisibility eyeVisibility : {XR_EYE_VISIBILITY_LEFT, XR_EYE_VISIBILITY_RIGHT}) {
                    CAPTURE(eyeVisibility);
                    XrCompositionLayerQuad quad = makeSimpleQuad();
                    quad.eyeVisibility = eyeVisibility;
                    CHECK(XR_SUCCESS == submitFrame({&quad}));
                }
            }

            {
                INFO("Spaces");
                for (XrSpace space : session.spaceVector) {
                    XrCompositionLayerQuad quad = makeSimpleQuad();
                    quad.space = space;
                    CHECK(XR_SUCCESS == submitFrame({&quad}));
                }
            }

            {
                INFO("XR_MIN_COMPOSITION_LAYERS_SUPPORTED layers");
                std::vector<XrCompositionLayerQuad> minQuadLayers(XR_MIN_COMPOSITION_LAYERS_SUPPORTED, makeSimpleQuad());

                std::vector<void*> minLayers;  // Convert into an array of pointers (needed by xrEndFrame).
                std::transform(minQuadLayers.begin(), minQuadLayers.end(), std::back_inserter(minLayers),
                               [](XrCompositionLayerQuad& q) { return (void*)&q; });

                CHECK(XR_SUCCESS == submitFrame(minLayers));
            }
        }

        {
            INFO("Invalid quad tests");

            {
                INFO("Invalid unreleased (and also never acquired) swapchain");
                XrCompositionLayerQuad quad = makeSimpleQuad();
                quad.subImage.swapchain = unreleasedColorSwapchain;
                CHECK(XR_ERROR_LAYER_INVALID == submitFrame({&quad}));
            }

            {
                INFO("Invalid pose");
                XrCompositionLayerQuad quad = makeSimpleQuad();
                quad.pose.orientation = {0, 0, 0, 0.98f};  // (exceeds allowed 1% norm deviation)
                CHECK(XR_ERROR_POSE_INVALID == submitFrame({&quad}));
            }

            {
                INFO("Invalid imageRect with negative offset");
                XrCompositionLayerQuad quad = makeSimpleQuad();
                quad.subImage.imageRect.offset = {-1, -1};
                CHECK(XR_ERROR_SWAPCHAIN_RECT_INVALID == submitFrame({&quad}));
            }

            {
                INFO("Invalid imageRect out of bounds");
                XrCompositionLayerQuad quad = makeSimpleQuad();
                quad.subImage.imageRect.offset = {1, 1};
                CHECK(XR_ERROR_SWAPCHAIN_RECT_INVALID == submitFrame({&quad}));
            }

            {
                INFO("Invalid swapchain array index");
                XrCompositionLayerQuad quad = makeSimpleQuad();
                quad.subImage.imageArrayIndex = 1;
                CHECK(XR_ERROR_VALIDATION_FAILURE == submitFrame({&quad}));
            }
        }
    }
}  // namespace Conformance
