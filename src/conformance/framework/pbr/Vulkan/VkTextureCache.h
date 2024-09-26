// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "VkCommon.h"
#include "VkResources.h"

#include "utilities/vulkan_utils.h"

#include <openxr/openxr.h>
#include <vulkan/vulkan_core.h>

#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>

namespace Pbr
{
    struct VulkanResources;
    struct VulkanTextureBundle;

    /// Cache of single-color textures.
    ///
    /// Device-dependent, drop when device is lost or destroyed.
    class VulkanTextureCache
    {
    public:
        VulkanTextureCache();

        VulkanTextureCache(VulkanTextureCache&&) = default;
        VulkanTextureCache& operator=(VulkanTextureCache&&) = default;

        /// Find or create a single pixel texture of the given color
        std::shared_ptr<VulkanTextureBundle> CreateTypedSolidColorTexture(Pbr::VulkanResources& pbrResources, XrColor4f color, bool sRGB);

    private:
        // in unique_ptr to make it moveable
        std::unique_ptr<std::mutex> m_cacheMutex;
        std::map<uint32_t, std::shared_ptr<VulkanTextureBundle>> m_solidColorTextureCache;
    };

}  // namespace Pbr
