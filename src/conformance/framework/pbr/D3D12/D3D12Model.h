// Copyright 2022-2023, The Khronos Group, Inc.
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

    class D3D12Model final : public Model
    {
    public:
        // Render the model.
        void Render(Pbr::D3D12Resources& pbrResources, _In_ ID3D12GraphicsCommandList* directCommandList,
                    DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat);

    private:
        // Updated the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::D3D12Resources& pbrResources);

        // Temporary buffer holds the world transforms, computed from the node's local transforms.
        mutable std::vector<XrMatrix4x4f> m_modelTransforms;
        mutable Conformance::D3D12BufferWithUpload<XrMatrix4x4f> m_modelTransformsStructuredBuffer;
        mutable Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_modelTransformsResourceViewHeap;

        mutable uint32_t TotalModifyCount{0};
    };
}  // namespace Pbr
