// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "D3D12Material.h"

#include "D3D12Resources.h"
#include "D3D12Texture.h"

#include "../PbrMaterial.h"

#include "utilities/d3d12_utils.h"
#include "utilities/throw_helpers.h"

#include <d3dx12.h>

using namespace DirectX;

namespace Pbr
{
    D3D12Material::D3D12Material(Pbr::D3D12Resources const& pbrResources)
    {
        static_assert((sizeof(ConstantBufferData) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");
        m_constantBuffer.Allocate(pbrResources.GetDevice().Get());

        D3D12_DESCRIPTOR_HEAP_DESC textureHeapDesc;
        textureHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        textureHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        textureHeapDesc.NumDescriptors = ShaderSlots::NumMaterialSlots;
        textureHeapDesc.NodeMask = 1;
        XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateDescriptorHeap(&textureHeapDesc, IID_PPV_ARGS(&m_textureHeap)));

        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        samplerHeapDesc.NumDescriptors = ShaderSlots::NumMaterialSlots;
        samplerHeapDesc.NodeMask = 1;
        XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));
    }

    std::shared_ptr<D3D12Material> D3D12Material::Clone(Pbr::D3D12Resources const& pbrResources) const
    {
        auto clone = std::make_shared<D3D12Material>(pbrResources);
        clone->CopyFrom(*this);
        clone->m_textureHeap = m_textureHeap;
        clone->m_samplerHeap = m_samplerHeap;
        return clone;
    }

    /* static */
    std::shared_ptr<D3D12Material> D3D12Material::CreateFlat(D3D12Resources& pbrResources, RGBAColor baseColorFactor,
                                                             float roughnessFactor /* = 1.0f */, float metallicFactor /* = 0.0f */,
                                                             RGBColor emissiveFactor /* = XMFLOAT3(0, 0, 0) */)
    {
        std::shared_ptr<D3D12Material> material = std::make_shared<D3D12Material>(pbrResources);

        if (baseColorFactor.a < 1.0f) {  // Alpha channel
            material->SetAlphaBlended(BlendState::AlphaBlended);
        }

        Pbr::D3D12Material::ConstantBufferData& parameters = material->Parameters();
        parameters.BaseColorFactor = baseColorFactor;
        parameters.EmissiveFactor = emissiveFactor;
        parameters.MetallicFactor = metallicFactor;
        parameters.RoughnessFactor = roughnessFactor;

        D3D12_SAMPLER_DESC defaultSamplerDesc = Pbr::D3D12Texture::DefaultSamplerDesc();
        auto setDefaultTexture = [&](Pbr::ShaderSlots::PSMaterial slot, Pbr::RGBAColor defaultRGBA) {
            auto solidTexture = pbrResources.CreateTypedSolidColorTexture(defaultRGBA);
            material->SetTexture(pbrResources.GetDevice().Get(), slot, solidTexture, &defaultSamplerDesc);
        };

        setDefaultTexture(ShaderSlots::BaseColor, RGBA::White);
        setDefaultTexture(ShaderSlots::MetallicRoughness, RGBA::White);
        // No occlusion.
        setDefaultTexture(ShaderSlots::Occlusion, RGBA::White);
        // Flat normal.
        setDefaultTexture(ShaderSlots::Normal, RGBA::FlatNormal);
        setDefaultTexture(ShaderSlots::Emissive, RGBA::White);

        return material;
    }

    void D3D12Material::SetTexture(_In_ ID3D12Device* device, ShaderSlots::PSMaterial slot, Conformance::D3D12ResourceWithSRVDesc& texture,
                                   _In_opt_ D3D12_SAMPLER_DESC* sampler)
    {
        m_textures[slot] = texture.resource.Get();

        CD3DX12_CPU_DESCRIPTOR_HANDLE textureHandle(m_textureHeap->GetCPUDescriptorHandleForHeapStart());
        auto textureDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        textureHandle.Offset(slot, textureDescriptorSize);
        device->CreateShaderResourceView(texture.resource.Get(), &texture.srvDesc, textureHandle);

        if (sampler) {
            CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(m_samplerHeap->GetCPUDescriptorHandleForHeapStart());
            auto samplerDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            samplerHandle.Offset(slot, samplerDescriptorSize);
            device->CreateSampler(sampler, samplerHandle);
        }
    }

    void D3D12Material::GetDescriptors(_In_ ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destTextureDescriptors,
                                       D3D12_CPU_DESCRIPTOR_HANDLE destSamplerDescriptors)
    {
        device->CopyDescriptorsSimple(ShaderSlots::NumMaterialSlots, destTextureDescriptors,
                                      m_textureHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        device->CopyDescriptorsSimple(ShaderSlots::NumMaterialSlots, destSamplerDescriptors,
                                      m_samplerHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    void D3D12Material::Bind(_In_ ID3D12GraphicsCommandList* directCommandList, D3D12Resources& pbrResources)
    {
        // If the parameters of the constant buffer have changed, update the constant buffer.
        if (m_parametersChanged) {
            m_parametersChanged = false;
            pbrResources.WithCopyCommandList(
                [&](ID3D12GraphicsCommandList* cmdList) { m_constantBuffer.AsyncUpload(cmdList, &m_parameters); });
        }

        directCommandList->SetGraphicsRootConstantBufferView(Pbr::ShaderSlots::ConstantBuffers::Material,
                                                             m_constantBuffer.GetResource()->GetGPUVirtualAddress());

        static_assert(Pbr::ShaderSlots::BaseColor == 0, "BaseColor must be the first slot");
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
