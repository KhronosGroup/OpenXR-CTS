// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "graphics_plugin_opengl_gltf.h"

#include "conformance_framework.h"
#include "graphics_plugin_opengl_gltf.h"
#include "report.h"

#include "pbr/GltfLoader.h"
#include "pbr/OpenGL/GLModel.h"
#include "pbr/OpenGL/GLPrimitive.h"
#include "pbr/OpenGL/GLResources.h"
#include "utilities/throw_helpers.h"

#include <memory>

namespace Conformance
{
    void GLGLTF::Render(Pbr::GLResources& resources, XrMatrix4x4f& modelToWorld) const
    {
        if (!GetModel()) {
            return;
        }

        resources.SetFillMode(GetFillMode());
        resources.SetModelToWorld(modelToWorld);
        resources.Bind();
        GetModel()->Render(resources);
    }

}  // namespace Conformance
#endif
