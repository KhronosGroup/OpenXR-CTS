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
    // This implements an automated programmatic test of the cylinder layer. However, a separate visual
    // test is required in order to validate that it looks correct.
    TEST_CASE("XR_KHR_composition_layer_cylinder", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported("XR_KHR_composition_layer_cylinder")) {
            return;
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            return;
        }

        auto timeout = (GetGlobalData().options.debugMode ? 3600s : 10s);
        CAPTURE(timeout);

        AutoBasicInstance instance({"XR_KHR_composition_layer_cylinder"});
        AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                     AutoBasicSession::createSpaces,
                                 instance);

        FrameIterator frameIterator(&session);
        FrameIterator::RunResult runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED, timeout);
        REQUIRE(runResult == FrameIterator::RunResult::Success);

        // At this point we have a session ready for us to generate custom frames for.
        // The current XrSessionState is XR_SESSION_STATE_FOCUSED.
        XrResult result;

        auto&& layerFlagsGenerator = bitmaskGeneratorIncluding0(
            {{"XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT", XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT},
             {"XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT", XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT},
             {"XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT", XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT}});
        std::array<XrEyeVisibility, 3> eyeVisibilityArray{XR_EYE_VISIBILITY_BOTH, XR_EYE_VISIBILITY_LEFT /* just these two */};

        while (layerFlagsGenerator.next()) {
            for (XrSpace space : session.spaceVector) {
                for (XrEyeVisibility eyeVisibility : eyeVisibilityArray) {
                    std::array<float, 3> radiusTestArray{0, 1.f, INFINITY};  // Spec explicitly supports radius 0 and +infinity

                    for (float radius : radiusTestArray) {
                        runResult = frameIterator.PrepareSubmitFrame();
                        REQUIRE(runResult == FrameIterator::RunResult::Success);

                        // Set up our cylinder layer. We always make two, and some of the time we
                        // split them into left and right eye layers. If we have a left eye then
                        // there must be a following layer which is the right eye.
                        std::vector<XrCompositionLayerCylinderKHR> cylinderLayerArray(2, {XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR});

                        cylinderLayerArray[0].layerFlags = layerFlagsGenerator.get().bitmask;
                        cylinderLayerArray[0].space = space;
                        cylinderLayerArray[0].eyeVisibility = eyeVisibility;

                        cylinderLayerArray[0].subImage.swapchain = session.swapchainVector[0];
                        cylinderLayerArray[0].subImage.imageRect = {
                            {0, 0}, {(int32_t)session.swapchainExtent.width, (int32_t)session.swapchainExtent.height}};
                        cylinderLayerArray[0].subImage.imageArrayIndex = 0;

                        cylinderLayerArray[0].pose = XrPosef{{0, 0, 0, 1}, {1, 1, 1}};
                        cylinderLayerArray[0].centralAngle = 3.14f / 3;
                        cylinderLayerArray[0].aspectRatio = 1.f;
                        cylinderLayerArray[0].radius = radius;

                        // Copy the first cylinder to the second and possibly change the eye
                        // visibility in order to exercise the left/right support.
                        cylinderLayerArray[1] = cylinderLayerArray[0];
                        if (cylinderLayerArray[0].eyeVisibility == XR_EYE_VISIBILITY_LEFT)
                            cylinderLayerArray[1].eyeVisibility = XR_EYE_VISIBILITY_RIGHT;

                        const XrCompositionLayerBaseHeader* headerPtrArray[3] = {
                            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection),
                            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&cylinderLayerArray[0]),
                            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&cylinderLayerArray[1])};
                        frameIterator.frameEndInfo.layerCount = 3;
                        frameIterator.frameEndInfo.layers = headerPtrArray;

                        // xrEndFrame requires the XR_KHR_composition_layer_cylinder extension to be enabled or else
                        // it will return XR_ERROR_LAYER_INVALID.
                        result = xrEndFrame(session.GetSession(), &frameIterator.frameEndInfo);
                        CHECK(result == XR_SUCCESS);
                    }
                }
            }
        }

        // Leave
        result = xrRequestExitSession(session.GetSession());
        CHECK(result == XR_SUCCESS);

        runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_STOPPING, timeout);
        CHECK(runResult == FrameIterator::RunResult::Success);
    }
}  // namespace Conformance
