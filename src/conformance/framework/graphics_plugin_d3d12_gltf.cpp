// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT
#if defined(XR_USE_GRAPHICS_API_D3D12) && !defined(MISSING_DIRECTX_COLORS)

#include "graphics_plugin_d3d12_gltf.h"

#include "pbr/D3D12/D3D12Model.h"
#include "pbr/D3D12/D3D12Resources.h"
#include "utilities/d3d_common.h"

#include <DirectXColors.h>
#include <d3dcompiler.h>

using namespace DirectX;

namespace Conformance
{
    void D3D12GLTF::Render(ComPtr<ID3D12GraphicsCommandList> directCommandList, Pbr::D3D12Resources& resources, XrMatrix4x4f& modelToWorld,
                           DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat)
    {
        resources.SetFillMode(GetFillMode());
        resources.Bind(directCommandList.Get());
        GetModelInstance().Render(resources, directCommandList.Get(), colorRenderTargetFormat, depthRenderTargetFormat,
                                  LoadXrMatrix(modelToWorld));
    }

}  // namespace Conformance
#endif
