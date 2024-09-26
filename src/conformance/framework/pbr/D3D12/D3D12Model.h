// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once
#include "D3D12Resources.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

namespace Pbr
{

    struct D3D12Primitive;

    struct ModelConstantBuffer
    {
        DirectX::XMFLOAT4X4 ModelToWorld;
    };

    static_assert((sizeof(ModelConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");

    class D3D12ModelInstance final : public ModelInstance
    {
    public:
        D3D12ModelInstance(Pbr::D3D12Resources& pbrResources, std::shared_ptr<const Model> model);

        /// Render the model.
        void Render(Pbr::D3D12Resources& pbrResources, _In_ ID3D12GraphicsCommandList* directCommandList,
                    DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat, DirectX::FXMMATRIX modelToWorld);

    private:
        /// Update the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::D3D12Resources& pbrResources, ID3D12GraphicsCommandList* directCommandList);

        ModelConstantBuffer m_modelBuffer;
        Conformance::D3D12BufferWithUpload<ModelConstantBuffer> m_modelConstantBuffer;

        Conformance::D3D12BufferWithUpload<XrMatrix4x4f> m_modelTransformsStructuredBuffer;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_modelTransformsResourceViewHeap;
    };
}  // namespace Pbr
