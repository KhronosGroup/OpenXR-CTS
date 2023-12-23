// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "common/xr_linear.h"

#include <type_traits>

namespace Pbr
{
    namespace Glsl
    {

        /// Parameters of the Scene
        struct SceneConstantBuffer
        {
            XrMatrix4x4f ViewProjection;
            XrVector3f EyePosition;
            float _pad0;
            XrVector3f LightDirection{};
            float _pad1;
            XrVector3f LightDiffuseColor{};
            float _pad2;
            uint32_t NumSpecularMipLevels{1};  // all glsl ints are 32 bit
            float _pad3[3];
        };

        // Following std140 layout rules, must match PbrPixelShader_glsl.frag and PbrVertexShader_glsl.vert
        // Can get the offsets by passing -q to glslangValidator
        static_assert(std::is_standard_layout<SceneConstantBuffer>::value, "Must be standard layout");
        static_assert(sizeof(float) == 4, "Single precision floats");
        static_assert(sizeof(XrVector3f) == 3 * sizeof(float), "No padding in vectors");
        static_assert(sizeof(XrVector4f) == 4 * sizeof(float), "No padding in vectors");
        static_assert(alignof(XrVector4f) == 4, "No padding in vectors");
        static_assert((sizeof(SceneConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");
        static_assert(sizeof(SceneConstantBuffer) == 128, "Size must be the same as known");
        static_assert(offsetof(SceneConstantBuffer, ViewProjection) == 0, "Offsets must match shader");
        static_assert(offsetof(SceneConstantBuffer, EyePosition) == 64, "Offsets must match shader");
        static_assert(offsetof(SceneConstantBuffer, LightDirection) == 80, "Offsets must match shader");
        static_assert(offsetof(SceneConstantBuffer, LightDiffuseColor) == 96, "Offsets must match shader");
        static_assert(offsetof(SceneConstantBuffer, NumSpecularMipLevels) == 112, "Offsets must match shader");

        /// Constant parameter of the Model
        struct ModelConstantBuffer
        {
            XrMatrix4x4f ModelToWorld;
        };

        static_assert((sizeof(ModelConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");

    }  // namespace Glsl
}  // namespace Pbr
