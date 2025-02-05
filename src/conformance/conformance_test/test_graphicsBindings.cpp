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

#include "conformance_framework.h"
#include "conformance_utils.h"
#include "graphics_plugin.h"
#include "matchers.h"
#include "utilities/utils.h"
#include "xr_dependencies.h"

#include <catch2/catch_test_macros.hpp>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <string>
#include <cstring>

namespace Conformance
{
    TEST_CASE("graphicsBindings", "")
    {
        // Enable all the graphics bindings the runtime supports at once
        // even though we will only use one of the bindings.

        GlobalData& globalData = GetGlobalData();

        std::vector<const char*> extraGraphicsBindings;

        bool foundEnabledGraphicsExtension = false;

#if defined(XR_USE_GRAPHICS_API_D3D11)
        if (globalData.IsInstanceExtensionSupported(XR_KHR_D3D11_ENABLE_EXTENSION_NAME)) {
            if (!globalData.IsInstanceExtensionEnabled(XR_KHR_D3D11_ENABLE_EXTENSION_NAME)) {
                extraGraphicsBindings.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
            }
            else {
                foundEnabledGraphicsExtension = true;
            }
        }
#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
#if defined(XR_USE_GRAPHICS_API_D3D12)
        if (globalData.IsInstanceExtensionSupported(XR_KHR_D3D12_ENABLE_EXTENSION_NAME)) {
            if (!globalData.IsInstanceExtensionEnabled(XR_KHR_D3D12_ENABLE_EXTENSION_NAME)) {
                extraGraphicsBindings.push_back(XR_KHR_D3D12_ENABLE_EXTENSION_NAME);
            }
            else {
                foundEnabledGraphicsExtension = true;
            }
        }
#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
        if (globalData.IsInstanceExtensionSupported(XR_MND_HEADLESS_EXTENSION_NAME)) {
            if (!globalData.IsInstanceExtensionEnabled(XR_MND_HEADLESS_EXTENSION_NAME)) {
                extraGraphicsBindings.push_back(XR_MND_HEADLESS_EXTENSION_NAME);
            }
            else {
                foundEnabledGraphicsExtension = true;
            }
        }
#if defined(XR_USE_GRAPHICS_API_OPENGL)
        if (globalData.IsInstanceExtensionSupported(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME)) {
            if (!globalData.IsInstanceExtensionEnabled(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME)) {
                extraGraphicsBindings.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
            }
            else {
                foundEnabledGraphicsExtension = true;
            }
        }
#endif  // defined(XR_USE_GRAPHICS_API_OPENGL)
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
        if (globalData.IsInstanceExtensionSupported(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME)) {
            if (!globalData.IsInstanceExtensionEnabled(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME)) {
                extraGraphicsBindings.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
            }
            else {
                foundEnabledGraphicsExtension = true;
            }
        }
#endif  // defined(XR_USE_GRAPHICS_API_OPENGL_ES)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
        if (globalData.IsInstanceExtensionSupported(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME)) {
            if (!globalData.IsInstanceExtensionEnabled(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME)) {
                extraGraphicsBindings.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
            }
            else {
                foundEnabledGraphicsExtension = true;
            }
        }
        if (globalData.IsInstanceExtensionSupported(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME)) {
            if (!globalData.IsInstanceExtensionEnabled(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME)) {
                extraGraphicsBindings.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
            }
            else {
                foundEnabledGraphicsExtension = true;
            }
        }
#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_METAL)
        if (globalData.IsInstanceExtensionSupported(XR_KHR_METAL_ENABLE_EXTENSION_NAME)) {
            if (!globalData.IsInstanceExtensionEnabled(XR_KHR_METAL_ENABLE_EXTENSION_NAME)) {
                extraGraphicsBindings.push_back(XR_KHR_METAL_ENABLE_EXTENSION_NAME);
            }
            else {
                foundEnabledGraphicsExtension = true;
            }
        }
#endif  // defined(XR_USE_GRAPHICS_API_METAL)

        // One graphics extension will be enabled by the CTS runner itself
        REQUIRE(foundEnabledGraphicsExtension);

        if (extraGraphicsBindings.empty()) {
            SKIP("Runtime only supports one graphics binding, nothing to test");
        }

        AutoBasicInstance instance(extraGraphicsBindings, AutoBasicInstance::createSystemId);

        AutoBasicSession session(AutoBasicSession::createInstance | AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                     AutoBasicSession::createSwapchains | AutoBasicSession::createSpaces,
                                 instance);
    }
}  // namespace Conformance
