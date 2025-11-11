// Copyright (c) 2025 The Khronos Group Inc.
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

#include "RuntimeFailure.h"
#include "ConformanceHooks.h"
#include "VulkanLayer.h"

#if defined(XR_USE_GRAPHICS_API_VULKAN)

XrResult ConformanceHooks::xrCreateVulkanDeviceKHR(HandleState* const handleState, XrInstance instance,
                                                   const XrVulkanDeviceCreateInfoKHR* createInfo, VkDevice* vulkanDevice,
                                                   VkResult* vulkanResult)
{
    XrResult result = ConformanceHooksBase::xrCreateVulkanDeviceKHR(handleState, instance, createInfo, vulkanDevice, vulkanResult);

    if (enabledExtensions.khr_vulkan_enable2 && enabledExtensions.khr_vulkan_swapchain_format_list) {
        NONCONFORMANT_IF(!DeviceVkExtensionEnabled(*vulkanDevice, VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME),
                         "Missing VK_KHR_image_format_list extension");
    }

    return result;
}

#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
