// Copyright (c) 2023-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#pragma once
#ifdef XR_USE_GRAPHICS_API_METAL
#include "gltf_model.h"

#include "common/xr_linear.h"
#include "gltf/GltfHelper.h"
#include "pbr/PbrSharedState.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "pbr/Metal/MetalModel.h"
#include "pbr/Metal/MetalResources.h"
#include "utilities/metal_utils.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Conformance
{
    class MetalGLTF : public RenderableGltfModelInstanceBase<Pbr::MetalModelInstance, Pbr::MetalResources>
    {
    public:
        using RenderableGltfModelInstanceBase::RenderableGltfModelInstanceBase;

        void Render(MTL::RenderCommandEncoder* renderCommandEncoder, Pbr::MetalResources& resources, XrMatrix4x4f& modelToWorld,
                    MTL::PixelFormat colorRenderTargetFormat, MTL::PixelFormat depthRenderTargetFormat);
    };
}  // namespace Conformance
#endif
