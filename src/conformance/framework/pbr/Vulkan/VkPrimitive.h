// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "VkMaterial.h"

#include "../PbrCommon.h"

#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

#include <nonstd/span.hpp>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace Pbr
{
    struct VulkanMaterial;
    struct VulkanResources;

    /// A primitive holds a vertex buffer, index buffer, and a pointer to a PBR material.
    struct VulkanPrimitive final
    {
        using Collection = std::vector<VulkanPrimitive>;

        VulkanPrimitive() = delete;
        VulkanPrimitive(Conformance::VertexBuffer<Pbr::Vertex, uint32_t>&& vertexAndIndexBuffer, std::shared_ptr<VulkanMaterial> material);
        VulkanPrimitive(Pbr::VulkanResources const& pbrResources, const Pbr::PrimitiveBuilder& primitiveBuilder,
                        const std::shared_ptr<Pbr::VulkanMaterial>& material);

        /// Get the material for the primitive.
        const std::shared_ptr<VulkanMaterial>& GetMaterial() const
        {
            return m_material;
        }

        /// Replace the material for the primitive
        void SetMaterial(std::shared_ptr<VulkanMaterial> material)
        {
            m_material = std::move(material);
        }

    protected:
        friend class VulkanModelInstance;
        void Render(Conformance::CmdBuffer& directCommandBuffer, VulkanResources& pbrResources, VkDescriptorSet descriptorSet,
                    VkRenderPass renderPass, VkSampleCountFlagBits sampleCount, VkDescriptorBufferInfo modelConstantBuffer,
                    VkDescriptorBufferInfo transformBuffer) const;

        /// The clone shares the vertex and index buffers - they are not cloned
        VulkanPrimitive Clone(Pbr::VulkanResources const& pbrResources) const;

    private:
        Conformance::VertexBuffer<Vertex, uint32_t> m_vertexAndIndexBuffer;
        std::shared_ptr<VulkanMaterial> m_material;
    };
}  // namespace Pbr
