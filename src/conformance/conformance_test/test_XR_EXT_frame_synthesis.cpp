// Copyright (c) 2019-2026 The Khronos Group Inc.
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
#include "utilities/xrduration_literals.h"
#include "utilities/xr_math_operators.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <openxr/openxr.h>

#include <cstring>
#include <limits>
#include <vector>

namespace Conformance
{
    using namespace openxr::math_operators;

    TEST_CASE("XR_EXT_frame_synthesis", "[XR_EXT_frame_synthesis]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME)) {
            SKIP(XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME " not supported");
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Test run not using graphics plugin");
        }

        auto graphicsPlugin = globalData.GetGraphicsPlugin();

        AutoBasicInstance instance({XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME});
        AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                     AutoBasicSession::createSpaces,
                                 instance);
        XrSystemId systemId = session.GetSystemId();

        FrameIterator frameIterator(&session);
        frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

        // At this point we have a session ready for us to generate custom frames for.
        // The current XrSessionState is XR_SESSION_STATE_FOCUSED.
        const uint32_t viewCount = frameIterator.compositionLayerProjection.viewCount;

        // Call xrEnumerateViewConfigurationViews to get the recommended resolution for the swapchains.
        std::vector<XrViewConfigurationView> viewConfigView(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        std::vector<XrFrameSynthesisConfigViewEXT> frameSynthesisConfigView(viewCount, {XR_TYPE_FRAME_SYNTHESIS_CONFIG_VIEW_EXT});
        for (uint32_t i = 0; i < viewCount; i++) {
            viewConfigView[i].next = &frameSynthesisConfigView[i];
        }
        uint32_t viewCountOut = 0;
        REQUIRE(XR_SUCCESS == xrEnumerateViewConfigurationViews(instance, systemId, session.viewConfigurationType, viewCount, &viewCountOut,
                                                                viewConfigView.data()));

        XrExtent2Di mvSwapchainExtent{(int32_t)frameSynthesisConfigView[0].recommendedMotionVectorImageRectWidth,
                                      (int32_t)frameSynthesisConfigView[0].recommendedMotionVectorImageRectHeight};

        // Create motion vector and depth buffer swapchains.
        std::vector<XrSwapchain> motionVectorSwapchains(viewCount);
        for (XrSwapchain& motionVectorSwapchain : motionVectorSwapchains) {
            REQUIRE(CreateMotionVectorSwapchain(session, graphicsPlugin.get(), &motionVectorSwapchain, &mvSwapchainExtent) == XR_SUCCESS);
        }

        std::vector<XrSwapchain> depthSwapchains(viewCount);
        for (XrSwapchain& depthSwapchain : depthSwapchains) {
            REQUIRE(CreateDepthSwapchain(session, graphicsPlugin.get(), &depthSwapchain, &mvSwapchainExtent) == XR_SUCCESS);
        }

        constexpr float minimum_useful_z = 0.01f;

        REQUIRE(frameIterator.PrepareSubmitFrame() == FrameIterator::RunResult::Success);
        {
            REQUIRE(CycleToNextSwapchainImage(motionVectorSwapchains.data(), 2, 3_xrSeconds) == XR_SUCCESS);
            REQUIRE(CycleToNextSwapchainImage(depthSwapchains.data(), 2, 3_xrSeconds) == XR_SUCCESS);
        }

        // Set up our XrProjectionViewFrameSynthesisInfoEXT - default values
        XrFrameSynthesisInfoEXT frameSynthesisInfo{XR_TYPE_FRAME_SYNTHESIS_INFO_EXT};
        frameSynthesisInfo.motionVectorSubImage.imageArrayIndex = 0;
        frameSynthesisInfo.motionVectorSubImage.imageRect = {
            {0, 0},
            {(int32_t)mvSwapchainExtent.width, (int32_t)mvSwapchainExtent.height},
        };
        frameSynthesisInfo.depthSubImage.imageArrayIndex = 0;
        frameSynthesisInfo.depthSubImage.imageRect = {
            {0, 0},
            {(int32_t)mvSwapchainExtent.width, (int32_t)mvSwapchainExtent.height},
        };

        // minDepth and maxDepth are the range of depth values the depthSwapchain could have,
        //   in the range of [0.0,1.0]. This is akin to min and max values of OpenGL's glDepthRange,
        //   but with the requirement here that maxDepth >= minDepth.
        // nearZ is the positive distance in meters of the minDepth value in the depth swapchain.
        //   Apps may use a nearZ that is greater than farZ to indicate depth values
        //   are reversed. nearZ can be infinite.
        // farZ is the positive distance in meters of the maxDepth value in the depth swapchain.
        //   farZ can be infinite. Apps must not use the same value as nearZ.

        frameSynthesisInfo.minDepth = 0.01f;
        frameSynthesisInfo.maxDepth = 1.0f;
        frameSynthesisInfo.nearZ = minimum_useful_z;
        frameSynthesisInfo.farZ = 100.0f;
        frameSynthesisInfo.appSpaceDeltaPose = Pose::Identity;

        frameSynthesisInfo.motionVectorScale = {1.0f, 1.0f, 1.0f, 1.0f};
        frameSynthesisInfo.motionVectorOffset = {0.0f, 0.0f, 0.0f, 0.0f};

        const XrCompositionLayerBaseHeader* headerPtrArray[1] = {
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection)};
        std::vector<XrFrameSynthesisInfoEXT> frameSynthesisInfos(viewCount, frameSynthesisInfo);

        auto&& synthesisFlagsGenerator = bitmaskGeneratorIncluding0(
            {XR_FRAME_SYNTHESIS_INFO_USE_2D_MOTION_VECTOR_BIT_EXT, XR_FRAME_SYNTHESIS_INFO_REQUEST_RELAXED_FRAME_INTERVAL_BIT_EXT});
        while (synthesisFlagsGenerator.next()) {
            DYNAMIC_SECTION("Flags " << XrFrameSynthesisInfoFlagsEXTCPP(synthesisFlagsGenerator.get()).ToString())
            {
                CAPTURE(XrFrameSynthesisInfoFlagsEXTRefCPP(frameSynthesisInfo.layerFlags) = synthesisFlagsGenerator.get());

                auto setUpFrame = [&] {
                    frameSynthesisInfos.assign(viewCount, frameSynthesisInfo);
                    for (size_t i = 0; i < viewCount; ++i) {
                        frameSynthesisInfos[i].motionVectorSubImage.swapchain = motionVectorSwapchains[i];
                        frameSynthesisInfos[i].depthSubImage.swapchain = depthSwapchains[i];
                        frameIterator.projectionViewVector[i].next = &frameSynthesisInfos[i];
                    }

                    headerPtrArray[0] = {reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection)};
                    frameIterator.frameEndInfo.layerCount = 1;
                    frameIterator.frameEndInfo.layers = headerPtrArray;
                };

                SECTION("Normal usage")
                {
                    setUpFrame();
                    CHECK(XR_SUCCESS == xrEndFrame(session, &frameIterator.frameEndInfo));
                }

                SECTION("Narrow depth range")
                {
                    CAPTURE(frameSynthesisInfo.minDepth = 0.5f);
                    CAPTURE(frameSynthesisInfo.maxDepth = 0.6f);
                    setUpFrame();
                    CHECK(XR_SUCCESS == xrEndFrame(session, &frameIterator.frameEndInfo));
                }

                SECTION("Infinity far Z")
                {
                    CAPTURE(frameSynthesisInfo.farZ = std::numeric_limits<float>::infinity());
                    setUpFrame();
                    CHECK(XR_SUCCESS == xrEndFrame(session, &frameIterator.frameEndInfo));
                }

                SECTION("Inverted Z")
                {
                    CAPTURE(frameSynthesisInfo.farZ = minimum_useful_z);
                    SECTION("Near at 100")
                    {
                        CAPTURE(frameSynthesisInfo.nearZ = 100.0f);

                        setUpFrame();
                        CHECK(XR_SUCCESS == xrEndFrame(session, &frameIterator.frameEndInfo));
                    }

                    SECTION("Near at infinity")
                    {
                        CAPTURE(frameSynthesisInfo.nearZ = std::numeric_limits<float>::infinity());

                        setUpFrame();
                        CHECK(XR_SUCCESS == xrEndFrame(session, &frameIterator.frameEndInfo));
                    }

                    SECTION("Near at float max")
                    {
                        CAPTURE(frameSynthesisInfo.nearZ = std::numeric_limits<float>::max());

                        setUpFrame();
                        CHECK(XR_SUCCESS == xrEndFrame(session, &frameIterator.frameEndInfo));
                    }
                }
                SECTION("Negative tests")
                {
                    SECTION("near-far Z both minimum")
                    {
                        frameSynthesisInfo.nearZ = frameSynthesisInfo.farZ = minimum_useful_z;
                        setUpFrame();
                        CHECK(XR_ERROR_VALIDATION_FAILURE == xrEndFrame(session, &frameIterator.frameEndInfo));
                    }

                    SECTION("near-far Z both 100")
                    {
                        frameSynthesisInfo.nearZ = frameSynthesisInfo.farZ = 100.0f;
                        setUpFrame();
                        CHECK(XR_ERROR_VALIDATION_FAILURE == xrEndFrame(session, &frameIterator.frameEndInfo));
                    }

                    SECTION("near-far Z both infinity")
                    {
                        // TODO does this break because infinity is denormal and has weird equality behavior?
                        frameSynthesisInfo.nearZ = frameSynthesisInfo.farZ = std::numeric_limits<float>::infinity();
                        setUpFrame();
                        CHECK(XR_ERROR_VALIDATION_FAILURE == xrEndFrame(session, &frameIterator.frameEndInfo));
                    }
                }
            }
        }
    }
}  // namespace Conformance
