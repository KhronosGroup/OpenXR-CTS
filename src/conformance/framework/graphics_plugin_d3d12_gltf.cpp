// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT
#if defined(XR_USE_GRAPHICS_API_D3D12) && !defined(MISSING_DIRECTX_COLORS)

#include "graphics_plugin_d3d12_gltf.h"

#include "conformance_framework.h"
#include "graphics_plugin_d3d12_gltf.h"
#include "report.h"

#include "pbr/D3D12/D3D12Primitive.h"
#include "pbr/D3D12/D3D12Resources.h"
#include "pbr/GltfLoader.h"
#include "utilities/d3d_common.h"
#include "utilities/throw_helpers.h"

#include <DirectXColors.h>
#include <d3dcompiler.h>

using namespace DirectX;

namespace Conformance
{
    void D3D12GLTF::Render(ComPtr<ID3D12GraphicsCommandList> directCommandList, Pbr::D3D12Resources& resources, XrMatrix4x4f& modelToWorld,
                           DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat)
    {
        if (!GetModel()) {
            return;
        }

        // move these to a base class helper
        resources.SetFillMode(GetFillMode());
        // end move

        resources.SetModelToWorld(LoadXrMatrix(modelToWorld));
        resources.Bind(directCommandList.Get());
        GetModel()->Render(resources, directCommandList.Get(), colorRenderTargetFormat, depthRenderTargetFormat);
    }

}  // namespace Conformance
#endif
