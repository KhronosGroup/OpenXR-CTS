// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "MetalMaterial.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <memory>
#include <vector>

namespace Pbr
{

    struct MetalResources;
    struct MetalMaterial;
    struct PrimitiveBuilder;

    /// A primitive holds a vertex buffer, index buffer, and a pointer to a PBR material.
    struct MetalPrimitive final
    {
        using Collection = std::vector<MetalPrimitive>;

        MetalPrimitive() = delete;
        MetalPrimitive(uint32_t indexCount, MTL::Buffer* indexBuffer, MTL::Buffer* vertexBuffer, std::shared_ptr<MetalMaterial> material,
                       std::vector<NodeIndex_t> nodeIndices);
        MetalPrimitive(const MetalResources& pbrResources, const Pbr::PrimitiveBuilder& primitiveBuilder,
                       const std::shared_ptr<MetalMaterial>& material, bool updatableBuffers = false);

        void UpdateBuffers(MTL::Device* device, const Pbr::PrimitiveBuilder& primitiveBuilder);

        /// Get the material for the primitive.
        const std::shared_ptr<MetalMaterial>& GetMaterial() const
        {
            return m_material;
        }

        /// Replace the material for the primitive
        void SetMaterial(std::shared_ptr<MetalMaterial> material)
        {
            m_material = std::move(material);
        }

        /// Get the nodes that the primitive represents
        const std::vector<NodeIndex_t>& GetNodes() const
        {
            return m_nodeIndices;
        }

    protected:
        friend class MetalModelInstance;
        void Render(Pbr::MetalResources const& pbrResources, MTL::RenderCommandEncoder* renderCommandEncoder,
                    MTL::PixelFormat colorRenderTargetFormat, MTL::PixelFormat depthRenderTargetFormat) const;
        MetalPrimitive Clone(const MetalResources& pbrResources) const;

    private:
        uint32_t m_indexCount;
        NS::SharedPtr<MTL::Buffer> m_indexBuffer;
        NS::SharedPtr<MTL::Buffer> m_vertexBuffer;
        std::shared_ptr<MetalMaterial> m_material;
        std::vector<NodeIndex_t> m_nodeIndices;
    };
}  // namespace Pbr
