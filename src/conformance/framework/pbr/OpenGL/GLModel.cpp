// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "GLModel.h"

#include "GLCommon.h"
#include "GLMaterial.h"
#include "GLPrimitive.h"
#include "GLResources.h"

#include "../GlslBuffers.h"
#include "../PbrHandles.h"
#include "../PbrModel.h"
#include "../PbrSharedState.h"

#include "common/gfxwrapper_opengl.h"
#include "utilities/opengl_utils.h"

#include <stddef.h>

namespace Pbr
{

    void GLModelInstance::Render(Pbr::GLResources const& pbrResources, XrMatrix4x4f modelToWorld)
    {
        // Update model buffer
        m_modelBuffer.ModelToWorld = modelToWorld;
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_UNIFORM_BUFFER, m_modelConstantBuffer.get()));
        XRC_CHECK_THROW_GLCMD(glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Glsl::ModelConstantBuffer), &m_modelBuffer));
        // Bind model buffer
        XRC_CHECK_THROW_GLCMD(glBindBufferBase(GL_UNIFORM_BUFFER, ShaderSlots::ConstantBuffers::Model, m_modelConstantBuffer.get()));

        UpdateTransforms(pbrResources);

        XRC_CHECK_THROW_GLCMD(glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                                               (int)ShaderSlots::GLSL::VSResourceViewsOffset + (int)ShaderSlots::Transforms,
                                               m_modelTransformsStructuredBuffer.get()));

        for (PrimitiveHandle primitiveHandle : GetModel().GetPrimitiveHandles()) {
            const Pbr::GLPrimitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            if (!IsAnyNodeVisible(primitive.GetNodes()))
                continue;

            primitive.GetMaterial()->Bind(pbrResources);
            primitive.Render(pbrResources.GetFillMode());
        }
    }

    GLModelInstance::GLModelInstance(Pbr::GLResources& /* pbrResources */, std::shared_ptr<const Model> model)
        : ModelInstance(std::move(model))
    {
        // Set up the model constant buffer.
        XRC_CHECK_THROW_GLCMD(glGenBuffers(1, m_modelConstantBuffer.resetAndPut()));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_UNIFORM_BUFFER, m_modelConstantBuffer.get()));
        XRC_CHECK_THROW_GLCMD(glBufferData(GL_UNIFORM_BUFFER, sizeof(Glsl::ModelConstantBuffer), nullptr, GL_DYNAMIC_DRAW));

        // Set up the transforms buffer.
        size_t nodeCount = GetModel().GetNodes().size();

        size_t elemSize = sizeof(XrMatrix4x4f);
        size_t count = nodeCount;
        size_t size = count * elemSize;

        XRC_CHECK_THROW_GLCMD(glGenBuffers(1, m_modelTransformsStructuredBuffer.resetAndPut()));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_modelTransformsStructuredBuffer.get()));
        XRC_CHECK_THROW_GLCMD(glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW));
    }

    void GLModelInstance::UpdateTransforms(Pbr::GLResources const& /* pbrResources */)
    {
        // If none of the node transforms have changed, no need to recompute/update the model transform structured buffer.
        if (ResolvedTransformsNeedUpdate()) {
            ResolveTransformsAndVisibilities(false);

            // Update node transform structured buffer.
            auto& resolvedTransforms = GetResolvedTransforms();
            XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_modelTransformsStructuredBuffer.get()));
            XRC_CHECK_THROW_GLCMD(
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(XrMatrix4x4f) * resolvedTransforms.size(), resolvedTransforms.data()));
            MarkResolvedTransformsUpdated();
        }
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)
