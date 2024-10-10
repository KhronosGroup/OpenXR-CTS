// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "PbrTexture.h"

#include "PbrCommon.h"
#include <utilities/image.h>

#include "stb_image.h"

#include <cassert>
#include <memory>
#include <string>

namespace Pbr
{
    std::array<uint8_t, 4> LoadRGBAUI4(RGBAColor color)
    {
        return std::array<uint8_t, 4>{(uint8_t)(color.r * 255.), (uint8_t)(color.g * 255.), (uint8_t)(color.b * 255.),
                                      (uint8_t)(color.a * 255.)};
    }

    namespace StbiLoader
    {
        void StbiDeleter::operator()(unsigned char* pointer) const
        {
            ::free(pointer);
        }

        OwningImage<stbi_unique_ptr> LoadTextureImage(span<const Conformance::Image::FormatParams> supportedFormats,  //
                                                      bool sRGB, const uint8_t* fileData, uint32_t fileSize)
        {
            namespace Image = Conformance::Image;

            int w, h, c, ok;
            ok = stbi_info_from_memory(fileData, fileSize, &w, &h, &c);
            if (!ok) {
                throw std::runtime_error("Failed to load image file metadata.");
            }

            // TODO: support 2-channel images, since the BRDF LUT is actually that
            assert(c >= 3);
            assert(c <= 4);

            auto colorSpaceType = sRGB ? Image::ColorSpaceType::sRGB : Image::ColorSpaceType::Linear;
            auto formatParams = FindRawFormat((Image::Channels)c, colorSpaceType, supportedFormats);

            int desiredComponentCount = (int)formatParams.channels;
            assert(desiredComponentCount >= c);
            // If c == 3 and desiredComponentCount == 4, a component will be padded with 1.0f
            stbi_unique_ptr rgbaData(stbi_load_from_memory(fileData, fileSize, &w, &h, &c, desiredComponentCount));
            if (!rgbaData) {
                throw std::runtime_error("Failed to load image file data.");
            }

            auto metadata = Image::ImageLevelMetadata::MakeUncompressed(w, h);

            size_t rgbaSize = w * h * desiredComponentCount;
            auto image = Image::Image{formatParams, {{metadata, {rgbaData.get(), rgbaSize}}}};
            return {std::move(rgbaData), image};
        }
    }  // namespace StbiLoader
}  // namespace Pbr
