// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "VkTextureCache.h"

#include "VkCommon.h"
#include "VkTexture.h"

#include <openxr/openxr.h>

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace Pbr
{
    struct VulkanResources;

    VulkanTextureCache::VulkanTextureCache(VkDevice device) : m_device(device), m_cacheMutex(std::make_unique<std::mutex>())
    {
        m_device = device;
    }

    std::shared_ptr<VulkanTextureBundle> VulkanTextureCache::CreateTypedSolidColorTexture(Pbr::VulkanResources& pbrResources,
                                                                                          XrColor4f color)
    {
        if (!IsValid()) {
            throw std::logic_error("VulkanTextureCache accessed before initialization");
        }
        const std::array<uint8_t, 4> rgba = VulkanTexture::LoadRGBAUI4(color);

        // Check cache to see if this flat texture already exists.
        const uint32_t colorKey = *reinterpret_cast<const uint32_t*>(rgba.data());
        {
            std::lock_guard<std::mutex> guard(*m_cacheMutex);
            auto textureIt = m_solidColorTextureCache.find(colorKey);
            if (textureIt != m_solidColorTextureCache.end()) {
                return textureIt->second;
            }
        }

        auto texture = std::make_shared<VulkanTextureBundle>(
            VulkanTexture::CreateTexture(pbrResources, rgba.data(), 4, 1, 1, VK_FORMAT_R8G8B8A8_UNORM));

        std::lock_guard<std::mutex> guard(*m_cacheMutex);
        // If the key already exists then the existing texture will be returned.
        return m_solidColorTextureCache.emplace(colorKey, texture).first->second;
    }
}  // namespace Pbr
