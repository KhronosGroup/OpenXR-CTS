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
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <limits>
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    // This implements an automated programmatic test of the cubemap layer. However, a separate visual
    // test is required in order to validate that it looks correct.
    TEST_CASE("XR_KHR_composition_layer_cube", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported("XR_KHR_composition_layer_cube")) {
            return;
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            return;
        }

        auto graphicsPlugin = globalData.GetGraphicsPlugin();
        auto timeout = (GetGlobalData().options.debugMode ? 3600_sec : 10_sec);
        CAPTURE(timeout);

        AutoBasicInstance instance({"XR_KHR_composition_layer_cube"});
        AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                     AutoBasicSession::createSpaces,
                                 instance);

        FrameIterator frameIterator(&session);
        FrameIterator::RunResult runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED, timeout);
        REQUIRE(runResult == FrameIterator::RunResult::Success);

        // At this point we have a session ready for us to generate custom frames for.
        // The current XrSessionState is XR_SESSION_STATE_FOCUSED.

        // Create a stereo cubemap.
        XrSwapchain swapchainPair[2];
        XrExtent2Di extents{256, 256};

        XrResult result = CreateColorSwapchain(session.GetSession(), graphicsPlugin.get(), &swapchainPair[0], &extents, 1, true /* cube */);
        REQUIRE_RESULT_SUCCEEDED(result);
        SwapchainCHECK swapchainCHECK0(swapchainPair[0]);  // Auto-deletes the swapchain.

        result = CreateColorSwapchain(session.GetSession(), graphicsPlugin.get(), &swapchainPair[1], &extents, 1, true /* cube */);
        REQUIRE_RESULT_SUCCEEDED(result);
        SwapchainCHECK swapchainCHECK1(swapchainPair[1]);  // Auto-deletes the swapchain.

        result = CycleToNextSwapchainImage(swapchainPair, 2, 3_xrSeconds);
        REQUIRE_RESULT_SUCCEEDED(result);

        auto&& layerFlagsGenerator = bitmaskGeneratorIncluding0(
            {{"XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT", XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT},
             {"XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT", XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT},
             {"XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT", XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT}});
        std::array<XrEyeVisibility, 3> eyeVisibilityArray{XR_EYE_VISIBILITY_BOTH, XR_EYE_VISIBILITY_LEFT /* just these two */};

        while (layerFlagsGenerator.next()) {
            for (XrSpace space : session.spaceVector) {
                for (XrEyeVisibility eyeVisibility : eyeVisibilityArray) {
                    std::array<XrQuaternionf, 4> orientationTestArray{
                        XrQuaternionf{0, 0, 0, 1},                        // No rotation; looking down the +x axis
                        XrQuaternionf{0, 0.7071f, 0, 0.7071f},            // 90 degree rotation around y axis; looking down the -z axis.
                        XrQuaternionf{0, 0, 0.7071f, 0.7071f},            // 90 degree rotation around z axis; looking down the +y axis.
                        XrQuaternionf{-0.709f, 0.383f, -0.381f, -0.454f}  // Misc value.
                    };

                    // typedef struct XrCompositionLayerCubeKHR {
                    //     XrStructureType             type;
                    //     const void* XR_MAY_ALIAS    next;
                    //     XrCompositionLayerFlags     layerFlags;
                    //     XrSpace                     space;
                    //     XrEyeVisibility             eyeVisibility;
                    //     XrSwapchain                 swapchain;
                    //     uint32_t                    imageArrayIndex;
                    //     XrQuaternionf               orientation;
                    // } XrCompositionLayerCubeKHR;

                    for (const XrQuaternionf& orientation : orientationTestArray) {
                        runResult = frameIterator.PrepareSubmitFrame();
                        REQUIRE(runResult == FrameIterator::RunResult::Success);

                        // Set up our cubemap layer. We always make two, and some of the time we
                        // split them into left and right eye layers. If we have a left eye then
                        // there must be a following layer which is the right eye.
                        std::vector<XrCompositionLayerCubeKHR> cubeLayerArray(2, {XR_TYPE_COMPOSITION_LAYER_CUBE_KHR});

                        cubeLayerArray[0].layerFlags = layerFlagsGenerator.get().bitmask;
                        cubeLayerArray[1].layerFlags = layerFlagsGenerator.get().bitmask;

                        cubeLayerArray[0].space = space;
                        cubeLayerArray[1].space = space;

                        cubeLayerArray[0].eyeVisibility = eyeVisibility;
                        cubeLayerArray[1].eyeVisibility =
                            ((eyeVisibility == XR_EYE_VISIBILITY_LEFT) ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH);

                        cubeLayerArray[0].swapchain = swapchainPair[0];
                        cubeLayerArray[1].swapchain = swapchainPair[1];

                        cubeLayerArray[0].imageArrayIndex = 0;  // We're not using index
                        cubeLayerArray[1].imageArrayIndex = 0;  // arrays here.

                        cubeLayerArray[0].orientation = orientation;
                        cubeLayerArray[1].orientation = orientation;

                        const XrCompositionLayerBaseHeader* headerPtrArray[3] = {
                            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection),
                            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&cubeLayerArray[0]),
                            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&cubeLayerArray[1])};
                        frameIterator.frameEndInfo.layerCount = 3;
                        frameIterator.frameEndInfo.layers = headerPtrArray;

                        // xrEndFrame requires the XR_KHR_composition_layer_cube extension to be enabled or else
                        // it will return XR_ERROR_LAYER_INVALID.
                        result = xrEndFrame(session.GetSession(), &frameIterator.frameEndInfo);
                        CHECK(result == XR_SUCCESS);
                    }
                }
            }
        }

        result = xrRequestExitSession(session.GetSession());
        CHECK(result == XR_SUCCESS);

        runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_STOPPING, timeout);
        CHECK(runResult == FrameIterator::RunResult::Success);
    }
}  // namespace Conformance
