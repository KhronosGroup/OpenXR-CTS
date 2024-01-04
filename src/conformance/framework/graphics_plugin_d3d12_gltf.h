// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#pragma once

#if defined(XR_USE_GRAPHICS_API_D3D12) && !defined(MISSING_DIRECTX_COLORS)
#include "gltf_model.h"

#include "gltf/GltfHelper.h"
#include "pbr/D3D12/D3D12Model.h"
#include "pbr/D3D12/D3D12Resources.h"
#include "pbr/PbrSharedState.h"

#include <DirectXMath.h>
#include <d3d12.h>

#include <dxgi1_6.h>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

namespace Conformance
{

    class D3D12GLTF : public GltfModelBase<Pbr::D3D12Model, Pbr::D3D12Resources>
    {
    public:
        using GltfModelBase::GltfModelBase;

        void Render(ComPtr<ID3D12GraphicsCommandList> directCommandList, Pbr::D3D12Resources& resources, XrMatrix4x4f& modelToWorld,
                    DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat);
    };
}  // namespace Conformance
#endif
