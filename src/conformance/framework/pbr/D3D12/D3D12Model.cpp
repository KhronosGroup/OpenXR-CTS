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

    void D3D12ModelInstance::Render(Pbr::D3D12Resources& pbrResources, _In_ ID3D12GraphicsCommandList* directCommandList,
                                    DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat,
                                    DirectX::FXMMATRIX modelToWorld)
    {
        XMStoreFloat4x4(&m_modelBuffer.ModelToWorld, XMMatrixTranspose(modelToWorld));
        pbrResources.WithCopyCommandList(
            [&](ID3D12GraphicsCommandList* cmdList) { m_modelConstantBuffer.AsyncUpload(cmdList, &m_modelBuffer); });
        // xxx: why do we copy the transform descriptor to a separate heap, again? is that relevant here?
        pbrResources.BindConstantBufferViews(directCommandList, m_modelConstantBuffer.GetResource()->GetGPUVirtualAddress());

        UpdateTransforms(pbrResources);

        pbrResources.SetTransforms(m_modelTransformsResourceViewHeap->GetCPUDescriptorHandleForHeapStart());

        for (PrimitiveHandle primitiveHandle : GetModel().GetPrimitiveHandles()) {
            const Pbr::D3D12Primitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            if (!IsAnyNodeVisible(primitive.GetNodes()))
                continue;

            primitive.Render(directCommandList, pbrResources, colorRenderTargetFormat, depthRenderTargetFormat);
        }
    }

    D3D12ModelInstance::D3D12ModelInstance(Pbr::D3D12Resources& pbrResources, std::shared_ptr<const Model> model)
        : ModelInstance(std::move(model))
    {
        // Set up the model constant buffer.
        static_assert((sizeof(ModelConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");
        m_modelConstantBuffer.Allocate(pbrResources.GetDevice().Get());

        // Set up the transforms buffer.
        XrMatrix4x4f identityMatrix;
        XrMatrix4x4f_CreateIdentity(&identityMatrix);  // or better yet poison it
        size_t nodeCount = GetModel().GetNodes().size();

        // Create/recreate the structured buffer and SRV which holds the node transforms.
        UINT elemSize = sizeof(XrMatrix4x4f);
        UINT count = (UINT)(nodeCount);
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

            XRC_CHECK_THROW_HRCMD(
                pbrResources.GetDevice()->CreateDescriptorHeap(&transformHeapDesc, IID_PPV_ARGS(&m_modelTransformsResourceViewHeap)));
        }

        pbrResources.GetDevice()->CreateShaderResourceView(
            m_modelTransformsStructuredBuffer.GetResource(), &srvDesc,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(m_modelTransformsResourceViewHeap->GetCPUDescriptorHandleForHeapStart()));
    }

    void D3D12ModelInstance::UpdateTransforms(Pbr::D3D12Resources& pbrResources)
    {
        // If none of the node transforms have changed, no need to recompute/update the model transform structured buffer.
        if (ResolvedTransformsNeedUpdate()) {
            ResolveTransformsAndVisibilities(true);

            // Update node transform structured buffer.
            auto& resolvedTransforms = GetResolvedTransforms();
            pbrResources.WithCopyCommandList([&](ID3D12GraphicsCommandList* cmdList) {
                m_modelTransformsStructuredBuffer.AsyncUpload(cmdList, resolvedTransforms.data(), resolvedTransforms.size());
            });
            MarkResolvedTransformsUpdated();
        }
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
