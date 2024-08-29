// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "../PbrMaterial.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <array>
#include <memory>

namespace Pbr
{

    struct MetalResources;

    /// A MetalMaterial contains the metallic roughness parameters and textures.
    /// Primitives specify which MetalMaterial to use when being rendered.
    struct MetalMaterial final : public Material
    {
        /// Create a uninitialized material. Textures and shader coefficients must be set.
        MetalMaterial(const MetalResources& pbrResources);

        /// Create a clone of this material.
        std::shared_ptr<MetalMaterial> Clone(const MetalResources& pbrResources) const;

        /// Create a flat (no texture) material.
        static std::shared_ptr<MetalMaterial> CreateFlat(const MetalResources& pbrResources, RGBAColor baseColorFactor,
                                                         float roughnessFactor = 1.0f, float metallicFactor = 0.0f,
                                                         RGBColor emissiveFactor = RGB::Black);

        /// Set a Metallic-Roughness texture.
        void SetTexture(ShaderSlots::PSMaterial slot, MTL::Texture* texture, MTL::SamplerState* sampler = nullptr);

        /// Bind this material to current context.
        void Bind(MTL::RenderCommandEncoder* renderCommandEncoder, const MetalResources& pbrResources) const;

        std::string Name;
        bool Hidden{false};

    private:
        static constexpr size_t TextureCount = ShaderSlots::NumMaterialSlots;
        std::array<NS::SharedPtr<MTL::Texture>, TextureCount> m_textures;
        std::array<NS::SharedPtr<MTL::SamplerState>, TextureCount> m_samplers;
        NS::SharedPtr<MTL::Buffer> m_constantBuffer;
    };
}  // namespace Pbr
