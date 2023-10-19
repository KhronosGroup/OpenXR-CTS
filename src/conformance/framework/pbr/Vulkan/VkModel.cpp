// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "VkModel.h"

#include "VkMaterial.h"
#include "VkPrimitive.h"
#include "VkResources.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

#include <algorithm>
#include <assert.h>
#include <memory>
#include <numeric>
#include <stddef.h>

namespace Pbr
{

    void VulkanModel::Render(Pbr::VulkanResources& pbrResources, Conformance::CmdBuffer& directCommandBuffer, VkRenderPass renderPass,
                             VkSampleCountFlagBits sampleCount)
    {
        pbrResources.UpdateBuffer();
        UpdateTransforms(pbrResources);

        auto primitives = GetPrimitives();
        if (m_descriptorSets.size() < primitives.size()) {
            AllocateDescriptorSets(pbrResources, (uint32_t)primitives.size());
        }

        for (size_t i = 0; i < primitives.size(); i++) {
            PrimitiveHandle primitiveHandle = primitives[i];
            VkDescriptorSet descriptorSet = m_descriptorSets[i];
            const Pbr::VulkanPrimitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            primitive.Render(directCommandBuffer, pbrResources, descriptorSet, renderPass, sampleCount,
                             m_modelTransformsStructuredBuffer.MakeDescriptor());
        }

        // Expect the caller to reset other state, but the geometry shader is cleared specially.
        //context->GSSetShader(nullptr, nullptr, 0);
    }

    void VulkanModel::AllocateDescriptorSets(Pbr::VulkanResources& pbrResources, uint32_t numSets)
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

    void VulkanModel::UpdateTransforms(Pbr::VulkanResources& pbrResources)
    {
        const auto& nodes = GetNodes();
        const uint32_t newTotalModifyCount = std::accumulate(nodes.begin(), nodes.end(), 0, [](uint32_t sumChangeCount, const Node& node) {
            return sumChangeCount + node.GetModifyCount();
        });

        // If none of the node transforms have changed, no need to recompute/update the model transform structured buffer.
        if (newTotalModifyCount != TotalModifyCount || m_modelTransformsStructuredBufferInvalid) {
            if (m_modelTransformsStructuredBufferInvalid)  // The structured buffer is reset when a Node is added.
            {
                XrMatrix4x4f identityMatrix;
                XrMatrix4x4f_CreateIdentity(&identityMatrix);  // or better yet poison it
                m_modelTransforms.resize(nodes.size(), identityMatrix);

                // Create/recreate the structured buffer and SRV which holds the node transforms.
                size_t elemSize = sizeof(decltype(m_modelTransforms)::value_type);
                uint32_t count = (uint32_t)(m_modelTransforms.size());
                uint32_t size = (uint32_t)(count * elemSize);

                m_modelTransformsStructuredBuffer.Init(pbrResources.GetDevice(), pbrResources.GetMemoryAllocator());
                m_modelTransformsStructuredBuffer.Create(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

                m_modelTransformsStructuredBufferInvalid = false;
            }

            // Nodes are guaranteed to come after their parents, so each node transform can be multiplied by its parent transform in a single pass.
            assert(nodes.size() == m_modelTransforms.size());
            XrMatrix4x4f identityMatrix;
            XrMatrix4x4f_CreateIdentity(&identityMatrix);
            for (const auto& node : nodes) {
                assert(node.ParentNodeIndex == RootParentNodeIndex || node.ParentNodeIndex < node.Index);
                const XrMatrix4x4f& parentTransform =
                    (node.ParentNodeIndex == RootParentNodeIndex) ? identityMatrix : m_modelTransforms[node.ParentNodeIndex];
                XrMatrix4x4f nodeTransform = node.GetTransform();
                XrMatrix4x4f_Multiply(&m_modelTransforms[node.Index], &parentTransform, &nodeTransform);
            }

            // Update node transform structured buffer.
            m_modelTransformsStructuredBuffer.Update(this->m_modelTransforms);
            TotalModifyCount = newTotalModifyCount;
        }
    }
}  // namespace Pbr
