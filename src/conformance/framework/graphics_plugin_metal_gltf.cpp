// Copyright (c) 2023-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#ifdef XR_USE_GRAPHICS_API_METAL

#include "graphics_plugin_metal_gltf.h"

#include "pbr/Metal/MetalModel.h"
#include "pbr/Metal/MetalPrimitive.h"
#include "pbr/Metal/MetalResources.h"
#include "utilities/metal_utils.h"

#include <memory>

namespace Conformance
{
    void MetalGLTF::Render(MTL::RenderCommandEncoder* renderCommandEncoder, Pbr::MetalResources& resources, XrMatrix4x4f& modelToWorld,
                           MTL::PixelFormat colorRenderTargetFormat, MTL::PixelFormat depthRenderTargetFormat)
    {
        renderCommandEncoder->pushDebugGroup(MTLSTR("MetalGLTF::Render"));

        resources.SetFillMode(GetFillMode());
        resources.SetModelToWorld(modelToWorld);

        // modelToWorld is set as an inline buffer inside the command buffer
        resources.Bind(renderCommandEncoder);

        GetModelInstance().Render(resources, renderCommandEncoder, colorRenderTargetFormat, depthRenderTargetFormat);

        renderCommandEncoder->popDebugGroup();
    }

}  // namespace Conformance

#endif
