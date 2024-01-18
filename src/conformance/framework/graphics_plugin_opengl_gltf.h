// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#pragma once

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)
#include "gltf_model.h"

#include "common/xr_linear.h"
#include "gltf/GltfHelper.h"
#include "pbr/OpenGL/GLModel.h"
#include "pbr/OpenGL/GLResources.h"
#include "pbr/PbrSharedState.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Pbr
{
    class GLModel;
    struct GLResources;
}  // namespace Pbr

namespace Conformance
{

    class GLGLTF : public RenderableGltfModelInstanceBase<Pbr::GLModelInstance, Pbr::GLResources>
    {
    public:
        using RenderableGltfModelInstanceBase::RenderableGltfModelInstanceBase;

        void Render(Pbr::GLResources& resources, XrMatrix4x4f& modelToWorld);
    };
}  // namespace Conformance
#endif
