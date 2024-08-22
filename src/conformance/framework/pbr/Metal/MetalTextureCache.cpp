// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalTextureCache.h"

#include "MetalResources.h"
#include "MetalTexture.h"

#include "../PbrMaterial.h"

#include "utilities/metal_utils.h"

#include <array>
#include <memory>
#include <mutex>

namespace Pbr
{
    MetalTextureCache::MetalTextureCache(MTL::Device* device) : m_cacheMutex(std::make_unique<std::mutex>())
    {
        m_device = NS::RetainPtr(device);
    }

    NS::SharedPtr<MTL::Texture> MetalTextureCache::CreateTypedSolidColorTexture(XrColor4f color)
    {
        if (!IsValid()) {
            throw std::logic_error("MetalTextureCache accessed before initialization");
        }
        const std::array<uint8_t, 4> rgba = MetalTexture::LoadRGBAUI4(color);

        // Check cache to see if this flat texture already exists.
        const uint32_t colorKey = *reinterpret_cast<const uint32_t*>(rgba.data());
        {
            std::lock_guard<std::mutex> guard(*m_cacheMutex);
            auto textureIt = m_solidColorTextureCache.find(colorKey);
            if (textureIt != m_solidColorTextureCache.end()) {
                return textureIt->second;
            }
        }

        NS::SharedPtr<MTL::Texture> texture =
            MetalTexture::CreateTexture(m_device.get(), rgba.data(), 4, 1, 1, MTL::PixelFormatRGBA8Unorm, MTLSTR("SolidColorTexture"));

        std::lock_guard<std::mutex> guard(*m_cacheMutex);
        // If the key already exists then the existing texture will be returned.
        return m_solidColorTextureCache.emplace(colorKey, texture).first->second;
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
