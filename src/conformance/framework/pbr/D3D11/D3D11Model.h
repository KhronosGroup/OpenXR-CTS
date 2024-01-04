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

    class D3D11Model final : public Model
    {
    public:
        // Render the model.
        void Render(Pbr::D3D11Resources const& pbrResources, _In_ ID3D11DeviceContext* context);

    private:
        // Updated the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::D3D11Resources const& pbrResources, _In_ ID3D11DeviceContext* context);

        // Temporary buffer holds the world transforms, computed from the node's local transforms.
        mutable std::vector<XrMatrix4x4f> m_modelTransforms;
        mutable Microsoft::WRL::ComPtr<ID3D11Buffer> m_modelTransformsStructuredBuffer;
        mutable Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_modelTransformsResourceView;

        mutable uint32_t TotalModifyCount{0};
    };
}  // namespace Pbr
