// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "graphics_plugin_opengl_gltf.h"

#include "pbr/OpenGL/GLModel.h"
#include "pbr/OpenGL/GLResources.h"

namespace Conformance
{
    void GLGLTF::Render(Pbr::GLResources& resources, XrMatrix4x4f& modelToWorld)
    {
        resources.SetFillMode(GetFillMode());
        resources.Bind();
        GetModelInstance().Render(resources, modelToWorld);
    }

}  // namespace Conformance
#endif
