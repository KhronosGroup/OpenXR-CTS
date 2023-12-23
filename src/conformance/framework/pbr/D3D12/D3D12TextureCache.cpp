// Copyright 2023, The Khronos Group, Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "D3D12TextureCache.h"

#include "D3D12Resources.h"
#include "D3D12Texture.h"

#include "../PbrMaterial.h"

#include "utilities/d3d12_utils.h"

#include <array>
#include <memory>
#include <mutex>

namespace Pbr
{
    D3D12TextureCache::D3D12TextureCache(ID3D12Device* device) : m_cacheMutex(std::make_unique<std::mutex>())
    {
        m_device = device;
    }

    Conformance::D3D12ResourceWithSRVDesc D3D12TextureCache::CreateTypedSolidColorTexture(Pbr::D3D12Resources& pbrResources,
                                                                                          XrColor4f color)
    {
        if (!IsValid()) {
            throw std::logic_error("D3D12TextureCache accessed before initialization");
        }
        const std::array<uint8_t, 4> rgba = D3D12Texture::LoadRGBAUI4(color);

        // Check cache to see if this flat texture already exists.
        const uint32_t colorKey = *reinterpret_cast<const uint32_t*>(rgba.data());
        {
            std::lock_guard<std::mutex> guard(*m_cacheMutex);
            auto textureIt = m_solidColorTextureCache.find(colorKey);
            if (textureIt != m_solidColorTextureCache.end()) {
                return textureIt->second;
            }
        }

        Conformance::D3D12ResourceWithSRVDesc texture =
            D3D12Texture::CreateTexture(pbrResources, rgba.data(), 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        std::lock_guard<std::mutex> guard(*m_cacheMutex);
        // If the key already exists then the existing texture will be returned.
        return m_solidColorTextureCache.emplace(colorKey, texture).first->second;
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
