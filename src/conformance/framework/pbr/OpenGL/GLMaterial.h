// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include "GLCommon.h"
#include "GLResources.h"

#include "../PbrCommon.h"
#include "../PbrMaterial.h"
#include "../PbrSharedState.h"

#include "common/gfxwrapper_opengl.h"

#include <array>
#include <map>
#include <memory>
#include <stddef.h>
#include <string>
#include <vector>

namespace Pbr
{
    struct GLResources;

    // A GLMaterial contains the metallic roughness parameters and textures.
    // Primitives specify which GLMaterial to use when being rendered.
    struct GLMaterial final : public Material
    {
        // Create a uninitialized material. Textures and shader coefficients must be set.
        GLMaterial(Pbr::GLResources const& pbrResources);

        // Create a clone of this material.
        std::shared_ptr<GLMaterial> Clone(Pbr::GLResources const& pbrResources) const;

        // Create a flat (no texture) material.
        static std::shared_ptr<GLMaterial> CreateFlat(const GLResources& pbrResources, RGBAColor baseColorFactor,
                                                      float roughnessFactor = 1.0f, float metallicFactor = 0.0f,
                                                      RGBColor emissiveFactor = RGB::Black);

        // Set a Metallic-Roughness texture.
        void SetTexture(ShaderSlots::PSMaterial slot, std::shared_ptr<ScopedGLTexture> textureView,
                        std::shared_ptr<ScopedGLSampler> sampler = nullptr);
        // void SetTexture(ShaderSlots::PSMaterial slot, ITexture& texture) override;

        // Bind this material to current context.
        void Bind(const GLResources& pbrResources) const;

        std::string Name;
        bool Hidden{false};

    private:
        static constexpr size_t TextureCount = ShaderSlots::NumMaterialSlots;
        std::array<std::shared_ptr<ScopedGLTexture>, TextureCount> m_textures;
        std::array<std::shared_ptr<ScopedGLSampler>, TextureCount> m_samplers;
        ScopedGLBuffer m_constantBuffer;
    };
}  // namespace Pbr
