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

#include "../PbrHandles.h"
#include "../PbrModel.h"
#include "../PbrSharedState.h"

#include "common/gfxwrapper_opengl.h"
#include "utilities/opengl_utils.h"

#include <algorithm>
#include <assert.h>
#include <numeric>
#include <stddef.h>

namespace Pbr
{

    void GLModel::Render(Pbr::GLResources const& pbrResources)
    {
        UpdateTransforms(pbrResources);

        XRC_CHECK_THROW_GLCMD(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, ShaderSlots::GLSL::VSResourceViewsOffset + ShaderSlots::Transforms,
                                               m_modelTransformsStructuredBuffer.get()));

        for (PrimitiveHandle primitiveHandle : GetPrimitives()) {
            const Pbr::GLPrimitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            primitive.GetMaterial()->Bind(pbrResources);
            primitive.Render(pbrResources.GetFillMode());
        }

        // Expect the caller to reset other state, but the geometry shader is cleared specially.
        //context->GSSetShader(nullptr, nullptr, 0);
    }

    void GLModel::UpdateTransforms(Pbr::GLResources const& /* pbrResources */)
    {
        const auto& nodes = GetNodes();
        const uint32_t newTotalModifyCount = std::accumulate(nodes.begin(), nodes.end(), 0, [](uint32_t sumChangeCount, const Node& node) {
            return sumChangeCount + node.GetModifyCount();
        });

        // If none of the node transforms have changed, no need to recompute/update the model transform structured buffer.
        if (newTotalModifyCount != TotalModifyCount || m_modelTransformsStructuredBufferInvalid) {
            if (m_modelTransformsStructuredBufferInvalid)  // The structured buffer is reset when a Node is added.
            {
                XrMatrix4x4f identityMatrix;
                XrMatrix4x4f_CreateIdentity(&identityMatrix);  // or better yet poison it
                m_modelTransforms.resize(nodes.size(), identityMatrix);

                size_t elemSize = sizeof(decltype(m_modelTransforms)::value_type);
                XRC_CHECK_THROW_GLCMD(glGenBuffers(1, m_modelTransformsStructuredBuffer.resetAndPut()));
                XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_modelTransformsStructuredBuffer.get()));
                XRC_CHECK_THROW_GLCMD(
                    glBufferData(GL_SHADER_STORAGE_BUFFER, elemSize * m_modelTransforms.size(), m_modelTransforms.data(), GL_DYNAMIC_DRAW));

                m_modelTransformsStructuredBufferInvalid = false;
            }

            // Nodes are guaranteed to come after their parents, so each node transform can be multiplied by its parent transform in a single pass.
            assert(nodes.size() == m_modelTransforms.size());
            XrMatrix4x4f identityMatrix;
            XrMatrix4x4f_CreateIdentity(&identityMatrix);
            for (const auto& node : nodes) {
                assert(node.ParentNodeIndex == RootParentNodeIndex || node.ParentNodeIndex < node.Index);
                const XrMatrix4x4f& parentTransform =
                    (node.ParentNodeIndex == RootParentNodeIndex) ? identityMatrix : m_modelTransforms[node.ParentNodeIndex];
                XrMatrix4x4f nodeTransform = node.GetTransform();
                XrMatrix4x4f_Multiply(&m_modelTransforms[node.Index], &parentTransform, &nodeTransform);
            }

            // Update node transform structured buffer.
            XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_modelTransformsStructuredBuffer.get()));
            XRC_CHECK_THROW_GLCMD(glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                                                  sizeof(decltype(m_modelTransforms)::value_type) * m_modelTransforms.size(),
                                                  this->m_modelTransforms.data()));
            TotalModifyCount = newTotalModifyCount;
        }
    }
}  // namespace Pbr

#endif
