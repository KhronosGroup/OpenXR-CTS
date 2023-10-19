// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "D3D11Model.h"

#include "D3D11Primitive.h"
#include "D3D11Resources.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

#include "utilities/throw_helpers.h"

#include <numeric>

namespace Pbr
{

    void D3D11Model::Render(Pbr::D3D11Resources const& pbrResources, _In_ ID3D11DeviceContext* context)
    {
        UpdateTransforms(pbrResources, context);

        ID3D11ShaderResourceView* vsShaderResources[] = {m_modelTransformsResourceView.Get()};
        context->VSSetShaderResources(Pbr::ShaderSlots::Transforms, _countof(vsShaderResources), vsShaderResources);

        for (PrimitiveHandle primitiveHandle : GetPrimitives()) {
            const Pbr::D3D11Primitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            primitive.GetMaterial()->Bind(context, pbrResources);
            primitive.Render(context);
        }

        // Expect the caller to reset other state, but the geometry shader is cleared specially.
        //context->GSSetShader(nullptr, nullptr, 0);
    }

    void D3D11Model::UpdateTransforms(Pbr::D3D11Resources const& pbrResources, _In_ ID3D11DeviceContext* context)
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
                // Use Usage=D3D11_USAGE_DYNAMIC and CPUAccessFlags=D3D11_CPU_ACCESS_WRITE with Map/Unmap instead?
                D3D11_BUFFER_DESC desc{};
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                desc.StructureByteStride = sizeof(decltype(m_modelTransforms)::value_type);
                desc.ByteWidth = (UINT)(m_modelTransforms.size() * desc.StructureByteStride);
                XRC_CHECK_THROW_HRCMD(
                    pbrResources.GetDevice()->CreateBuffer(&desc, nullptr, m_modelTransformsStructuredBuffer.ReleaseAndGetAddressOf()));

                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                // TODO this looks weird
                srvDesc.Buffer.NumElements = (UINT)m_modelTransforms.size();
                srvDesc.Buffer.ElementWidth = (UINT)m_modelTransforms.size();
                m_modelTransformsResourceView = nullptr;
                XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateShaderResourceView(
                    m_modelTransformsStructuredBuffer.Get(), &srvDesc, m_modelTransformsResourceView.ReleaseAndGetAddressOf()));

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
            context->UpdateSubresource(m_modelTransformsStructuredBuffer.Get(), 0, nullptr, this->m_modelTransforms.data(), 0, 0);
            TotalModifyCount = newTotalModifyCount;
        }
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
