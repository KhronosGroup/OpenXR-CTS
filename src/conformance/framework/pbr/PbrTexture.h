// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "PbrCommon.h"
#include <utilities/image.h>

#include <memory>

namespace Pbr
{
    using nonstd::span;

    std::array<uint8_t, 4> LoadRGBAUI4(RGBAColor color);

    namespace StbiLoader
    {
        template <typename T>
        struct OwningImage
        {
            T backingData;
            Conformance::Image::Image image;
        };

        struct StbiDeleter
        {
            typedef unsigned char* pointer;
            void operator()(unsigned char* pointer) const;
        };

        using stbi_unique_ptr = std::unique_ptr<unsigned char, StbiDeleter>;

        OwningImage<stbi_unique_ptr> LoadTextureImage(span<const Conformance::Image::FormatParams> supportedFormats,  //
                                                      bool sRGB, const uint8_t* fileData, uint32_t fileSize);
    }  // namespace StbiLoader
}  // namespace Pbr
