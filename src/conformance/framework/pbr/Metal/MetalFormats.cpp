// Copyright 2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalFormats.h"

#include <utilities/image.h>

#include <Foundation/Foundation.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <unordered_map>

namespace Pbr
{
    namespace Image = Conformance::Image;

    namespace
    {
        using Image::Codec;
        using Image::Channels;
        using ColorSpace = Image::ColorSpaceType;

        std::unordered_map<Image::FormatParams, MTL::PixelFormat, Image::FormatParamsHash> MetalFormatMap = {
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpace::sRGB}, MTL::PixelFormatRGBA8Unorm_sRGB},
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpace::Linear}, MTL::PixelFormatRGBA8Unorm},
            {{Codec::BC7, Channels::RGBA, ColorSpace::sRGB}, MTL::PixelFormatBC7_RGBAUnorm_sRGB},
            {{Codec::BC7, Channels::RGBA, ColorSpace::Linear}, MTL::PixelFormatBC7_RGBAUnorm},
            {{Codec::BC7, Channels::RGB, ColorSpace::sRGB}, MTL::PixelFormatBC7_RGBAUnorm_sRGB},
            {{Codec::BC7, Channels::RGB, ColorSpace::Linear}, MTL::PixelFormatBC7_RGBAUnorm},
            {{Codec::ETC, Channels::RGB, ColorSpace::sRGB}, MTL::PixelFormatETC2_RGB8_sRGB},
            {{Codec::ETC, Channels::RGB, ColorSpace::Linear}, MTL::PixelFormatETC2_RGB8},
            {{Codec::ETC, Channels::RGBA, ColorSpace::sRGB}, MTL::PixelFormatETC2_RGB8A1_sRGB},
            {{Codec::ETC, Channels::RGBA, ColorSpace::Linear}, MTL::PixelFormatETC2_RGB8A1},
        };
    }  // namespace

    const std::unordered_map<Image::FormatParams, MTL::PixelFormat, Image::FormatParamsHash>& GetMetalFormatMap()
    {
        return MetalFormatMap;
    }

    bool IsKnownFormatSupportedByDriver(MTL::Device* device, MTL::PixelFormat format)
    {
        switch (format) {
        case MTL::PixelFormatRGBA8Unorm_sRGB:
        case MTL::PixelFormatRGBA8Unorm:
            return true;
        case MTL::PixelFormatBC7_RGBAUnorm_sRGB:
        case MTL::PixelFormatBC7_RGBAUnorm:
            return device->supportsBCTextureCompression();
        case MTL::PixelFormatETC2_RGB8_sRGB:
        case MTL::PixelFormatETC2_RGB8:
        case MTL::PixelFormatETC2_RGB8A1_sRGB:
        case MTL::PixelFormatETC2_RGB8A1:
            return device->supportsFamily(MTL::GPUFamilyApple2);
        default:
            throw std::logic_error("IsFormatSupportedByDriver call had format not defined in format map");
        }
    }

    MTL::PixelFormat ToMetalFormat(Image::FormatParams format, bool throwIfNotFound /* = true */)
    {
        auto matchingFormat = MetalFormatMap.find(format);
        if (matchingFormat == MetalFormatMap.end()) {
            if (throwIfNotFound) {
                throw std::logic_error("ToMetalFormat call had format not defined in format map (and throwIfNotFound was true)");
            }
            return MTL::PixelFormatInvalid;
        }
        return matchingFormat->second;
    }
}  // namespace Pbr
#endif
