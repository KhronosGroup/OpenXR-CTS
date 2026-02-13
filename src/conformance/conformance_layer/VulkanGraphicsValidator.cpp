// Copyright (c) 2025-2026 The Khronos Group Inc.
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

#include "ConformanceHooks.h"
#include "IGraphicsValidator.h"
#include "VulkanLayer.h"

#if defined(XR_USE_GRAPHICS_API_VULKAN)

namespace Conformance
{
    struct VulkanGraphicsValidator : IGraphicsValidator
    {
        VkInstance instance;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        uint32_t queueFamilyIndex;
        uint32_t queueIndex;
        bool allowAccessVkQueue;

        VulkanGraphicsValidator(const XrGraphicsBindingVulkanKHR* graphicsBinding)
            : instance(graphicsBinding->instance)
            , physicalDevice(graphicsBinding->physicalDevice)
            , device(graphicsBinding->device)
            , queueFamilyIndex(graphicsBinding->queueFamilyIndex)
            , queueIndex(graphicsBinding->queueIndex)
        {
        }

        void ValidateSwapchainFormats(ConformanceHooksBase* conformanceHooks, uint32_t count, uint64_t* formats) const override
        {
            // TODO
            (void)conformanceHooks;
            (void)count;
            (void)formats;
        }

        void ValidateSwapchainImageStructs(ConformanceHooksBase* conformanceHooks, uint64_t swapchainFormat, uint32_t count,
                                           XrSwapchainImageBaseHeader* images) const override
        {
            // TODO
            (void)conformanceHooks;
            (void)swapchainFormat;
            (void)count;
            (void)images;
        }
        void ValidateUsageFlags(ConformanceHooksBase* conformanceHooks, uint64_t usageFlags, uint32_t count,
                                XrSwapchainImageBaseHeader* images) const override
        {
            // TODO
            (void)conformanceHooks;
            (void)usageFlags;
            (void)count;
            (void)images;
        }

        void AllowVkQueueAccess(bool allowed) override
        {
            allowAccessVkQueue = allowed;
            ResetVkQueueAccess(device, queueFamilyIndex, queueIndex);
        }

        bool CheckState() const override
        {
            bool ret = true;
            if (allowAccessVkQueue) {
                // VkQueue access is allowed, no need to check further
                return true;
            }
            ret = !CheckVkQueueAccess(device, queueFamilyIndex, queueIndex);
            ResetVkQueueAccess(device, queueFamilyIndex, queueIndex);
            return ret;
        }
    };

    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator_Vulkan(const XrGraphicsBindingVulkanKHR* graphicsBinding)
    {
        return std::make_shared<VulkanGraphicsValidator>(graphicsBinding);
    }

}  // namespace Conformance

#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
