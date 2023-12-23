// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "D3D12Primitive.h"

#include "D3D12Resources.h"

#include "../PbrCommon.h"

#include "utilities/array_size.h"
#include "utilities/throw_helpers.h"

#include <d3dx12.h>

using namespace DirectX;

namespace Pbr
{
    D3D12Primitive::D3D12Primitive(UINT indexCount, Conformance::D3D12BufferWithUpload<uint32_t> indexBuffer, UINT vertexCount,
                                   Conformance::D3D12BufferWithUpload<Pbr::Vertex> vertexBuffer, std::shared_ptr<D3D12Material> material)
        : m_indexCount(indexCount)
        , m_indexBuffer(std::move(indexBuffer))
        , m_vertexCount(vertexCount)
        , m_vertexBuffer(std::move(vertexBuffer))
        , m_material(std::move(material))
    {
    }

    D3D12Primitive::D3D12Primitive(Pbr::D3D12Resources& pbrResources, const Pbr::PrimitiveBuilder& primitiveBuilder,
                                   const std::shared_ptr<Pbr::D3D12Material>& material)
        : D3D12Primitive((UINT)primitiveBuilder.Indices.size(), {}, (UINT)primitiveBuilder.Vertices.size(), {}, std::move(material))
    {
        m_indexBuffer.Allocate(pbrResources.GetDevice().Get(), primitiveBuilder.Indices.size());
        m_vertexBuffer.Allocate(pbrResources.GetDevice().Get(), primitiveBuilder.Vertices.size());
        UpdateBuffers(pbrResources, primitiveBuilder);

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        srvHeapDesc.NumDescriptors = ShaderSlots::NumVSResourceViews + ShaderSlots::NumTextures;
        srvHeapDesc.NodeMask = 1;
        XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        samplerHeapDesc.NumDescriptors = ShaderSlots::NumSamplers;
        samplerHeapDesc.NodeMask = 1;
        XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));
    }

    D3D12Primitive D3D12Primitive::Clone(Pbr::D3D12Resources const& pbrResources) const
    {
        return D3D12Primitive(m_indexCount, m_indexBuffer, m_vertexCount, m_vertexBuffer, m_material->Clone(pbrResources));
    }

    void D3D12Primitive::UpdateBuffers(Pbr::D3D12Resources& pbrResources, const Pbr::PrimitiveBuilder& primitiveBuilder)
    {
        // Update vertex buffer.
        {
            size_t elemCount = primitiveBuilder.Vertices.size();
            if (m_vertexBuffer.Fits(elemCount)) {

                pbrResources.WithCopyCommandList([&](ID3D12GraphicsCommandList* cmdList) {
                    m_vertexBuffer.AsyncUpload(cmdList, primitiveBuilder.Vertices.data(), elemCount);
                });
            }
            else {
                m_vertexBuffer.Allocate(pbrResources.GetDevice().Get(), elemCount);
            }
        }

        // Update index buffer.
        {
            size_t elemCount = primitiveBuilder.Indices.size();
            if (m_indexBuffer.Fits(elemCount)) {

                pbrResources.WithCopyCommandList([&](ID3D12GraphicsCommandList* cmdList) {
                    m_indexBuffer.AsyncUpload(cmdList, primitiveBuilder.Indices.data(), elemCount);
                });
            }
            else {
                m_indexBuffer.Allocate(pbrResources.GetDevice().Get(), elemCount);
            }

            m_indexCount = (UINT)primitiveBuilder.Indices.size();
        }
    }

    void D3D12Primitive::Render(_In_ ID3D12GraphicsCommandList* directCommandList, D3D12Resources& pbrResources,
                                DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat) const
    {
        GetMaterial()->Bind(directCommandList, pbrResources);
        pbrResources.BindDescriptorHeaps(directCommandList, m_srvHeap.Get(), m_samplerHeap.Get());

        UINT srvDescriptorSize = pbrResources.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        UINT samplerDescriptorSize = pbrResources.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
        CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(m_samplerHeap->GetCPUDescriptorHandleForHeapStart());

        // vertex shader resource views
        pbrResources.GetTransforms(srvHandle);
        srvHandle.Offset(ShaderSlots::NumVSResourceViews, srvDescriptorSize);

        GetMaterial()->GetDescriptors(pbrResources.GetDevice().Get(), srvHandle, samplerHandle);
        srvHandle.Offset(ShaderSlots::NumMaterialSlots, srvDescriptorSize);
        samplerHandle.Offset(ShaderSlots::NumMaterialSlots, samplerDescriptorSize);

        pbrResources.GetGlobalTexturesAndSamplers(srvHandle, samplerHandle);

        BlendState blendState = GetMaterial()->GetAlphaBlended();
        DoubleSided doubleSided = GetMaterial()->GetDoubleSided();

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState =
            pbrResources.GetOrCreatePipelineState(colorRenderTargetFormat, depthRenderTargetFormat, blendState, doubleSided);

        const UINT vertexStride = sizeof(Pbr::Vertex);
        const UINT indexStride = sizeof(uint32_t);
        const D3D12_VERTEX_BUFFER_VIEW vertexBufferView[] = {
            {m_vertexBuffer.GetResource()->GetGPUVirtualAddress(), m_vertexCount * vertexStride, vertexStride}};
        directCommandList->IASetVertexBuffers(0, (UINT)Conformance::ArraySize(vertexBufferView), vertexBufferView);
        D3D12_INDEX_BUFFER_VIEW indexBufferView{m_indexBuffer.GetResource()->GetGPUVirtualAddress(), m_indexCount * indexStride,
                                                DXGI_FORMAT_R32_UINT};
        directCommandList->IASetIndexBuffer(&indexBufferView);
        directCommandList->SetPipelineState(pipelineState.Get());
        directCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        directCommandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
