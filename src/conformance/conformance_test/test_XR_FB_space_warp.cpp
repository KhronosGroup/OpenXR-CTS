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
#include "utilities/xrduration_literals.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <cstring>
#include <limits>
#include <vector>

namespace Conformance
{
    TEST_CASE("XR_FB_space_warp", "[XR_FB_space_warp]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_FB_SPACE_WARP_EXTENSION_NAME)) {
            SKIP(XR_FB_SPACE_WARP_EXTENSION_NAME " not supported");
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Test run not using graphics plugin");
        }

        auto graphicsPlugin = globalData.GetGraphicsPlugin();

        AutoBasicInstance instance({XR_FB_SPACE_WARP_EXTENSION_NAME});
        AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                     AutoBasicSession::createSpaces,
                                 instance);

        XrSystemSpaceWarpPropertiesFB spaceWarpProperties = {XR_TYPE_SYSTEM_SPACE_WARP_PROPERTIES_FB};

        XrSystemProperties systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};
        systemProperties.next = &spaceWarpProperties;
        REQUIRE(xrGetSystemProperties(instance, session.GetSystemId(), &systemProperties) == XR_SUCCESS);

        FrameIterator frameIterator(&session);
        frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

        // At this point we have a session ready for us to generate custom frames for.
        // The current XrSessionState is XR_SESSION_STATE_FOCUSED.

        XrExtent2Di mvSwapchainExtent{(int32_t)spaceWarpProperties.recommendedMotionVectorImageRectWidth,
                                      (int32_t)spaceWarpProperties.recommendedMotionVectorImageRectHeight};

        const uint32_t viewCount = frameIterator.compositionLayerProjection.viewCount;

        // Create motion vector and depth buffer swapchains.
        std::vector<XrSwapchain> motionVectorSwapchains(viewCount);
        for (XrSwapchain& motionVectorSwapchain : motionVectorSwapchains) {
            REQUIRE(CreateMotionVectorSwapchain(session.GetSession(), graphicsPlugin.get(), &motionVectorSwapchain, &mvSwapchainExtent) ==
                    XR_SUCCESS);
        }

        std::vector<XrSwapchain> depthSwapchains(viewCount);
        for (XrSwapchain& depthSwapchain : depthSwapchains) {
            REQUIRE(CreateDepthSwapchain(session.GetSession(), graphicsPlugin.get(), &depthSwapchain, &mvSwapchainExtent) == XR_SUCCESS);
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

            struct SpaceWarpVaryingInfo
            {  // This is a subset of XrCompositionLayerSpaceWarpInfoFB.
                uint64_t layerFlags;
                float minDepth;
                float maxDepth;
                float nearZ;
                float farZ;
            };

            constexpr float minimum_useful_z = 0.01f;
            std::vector<SpaceWarpVaryingInfo> varyingInfoTestArray{
                SpaceWarpVaryingInfo{0, 0.0f, 1.0f, minimum_useful_z, 100.0f},
                SpaceWarpVaryingInfo{0, 0.5f, 0.6f, minimum_useful_z, 100.0f},
                SpaceWarpVaryingInfo{0, 0.0f, 1.0f, minimum_useful_z, std::numeric_limits<float>::infinity()},
                SpaceWarpVaryingInfo{0, 0.0f, 1.0f, 100.0f, minimum_useful_z},
                SpaceWarpVaryingInfo{0, 0.0f, 1.0f, std::numeric_limits<float>::infinity(), minimum_useful_z},
                SpaceWarpVaryingInfo{0, 0.0f, 1.0f, std::numeric_limits<float>::max(), minimum_useful_z},
                SpaceWarpVaryingInfo{0, 0.0f, 1.0f, std::numeric_limits<float>::max(), minimum_useful_z},
                SpaceWarpVaryingInfo{XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB, 0.0f, 1.0f, minimum_useful_z, 100.0f},
                SpaceWarpVaryingInfo{XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB, 0.5f, 0.6f, minimum_useful_z, 100.0f},
                SpaceWarpVaryingInfo{XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB, 0.0f, 1.0f, minimum_useful_z,
                                     std::numeric_limits<float>::infinity()},
                SpaceWarpVaryingInfo{XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB, 0.0f, 1.0f, 100.0f, minimum_useful_z},
                SpaceWarpVaryingInfo{XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB, 0.0f, 1.0f,
                                     std::numeric_limits<float>::infinity(), minimum_useful_z},
                SpaceWarpVaryingInfo{XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB, 0.0f, 1.0f, std::numeric_limits<float>::max(),
                                     minimum_useful_z},
                SpaceWarpVaryingInfo{XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB, 0.0f, 1.0f, std::numeric_limits<float>::max(),
                                     minimum_useful_z}};

            for (const SpaceWarpVaryingInfo& varyingInfo : varyingInfoTestArray) {
                REQUIRE(frameIterator.PrepareSubmitFrame() == FrameIterator::RunResult::Success);

                {
                    REQUIRE(CycleToNextSwapchainImage(motionVectorSwapchains.data(), 2, 3_xrSeconds) == XR_SUCCESS);
                    REQUIRE(CycleToNextSwapchainImage(depthSwapchains.data(), 2, 3_xrSeconds) == XR_SUCCESS);
                }

                // Set up our XrCompositionLayerSpaceWarpInfoFB
                XrCompositionLayerSpaceWarpInfoFB spaceWarpInfo{XR_TYPE_COMPOSITION_LAYER_SPACE_WARP_INFO_FB};
                spaceWarpInfo.motionVectorSubImage.imageArrayIndex = 0;
                spaceWarpInfo.motionVectorSubImage.imageRect = {
                    {0, 0},
                    {(int32_t)mvSwapchainExtent.width, (int32_t)mvSwapchainExtent.height},
                };
                spaceWarpInfo.depthSubImage.imageArrayIndex = 0;
                spaceWarpInfo.depthSubImage.imageRect = {
                    {0, 0},
                    {(int32_t)mvSwapchainExtent.width, (int32_t)mvSwapchainExtent.height},
                };
                spaceWarpInfo.layerFlags = varyingInfo.layerFlags;
                spaceWarpInfo.minDepth = varyingInfo.minDepth;
                spaceWarpInfo.maxDepth = varyingInfo.maxDepth;
                spaceWarpInfo.nearZ = varyingInfo.nearZ;
                spaceWarpInfo.farZ = varyingInfo.farZ;
                spaceWarpInfo.appSpaceDeltaPose = {};

                std::vector<XrCompositionLayerSpaceWarpInfoFB> spaceWarpInfos(viewCount, spaceWarpInfo);
                for (size_t i = 0; i < viewCount; ++i) {
                    spaceWarpInfos[i].motionVectorSubImage.swapchain = motionVectorSwapchains[i];
                    spaceWarpInfos[i].depthSubImage.swapchain = depthSwapchains[i];
                    frameIterator.projectionViewVector[i].next = &spaceWarpInfos[i];
                }

                const XrCompositionLayerBaseHeader* headerPtrArray[1] = {
                    reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection)};
                frameIterator.frameEndInfo.layerCount = 1;
                frameIterator.frameEndInfo.layers = headerPtrArray;

                // xrEndFrame requires the XR_FB_space_warp extension to be
                // enabled or else it must return XR_ERROR_LAYER_INVALID.
                XrResult result = xrEndFrame(session.GetSession(), &frameIterator.frameEndInfo);
                CHECK(result == XR_SUCCESS);
            }
        }

        // Remove pointers to the now deleted spaceWarpInfos
        for (size_t i = 0; i < viewCount; ++i) {
            frameIterator.projectionViewVector[i].next = nullptr;
        }
    }
}  // namespace Conformance
