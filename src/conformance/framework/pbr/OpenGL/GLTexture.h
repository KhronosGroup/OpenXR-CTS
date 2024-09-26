// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
//
// Shared data types and functions used throughout the Pbr rendering library.
//

#pragma once

#include "GLCommon.h"
#include "GLResources.h"

#include <utilities/image.h>
#include "../PbrCommon.h"

#include "common/gfxwrapper_opengl.h"

#include <openxr/openxr.h>

#include <stdint.h>

namespace Pbr
{
    namespace GLTexture
    {
        ScopedGLTexture LoadTextureImage(const GLResources& pbrResources, bool sRGB, const uint8_t* fileData, uint32_t fileSize);
        ScopedGLTexture CreateFlatCubeTexture(RGBAColor color, bool sRGB);
        ScopedGLTexture CreateTexture(const Conformance::Image::Image& image);
        ScopedGLSampler CreateSampler(GLenum edgeSamplingMode = GL_CLAMP_TO_EDGE);
    }  // namespace GLTexture
}  // namespace Pbr
