// Copyright 2022-2026 The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "GLPrimitive.h"

#include "GLCommon.h"
#include "PbrCommon.h"
#include "PbrSharedState.h"

#include "common/gfxwrapper_opengl.h"
#include "utilities/opengl_utils.h"

#include <stddef.h>

namespace Pbr
{
    struct GLMaterial;
}  // namespace Pbr

namespace
{
    struct VertexInputAttributeDescription
    {
        GLuint index;
        GLint size;
        GLenum type;
        bool asFloat;
        GLboolean normalized;
        size_t offset;
    };

    static constexpr VertexInputAttributeDescription c_attrDesc[6] = {
        {0, 3, GL_FLOAT, true, GL_FALSE, offsetof(Pbr::Vertex, Position)},
        {1, 3, GL_FLOAT, true, GL_FALSE, offsetof(Pbr::Vertex, Normal)},
        {2, 4, GL_FLOAT, true, GL_FALSE, offsetof(Pbr::Vertex, Tangent)},
        {3, 4, GL_FLOAT, true, GL_FALSE, offsetof(Pbr::Vertex, Color0)},
        {4, 2, GL_FLOAT, true, GL_FALSE, offsetof(Pbr::Vertex, TexCoord0)},
        {5, 1, GL_UNSIGNED_SHORT, false, GL_FALSE, offsetof(Pbr::Vertex, ModelTransformIndex)},
    };

    GLsizei GetPbrVertexByteSize(size_t size)
    {
        return (GLsizei)(sizeof(decltype(Pbr::PrimitiveBuilder::Vertices)::value_type) * size);
    }
    GLsizei GetPbrIndexByteSize(size_t size)
    {
        return (GLsizei)(sizeof(decltype(Pbr::PrimitiveBuilder::Indices)::value_type) * size);
    }

    Pbr::ScopedGLBuffer CreateVertexBuffer(nonstd::span<const Pbr::Vertex> vtx)
    {
        // Create Vertex Buffer
        auto buffer = Pbr::ScopedGLBuffer{};
        XRC_CHECK_THROW_GLCMD(glGenBuffers(1, buffer.resetAndPut()));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, buffer.get()));
        XRC_CHECK_THROW_GLCMD(glBufferData(GL_ARRAY_BUFFER, GetPbrVertexByteSize(vtx.size()), vtx.data(), GL_STATIC_DRAW));
        return buffer;
    }

    Pbr::ScopedGLBuffer CreateIndexBuffer(nonstd::span<const uint32_t> idx)
    {
        // Create Index Buffer
        auto buffer = Pbr::ScopedGLBuffer{};
        XRC_CHECK_THROW_GLCMD(glGenBuffers(1, buffer.resetAndPut()));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.get()));
        XRC_CHECK_THROW_GLCMD(glBufferData(GL_ELEMENT_ARRAY_BUFFER, GetPbrIndexByteSize(idx.size()), idx.data(), GL_STATIC_DRAW));
        return buffer;
    }

    Pbr::ScopedGLVertexArray CreateVAO(Pbr::ScopedGLBuffer& vertexBuffer, Pbr::ScopedGLBuffer& indexBuffer)
    {
        // Create Vertex Array Object
        auto vao = Pbr::ScopedGLVertexArray{};
        XRC_CHECK_THROW_GLCMD(glGenVertexArrays(1, vao.resetAndPut()));
        XRC_CHECK_THROW_GLCMD(glBindVertexArray(vao.get()));
        for (auto attr : c_attrDesc) {
            XRC_CHECK_THROW_GLCMD(glEnableVertexAttribArray(attr.index));
        }
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get()));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get()));

        for (auto attr : c_attrDesc) {
            if (attr.asFloat) {
                XRC_CHECK_THROW_GLCMD(glVertexAttribPointer(attr.index, attr.size, attr.type, attr.normalized, GetPbrVertexByteSize(1),
                                                            reinterpret_cast<const void*>(attr.offset)));
            }
            else {
                XRC_CHECK_THROW_GLCMD(glVertexAttribIPointer(attr.index, attr.size, attr.type, GetPbrVertexByteSize(1),
                                                             reinterpret_cast<const void*>(attr.offset)));
            }
        }
        return vao;
    }
}  // namespace

namespace Pbr
{
    GLPrimitive::GLPrimitive(GLsizei indexCount, ScopedGLBuffer indexBuffer, ScopedGLBuffer vertexBuffer, ScopedGLVertexArray vao,
                             std::shared_ptr<GLMaterial> material, std::vector<NodeIndex_t> nodeIndices)
        : m_indexCount(indexCount)
        , m_indexBuffer(std::move(indexBuffer))
        , m_vertexBuffer(std::move(vertexBuffer))
        , m_vao(std::move(vao))
        , m_material(std::move(material))
        , m_nodeIndices(std::move(nodeIndices))
    {
    }

    GLPrimitive::GLPrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder, const std::shared_ptr<Pbr::GLMaterial>& material)
        : GLPrimitive((GLsizei)primitiveBuilder.Indices.size(),                                                             //
                      primitiveBuilder.Indices.empty() ? ScopedGLBuffer{} : CreateIndexBuffer(primitiveBuilder.Indices),    //
                      primitiveBuilder.Indices.empty() ? ScopedGLBuffer{} : CreateVertexBuffer(primitiveBuilder.Vertices),  //
                      ScopedGLVertexArray{}, std::move(material), primitiveBuilder.NodeIndicesVector())
    {
        if (primitiveBuilder.Indices.size() > 0) {
            m_vao = CreateVAO(m_vertexBuffer, m_indexBuffer);
        }
    }

    void GLPrimitive::UpdateBuffers(span<const uint32_t> idx, span<const Pbr::Vertex> vtx)
    {
        if (idx.empty()) {
            m_indexBuffer.reset();
            m_vertexBuffer.reset();
            m_indexCount = 0;
            m_vertexCount = 0;
            return;
        }

        bool vaoNeedsUpdate = false;

        // Update vertex buffer.
        {
            GLsizei requiredSize = GetPbrVertexByteSize(vtx.size());
            if (m_vertexCount >= requiredSize) {
                XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.get()));
                XRC_CHECK_THROW_GLCMD(glBufferSubData(GL_ARRAY_BUFFER, 0, requiredSize, vtx.data()));
            }
            else {
                m_vertexBuffer = CreateVertexBuffer(vtx);
                vaoNeedsUpdate = true;
            }

            m_vertexCount = (GLsizei)vtx.size();
        }

        // Update index buffer.
        {
            GLsizei requiredSize = GetPbrIndexByteSize(idx.size());
            if (m_indexCount >= requiredSize) {
                XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer.get()));
                XRC_CHECK_THROW_GLCMD(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, requiredSize, idx.data()));
            }
            else {
                m_indexBuffer = CreateIndexBuffer(idx);
                vaoNeedsUpdate = true;
            }

            m_indexCount = (GLsizei)idx.size();
        }

        if (vaoNeedsUpdate) {
            m_vao = CreateVAO(m_vertexBuffer, m_indexBuffer);
        }
    }

    void GLPrimitive::Render(FillMode fillMode) const
    {
        if (m_indexCount == 0) {
            return;
        }

        (void)fillMode;  // suppress unused warning under GL
        GLenum drawMode =
#if defined(XR_USE_GRAPHICS_API_OPENGL)
            GL_TRIANGLES  // use glPolygonMode(..., GL_LINE)
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES)
            fillMode == FillMode::Wireframe ? GL_LINES : GL_TRIANGLES
#endif
            ;

        XRC_CHECK_THROW_GLCMD(glBindVertexArray(m_vao.get()));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.get()));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer.get()));
        XRC_CHECK_THROW_GLCMD(glDrawElements(drawMode, m_indexCount, GL_UNSIGNED_INT, nullptr));
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)
