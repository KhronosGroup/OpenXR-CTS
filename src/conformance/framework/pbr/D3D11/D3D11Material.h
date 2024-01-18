// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "D3D11Resources.h"

#include "../PbrMaterial.h"

#include <DirectXColors.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <memory>

namespace Pbr
{
    /// A D3D11Material contains the metallic roughness parameters and textures.
    /// Primitives specify which D3D11Material to use when being rendered.
    struct D3D11Material final : public Material
    {
        /// Create a uninitialized material. Textures and shader coefficients must be set.
        D3D11Material(Pbr::D3D11Resources const& pbrResources);

        /// Create a clone of this material.
        std::shared_ptr<D3D11Material> Clone(Pbr::D3D11Resources const& pbrResources) const;

        /// Create a flat (no texture) material.
        static std::shared_ptr<D3D11Material> CreateFlat(const D3D11Resources& pbrResources, RGBAColor baseColorFactor,
                                                         float roughnessFactor = 1.0f, float metallicFactor = 0.0f,
                                                         RGBColor emissiveFactor = RGB::Black);

        /// Set a Metallic-Roughness texture.
        void SetTexture(ShaderSlots::PSMaterial slot, _In_ ID3D11ShaderResourceView* textureView,
                        _In_opt_ ID3D11SamplerState* sampler = nullptr);

        /// Bind this material to current context.
        void Bind(_In_ ID3D11DeviceContext* context, const D3D11Resources& pbrResources) const;

        std::string Name;
        bool Hidden{false};

    private:
        static constexpr size_t TextureCount = ShaderSlots::NumMaterialSlots;
        std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, TextureCount> m_textures;
        std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, TextureCount> m_samplers;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
    };
}  // namespace Pbr
