// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_VULKAN)

#include "VkModel.h"

#include "VkMaterial.h"
#include "VkPrimitive.h"
#include "VkResources.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

#include <memory>
#include <stddef.h>

namespace Pbr
{

    void VulkanModelInstance::Render(Pbr::VulkanResources& pbrResources, Conformance::CmdBuffer& directCommandBuffer,
                                     VkRenderPass renderPass, VkSampleCountFlagBits sampleCount, XrMatrix4x4f modelToWorld)
    {
        pbrResources.UpdateBuffer();
        m_modelBuffer.ModelToWorld = modelToWorld;
        m_modelConstantBuffer.Update({&m_modelBuffer, 1});
        UpdateTransforms(pbrResources);

        auto& primitiveHandles = GetModel().GetPrimitiveHandles();
        if (m_descriptorSets.size() < primitiveHandles.size()) {
            AllocateDescriptorSets(pbrResources, (uint32_t)primitiveHandles.size());
        }

        for (size_t i = 0; i < primitiveHandles.size(); i++) {
            PrimitiveHandle primitiveHandle = primitiveHandles[i];
            VkDescriptorSet descriptorSet = m_descriptorSets[i];
            const Pbr::VulkanPrimitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            if (!IsAnyNodeVisible(primitive.GetNodes()))
                continue;

            primitive.Render(directCommandBuffer, pbrResources, descriptorSet, renderPass, sampleCount,
                             m_modelConstantBuffer.MakeDescriptor(), m_modelTransformsStructuredBuffer.MakeDescriptor());
        }
    }

    void VulkanModelInstance::AllocateDescriptorSets(Pbr::VulkanResources& pbrResources, uint32_t numSets)
    {
        m_descriptorPool.adopt(pbrResources.MakeDescriptorPool(numSets), pbrResources.GetDevice());

        auto descriptorSetLayout = pbrResources.GetDescriptorSetLayout();

        std::vector<VkDescriptorSetLayout> layouts(numSets, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool.get();
        allocInfo.descriptorSetCount = numSets;
        allocInfo.pSetLayouts = layouts.data();

        m_descriptorSets.resize(numSets);
        XRC_CHECK_THROW_VKCMD(vkAllocateDescriptorSets(pbrResources.GetDevice(), &allocInfo, m_descriptorSets.data()));
    }
    VulkanModelInstance::VulkanModelInstance(Pbr::VulkanResources& pbrResources, std::shared_ptr<const Model> model)
        : ModelInstance(std::move(model))
    {
        // Set up the model constant buffer.
        m_modelConstantBuffer.Init(pbrResources.GetDevice(), pbrResources.GetMemoryAllocator());
        m_modelConstantBuffer.Create(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        XRC_CHECK_THROW_VKCMD(
            pbrResources.GetDebugNamer().SetName(VK_OBJECT_TYPE_BUFFER, (uint64_t)m_modelConstantBuffer.buf, "CTS model constant buffer"));

        size_t nodeCount = GetModel().GetNodes().size();

        // Create/recreate the structured buffer and SRV which holds the node transforms.
        size_t elemSize = sizeof(XrMatrix4x4f);
        uint32_t count = (uint32_t)(nodeCount);
        uint32_t size = (uint32_t)(count * elemSize);

        m_modelTransformsStructuredBuffer.Init(pbrResources.GetDevice(), pbrResources.GetMemoryAllocator());
        m_modelTransformsStructuredBuffer.Create(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        XRC_CHECK_THROW_VKCMD(pbrResources.GetDebugNamer().SetName(VK_OBJECT_TYPE_BUFFER, (uint64_t)m_modelTransformsStructuredBuffer.buf,
                                                                   "CTS model transform buffer"));
    }
    void VulkanModelInstance::UpdateTransforms(Pbr::VulkanResources& /* pbrResources */)
    {
        // If none of the node transforms have changed, no need to recompute/update the model transform structured buffer.
        if (ResolvedTransformsNeedUpdate()) {
            ResolveTransformsAndVisibilities(false);

            // Update node transform structured buffer.
            m_modelTransformsStructuredBuffer.Update(GetResolvedTransforms());
            MarkResolvedTransformsUpdated();
        }
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
