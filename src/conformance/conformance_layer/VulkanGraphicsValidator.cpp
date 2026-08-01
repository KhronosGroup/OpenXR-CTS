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

#include <map>
#include <thread>

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
        std::map<std::thread::id, bool> allowAccessVkQueue;

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
            const VkFormat expectedFormat = static_cast<VkFormat>(swapchainFormat);
            const XrSwapchainImageVulkanKHR* const vkImages = reinterpret_cast<const XrSwapchainImageVulkanKHR*>(images);
            for (uint32_t i = 0; i < count; ++i) {
                if (vkImages[i].type != XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR) {
                    conformanceHooks->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
                                                         "xrEnumerateSwapchainImages failed due to image header structure not Vulkan: %d",
                                                         vkImages[i].type);
                }

                const VkFormat imgFormat = GetVkImageFormat(vkImages[i].image);
                if (imgFormat != expectedFormat) {
                    conformanceHooks->ConformanceFailure(
                        XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrEnumerateSwapchainImages",
                        "xrEnumerateSwapchainImages warning: VkImage format is not expected format %d: Swapchain : %d", expectedFormat,
                        imgFormat);
                }
            }
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
            allowAccessVkQueue[std::this_thread::get_id()] = allowed;
            ResetVkQueueAccess(device, queueFamilyIndex, queueIndex);
        }

        bool CheckState() const override
        {
            const bool checkAccess = !allowAccessVkQueue.at(std::this_thread::get_id());
            if (checkAccess) {
                bool accessed = CheckVkQueueAccess(device, queueFamilyIndex, queueIndex);
                ResetVkQueueAccess(device, queueFamilyIndex, queueIndex);
                if (accessed) {
                    return false;
                }
            }

            return true;
        }
    };

    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator_Vulkan(const XrGraphicsBindingVulkanKHR* graphicsBinding)
    {
        return std::make_shared<VulkanGraphicsValidator>(graphicsBinding);
    }

}  // namespace Conformance

#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
