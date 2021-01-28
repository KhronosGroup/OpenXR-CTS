// Copyright (c) 2019-2021, The Khronos Group Inc.
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

#ifdef XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

#include <windows.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

using namespace Microsoft::WRL;

namespace Conformance
{
    TEST_CASE("XR_KHR_D3D11_enable", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionEnabled("XR_KHR_D3D11_enable")) {
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

        SECTION("NULL D3D11 device")
        {
            graphicsPlugin->InitializeDevice(instance, systemId, true);
            XrGraphicsBindingD3D11KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(graphicsPlugin->GetGraphicsBinding());
            graphicsBinding.device = nullptr;
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

        SECTION("Singlethreaded D3D11 device")
        {
            // Verify that the runtime supports D3D11_CREATE_DEVICE_SINGLETHREADED.
            graphicsPlugin->InitializeDevice(instance, systemId, true, D3D11_CREATE_DEVICE_SINGLETHREADED);
            XrGraphicsBindingD3D11KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(graphicsPlugin->GetGraphicsBinding());
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }
    }
}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_D3D11
