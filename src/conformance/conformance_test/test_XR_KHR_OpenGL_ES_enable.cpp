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
#include "matchers.h"
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES

#include "xr_dependencies.h"
#include <openxr/openxr_platform.h>

namespace Conformance
{

    TEST_CASE("XR_KHR_opengl_es_enable", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionEnabled(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME)) {
            SKIP(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME " not enabled");
        }

        AutoBasicInstance instance{AutoBasicInstance::createSystemId};
        XrSystemId systemId = instance.systemId;

        // Create the graphics plugin we'll need to exercise session create functionality below.
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin;

        if (!globalData.options.graphicsPlugin.empty()) {
            REQUIRE_NOTHROW(graphicsPlugin = Conformance::CreateGraphicsPlugin(globalData.options.graphicsPlugin.c_str(),
                                                                               globalData.GetPlatformPlugin()));
            REQUIRE(graphicsPlugin->Initialize());
        }

        // We'll use this XrSession and XrSessionCreateInfo for testing below.
        XrSession session = XR_NULL_HANDLE_CPP;
        XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO, nullptr, 0, systemId};
        CleanupSessionOnScopeExit cleanup(session);

        SECTION("No graphics binding")
        {
            graphicsPlugin->InitializeDevice(instance, systemId, true);
            sessionCreateInfo.next = nullptr;
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

#if defined(XR_USE_PLATFORM_ANDROID)
        // tests related to the graphics binding are OS specific
        SECTION("NULL context: context is NULL")
        {
            graphicsPlugin->InitializeDevice(instance, systemId, true);
            XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingOpenGLESAndroidKHR*>(graphicsPlugin->GetGraphicsBinding());
            graphicsBinding.context = EGL_NO_CONTEXT;
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }
        SECTION("Multiple session with same device")
        {
            auto createSwapchains = [](std::shared_ptr<IGraphicsPlugin> graphicsPlugin, XrSession session) {
                for (int i = 0; i < 3; ++i) {
                    XrSwapchain swapchain;
                    XrExtent2Di widthHeight{0, 0};  // 0,0 means Use defaults.
                    XrResult result = CreateColorSwapchain(session, graphicsPlugin.get(), &swapchain, &widthHeight);
                    CHECK_THAT(result, In<XrResult>({XR_SUCCESS, XR_ERROR_LIMIT_REACHED}));

                    if (XR_SUCCEEDED(result)) {
                        CHECK_RESULT_UNQUALIFIED_SUCCESS(xrDestroySwapchain(swapchain));
                    }
                }
            };

            graphicsPlugin->InitializeDevice(instance, systemId, true);
            XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingOpenGLESAndroidKHR*>(graphicsPlugin->GetGraphicsBinding());
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            for (int i = 0; i < 3; ++i) {
                CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);
                createSwapchains(graphicsPlugin, session);
                CHECK(xrDestroySession(session) == XR_SUCCESS);
                session = XR_NULL_HANDLE;
            }
            graphicsPlugin->ShutdownDevice();
        }
#endif  // defined(XR_USE_PLATFORM_ANDROID)
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_OPENGL_ES
