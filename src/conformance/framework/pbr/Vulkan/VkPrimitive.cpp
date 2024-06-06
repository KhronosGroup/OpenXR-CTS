// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_VULKAN)

#include "VkPrimitive.h"

#include "VkMaterial.h"
#include "VkResources.h"

#include "../PbrCommon.h"
#include "../PbrSharedState.h"

#include "utilities/vulkan_utils.h"

#include <array>
#include <cstdint>
#include <stddef.h>

namespace
{
    Conformance::VertexBuffer<Pbr::Vertex, uint32_t> CreateVertexBuffer(VkDevice device,
                                                                        const Conformance::MemoryAllocator& memoryAllocator,
                                                                        const Pbr::PrimitiveBuilder& primitiveBuilder)
    {
        // Create Vertex Buffer
        Conformance::VertexBuffer<Pbr::Vertex, uint32_t> buffer;
        std::vector<VkVertexInputAttributeDescription> attr{};
        buffer.Init(device, &memoryAllocator, attr);
        buffer.Create((uint32_t)primitiveBuilder.Indices.size(), (uint32_t)primitiveBuilder.Vertices.size());
        buffer.UpdateIndices(primitiveBuilder.Indices);
        buffer.UpdateVertices(primitiveBuilder.Vertices);
        return buffer;
    }
}  // namespace

namespace Pbr
{
    VulkanPrimitive::VulkanPrimitive(Conformance::VertexBuffer<Pbr::Vertex, uint32_t>&& vertexAndIndexBuffer,
                                     std::shared_ptr<VulkanMaterial> material, std::vector<NodeIndex_t> nodeIndices)
        : m_vertexAndIndexBuffer(std::move(vertexAndIndexBuffer)), m_material(std::move(material)), m_nodeIndices(std::move(nodeIndices))
    {
    }

    VulkanPrimitive::VulkanPrimitive(Pbr::VulkanResources const& pbrResources, const Pbr::PrimitiveBuilder& primitiveBuilder,
                                     const std::shared_ptr<Pbr::VulkanMaterial>& material)
        : VulkanPrimitive(CreateVertexBuffer(pbrResources.GetDevice(), pbrResources.GetMemoryAllocator(), primitiveBuilder),
                          std::move(material), primitiveBuilder.NodeIndicesVector())
    {
    }

    void VulkanPrimitive::Render(Conformance::CmdBuffer& directCommandBuffer, VulkanResources& pbrResources, VkDescriptorSet descriptorSet,
                                 VkRenderPass renderPass, VkSampleCountFlagBits sampleCount, VkDescriptorBufferInfo modelConstantBuffer,
                                 VkDescriptorBufferInfo transformBuffer) const
    {
        GetMaterial()->UpdateBuffer();

        auto materialConstantBuffer = GetMaterial()->GetMaterialConstantBuffer();
        auto materialTextures = GetMaterial()->GetTextureDescriptors();
        std::unique_ptr<VulkanWriteDescriptorSets> wds = pbrResources.BuildWriteDescriptorSets(
            modelConstantBuffer, materialConstantBuffer, transformBuffer, materialTextures, descriptorSet);

        vkUpdateDescriptorSets(pbrResources.GetDevice(), static_cast<uint32_t>(wds->writeDescriptorSets.size()),
                               wds->writeDescriptorSets.data(), 0, NULL);

        BlendState blendState = GetMaterial()->GetAlphaBlended();
        DoubleSided doubleSided = GetMaterial()->GetDoubleSided();

        Conformance::Pipeline& pipeline = pbrResources.GetOrCreatePipeline(renderPass, sampleCount, blendState, doubleSided);

        vkCmdBindDescriptorSets(directCommandBuffer.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pbrResources.GetPipelineLayout(), 0, 1,
                                &descriptorSet, 0, nullptr);
        vkCmdBindPipeline(directCommandBuffer.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipe);

        const VkDeviceSize vertexOffset = 0;
        const VkDeviceSize indexOffset = 0;

        // Bind index and vertex buffers
        vkCmdBindIndexBuffer(directCommandBuffer.buf, m_vertexAndIndexBuffer.idx.buf, indexOffset, VK_INDEX_TYPE_UINT32);

        CHECKPOINT();

        vkCmdBindVertexBuffers(directCommandBuffer.buf, 0, 1, &m_vertexAndIndexBuffer.vtx.buf, &vertexOffset);

        CHECKPOINT();

        vkCmdDrawIndexed(directCommandBuffer.buf, m_vertexAndIndexBuffer.count.idx, 1, 0, 0, 0);

        CHECKPOINT();
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
