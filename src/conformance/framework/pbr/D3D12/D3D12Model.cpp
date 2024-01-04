// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "D3D12Model.h"

#include "D3D12Primitive.h"
#include "D3D12Resources.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

#include "utilities/d3d12_utils.h"
#include "utilities/throw_helpers.h"

#include <d3dx12.h>

#include <numeric>

namespace Pbr
{

    void D3D12Model::Render(Pbr::D3D12Resources& pbrResources, _In_ ID3D12GraphicsCommandList* directCommandList,
                            DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat)
    {
        UpdateTransforms(pbrResources);

        pbrResources.SetTransforms(m_modelTransformsResourceViewHeap->GetCPUDescriptorHandleForHeapStart());

        for (PrimitiveHandle primitiveHandle : GetPrimitives()) {
            const Pbr::D3D12Primitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            primitive.Render(directCommandList, pbrResources, colorRenderTargetFormat, depthRenderTargetFormat);
        }

        // Expect the caller to reset other state, but the geometry shader is cleared specially.
        //context->GSSetShader(nullptr, nullptr, 0);
    }

    void D3D12Model::UpdateTransforms(Pbr::D3D12Resources& pbrResources)
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
                UINT elemSize = sizeof(decltype(m_modelTransforms)::value_type);
                UINT count = (UINT)(m_modelTransforms.size());
                UINT size = (UINT)(count * elemSize);

                m_modelTransformsStructuredBuffer = Conformance::D3D12BufferWithUpload<XrMatrix4x4f>(pbrResources.GetDevice().Get(), size);

                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Buffer.NumElements = count;
                srvDesc.Buffer.StructureByteStride = elemSize;

                if (m_modelTransformsResourceViewHeap == nullptr) {
                    D3D12_DESCRIPTOR_HEAP_DESC transformHeapDesc;
                    transformHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                    transformHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                    transformHeapDesc.NumDescriptors = ShaderSlots::NumTextures;
                    transformHeapDesc.NodeMask = 1;

                    XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateDescriptorHeap(&transformHeapDesc,
                                                                                         IID_PPV_ARGS(&m_modelTransformsResourceViewHeap)));
                }

                pbrResources.GetDevice()->CreateShaderResourceView(
                    m_modelTransformsStructuredBuffer.GetResource(), &srvDesc,
                    CD3DX12_CPU_DESCRIPTOR_HANDLE(m_modelTransformsResourceViewHeap->GetCPUDescriptorHandleForHeapStart()));

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
                XrMatrix4x4f nodeTransformTranspose;
                XrMatrix4x4f_Transpose(&nodeTransformTranspose, &nodeTransform);
                XrMatrix4x4f_Multiply(&m_modelTransforms[node.Index], &nodeTransformTranspose, &parentTransform);
            }

            // Update node transform structured buffer.
            pbrResources.WithCopyCommandList([&](ID3D12GraphicsCommandList* cmdList) {
                m_modelTransformsStructuredBuffer.AsyncUpload(cmdList, this->m_modelTransforms.data(), this->m_modelTransforms.size());
            });
            TotalModifyCount = newTotalModifyCount;
        }
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
