// Copyright 2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "GLFormats.h"

#include <utilities/image.h>

#include "common/gfxwrapper_opengl.h"

#include <unordered_map>

namespace Pbr
{
    namespace Image = Conformance::Image;

    namespace
    {
        using Image::Codec;
        using Image::Channels;
        using ColorSpace = Image::ColorSpaceType;
        const GLenum NotApp = GLFormatData::Unpopulated;
        std::unordered_map<Image::FormatParams, GLFormatData, Image::FormatParamsHash> GLFormatMap = {
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpace::sRGB}, {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE}},
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpace::Linear}, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}},
            {{Codec::Raw8bpc, Channels::RGB, ColorSpace::sRGB}, {GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE}},
            {{Codec::Raw8bpc, Channels::RGB, ColorSpace::Linear}, {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE}},
            {{Codec::ETC, Channels::RGB, ColorSpace::sRGB}, {GL_COMPRESSED_SRGB8_ETC2, NotApp, NotApp}},
            {{Codec::ETC, Channels::RGB, ColorSpace::Linear}, {GL_COMPRESSED_RGB8_ETC2, NotApp, NotApp}},
            {{Codec::ETC, Channels::RGBA, ColorSpace::sRGB}, {GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, NotApp, NotApp}},
            {{Codec::ETC, Channels::RGBA, ColorSpace::Linear}, {GL_COMPRESSED_RGBA8_ETC2_EAC, NotApp, NotApp}},
        };
    }  // namespace

    const std::unordered_map<Image::FormatParams, GLFormatData, Image::FormatParamsHash>& GetGLFormatMap()
    {
        return GLFormatMap;
    }

    GLFormatData ToGLFormatData(Image::FormatParams format, bool throwIfNotFound /* = true */)
    {
        auto matchingFormat = GLFormatMap.find(format);
        if (matchingFormat == GLFormatMap.end()) {
            if (throwIfNotFound) {
                throw std::logic_error("ToGLFormatData call had format not defined in format map (and throwIfNotFound was true)");
            }
            return {GLFormatData::Unpopulated, GLFormatData::Unpopulated, GLFormatData::Unpopulated};
        }
        return matchingFormat->second;
    }
}  // namespace Pbr
#endif
