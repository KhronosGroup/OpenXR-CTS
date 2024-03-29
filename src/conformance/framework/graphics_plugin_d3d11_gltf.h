// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#pragma once

#if defined(XR_USE_GRAPHICS_API_D3D11) && !defined(MISSING_DIRECTX_COLORS)
#include "gltf_model.h"

#include "gltf/GltfHelper.h"
#include "pbr/D3D11/D3D11Model.h"
#include "pbr/D3D11/D3D11Resources.h"
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
    struct CmdBuffer;

    class D3D11GLTF : public RenderableGltfModelInstanceBase<Pbr::D3D11ModelInstance, Pbr::D3D11Resources>
    {
    public:
        using RenderableGltfModelInstanceBase::RenderableGltfModelInstanceBase;

        void Render(ComPtr<ID3D11DeviceContext> deviceContext, Pbr::D3D11Resources& resources, XrMatrix4x4f& modelToWorld);
    };
}  // namespace Conformance
#endif
