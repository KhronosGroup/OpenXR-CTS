// Copyright (c) 2019-2023, The Khronos Group Inc.
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
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <limits>
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    // This implements an automated programmatic test of depth layers. However, a separate visual
    // test is required in order to validate that it looks correct.
    TEST_CASE("XR_KHR_composition_layer_depth", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported("XR_KHR_composition_layer_depth")) {
            return;
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            return;
        }

        auto graphicsPlugin = globalData.GetGraphicsPlugin();

        auto timeout = (globalData.options.debugMode ? 3600s : 10s);
        CAPTURE(timeout);
        AutoBasicInstance instance({"XR_KHR_composition_layer_depth"});
        AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                     AutoBasicSession::createSpaces,
                                 instance);
        REQUIRE(session.IsValidHandle());

        FrameIterator frameIterator(&session);
        FrameIterator::RunResult runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED, timeout);
        REQUIRE(runResult == FrameIterator::RunResult::Success);

        // At this point we have a session ready for us to generate custom frames for.
        // The current XrSessionState is XR_SESSION_STATE_FOCUSED.

        const uint32_t viewCount = frameIterator.compositionLayerProjection.viewCount;

        // Create depth buffer swapchains.
        std::vector<XrSwapchain> depthSwapchains(viewCount);
        for (XrSwapchain& depthSwapchain : depthSwapchains) {
            XrResult result = CreateDepthSwapchain(session.GetSession(), graphicsPlugin.get(), &depthSwapchain, &session.swapchainExtent);
            REQUIRE_RESULT_SUCCEEDED(result);
        }

        auto&& layerFlagsGenerator = bitmaskGeneratorIncluding0({XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
                                                                 XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
                                                                 XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT});
        while (layerFlagsGenerator.next()) {
            // minDepth and maxDepth are the range of depth values the depthSwapchain could have,
            //   in the range of [0.0,1.0]. This is akin to min and max values of OpenGL's glDepthRange,
            //   but with the requirement here that maxDepth >= minDepth.
            // nearZ is the positive distance in meters of the minDepth value in the depth swapchain.
            //   Apps may use a nearZ that is greater than farZ to indicate depth values
            //   are reversed. nearZ can be infinite.
            // farZ is the positive distance in meters of the maxDepth value in the depth swapchain.
            //   farZ can be infinite. Apps must not use the same value as nearZ.

            struct DepthVaryingInfo
            {  // This is a subset of XrCompositionLayerDepthInfoKHR.
                float minDepth;
                float maxDepth;
                float nearZ;
                float farZ;
            };

            constexpr float minimum_useful_z = 0.01f;
            std::vector<DepthVaryingInfo> varyingInfoTestArray{
                DepthVaryingInfo{0.0f, 1.0f, minimum_useful_z, 100.0f},
                DepthVaryingInfo{0.5f, 0.6f, minimum_useful_z, 100.0f},
                DepthVaryingInfo{0.0f, 1.0f, minimum_useful_z, std::numeric_limits<float>::infinity()},
                DepthVaryingInfo{0.0f, 1.0f, 100.0f, minimum_useful_z},
                DepthVaryingInfo{0.0f, 1.0f, std::numeric_limits<float>::infinity(), minimum_useful_z},
                DepthVaryingInfo{0.0f, 1.0f, std::numeric_limits<float>::max(), minimum_useful_z},
                DepthVaryingInfo{0.0f, 1.0f, std::numeric_limits<float>::max(), minimum_useful_z}};

            for (const DepthVaryingInfo& varyingInfo : varyingInfoTestArray) {
                runResult = frameIterator.PrepareSubmitFrame();
                REQUIRE(runResult == FrameIterator::RunResult::Success);

                {
                    XrResult result = CycleToNextSwapchainImage(depthSwapchains.data(), 2, 3_xrSeconds);
                    REQUIRE_RESULT_SUCCEEDED(result);
                }

                // Set up our XrCompositionLayerDepthInfoKHR
                XrCompositionLayerDepthInfoKHR depthInfoLayer{XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR};

                depthInfoLayer.subImage.imageArrayIndex = 0;
                depthInfoLayer.subImage.imageRect = {
                    {0, 0},
                    {(int32_t)session.swapchainExtent.width, (int32_t)session.swapchainExtent.height},
                };
                depthInfoLayer.minDepth = varyingInfo.minDepth;
                depthInfoLayer.maxDepth = varyingInfo.maxDepth;
                depthInfoLayer.nearZ = varyingInfo.nearZ;
                depthInfoLayer.farZ = varyingInfo.farZ;

                std::vector<XrCompositionLayerDepthInfoKHR> depthInfos(viewCount, depthInfoLayer);
                for (size_t i = 0; i < viewCount; ++i) {
                    depthInfos[i].subImage.swapchain = depthSwapchains[i];
                    frameIterator.projectionViewVector[i].next = &depthInfos[i];
                }

                const XrCompositionLayerBaseHeader* headerPtrArray[1] = {
                    reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection)};
                frameIterator.frameEndInfo.layerCount = 1;
                frameIterator.frameEndInfo.layers = headerPtrArray;

                // xrEndFrame requires the XR_KHR_composition_layer_depth extension to be
                // enabled or else it must return XR_ERROR_LAYER_INVALID.
                XrResult result = xrEndFrame(session.GetSession(), &frameIterator.frameEndInfo);
                CHECK(result == XR_SUCCESS);
            }
        }

        // Remove pointers to the now deleted depthInfos
        for (size_t i = 0; i < viewCount; ++i) {
            frameIterator.projectionViewVector[i].next = nullptr;
        }

        // Leave
        {
            XrResult result = xrRequestExitSession(session.GetSession());
            CHECK(result == XR_SUCCESS);
        }

        runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_STOPPING, timeout);
        CHECK(runResult == FrameIterator::RunResult::Success);

        for (const XrSwapchain& swapchain : depthSwapchains) {
            XrResult result = xrDestroySwapchain(swapchain);
            CHECK(result == XR_SUCCESS);
        }
    }
}  // namespace Conformance
