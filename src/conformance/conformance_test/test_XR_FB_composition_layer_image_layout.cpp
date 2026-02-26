// Copyright (c) 2025-2026 The Khronos Group Inc.
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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

#include "common/xr_dependencies.h"
#include "composition_utils.h"
#include "conformance_framework.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <catch2/catch_test_macros.hpp>

namespace Conformance
{
    static void TestImageLayout(CompositionHelper& compositionHelper, InteractiveLayerManager& interactiveLayerManager,
                                XrCompositionLayerImageLayoutFlagsFB flags)
    {
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        const XrSwapchain swapchain = compositionHelper.CreateStaticSwapchainImage(CreateTextImage(128, 128, "Test", 48));

        XrCompositionLayerImageLayoutFB imageLayout{XR_TYPE_COMPOSITION_LAYER_IMAGE_LAYOUT_FB};
        imageLayout.flags = flags;

        XrCompositionLayerQuad* const quadLayer =
            compositionHelper.CreateQuadLayer(swapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {0, 0, -3.0f}});
        quadLayer->next = &imageLayout;

        interactiveLayerManager.AddLayer(quadLayer);

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    TEST_CASE("XR_FB_composition_layer_image_layout", "[XR_FB_composition_layer_image_layout][composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_FB_COMPOSITION_LAYER_IMAGE_LAYOUT_EXTENSION_NAME)) {
            SKIP(XR_FB_COMPOSITION_LAYER_IMAGE_LAYOUT_EXTENSION_NAME " not supported");
        }

        SECTION("Extension not enabled")
        {
            if (!globalData.IsInstanceExtensionEnabled(XR_FB_COMPOSITION_LAYER_IMAGE_LAYOUT_EXTENSION_NAME)) {
                CompositionHelper compositionHelper("Image layout extension disabled");
                InteractiveLayerManager interactiveLayerManager(compositionHelper, "image_layout_without_extension.jpg",
                                                                "Change the image layout when the extension is not enabled. "
                                                                "The quad layers should show a text \"Test\".");
                TestImageLayout(compositionHelper, interactiveLayerManager, XR_COMPOSITION_LAYER_IMAGE_LAYOUT_VERTICAL_FLIP_BIT_FB);
            }
            else {
                WARN(XR_FB_COMPOSITION_LAYER_IMAGE_LAYOUT_EXTENSION_NAME
                     " force-enabled, cannot test behavior when extension is disabled.");
            }
        }

        SECTION("ImageLayoutFlagsFB = 1")
        {
            CompositionHelper compositionHelper("ImageLayoutFlagsFB = 1", {"XR_FB_composition_layer_image_layout"});
            InteractiveLayerManager interactiveLayerManager(compositionHelper, "image_layout_flags_1.jpg",
                                                            "Change the image layout with XrCompositionLayerImageLayoutFlagsFB = 1. "
                                                            "The quad layer should show a vertically flipped text \"Test\".");
            TestImageLayout(compositionHelper, interactiveLayerManager, XR_COMPOSITION_LAYER_IMAGE_LAYOUT_VERTICAL_FLIP_BIT_FB);
        }

        SECTION("ImageLayoutFlagsFB = 0")
        {
            CompositionHelper compositionHelper("ImageLayoutFlagsFB = 0", {"XR_FB_composition_layer_image_layout"});
            InteractiveLayerManager interactiveLayerManager(compositionHelper, "image_layout_flags_0.jpg",
                                                            "Change the image layout with XrCompositionLayerImageLayoutFlagsFB = 0. "
                                                            "The quad layer should show a text \"Test\".");
            TestImageLayout(compositionHelper, interactiveLayerManager, 0);
        }
    }
}  // namespace Conformance
