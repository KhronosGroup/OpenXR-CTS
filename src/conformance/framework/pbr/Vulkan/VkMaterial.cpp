// Copyright 2023, The Khronos Group, Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#include "VkMaterial.h"

#include "VkCommon.h"
#include "VkResources.h"
#include "VkTexture.h"

#include "../PbrMaterial.h"

#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

namespace Pbr
{
    VulkanMaterial::VulkanMaterial(Pbr::VulkanResources const& pbrResources)
    {
        static_assert((sizeof(ConstantBufferData) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");
        m_constantBuffer.Init(pbrResources.GetDevice(), pbrResources.GetMemoryAllocator());
        m_constantBuffer.Create(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    std::shared_ptr<VulkanMaterial> VulkanMaterial::Clone(Pbr::VulkanResources const& pbrResources) const
    {
        auto clone = std::make_shared<VulkanMaterial>(pbrResources);
        clone->CopyFrom(*this);
        clone->m_textures = m_textures;
        clone->m_samplers = m_samplers;
        return clone;
    }

    /* static */
    std::shared_ptr<VulkanMaterial> VulkanMaterial::CreateFlat(VulkanResources& pbrResources, RGBAColor baseColorFactor,
                                                               float roughnessFactor /* = 1.0f */, float metallicFactor /* = 0.0f */,
                                                               RGBColor emissiveFactor /* = XMFLOAT3(0, 0, 0) */)
    {
        std::shared_ptr<VulkanMaterial> material = std::make_shared<VulkanMaterial>(pbrResources);

        if (baseColorFactor.a < 1.0f) {  // Alpha channel
            material->SetAlphaBlended(BlendState::AlphaBlended);
        }

        Pbr::VulkanMaterial::ConstantBufferData& parameters = material->Parameters();
        parameters.BaseColorFactor = baseColorFactor;
        parameters.EmissiveFactor = emissiveFactor;
        parameters.MetallicFactor = metallicFactor;
        parameters.RoughnessFactor = roughnessFactor;

        auto defaultSampler = std::make_shared<Conformance::ScopedVkSampler>(Pbr::VulkanTexture::CreateSampler(pbrResources.GetDevice()),
                                                                             pbrResources.GetDevice());
        auto setDefaultTexture = [&](Pbr::ShaderSlots::PSMaterial slot, Pbr::RGBAColor defaultRGBA) {
            material->SetTexture(slot, pbrResources.CreateTypedSolidColorTexture(defaultRGBA), defaultSampler);
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

    void VulkanMaterial::SetTexture(ShaderSlots::PSMaterial slot, std::shared_ptr<VulkanTextureBundle> textureView,
                                    std::shared_ptr<Conformance::ScopedVkSampler> sampler)
    {
        m_textures[slot] = textureView;

        if (sampler) {
            m_samplers[slot] = sampler;
        }
    }

    // Get the material constant buffer for binding
    VkDescriptorBufferInfo VulkanMaterial::GetMaterialConstantBuffer()
    {
        return m_constantBuffer.MakeDescriptor();
    }

    // Get the combined image sampler descriptors for binding
    std::vector<VkDescriptorImageInfo> VulkanMaterial::GetTextureDescriptors()
    {
        std::vector<VkDescriptorImageInfo> ret(TextureCount);

        for (size_t i = 0; i < TextureCount; i++) {
            ret[i].sampler = m_samplers[i]->get();
            ret[i].imageView = m_textures[i]->view.get();
            ret[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        return ret;
    }

    void VulkanMaterial::UpdateBuffer()
    {
        // If the parameters of the constant buffer have changed, update the constant buffer.
        if (m_parametersChanged) {
            m_parametersChanged = false;
            m_constantBuffer.Update({&m_parameters, 1});
        }
    }
}  // namespace Pbr
