// Copyright 2023-2026 The Khronos Group Inc.
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

#include "PbrCommon.h"
#include "PbrSharedState.h"

#include "utilities/vulkan_utils.h"

#include <array>
#include <cstdint>
#include <stddef.h>

namespace
{
    void UpdateVertexBuffer(Conformance::VertexBuffer<Pbr::Vertex, uint32_t>& buffer, nonstd::span<const uint32_t> idx,
                            nonstd::span<const Pbr::Vertex> vtx)
    {
        if (buffer.count.idx < idx.size() || buffer.count.vtx < vtx.size()) {
            buffer.Deallocate();
            buffer.Create((uint32_t)idx.size(), (uint32_t)vtx.size());
        }
        buffer.UpdateIndices(idx);
        buffer.UpdateVertices(vtx);
    }
    Conformance::VertexBuffer<Pbr::Vertex, uint32_t> CreateVertexBuffer(VkDevice device,
                                                                        const Conformance::MemoryAllocator& memoryAllocator,
                                                                        nonstd::span<const uint32_t> idx,
                                                                        nonstd::span<const Pbr::Vertex> vtx)
    {
        // Create Vertex Buffer
        Conformance::VertexBuffer<Pbr::Vertex, uint32_t> buffer;
        std::vector<VkVertexInputAttributeDescription> attr{};  // unsure why this is empty
        buffer.Init(device, &memoryAllocator, attr);
        if (!idx.empty()) {
            UpdateVertexBuffer(buffer, idx, vtx);
        }
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
        : VulkanPrimitive(CreateVertexBuffer(pbrResources.GetDevice(), pbrResources.GetMemoryAllocator(),  //
                                             primitiveBuilder.Indices, primitiveBuilder.Vertices),
                          std::move(material), primitiveBuilder.NodeIndicesVector())
    {
    }

    void VulkanPrimitive::UpdateBuffers(span<const uint32_t> idx, span<const Pbr::Vertex> vtx)
    {
        if (idx.empty()) {
            m_vertexAndIndexBuffer.Deallocate();
            return;
        }

        UpdateVertexBuffer(m_vertexAndIndexBuffer, idx, vtx);
    }

    void VulkanPrimitive::Render(Conformance::CmdBuffer& directCommandBuffer, VulkanResources& pbrResources, VkDescriptorSet descriptorSet,
                                 VkRenderPass renderPass, VkSampleCountFlagBits sampleCount, VkDescriptorBufferInfo modelConstantBuffer,
                                 VkDescriptorBufferInfo transformBuffer) const
    {
        if (m_vertexAndIndexBuffer.count.idx == 0) {
            return;
        }

        GetMaterial()->UpdateBuffer();

        auto materialConstantBuffer = GetMaterial()->GetMaterialConstantBuffer();
        auto materialTextures = GetMaterial()->GetTextureDescriptors();
        std::unique_ptr<VulkanWriteDescriptorSets> wds = pbrResources.BuildWriteDescriptorSets(
            modelConstantBuffer, materialConstantBuffer, transformBuffer, materialTextures, descriptorSet);

        vkUpdateDescriptorSets(pbrResources.GetDevice(), static_cast<uint32_t>(wds->writeDescriptorSets.size()),
                               wds->writeDescriptorSets.data(), 0, nullptr);

        Shader shader = GetMaterial()->GetShader();
        BlendState blendState = GetMaterial()->GetAlphaBlended();
        DoubleSided doubleSided = GetMaterial()->GetDoubleSided();

        pbrResources.SetFillMode(GetMaterial()->GetFillMode());
        Conformance::Pipeline& pipeline = pbrResources.GetOrCreatePipeline(renderPass, sampleCount, shader, blendState, doubleSided);

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
