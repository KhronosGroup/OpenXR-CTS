// Copyright 2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_VULKAN)

#include "VkFormats.h"

#include <utilities/image.h>

#include <vulkan/vulkan_core.h>

#include <unordered_map>

namespace Pbr
{
    namespace Image = Conformance::Image;

    namespace
    {
        using Image::Codec;
        using Image::Channels;
        using ColorSpace = Image::ColorSpaceType;
        std::unordered_map<Image::FormatParams, VkFormat, Image::FormatParamsHash> VkFormatMap = {
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpace::sRGB}, VK_FORMAT_R8G8B8A8_SRGB},
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpace::Linear}, VK_FORMAT_R8G8B8A8_UNORM},
            {{Codec::Raw8bpc, Channels::RGB, ColorSpace::sRGB}, VK_FORMAT_R8G8B8_SRGB},
            {{Codec::Raw8bpc, Channels::RGB, ColorSpace::Linear}, VK_FORMAT_R8G8B8_UNORM},
            {{Codec::BC7, Channels::RGBA, ColorSpace::sRGB}, VK_FORMAT_BC7_SRGB_BLOCK},
            {{Codec::BC7, Channels::RGBA, ColorSpace::Linear}, VK_FORMAT_BC7_UNORM_BLOCK},
            {{Codec::BC7, Channels::RGB, ColorSpace::sRGB}, VK_FORMAT_BC7_SRGB_BLOCK},
            {{Codec::BC7, Channels::RGB, ColorSpace::Linear}, VK_FORMAT_BC7_UNORM_BLOCK},
            {{Codec::ETC, Channels::RGB, ColorSpace::sRGB}, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK},
            {{Codec::ETC, Channels::RGB, ColorSpace::Linear}, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK},
            {{Codec::ETC, Channels::RGBA, ColorSpace::sRGB}, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK},
            {{Codec::ETC, Channels::RGBA, ColorSpace::Linear}, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK},
        };
    }  // namespace

    const std::unordered_map<Image::FormatParams, VkFormat, Image::FormatParamsHash>& GetVkFormatMap()
    {
        return VkFormatMap;
    }

    VkFormat ToVkFormat(Image::FormatParams format, bool throwIfNotFound /* = true */)
    {
        auto matchingFormat = VkFormatMap.find(format);
        if (matchingFormat == VkFormatMap.end()) {
            if (throwIfNotFound) {
                throw std::logic_error("ToVkFormat call had format not defined in format map (and throwIfNotFound was true)");
            }
            return VK_FORMAT_UNDEFINED;
        }
        return matchingFormat->second;
    }
}  // namespace Pbr
#endif
