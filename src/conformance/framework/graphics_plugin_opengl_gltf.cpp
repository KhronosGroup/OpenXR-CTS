// Copyright (c) 2022-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "graphics_plugin_opengl_gltf.h"

#include "common/xr_linear.h"
#include "pbr/OpenGL/GLModel.h"
#include "pbr/OpenGL/GLResources.h"

namespace Conformance
{
    void GLGLTF::Render(Pbr::GLResources& resources)
    {
        resources.SetFillMode(GetFillMode());
        resources.Bind();
        GetModelInstance().Render(resources);
    }

}  // namespace Conformance
#endif
