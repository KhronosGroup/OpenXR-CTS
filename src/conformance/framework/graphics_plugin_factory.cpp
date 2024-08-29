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

#include "graphics_plugin.h"
#include "utilities/utils.h"
#include <map>

namespace Conformance
{

// Graphics API factories are forward declared here.
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGLES(std::shared_ptr<IPlatformPlugin> platformPlugin);
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGL(std::shared_ptr<IPlatformPlugin> platformPlugin);
#endif

#ifdef XR_USE_GRAPHICS_API_VULKAN
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Vulkan(std::shared_ptr<IPlatformPlugin> platformPlugin);
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Vulkan2(std::shared_ptr<IPlatformPlugin> platformPlugin);
#endif

#ifdef XR_USE_GRAPHICS_API_D3D11
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D11(std::shared_ptr<IPlatformPlugin> platformPlugin);
#endif

#ifdef XR_USE_GRAPHICS_API_D3D12
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D12(std::shared_ptr<IPlatformPlugin> platformPlugin);
#endif

#ifdef XR_USE_GRAPHICS_API_METAL
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Metal(std::shared_ptr<IPlatformPlugin> platformPlugin);
#endif

    using GraphicsPluginFactory = std::function<std::shared_ptr<IGraphicsPlugin>(std::shared_ptr<IPlatformPlugin> platformPlugin)>;

    const std::map<std::string, GraphicsPluginFactory, IgnoreCaseStringLess> graphicsPluginMap = {
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
        {"OpenGLES",
         [](std::shared_ptr<IPlatformPlugin> platformPlugin) { return CreateGraphicsPlugin_OpenGLES(std::move(platformPlugin)); }},
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
        {"OpenGL", [](std::shared_ptr<IPlatformPlugin> platformPlugin) { return CreateGraphicsPlugin_OpenGL(std::move(platformPlugin)); }},
#endif

#ifdef XR_USE_GRAPHICS_API_VULKAN
        {"Vulkan", [](std::shared_ptr<IPlatformPlugin> platformPlugin) { return CreateGraphicsPlugin_Vulkan(std::move(platformPlugin)); }},
        {"Vulkan2",
         [](std::shared_ptr<IPlatformPlugin> platformPlugin) { return CreateGraphicsPlugin_Vulkan2(std::move(platformPlugin)); }},
#endif

#ifdef XR_USE_GRAPHICS_API_D3D11
        {"D3D11", [](std::shared_ptr<IPlatformPlugin> platformPlugin) { return CreateGraphicsPlugin_D3D11(std::move(platformPlugin)); }},
#endif

#ifdef XR_USE_GRAPHICS_API_D3D12
        {"D3D12", [](std::shared_ptr<IPlatformPlugin> platformPlugin) { return CreateGraphicsPlugin_D3D12(std::move(platformPlugin)); }},
#endif

#ifdef XR_USE_GRAPHICS_API_METAL
        {"Metal", [](std::shared_ptr<IPlatformPlugin> platformPlugin) { return CreateGraphicsPlugin_Metal(std::move(platformPlugin)); }},
#endif
    };

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin(const char* graphicsAPI,
                                                          std::shared_ptr<IPlatformPlugin> platformPlugin) noexcept(false)
    {
        if ((graphicsAPI == nullptr) || (graphicsAPI[0] == 0)) {
            throw std::invalid_argument("CreateGraphicsPlugin: No graphics API specified");
        }

        auto apiIt = graphicsPluginMap.find(graphicsAPI);
        if (apiIt == graphicsPluginMap.end()) {
            throw std::invalid_argument("CreateGraphicsPlugin: Unsupported graphics API");
        }

        return apiIt->second(std::move(platformPlugin));
    }

}  // namespace Conformance
