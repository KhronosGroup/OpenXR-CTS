// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include "GLCommon.h"
#include "GLMaterial.h"

#include "../PbrSharedState.h"

#include "common/gfxwrapper_opengl.h"

#include <memory>
#include <utility>
#include <vector>

namespace Pbr
{
    struct GLMaterial;
    struct PrimitiveBuilder;

    /// A primitive holds a vertex buffer, index buffer, and a pointer to a PBR material.
    struct GLPrimitive final
    {
        using Collection = std::vector<GLPrimitive>;

        GLPrimitive() = delete;
        GLPrimitive(GLsizei indexCount, ScopedGLBuffer indexBuffer, ScopedGLBuffer vertexBuffer, ScopedGLVertexArray vao,
                    std::shared_ptr<GLMaterial> material, std::vector<NodeIndex_t> nodeIndices);
        GLPrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder, const std::shared_ptr<GLMaterial>& material);

        void UpdateBuffers(const Pbr::PrimitiveBuilder& primitiveBuilder);

        /// Get the material for the primitive.
        const std::shared_ptr<GLMaterial>& GetMaterial() const
        {
            return m_material;
        }

        /// Replace the material for the primitive
        void SetMaterial(std::shared_ptr<GLMaterial> material)
        {
            m_material = std::move(material);
        }

        /// Get the nodes that the primitive represents
        const std::vector<NodeIndex_t>& GetNodes() const
        {
            return m_nodeIndices;
        }

    protected:
        friend class GLModelInstance;
        void Render(FillMode fillMode) const;

    private:
        GLsizei m_indexCount;
        ScopedGLBuffer m_indexBuffer;
        GLsizei m_vertexCount;
        ScopedGLBuffer m_vertexBuffer;
        ScopedGLVertexArray m_vao;
        std::shared_ptr<GLMaterial> m_material;
        std::vector<NodeIndex_t> m_nodeIndices;
    };
}  // namespace Pbr
