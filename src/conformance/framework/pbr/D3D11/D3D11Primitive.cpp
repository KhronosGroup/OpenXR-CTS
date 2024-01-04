// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "D3D11Primitive.h"

#include "D3D11Resources.h"

#include "../PbrCommon.h"

#include "utilities/throw_helpers.h"

using namespace DirectX;

namespace
{
    UINT GetPbrVertexByteSize(size_t size)
    {
        return (UINT)(sizeof(decltype(Pbr::PrimitiveBuilder::Vertices)::value_type) * size);
    }
    UINT GetPbrIndexByteSize(size_t size)
    {
        return (UINT)(sizeof(decltype(Pbr::PrimitiveBuilder::Indices)::value_type) * size);
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> CreateVertexBuffer(_In_ ID3D11Device* device, const Pbr::PrimitiveBuilder& primitiveBuilder,
                                                            bool updatableBuffers)
    {
        // Create Vertex Buffer
        D3D11_BUFFER_DESC desc{};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = GetPbrVertexByteSize(primitiveBuilder.Vertices.size());
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        if (updatableBuffers) {
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
        }

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = primitiveBuilder.Vertices.data();

        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        XRC_CHECK_THROW_HRCMD(device->CreateBuffer(&desc, &initData, vertexBuffer.ReleaseAndGetAddressOf()));
        return vertexBuffer;
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> CreateIndexBuffer(_In_ ID3D11Device* device, const Pbr::PrimitiveBuilder& primitiveBuilder,
                                                           bool updatableBuffers)
    {
        // Create Index Buffer
        D3D11_BUFFER_DESC desc{};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = GetPbrIndexByteSize(primitiveBuilder.Indices.size());
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        if (updatableBuffers) {
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
        }

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = primitiveBuilder.Indices.data();

        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        XRC_CHECK_THROW_HRCMD(device->CreateBuffer(&desc, &initData, indexBuffer.ReleaseAndGetAddressOf()));
        return indexBuffer;
    }
}  // namespace

namespace Pbr
{
    D3D11Primitive::D3D11Primitive(UINT indexCount, Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer,
                                   Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer, std::shared_ptr<D3D11Material> material)
        : m_indexCount(indexCount)
        , m_indexBuffer(std::move(indexBuffer))
        , m_vertexBuffer(std::move(vertexBuffer))
        , m_material(std::move(material))
    {
    }

    D3D11Primitive::D3D11Primitive(Pbr::D3D11Resources const& pbrResources, const Pbr::PrimitiveBuilder& primitiveBuilder,
                                   const std::shared_ptr<Pbr::D3D11Material>& material, bool updatableBuffers)
        : D3D11Primitive((UINT)primitiveBuilder.Indices.size(),
                         CreateIndexBuffer(pbrResources.GetDevice().Get(), primitiveBuilder, updatableBuffers),
                         CreateVertexBuffer(pbrResources.GetDevice().Get(), primitiveBuilder, updatableBuffers), std::move(material))
    {
    }

    D3D11Primitive D3D11Primitive::Clone(Pbr::D3D11Resources const& pbrResources) const
    {
        return D3D11Primitive(m_indexCount, m_indexBuffer, m_vertexBuffer, m_material->Clone(pbrResources));
    }

    void D3D11Primitive::UpdateBuffers(_In_ ID3D11Device* device, _In_ ID3D11DeviceContext* context,
                                       const Pbr::PrimitiveBuilder& primitiveBuilder)
    {
        // Update vertex buffer.
        {
            D3D11_BUFFER_DESC vertDesc;
            m_vertexBuffer->GetDesc(&vertDesc);

            UINT requiredSize = GetPbrVertexByteSize(primitiveBuilder.Vertices.size());
            if (vertDesc.ByteWidth >= requiredSize) {
                context->UpdateSubresource(m_vertexBuffer.Get(), 0, nullptr, primitiveBuilder.Vertices.data(), requiredSize, requiredSize);
            }
            else {
                m_vertexBuffer = CreateVertexBuffer(device, primitiveBuilder, true);
            }
        }

        // Update index buffer.
        {
            D3D11_BUFFER_DESC idxDesc;
            m_indexBuffer->GetDesc(&idxDesc);

            UINT requiredSize = GetPbrIndexByteSize(primitiveBuilder.Indices.size());
            if (idxDesc.ByteWidth >= requiredSize) {
                context->UpdateSubresource(m_indexBuffer.Get(), 0, nullptr, primitiveBuilder.Indices.data(), requiredSize, requiredSize);
            }
            else {
                m_indexBuffer = CreateIndexBuffer(device, primitiveBuilder, true);
            }

            m_indexCount = (UINT)primitiveBuilder.Indices.size();
        }
    }

    void D3D11Primitive::Render(_In_ ID3D11DeviceContext* context) const
    {
        const UINT stride = sizeof(Pbr::Vertex);
        const UINT offset = 0;
        ID3D11Buffer* const vertexBuffers[] = {m_vertexBuffer.Get()};
        context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
