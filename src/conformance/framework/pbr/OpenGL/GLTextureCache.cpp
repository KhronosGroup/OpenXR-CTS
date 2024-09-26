// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "GLTextureCache.h"

#include "GLCommon.h"
#include "GLTexture.h"

#include "../PbrTexture.h"

#include "common/gfxwrapper_opengl.h"

#include <openxr/openxr.h>

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace Pbr
{
    GLTextureCache::GLTextureCache() : m_cacheMutex(std::make_unique<std::mutex>())
    {
    }

    std::shared_ptr<ScopedGLTexture> GLTextureCache::CreateTypedSolidColorTexture(XrColor4f color, bool sRGB)
    {
        if (!IsValid()) {
            throw std::logic_error("GLTextureCache accessed before initialization");
        }
        const std::array<uint8_t, 4> rgba = LoadRGBAUI4(color);

        // Check cache to see if this flat texture already exists.
        const uint32_t colorKey = *reinterpret_cast<const uint32_t*>(rgba.data());
        {
            std::lock_guard<std::mutex> guard(*m_cacheMutex);
            auto textureIt = m_solidColorTextureCache.find(colorKey);
            if (textureIt != m_solidColorTextureCache.end()) {
                return textureIt->second;
            }
        }

        auto formatParams = Conformance::Image::FormatParams::R8G8B8A8(sRGB);
        auto metadata = Conformance::Image::ImageLevelMetadata::MakeUncompressed(1, 1);
        auto image = Conformance::Image::Image{formatParams, {{metadata, rgba}}};

        auto texture = std::make_shared<ScopedGLTexture>(GLTexture::CreateTexture(image));

        std::lock_guard<std::mutex> guard(*m_cacheMutex);
        // If the key already exists then the existing texture will be returned.
        return m_solidColorTextureCache.emplace(colorKey, texture).first->second;
    }
}  // namespace Pbr

#endif
