// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once
#include "D3D11Resources.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

namespace Pbr
{
    struct D3D11Primitive;
    struct D3D11Resources;

    struct ModelConstantBuffer
    {
        DirectX::XMFLOAT4X4 ModelToWorld;
    };

    static_assert((sizeof(ModelConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");

    class D3D11ModelInstance final : public ModelInstance
    {
    public:
        D3D11ModelInstance(Pbr::D3D11Resources& pbrResources, std::shared_ptr<const Model> model);

        /// Render the model.
        void Render(Pbr::D3D11Resources const& pbrResources, _In_ ID3D11DeviceContext* context, DirectX::FXMMATRIX modelToWorld);

    private:
        void AllocateDescriptorSets(Pbr::D3D11Resources& pbrResources, uint32_t numSets);
        /// Update the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::D3D11Resources const& pbrResources, _In_ ID3D11DeviceContext* context);

        ModelConstantBuffer m_modelBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_modelConstantBuffer;

        Microsoft::WRL::ComPtr<ID3D11Buffer> m_modelTransformsStructuredBuffer;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_modelTransformsResourceView;
    };
}  // namespace Pbr
