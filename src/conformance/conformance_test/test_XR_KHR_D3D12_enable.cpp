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

#ifdef XR_USE_GRAPHICS_API_D3D12
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "graphics_plugin.h"
#include "matchers.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <windows.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <string>
#include <cstring>

using namespace Microsoft::WRL;

namespace Conformance
{
    TEST_CASE("XR_KHR_D3D12_enable", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionEnabled(XR_KHR_D3D12_ENABLE_EXTENSION_NAME)) {
            SKIP(XR_KHR_D3D12_ENABLE_EXTENSION_NAME " not enabled");
        }

        AutoBasicInstance instance;

        XrSystemId systemId{XR_NULL_SYSTEM_ID};
        REQUIRE(XR_SUCCESS == FindBasicSystem(instance.GetInstance(), &systemId));

        // Create the graphics plugin we'll need to exercise session create functionality below.
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin;

        if (!globalData.options.graphicsPlugin.empty()) {
            REQUIRE_NOTHROW(graphicsPlugin = Conformance::CreateGraphicsPlugin(globalData.options.graphicsPlugin.c_str(),
                                                                               globalData.GetPlatformPlugin()));
            REQUIRE(graphicsPlugin->Initialize());
        }

        // We'll use this XrSession and XrSessionCreateInfo for testing below.
        XrSession session = XR_NULL_HANDLE_CPP;

        XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
        sessionCreateInfo.systemId = systemId;

        CleanupSessionOnScopeExit cleanup(session);

        SECTION("No graphics binding")
        {
            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            sessionCreateInfo.next = nullptr;
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

        SECTION("NULL D3D12 device")
        {
            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            XrGraphicsBindingD3D12KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(graphicsPlugin->GetGraphicsBinding());

            graphicsBinding.device = nullptr;
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);

            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

        SECTION("Valid session after bad session")
        {
            // Pass invalid binding the first time
            {
                REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
                XrGraphicsBindingD3D12KHR graphicsBinding =
                    *reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(graphicsPlugin->GetGraphicsBinding());
                graphicsBinding.device = nullptr;
                sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
                CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
                cleanup.Destroy();
                graphicsPlugin->ShutdownDevice();
            }

            // Using the same instance pass valid binding the second time
            {
                REQUIRE(XR_SUCCESS == FindBasicSystem(instance.GetInstance(), &systemId));
                sessionCreateInfo.systemId = systemId;

                REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
                XrGraphicsBindingD3D12KHR graphicsBinding =
                    *reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(graphicsPlugin->GetGraphicsBinding());
                sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
                CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);
                cleanup.Destroy();
                graphicsPlugin->ShutdownDevice();
            }
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

            auto xrGetD3D12GraphicsRequirementsKHR =
                GetInstanceExtensionFunction<PFN_xrGetD3D12GraphicsRequirementsKHR>(instance, "xrGetD3D12GraphicsRequirementsKHR");

            XrGraphicsRequirementsD3D12KHR referenceGraphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
            REQUIRE(xrGetD3D12GraphicsRequirementsKHR(instance, systemId, &referenceGraphicsRequirements) == XR_SUCCESS);

            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            XrGraphicsBindingD3D12KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(graphicsPlugin->GetGraphicsBinding());
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            for (int i = 0; i < 3; ++i) {
                REQUIRE(XR_SUCCESS == FindBasicSystem(instance.GetInstance(), &systemId));
                sessionCreateInfo.systemId = systemId;

                XrGraphicsRequirementsD3D12KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
                REQUIRE(xrGetD3D12GraphicsRequirementsKHR(instance, systemId, &graphicsRequirements) == XR_SUCCESS);

                // We expect that the graphics requirements don't change...
                REQUIRE(referenceGraphicsRequirements.adapterLuid.HighPart == graphicsRequirements.adapterLuid.HighPart);
                REQUIRE(referenceGraphicsRequirements.adapterLuid.LowPart == graphicsRequirements.adapterLuid.LowPart);
                REQUIRE(referenceGraphicsRequirements.minFeatureLevel == graphicsRequirements.minFeatureLevel);

                CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);
                createSwapchains(graphicsPlugin, session);
                CHECK(xrDestroySession(session) == XR_SUCCESS);
                session = XR_NULL_HANDLE;
            }
            graphicsPlugin->ShutdownDevice();
        }
    }
}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_D3D12
