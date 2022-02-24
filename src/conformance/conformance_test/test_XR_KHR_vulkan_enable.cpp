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
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include "xr_dependencies.h"
#include <openxr/openxr_platform.h>

namespace Conformance
{
    TEST_CASE("XR_KHR_vulkan_enable", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionEnabled("XR_KHR_vulkan_enable")) {
            return;
        }

        AutoBasicInstance instance;

        XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO, nullptr, globalData.options.formFactorValue};
        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrGetSystem(instance, &systemGetInfo, &systemId));

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

        SECTION("Valid vulkan device")
        {
            graphicsPlugin->InitializeDevice(instance, systemId, true);
            XrGraphicsBindingVulkanKHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingVulkanKHR*>(graphicsPlugin->GetGraphicsBinding());
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

        SECTION("NULL vulkan device")
        {
            graphicsPlugin->InitializeDevice(instance, systemId, true);
            XrGraphicsBindingVulkanKHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingVulkanKHR*>(graphicsPlugin->GetGraphicsBinding());
            graphicsBinding.device = VK_NULL_HANDLE;
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
                    XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);

                    if (XR_SUCCEEDED(result)) {
                        XRC_CHECK_THROW_XRCMD(xrDestroySwapchain(swapchain));
                    }
                }
            };

            graphicsPlugin->InitializeDevice(instance, systemId, true);
            XrGraphicsBindingVulkanKHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingVulkanKHR*>(graphicsPlugin->GetGraphicsBinding());
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            for (int i = 0; i < 3; ++i) {
                CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);
                createSwapchains(graphicsPlugin, session);
                CHECK(xrDestroySession(session) == XR_SUCCESS);
                session = XR_NULL_HANDLE;
            }
            graphicsPlugin->ShutdownDevice();
        }
    }
}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_VULKAN
