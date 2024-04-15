// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "D3D12Resources.h"

#include "../PbrMaterial.h"

#include <DirectXColors.h>
#include <d3d12.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <array>
#include <memory>

namespace Pbr
{
    /// A D3D12Material contains the metallic roughness parameters and textures.
    /// Primitives specify which D3D12Material to use when being rendered.
    struct D3D12Material final : public Material
    {
        /// Create a uninitialized material. Textures and shader coefficients must be set.
        D3D12Material(Pbr::D3D12Resources const& pbrResources);

        /// Create a clone of this material. Shares the texture and sampler heap with this material.
        std::shared_ptr<D3D12Material> Clone(Pbr::D3D12Resources const& pbrResources) const;

        /// Create a flat (no texture) material.
        static std::shared_ptr<D3D12Material> CreateFlat(D3D12Resources& pbrResources, RGBAColor baseColorFactor,
                                                         float roughnessFactor = 1.0f, float metallicFactor = 0.0f,
                                                         RGBColor emissiveFactor = RGB::Black);

        /// Set a Metallic-Roughness texture.
        void SetTexture(_In_ ID3D12Device* device, ShaderSlots::PSMaterial slot, Conformance::D3D12ResourceWithSRVDesc& texture,
                        _In_opt_ D3D12_SAMPLER_DESC* sampler);

        /// Write the descriptors of this material to a texture and sampler heap
        void GetDescriptors(_In_ ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destTextureDescriptors,
                            D3D12_CPU_DESCRIPTOR_HANDLE destSamplerDescriptors);

        /// Bind this material to current context.
        void Bind(_In_ ID3D12GraphicsCommandList* directCommandList, D3D12Resources& pbrResources);

        std::string Name;
        bool Hidden{false};

    private:
        static constexpr size_t TextureCount = ShaderSlots::NumMaterialSlots;
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, TextureCount> m_textures;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_textureHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
        Conformance::D3D12BufferWithUpload<ConstantBufferData> m_constantBuffer;
    };
}  // namespace Pbr
