// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT
#if defined(XR_USE_GRAPHICS_API_D3D11) && !defined(MISSING_DIRECTX_COLORS)

#include "graphics_plugin_d3d11_gltf.h"

#include "conformance_framework.h"
#include "report.h"

#include "pbr/D3D11/D3D11Primitive.h"
#include "pbr/D3D11/D3D11Resources.h"
#include "pbr/GltfLoader.h"
#include "utilities/d3d_common.h"
#include "utilities/throw_helpers.h"

#include <DirectXColors.h>
#include <d3dcompiler.h>

using namespace DirectX;

namespace Conformance
{
    void D3D11GLTF::Render(ComPtr<ID3D11DeviceContext> deviceContext, Pbr::D3D11Resources& resources, XrMatrix4x4f& modelToWorld) const
    {
        if (!GetModel()) {
            return;
        }

        resources.SetFillMode(GetFillMode());
        resources.SetModelToWorld(LoadXrMatrix(modelToWorld), deviceContext.Get());
        resources.Bind(deviceContext.Get());
        GetModel()->Render(resources, deviceContext.Get());
    }

}  // namespace Conformance
#endif
