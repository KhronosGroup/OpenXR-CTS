// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "D3D12Material.h"

#include <d3d12.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <memory>
#include <vector>

namespace Pbr
{
    /// A primitive holds a vertex buffer, index buffer, and a pointer to a PBR material.
    struct D3D12Primitive final
    {
        using Collection = std::vector<D3D12Primitive>;

        D3D12Primitive() = delete;
        D3D12Primitive(UINT indexCount, Conformance::D3D12BufferWithUpload<uint32_t> indexBuffer, UINT vertexCount,
                       Conformance::D3D12BufferWithUpload<Pbr::Vertex> vertexBuffer, std::shared_ptr<D3D12Material> material,
                       std::vector<NodeIndex_t> nodeIndices);
        D3D12Primitive(Pbr::D3D12Resources& pbrResources, ID3D12GraphicsCommandList* copyCommandList,
                       const Pbr::PrimitiveBuilder& primitiveBuilder, const std::shared_ptr<D3D12Material>& material);

        void UpdateBuffers(Pbr::D3D12Resources& pbrResources, ID3D12GraphicsCommandList* copyCommandList,
                           const Pbr::PrimitiveBuilder& primitiveBuilder);

        /// Get the material for the primitive.
        const std::shared_ptr<D3D12Material>& GetMaterial() const
        {
            return m_material;
        }

        /// Replace the material for the primitive
        void SetMaterial(std::shared_ptr<D3D12Material> material)
        {
            m_material = std::move(material);
        }

        /// Get the nodes that the primitive represents
        const std::vector<NodeIndex_t>& GetNodes() const
        {
            return m_nodeIndices;
        }

    protected:
        friend class D3D12ModelInstance;
        void Render(_In_ ID3D12GraphicsCommandList* directCommandList, D3D12Resources& pbrResources, DXGI_FORMAT colorRenderTargetFormat,
                    DXGI_FORMAT depthRenderTargetFormat) const;

        /// The clone shares the vertex and index buffers - they are not cloned
        D3D12Primitive Clone(Pbr::D3D12Resources const& pbrResources) const;

    private:
        UINT m_indexCount;
        Conformance::D3D12BufferWithUpload<uint32_t> m_indexBuffer;
        UINT m_vertexCount;
        Conformance::D3D12BufferWithUpload<Pbr::Vertex> m_vertexBuffer;
        std::shared_ptr<D3D12Material> m_material;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
        std::vector<NodeIndex_t> m_nodeIndices;
    };
}  // namespace Pbr
