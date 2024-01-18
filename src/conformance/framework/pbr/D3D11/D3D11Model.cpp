// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "D3D11Model.h"

#include "D3D11Material.h"
#include "D3D11Primitive.h"
#include "D3D11Resources.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

#include "utilities/throw_helpers.h"

#include <numeric>

namespace Pbr
{
    void D3D11ModelInstance::Render(Pbr::D3D11Resources const& pbrResources, _In_ ID3D11DeviceContext* context,
                                    DirectX::FXMMATRIX modelToWorld)
    {
        XMStoreFloat4x4(&m_modelBuffer.ModelToWorld, XMMatrixTranspose(modelToWorld));
        context->UpdateSubresource(m_modelConstantBuffer.Get(), 0, nullptr, &m_modelBuffer, 0, 0);
        pbrResources.BindConstantBuffers(context, m_modelConstantBuffer.Get());

        UpdateTransforms(pbrResources, context);

        ID3D11ShaderResourceView* vsShaderResources[] = {m_modelTransformsResourceView.Get()};
        context->VSSetShaderResources(Pbr::ShaderSlots::Transforms, _countof(vsShaderResources), vsShaderResources);

        for (PrimitiveHandle primitiveHandle : GetModel().GetPrimitiveHandles()) {
            const Pbr::D3D11Primitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            primitive.GetMaterial()->Bind(context, pbrResources);
            primitive.Render(context);
        }
    }

    D3D11ModelInstance::D3D11ModelInstance(Pbr::D3D11Resources& pbrResources, std::shared_ptr<const Model> model)
        : ModelInstance(std::move(model))
    {
        // Set up the model constant buffer.
        const CD3D11_BUFFER_DESC modelConstantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        XRC_CHECK_THROW_HRCMD(
            pbrResources.GetDevice()->CreateBuffer(&modelConstantBufferDesc, nullptr, m_modelConstantBuffer.ReleaseAndGetAddressOf()));

        // Set up the transforms buffer.
        XrMatrix4x4f identityMatrix;
        XrMatrix4x4f_CreateIdentity(&identityMatrix);  // or better yet poison it
        size_t nodeCount = GetModel().GetNodes().size();

        // Create/recreate the structured buffer and SRV which holds the node transforms.
        // Use Usage=D3D11_USAGE_DYNAMIC and CPUAccessFlags=D3D11_CPU_ACCESS_WRITE with Map/Unmap instead?
        D3D11_BUFFER_DESC desc{};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(XrMatrix4x4f);
        desc.ByteWidth = (UINT)(nodeCount * desc.StructureByteStride);
        XRC_CHECK_THROW_HRCMD(
            pbrResources.GetDevice()->CreateBuffer(&desc, nullptr, m_modelTransformsStructuredBuffer.ReleaseAndGetAddressOf()));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        // TODO this looks weird
        srvDesc.Buffer.NumElements = (UINT)nodeCount;
        srvDesc.Buffer.ElementWidth = (UINT)nodeCount;
        m_modelTransformsResourceView = nullptr;
        XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateShaderResourceView(m_modelTransformsStructuredBuffer.Get(), &srvDesc,
                                                                                 m_modelTransformsResourceView.ReleaseAndGetAddressOf()));
    }

    void D3D11ModelInstance::UpdateTransforms(Pbr::D3D11Resources const& /*pbrResources*/, _In_ ID3D11DeviceContext* context)
    {
        // If none of the node transforms have changed, no need to recompute/update the model transform structured buffer.
        if (WereNodeLocalTransformsUpdated()) {
            ResolveTransforms(true);

            // Update node transform structured buffer.
            context->UpdateSubresource(m_modelTransformsStructuredBuffer.Get(), 0, nullptr, GetResolvedTransforms().data(), 0, 0);
            ClearTransformsUpdatedFlag();
        }
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
