// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalPrimitive.h"
#include "MetalResources.h"
#include "MetalMaterial.h"

namespace
{

    uint32_t GetPbrVertexByteSize(size_t size)
    {
        return (uint32_t)(sizeof(decltype(Pbr::PrimitiveBuilder::Vertices)::value_type) * size);
    }
    uint32_t GetPbrIndexByteSize(size_t size)
    {
        return (uint32_t)(sizeof(decltype(Pbr::PrimitiveBuilder::Indices)::value_type) * size);
    }

}  // namespace

namespace Pbr
{

    MetalPrimitive::MetalPrimitive(uint32_t indexCount, MTL::Buffer* indexBuffer, MTL::Buffer* vertexBuffer,
                                   std::shared_ptr<MetalMaterial> material, std::vector<NodeIndex_t> nodeIndices)
        : m_indexCount(indexCount)
        , m_indexBuffer(NS::RetainPtr(indexBuffer))
        , m_vertexBuffer(NS::RetainPtr(vertexBuffer))
        , m_material(std::move(material))
        , m_nodeIndices(std::move(nodeIndices))
    {
    }

    MetalPrimitive::MetalPrimitive(const MetalResources& pbrResources, const Pbr::PrimitiveBuilder& primitiveBuilder,
                                   const std::shared_ptr<MetalMaterial>& material, bool /*updatableBuffers*/)
        : MetalPrimitive((uint32_t)primitiveBuilder.Indices.size(), nullptr, nullptr, std::move(material),
                         primitiveBuilder.NodeIndicesVector())
    {
        m_indexBuffer = NS::TransferPtr(pbrResources.GetDevice()->newBuffer(
            primitiveBuilder.Indices.data(), GetPbrIndexByteSize(primitiveBuilder.Indices.size()), MTL::ResourceStorageModeManaged));
        m_vertexBuffer = NS::TransferPtr(pbrResources.GetDevice()->newBuffer(
            primitiveBuilder.Vertices.data(), GetPbrVertexByteSize(primitiveBuilder.Vertices.size()), MTL::ResourceStorageModeManaged));
    }

    MetalPrimitive MetalPrimitive::Clone(const MetalResources& pbrResources) const
    {
        return MetalPrimitive(m_indexCount, m_indexBuffer.get(), m_vertexBuffer.get(), m_material->Clone(pbrResources), m_nodeIndices);
    }

    void MetalPrimitive::UpdateBuffers(MTL::Device* device, const Pbr::PrimitiveBuilder& primitiveBuilder)
    {
        // Update vertex buffer.
        {
            uint32_t required_vb_size = GetPbrVertexByteSize(primitiveBuilder.Vertices.size());
            if (required_vb_size <= m_vertexBuffer->length()) {
                memcpy(m_vertexBuffer->contents(), primitiveBuilder.Vertices.data(), required_vb_size);
                m_vertexBuffer->didModifyRange(NS::Range(0, required_vb_size));
            }
            else {
                m_vertexBuffer =
                    NS::TransferPtr(device->newBuffer(primitiveBuilder.Vertices.data(), required_vb_size, MTL::ResourceStorageModeManaged));
            }
        }

        // Update index buffer.
        {
            uint32_t required_ib_size = GetPbrIndexByteSize(primitiveBuilder.Indices.size());
            if (required_ib_size <= m_indexBuffer->length()) {
                memcpy(m_indexBuffer->contents(), primitiveBuilder.Indices.data(), required_ib_size);
                m_indexBuffer->didModifyRange(NS::Range(0, required_ib_size));
            }
            else {
                m_indexBuffer =
                    NS::TransferPtr(device->newBuffer(primitiveBuilder.Indices.data(), required_ib_size, MTL::ResourceStorageModeManaged));
            }
        }

        m_indexCount = (uint32_t)primitiveBuilder.Indices.size();
    }

    void MetalPrimitive::Render(Pbr::MetalResources const& pbrResources, MTL::RenderCommandEncoder* renderCommandEncoder,
                                MTL::PixelFormat colorRenderTargetFormat, MTL::PixelFormat depthRenderTargetFormat) const
    {
        renderCommandEncoder->pushDebugGroup(MTLSTR("MetalPrimitive::Render"));

        BlendState blendState = GetMaterial()->GetAlphaBlended();
        MetalPipelineStateBundle pipelineStateBundle =
            pbrResources.GetOrCreatePipelineState(colorRenderTargetFormat, depthRenderTargetFormat, blendState);
        renderCommandEncoder->setRenderPipelineState(pipelineStateBundle.m_renderPipelineState.get());
        renderCommandEncoder->setDepthStencilState(pipelineStateBundle.m_depthStencilState.get());
        m_material->Bind(renderCommandEncoder, pbrResources);
        renderCommandEncoder->setVertexBuffer(m_vertexBuffer.get(), 0, 4);  // matches ConstantBuffers.VertexData in PbrShader.metal
        renderCommandEncoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, m_indexCount, MTL::IndexTypeUInt32, m_indexBuffer.get(), 0);

        renderCommandEncoder->popDebugGroup();
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_METAL)
