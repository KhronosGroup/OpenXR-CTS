// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "D3D11TextureCache.h"

#include "D3D11Texture.h"

#include "../PbrMaterial.h"

#include <array>
#include <memory>
#include <mutex>

namespace Pbr
{
    D3D11TextureCache::D3D11TextureCache(ID3D11Device* device) : m_cacheMutex(std::make_unique<std::mutex>())
    {
        m_device = device;
    }

    ComPtr<ID3D11ShaderResourceView> D3D11TextureCache::CreateTypedSolidColorTexture(XrColor4f color)
    {
        if (!IsValid()) {
            throw std::logic_error("D3D11TextureCache accessed before initialization");
        }
        const std::array<uint8_t, 4> rgba = D3D11Texture::LoadRGBAUI4(color);

        // Check cache to see if this flat texture already exists.
        const uint32_t colorKey = *reinterpret_cast<const uint32_t*>(rgba.data());
        {
            std::lock_guard<std::mutex> guard(*m_cacheMutex);
            auto textureIt = m_solidColorTextureCache.find(colorKey);
            if (textureIt != m_solidColorTextureCache.end()) {
                return textureIt->second;
            }
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture =
            Pbr::D3D11Texture::CreateTexture(m_device.Get(), rgba.data(), 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        std::lock_guard<std::mutex> guard(*m_cacheMutex);
        // If the key already exists then the existing texture will be returned.
        return m_solidColorTextureCache.emplace(colorKey, texture).first->second;
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
