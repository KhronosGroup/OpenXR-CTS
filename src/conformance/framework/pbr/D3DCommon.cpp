// Copyright 2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11) || defined(XR_USE_GRAPHICS_API_D3D12)

#include "D3DCommon.h"

#include <utilities/image.h>

#include <dxgiformat.h>
#include <d3d11.h>

#include <unordered_map>

namespace Pbr
{
    namespace Image = Conformance::Image;

    namespace
    {
        using Image::Codec;
        using Image::Channels;
        using ColorSpace = Image::ColorSpaceType;
        std::unordered_map<Image::FormatParams, DXGI_FORMAT, Image::FormatParamsHash> DXGIFormatMap = {
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpace::sRGB}, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpace::Linear}, DXGI_FORMAT_R8G8B8A8_UNORM},
            {{Codec::BC7, Channels::RGBA, ColorSpace::Linear}, DXGI_FORMAT_BC7_UNORM},
            {{Codec::BC7, Channels::RGB, ColorSpace::Linear}, DXGI_FORMAT_BC7_UNORM},
            {{Codec::BC7, Channels::RGBA, ColorSpace::sRGB}, DXGI_FORMAT_BC7_UNORM_SRGB},
            {{Codec::BC7, Channels::RGB, ColorSpace::sRGB}, DXGI_FORMAT_BC7_UNORM_SRGB},
        };
    }  // namespace

    const std::unordered_map<Image::FormatParams, DXGI_FORMAT, Image::FormatParamsHash>& GetDXGIFormatMap()
    {
        return DXGIFormatMap;
    }

    DXGI_FORMAT ToDXGIFormat(Image::FormatParams format, bool throwIfNotFound /* = true */)
    {
        auto matchingFormat = DXGIFormatMap.find(format);
        if (matchingFormat == DXGIFormatMap.end()) {
            if (throwIfNotFound) {
                throw std::logic_error("ToDXGIFormat call had format not defined in format map (and throwIfNotFound was true)");
            }
            return DXGI_FORMAT_UNKNOWN;
        }
        return matchingFormat->second;
    }
}  // namespace Pbr
#endif
