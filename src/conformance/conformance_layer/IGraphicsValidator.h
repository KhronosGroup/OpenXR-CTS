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

#pragma once

#include <memory>

#include "Common.h"

namespace Conformance
{
    struct IGraphicsValidator
    {
        virtual ~IGraphicsValidator() = default;

        virtual void ValidateSwapchainFormats(ConformanceHooksBase* conformanceHooks, uint32_t count, uint64_t* formats) const = 0;
        virtual void ValidateSwapchainImageStructs(ConformanceHooksBase* conformanceHooks, uint64_t swapchainFormat, uint32_t count,
                                                   XrSwapchainImageBaseHeader* images) const = 0;
        virtual void ValidateUsageFlags(ConformanceHooksBase* conformanceHooks, uint64_t usageFlags, uint32_t count,
                                        XrSwapchainImageBaseHeader* images) const = 0;

        // Vulkan specific function, allows or disallows the access to the VkQueue used in the graphics binding
        virtual void AllowVkQueueAccess(bool /*allowed*/)
        {
        }

        // Checks if the internal state of the validator respects what is expected
        virtual bool CheckState() const = 0;
    };

// Graphics Validator factories are forward declared here.
#ifdef XR_USE_GRAPHICS_API_D3D11
    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator_D3D11();
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator_Vulkan(const XrGraphicsBindingVulkanKHR* graphicsBinding);
#endif

    // Create a graphics plugin for the graphics API specified in the options.
    // Throws std::invalid_argument if the graphics API is empty, unknown, or unsupported.
    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator(const XrBaseInStructure* graphicsBinding);

}  // namespace Conformance
