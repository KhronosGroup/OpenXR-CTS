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

#if defined(XR_USE_PLATFORM_ANDROID)

#include "common/xr_linear.h"
#include "common/xr_dependencies.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_options.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"
#include "utilities/xr_math_operators.h"
#include "utilities/xrduration_literals.h"

#include <catch2/catch_test_macros.hpp>
#include <jni.h>
#include "jnipp/jnipp.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <sstream>

using namespace Conformance;

namespace Conformance
{
    using namespace openxr::math_operators;

    // Purpose: Verify behavior of Android surface swapchains when presented on a composition layer.
    TEST_CASE("XR_KHR_android_surface_swapchain-interactive", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Test run not using graphics plugin");
        }

        if (!globalData.IsInstanceExtensionSupported(XR_KHR_ANDROID_SURFACE_SWAPCHAIN_EXTENSION_NAME)) {
            SKIP(XR_KHR_ANDROID_SURFACE_SWAPCHAIN_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper("Android Surface Swapchain", {"XR_KHR_android_surface_swapchain"});
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "android_surface_swapchain.png",
            "This test draws the \"TEXT FROM ANDROID\" at the top of the Android surface. "
            // This extension is under-specified and some OpenXR runtimes will show content vertically mirrored,
            // so we have to allow that behavior here.
            "This may appear vertically mirrored.");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        XrSwapchain surfaceSwapchain;
        jobject surface;

        XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        createInfo.usageFlags =
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
        createInfo.width = 1024;
        createInfo.height = 1024;

        auto xrCreateSwapchainAndroidSurfaceKHR =
            GetInstanceExtensionFunction<PFN_xrCreateSwapchainAndroidSurfaceKHR>(instance, "xrCreateSwapchainAndroidSurfaceKHR");

        XRC_CHECK_THROW_XRCMD(xrCreateSwapchainAndroidSurfaceKHR(session, &createInfo, &surfaceSwapchain, &surface));

        {
            jni::init((JavaVM*)Conformance_Android_Get_Application_VM());
            jni::Object surfaceWrapper(surface);

            jni::Class paintClass("android/graphics/Paint");
            jni::Class surfaceClass("android/view/Surface");

            jni::Object paint = paintClass.newInstance();
            paint.call<void>("setColor", (int)0xff000000);
            paint.call<void>("setTextSize", 64.0f);

            jni::method_t lockCanvasMethod = surfaceClass.getMethod("lockCanvas", "(Landroid/graphics/Rect;)Landroid/graphics/Canvas;");
            jni::Object canvas = surfaceWrapper.call<jni::Object>(lockCanvasMethod, nullptr);
            canvas.call<void>("drawColor", (int)0xffffffff);
            canvas.call<void>("drawText", "TEXT FROM ANDROID", 10.0f, 64.0f, paint);

            jni::method_t unlockCanvasAndPostMethod = surfaceClass.getMethod("unlockCanvasAndPost", "(Landroid/graphics/Canvas;)V");
            surfaceWrapper.call<void>(unlockCanvasAndPostMethod, canvas);
        }

        const XrSpace space = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

        XrCompositionLayerQuad layer{XR_TYPE_COMPOSITION_LAYER_QUAD};
        layer.space = space;
        layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        layer.subImage.swapchain = surfaceSwapchain;
        layer.subImage.imageRect.extent = {1024, 1024};
        layer.pose = {Quat::Identity, {0.0f, 0.0f, -2.0f}};
        layer.size = {1.0f, 1.0f};

        interactiveLayerManager.AddLayer(&layer);

        RenderLoop(session, [&](const XrFrameState& frameState) {
            if (!interactiveLayerManager.EndFrame(frameState)) {
                SUCCEED("User has marked this test as passed");
                return false;
            }
            return true;
        }).Loop();

        REQUIRE(XR_SUCCESS == xrDestroySwapchain(surfaceSwapchain));
    }

}  // namespace Conformance

#endif  // defined(XR_USE_PLATFORM_ANDROID)
