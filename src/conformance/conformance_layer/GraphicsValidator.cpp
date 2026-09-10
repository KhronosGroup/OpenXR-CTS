// Copyright (c) 2019-2026 The Khronos Group Inc.
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

#include <map>
#include <memory>

#include "RuntimeFailure.h"
#include "IGraphicsValidator.h"

namespace Conformance
{
    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator(const XrBaseInStructure *graphicsBinding)
    {
        switch (graphicsBinding->type) {
#ifdef XR_USE_GRAPHICS_API_D3D11
        case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR:
            return CreateGraphicsValidator_D3D11();
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
        case XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR:
            return CreateGraphicsValidator_Vulkan(reinterpret_cast<const XrGraphicsBindingVulkanKHR *>(graphicsBinding));
#endif
        default:
            break;
        }
        return nullptr;
    }

}  // namespace Conformance
