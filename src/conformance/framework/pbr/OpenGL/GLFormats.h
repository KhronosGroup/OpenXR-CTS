// Copyright 2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include <utilities/image.h>

#include "common/gfxwrapper_opengl.h"

#include <unordered_map>

namespace Pbr
{
    struct GLFormatData
    {
        GLenum InternalFormat;
        GLenum UncompressedFormat;
        GLenum UncompressedType;
        static const GLenum Unpopulated = GLenum(-1);
    };

    GLFormatData ToGLFormatData(Conformance::Image::FormatParams format, bool throwIfNotFound = true);
    const std::unordered_map<Conformance::Image::FormatParams, GLFormatData, Conformance::Image::FormatParamsHash>& GetGLFormatMap();
}  // namespace Pbr
