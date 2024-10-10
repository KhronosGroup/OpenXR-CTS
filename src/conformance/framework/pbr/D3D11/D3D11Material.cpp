// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "D3D11Material.h"

#include "D3D11Resources.h"
#include "D3D11Texture.h"

#include "../PbrMaterial.h"

#include "utilities/throw_helpers.h"

#include <algorithm>

using namespace DirectX;

namespace Pbr
{
    D3D11Material::D3D11Material(Pbr::D3D11Resources const& pbrResources)
    {
        const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ConstantBufferData), D3D11_BIND_CONSTANT_BUFFER);
        XRC_CHECK_THROW_HRCMD(
            pbrResources.GetDevice()->CreateBuffer(&constantBufferDesc, nullptr, m_constantBuffer.ReleaseAndGetAddressOf()));
    }

    std::shared_ptr<D3D11Material> D3D11Material::Clone(Pbr::D3D11Resources const& pbrResources) const
    {
        auto clone = std::make_shared<D3D11Material>(pbrResources);
        clone->CopyFrom(*this);
        clone->m_textures = m_textures;
        clone->m_samplers = m_samplers;
        return clone;
    }

    /* static */
    std::shared_ptr<D3D11Material> D3D11Material::CreateFlat(const D3D11Resources& pbrResources, RGBAColor baseColorFactor,
                                                             float roughnessFactor /* = 1.0f */, float metallicFactor /* = 0.0f */,
                                                             RGBColor emissiveFactor /* = XMFLOAT3(0, 0, 0) */)
    {
        std::shared_ptr<D3D11Material> material = std::make_shared<D3D11Material>(pbrResources);

        if (baseColorFactor.a < 1.0f) {  // Alpha channel
            material->SetAlphaBlended(BlendState::AlphaBlended);
        }

        Pbr::D3D11Material::ConstantBufferData& parameters = material->Parameters();
        parameters.BaseColorFactor = baseColorFactor;
        parameters.EmissiveFactor = emissiveFactor;
        parameters.MetallicFactor = metallicFactor;
        parameters.RoughnessFactor = roughnessFactor;

        const Microsoft::WRL::ComPtr<ID3D11SamplerState> defaultSampler = Pbr::D3D11Texture::CreateSampler(pbrResources.GetDevice().Get());
        material->SetTexture(ShaderSlots::BaseColor, pbrResources.CreateTypedSolidColorTexture(RGBA::White, true).Get(),
                             defaultSampler.Get());
        material->SetTexture(ShaderSlots::MetallicRoughness, pbrResources.CreateTypedSolidColorTexture(RGBA::White, false).Get(),
                             defaultSampler.Get());
        // No occlusion.
        material->SetTexture(ShaderSlots::Occlusion, pbrResources.CreateTypedSolidColorTexture(RGBA::White, false).Get(),
                             defaultSampler.Get());
        // Flat normal.
        material->SetTexture(ShaderSlots::Normal, pbrResources.CreateTypedSolidColorTexture(RGBA::FlatNormal, false).Get(),
                             defaultSampler.Get());
        material->SetTexture(ShaderSlots::Emissive, pbrResources.CreateTypedSolidColorTexture(RGBA::White, true).Get(),
                             defaultSampler.Get());

        return material;
    }

    void D3D11Material::SetTexture(ShaderSlots::PSMaterial slot, _In_ ID3D11ShaderResourceView* textureView,
                                   _In_opt_ ID3D11SamplerState* sampler)
    {
        m_textures[slot] = textureView;

        if (sampler) {
            m_samplers[slot] = sampler;
        }
    }

    void D3D11Material::Bind(_In_ ID3D11DeviceContext* context, const D3D11Resources& pbrResources) const
    {
        // If the parameters of the constant buffer have changed, update the constant buffer.
        if (m_parametersChanged) {
            m_parametersChanged = false;
            context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &m_parameters, 0, 0);
        }

        pbrResources.SetBlendState(context, m_alphaBlended == BlendState::AlphaBlended);
        pbrResources.SetDepthStencilState(context, m_alphaBlended == BlendState::AlphaBlended);
        pbrResources.SetRasterizerState(context, m_doubleSided == DoubleSided::DoubleSided);

        ID3D11Buffer* psConstantBuffers[] = {m_constantBuffer.Get()};
        context->PSSetConstantBuffers(Pbr::ShaderSlots::ConstantBuffers::Material, 1, psConstantBuffers);

        static_assert(Pbr::ShaderSlots::BaseColor == 0, "BaseColor must be the first slot");

        std::array<ID3D11ShaderResourceView*, TextureCount> textures;
        std::transform(m_textures.begin(), m_textures.end(), textures.begin(), [](const auto& texture) { return texture.Get(); });
        context->PSSetShaderResources(Pbr::ShaderSlots::BaseColor, (UINT)textures.size(), textures.data());

        std::array<ID3D11SamplerState*, TextureCount> samplers;
        std::transform(m_samplers.begin(), m_samplers.end(), samplers.begin(), [](const auto& sampler) { return sampler.Get(); });
        context->PSSetSamplers(Pbr::ShaderSlots::BaseColor, (UINT)samplers.size(), samplers.data());
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
