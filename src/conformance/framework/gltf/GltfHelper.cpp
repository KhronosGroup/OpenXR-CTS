// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "GltfHelper.h"

#include "common/xr_linear.h"
#include <utilities/image.h>
#include "utilities/xr_math_operators.h"

#include <openxr/openxr.h>
#include <nonstd/span.hpp>
#include <tinygltf/tiny_gltf.h>
#include <mikktspace.h>

#include <assert.h>
#include <cctype>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string.h>
#include <string>
#include <vector>

#define TRIANGLE_VERTEX_COUNT 3  // #define so it can be used in lambdas without capture

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
// only for size comparison static assert
#include <DirectXMath.h>

static_assert(sizeof(XrVector2f) == sizeof(DirectX::XMFLOAT2), "Size of 2D vectors must match");
static_assert(sizeof(XrVector3f) == sizeof(DirectX::XMFLOAT3), "Size of 3D vectors must match");
static_assert(sizeof(XrVector4f) == sizeof(DirectX::XMFLOAT4), "Size of 4D vectors must match");
#endif

namespace
{
    using namespace openxr::math_operators;

    // The glTF 2 specification recommends using the MikkTSpace algorithm to generate
    // tangents when none are available. This function takes a GltfHelper Primitive which has
    // no tangents and uses the MikkTSpace algorithm to generate the tangents. This can
    // be computationally expensive.
    void ComputeTriangleTangents(GltfHelper::Primitive& primitive)
    {
        // Set up the callbacks so that MikkTSpace can read the Primitive data.
        SMikkTSpaceInterface mikkInterface{};
        mikkInterface.m_getNumFaces = [](const SMikkTSpaceContext* pContext) {
            auto primitive = static_cast<const GltfHelper::Primitive*>(pContext->m_pUserData);
            assert((primitive->Indices.size() % TRIANGLE_VERTEX_COUNT) == 0);  // Only triangles are supported.
            return (int)(primitive->Indices.size() / TRIANGLE_VERTEX_COUNT);
        };
        mikkInterface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext* /* pContext */, int /* iFace */) {
            return TRIANGLE_VERTEX_COUNT;
        };
        mikkInterface.m_getPosition = [](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) {
            auto primitive = static_cast<const GltfHelper::Primitive*>(pContext->m_pUserData);
            const auto vertexIndex = primitive->Indices[(iFace * TRIANGLE_VERTEX_COUNT) + iVert];
            memcpy(fvPosOut, &primitive->Vertices[vertexIndex].Position, sizeof(float) * 3);
        };
        mikkInterface.m_getNormal = [](const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert) {
            auto primitive = static_cast<const GltfHelper::Primitive*>(pContext->m_pUserData);
            const auto vertexIndex = primitive->Indices[(iFace * TRIANGLE_VERTEX_COUNT) + iVert];
            memcpy(fvNormOut, &primitive->Vertices[vertexIndex].Normal, sizeof(float) * 3);
        };
        mikkInterface.m_getTexCoord = [](const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert) {
            auto primitive = static_cast<const GltfHelper::Primitive*>(pContext->m_pUserData);
            const auto vertexIndex = primitive->Indices[(iFace * TRIANGLE_VERTEX_COUNT) + iVert];
            memcpy(fvTexcOut, &primitive->Vertices[vertexIndex].TexCoord0, sizeof(float) * 2);
        };
        mikkInterface.m_setTSpaceBasic = [](const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace,
                                            const int iVert) {
            auto primitive = static_cast<GltfHelper::Primitive*>(pContext->m_pUserData);
            const auto vertexIndex = primitive->Indices[(iFace * TRIANGLE_VERTEX_COUNT) + iVert];
            primitive->Vertices[vertexIndex].Tangent.x = fvTangent[0];
            primitive->Vertices[vertexIndex].Tangent.y = fvTangent[1];
            primitive->Vertices[vertexIndex].Tangent.z = fvTangent[2];
            // handedness difference, see:
            // https://github.com/KhronosGroup/glTF-Sample-Models/issues/174
            // https://github.com/KhronosGroup/glTF/issues/2056
            primitive->Vertices[vertexIndex].Tangent.w = -fSign;
        };

        // Run the MikkTSpace algorithm.
        SMikkTSpaceContext mikkContext{};
        mikkContext.m_pUserData = &primitive;
        mikkContext.m_pInterface = &mikkInterface;
        if (genTangSpaceDefault(&mikkContext) == 0) {
            throw std::runtime_error("Failed to generate tangents");
        }
    }

    // Generates normals for the triangles in the GltfHelper Primitive object.
    void ComputeTriangleNormals(GltfHelper::Primitive& primitive)
    {
        assert((primitive.Indices.size() % TRIANGLE_VERTEX_COUNT) == 0);  // Only triangles are supported.

        // Loop through each triangle
        for (uint32_t i = 0; i < primitive.Indices.size(); i += TRIANGLE_VERTEX_COUNT) {
            // References to the three vertices of the triangle.
            GltfHelper::Vertex& v0 = primitive.Vertices[primitive.Indices[i]];
            GltfHelper::Vertex& v1 = primitive.Vertices[primitive.Indices[i + 1]];
            GltfHelper::Vertex& v2 = primitive.Vertices[primitive.Indices[i + 2]];

            // Compute normal. Normalization happens later.
            XrVector3f d0 = v2.Position - v0.Position;
            XrVector3f d1 = v1.Position - v0.Position;
            XrVector3f normal = Vector::CrossProduct(d0, d1);

            // Add the normal to the three vertices of the triangle. Normals are added
            // so that reused vertices will get the average normal (done later).
            // Note that the normals are not normalized at this point, so larger triangles
            // will have more weight than small triangles which share a vertex. This
            // appears to give better results.
            v0.Normal += normal;
            v1.Normal += normal;
            v2.Normal += normal;
        }

        // Since the same vertex may have been used by multiple triangles, and the cross product normals
        // aren't normalized yet, normalize the computed normals.
        for (GltfHelper::Vertex& vertex : primitive.Vertices) {
            Vector::Normalize(vertex.Normal);
        }
    }

    // Some data, like texCoords, can be represented 32bit float or normalized unsigned short or byte.
    // ReadNormalizedFloat provides overloads for all three types.
    template <typename T>
    float ReadNormalizedFloat(const uint8_t* ptr);
    template <>
    float ReadNormalizedFloat<float>(const uint8_t* ptr)
    {
        return *reinterpret_cast<const float*>(ptr);
    }
    template <>
    float ReadNormalizedFloat<uint16_t>(const uint8_t* ptr)
    {
        return *reinterpret_cast<const uint16_t*>(ptr) / (float)std::numeric_limits<uint16_t>::max();
    }
    template <>
    float ReadNormalizedFloat<uint8_t>(const uint8_t* ptr)
    {
        return *reinterpret_cast<const uint8_t*>(ptr) / (float)std::numeric_limits<uint8_t>::max();
    }

    XrMatrix4x4f Double4x4ToXrMatrix4x4f(const XrMatrix4x4f& defaultMatrix, const std::vector<double>& doubleData)
    {
        if (doubleData.size() != 16) {
            return defaultMatrix;
        }

        return XrMatrix4x4f{{(float)doubleData[0], (float)doubleData[1], (float)doubleData[2], (float)doubleData[3], (float)doubleData[4],
                             (float)doubleData[5], (float)doubleData[6], (float)doubleData[7], (float)doubleData[8], (float)doubleData[9],
                             (float)doubleData[10], (float)doubleData[11], (float)doubleData[12], (float)doubleData[13],
                             (float)doubleData[14], (float)doubleData[15]}};
    }

    XrVector3f DoublesToXrVector3f(const XrVector3f& defaultVector, const std::vector<double>& doubleData)
    {
        if (doubleData.size() != 3) {
            return defaultVector;
        }

        return XrVector3f{(float)doubleData[0], (float)doubleData[1], (float)doubleData[2]};
    }

    XrQuaternionf DoublesToXrQuaternionf(const XrQuaternionf& defaultVector, const std::vector<double>& doubleData)
    {
        if (doubleData.size() != 4) {
            return defaultVector;
        }

        return XrQuaternionf{(float)doubleData[0], (float)doubleData[1], (float)doubleData[2], (float)doubleData[3]};
    }

    // Validate that an accessor does not go out of bounds of the buffer view that it references and that the buffer view does not exceed
    // the bounds of the buffer that it references.
    void ValidateAccessor(const tinygltf::Accessor& accessor, const tinygltf::BufferView& bufferView, const tinygltf::Buffer& buffer,
                          size_t byteStride, size_t elementSize)
    {
        // Make sure the accessor does not go out of range of the buffer view.
        if (accessor.byteOffset + (accessor.count - 1) * byteStride + elementSize > bufferView.byteLength) {
            throw std::out_of_range("Accessor goes out of range of bufferview.");
        }

        // Make sure the buffer view does not go out of range of the buffer.
        if (bufferView.byteOffset + bufferView.byteLength > buffer.data.size()) {
            throw std::out_of_range("BufferView goes out of range of buffer.");
        }
    }

    // Reads the tangent data (VEC4) from a glTF primitive into a GltfHelper Primitive.
    void ReadTangentToVertexField(const tinygltf::Accessor& accessor, const tinygltf::BufferView& bufferView,
                                  const tinygltf::Buffer& buffer, GltfHelper::Primitive& primitive)
    {
        if (accessor.type != TINYGLTF_TYPE_VEC4) {
            throw std::runtime_error("Accessor for primitive attribute has incorrect type (VEC4 expected).");
        }

        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
            throw std::runtime_error("Accessor for primitive attribute has incorrect component type (FLOAT expected).");
        }

        // If stride is not specified, it is tightly packed.
        constexpr size_t PackedSize = sizeof(XrVector4f);
        const size_t stride = bufferView.byteStride == 0 ? PackedSize : bufferView.byteStride;
        ValidateAccessor(accessor, bufferView, buffer, stride, PackedSize);

        // Resize the vertices vector, if necessary, to include room for the attribute data.
        // If there are multiple attributes for a primitive, the first one will resize, and the subsequent will not need to.
        primitive.Vertices.resize(accessor.count);

        // Copy the attribute value over from the glTF buffer into the appropriate vertex field.
        const uint8_t* bufferPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        for (size_t i = 0; i < accessor.count; i++, bufferPtr += stride) {
            primitive.Vertices[i].Tangent = *reinterpret_cast<const XrVector4f*>(bufferPtr);
        }
    }

    // Reads the TexCoord data (VEC2) from a glTF primitive into a GltfHelper Primitive.
    // This function uses a template type to express the VEC2 component type (byte, ushort, or float).
    template <typename TComponentType, XrVector2f GltfHelper::Vertex::*field>
    void ReadTexCoordToVertexField(const tinygltf::Accessor& accessor, const tinygltf::BufferView& bufferView,
                                   const tinygltf::Buffer& buffer, GltfHelper::Primitive& primitive)
    {
        // If stride is not specified, it is tightly packed.
        constexpr size_t PackedSize = sizeof(TComponentType) * 2;
        const size_t stride = bufferView.byteStride == 0 ? PackedSize : bufferView.byteStride;
        ValidateAccessor(accessor, bufferView, buffer, stride, PackedSize);

        // Resize the vertices vector, if necessary, to include room for the attribute data.
        // If there are multiple attributes for a primitive, the first one will resize, and the subsequent will not need to.
        primitive.Vertices.resize(accessor.count);

        // Copy the attribute value over from the glTF buffer into the appropriate vertex field.
        const uint8_t* bufferPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        for (size_t i = 0; i < accessor.count; i++, bufferPtr += stride) {
            (primitive.Vertices[i].*field).x = ReadNormalizedFloat<TComponentType>(bufferPtr + sizeof(TComponentType) * 0);
            (primitive.Vertices[i].*field).y = ReadNormalizedFloat<TComponentType>(bufferPtr + sizeof(TComponentType) * 1);
        }
    }

    // Reads the TexCoord data (VEC2) from a glTF primitive into a GltfHelper Primitive.
    template <XrVector2f GltfHelper::Vertex::*field>
    void ReadTexCoordToVertexField(const tinygltf::Accessor& accessor, const tinygltf::BufferView& bufferView,
                                   const tinygltf::Buffer& buffer, GltfHelper::Primitive& primitive)
    {
        if (accessor.type != TINYGLTF_TYPE_VEC2) {
            throw std::runtime_error("Accessor for primitive TexCoord must have VEC2 type.");
        }

        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
            ReadTexCoordToVertexField<float, field>(accessor, bufferView, buffer, primitive);
        }
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            if (!accessor.normalized) {
                throw std::runtime_error("Accessor for TEXTCOORD_n unsigned byte must be normalized.");
            }
            ReadTexCoordToVertexField<uint8_t, field>(accessor, bufferView, buffer, primitive);
        }
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            if (!accessor.normalized) {
                throw std::runtime_error("Accessor for TEXTCOORD_n unsigned short must be normalized.");
            }
            ReadTexCoordToVertexField<uint16_t, field>(accessor, bufferView, buffer, primitive);
        }
        else {
            throw std::runtime_error("Accessor for TEXTCOORD_n uses unsupported component type.");
        }
    }

    // Reads the Color data (VEC3 or VEC4) from a glTF primitive into a GltfHelper Primitive.
    // This function uses a template type to express the VEC3/4 component type (byte, ushort, or float).
    template <typename TComponentType, XrColor4f GltfHelper::Vertex::*field>
    void ReadColorToVertexField(size_t componentCount, const tinygltf::Accessor& accessor, const tinygltf::BufferView& bufferView,
                                const tinygltf::Buffer& buffer, GltfHelper::Primitive& primitive)
    {
        // If stride is not specified, it is tightly packed.
        const size_t packedSize = sizeof(TComponentType) * componentCount;
        const size_t stride = bufferView.byteStride == 0 ? packedSize : bufferView.byteStride;
        ValidateAccessor(accessor, bufferView, buffer, stride, packedSize);

        // Resize the vertices vector, if necessary, to include room for the attribute data.
        // If there are multiple attributes for a primitive, the first one will resize, and the subsequent will not need to.
        primitive.Vertices.resize(accessor.count);

        // Copy the attribute value over from the glTF buffer into the appropriate vertex field.
        const uint8_t* bufferPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        for (size_t i = 0; i < accessor.count; i++, bufferPtr += stride) {
            (primitive.Vertices[i].*field).r = ReadNormalizedFloat<TComponentType>(bufferPtr + sizeof(TComponentType) * 0);
            (primitive.Vertices[i].*field).g = ReadNormalizedFloat<TComponentType>(bufferPtr + sizeof(TComponentType) * 1);
            (primitive.Vertices[i].*field).b = ReadNormalizedFloat<TComponentType>(bufferPtr + sizeof(TComponentType) * 2);

            if (componentCount == 4)  // Alpha
            {
                (primitive.Vertices[i].*field).a = ReadNormalizedFloat<TComponentType>(bufferPtr + sizeof(TComponentType) * 3);
            }
        }
    }

    // Reads the Color data (VEC3/4) from a glTF primitive into a GltfHelper Primitive.
    template <XrColor4f GltfHelper::Vertex::*field>
    void ReadColorToVertexField(const tinygltf::Accessor& accessor, const tinygltf::BufferView& bufferView, const tinygltf::Buffer& buffer,
                                GltfHelper::Primitive& primitive)
    {
        int componentCount;
        if (accessor.type == TINYGLTF_TYPE_VEC3) {
            componentCount = 3;
        }
        else if (accessor.type == TINYGLTF_TYPE_VEC4) {
            componentCount = 4;
        }
        else {
            throw std::runtime_error("Accessor for primitive Color must have VEC3 or VEC4 type.");
        }

        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
            ReadColorToVertexField<float, field>(componentCount, accessor, bufferView, buffer, primitive);
        }
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            if (!accessor.normalized) {
                throw std::runtime_error("Accessor for COLOR_0 unsigned byte must be normalized.");
            }
            ReadColorToVertexField<uint8_t, field>(componentCount, accessor, bufferView, buffer, primitive);
        }
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            if (!accessor.normalized) {
                throw std::runtime_error("Accessor for COLOR_0 unsigned short must be normalized.");
            }
            ReadColorToVertexField<uint16_t, field>(componentCount, accessor, bufferView, buffer, primitive);
        }
        else {
            throw std::runtime_error("Accessor for COLOR_0 uses unsupported component type.");
        }
    }

    // Reads VEC3 attribute data (like POSITION and NORMAL) from a glTF primitive into a GltfHelper Primitive. The specific Vertex field is specified as a template parameter.
    template <XrVector3f GltfHelper::Vertex::*field>
    void ReadVec3ToVertexField(const tinygltf::Accessor& accessor, const tinygltf::BufferView& bufferView, const tinygltf::Buffer& buffer,
                               GltfHelper::Primitive& primitive)
    {
        if (accessor.type != TINYGLTF_TYPE_VEC3) {
            throw std::runtime_error("Accessor for primitive attribute has incorrect type (VEC3 expected).");
        }

        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
            throw std::runtime_error("Accessor for primitive attribute has incorrect component type (FLOAT expected).");
        }

        // If stride is not specified, it is tightly packed.
        constexpr size_t PackedSize = sizeof(XrVector3f);
        const size_t stride = bufferView.byteStride == 0 ? PackedSize : bufferView.byteStride;
        ValidateAccessor(accessor, bufferView, buffer, stride, PackedSize);

        // Resize the vertices vector, if necessary, to include room for the attribute data.
        // If there are multiple attributes for a primitive, the first one will resize, and the subsequent will not need to.
        primitive.Vertices.resize(accessor.count);

        // Copy the attribute value over from the glTF buffer into the appropriate vertex field.
        const uint8_t* bufferPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        for (size_t i = 0; i < accessor.count; i++, bufferPtr += stride) {
            (primitive.Vertices[i].*field) = *reinterpret_cast<const XrVector3f*>(bufferPtr);
        }
    }

    // Load a primitive's (vertex) attributes. Vertex attributes can be positions, normals, tangents, texture coordinates, colors, and more.
    void LoadAttributeAccessor(const tinygltf::Model& gltfModel, const std::string& attributeName, int accessorId,
                               GltfHelper::Primitive& primitive)
    {
        const auto& accessor = gltfModel.accessors.at(accessorId);

        if (accessor.bufferView == -1) {
            throw std::runtime_error("Accessor for primitive attribute specifies no bufferview.");
        }

        // WARNING: This version of the tinygltf loader does not support sparse accessors, so neither does this renderer.

        const tinygltf::BufferView& bufferView = gltfModel.bufferViews.at(accessor.bufferView);
        if (bufferView.target != TINYGLTF_TARGET_ARRAY_BUFFER &&
            bufferView.target != 0)  // Allow 0 (not specified) even though spec doesn't seem to allow this (BoomBox GLB fails)
        {
            throw std::runtime_error("Accessor for primitive attribute uses bufferview with invalid 'target' type.");
        }

        const tinygltf::Buffer& buffer = gltfModel.buffers.at(bufferView.buffer);

        if (attributeName.compare("POSITION") == 0) {
            ReadVec3ToVertexField<&GltfHelper::Vertex::Position>(accessor, bufferView, buffer, primitive);
        }
        else if (attributeName.compare("NORMAL") == 0) {
            ReadVec3ToVertexField<&GltfHelper::Vertex::Normal>(accessor, bufferView, buffer, primitive);
        }
        else if (attributeName.compare("TANGENT") == 0) {
            ReadTangentToVertexField(accessor, bufferView, buffer, primitive);
        }
        else if (attributeName.compare("TEXCOORD_0") == 0) {
            ReadTexCoordToVertexField<&GltfHelper::Vertex::TexCoord0>(accessor, bufferView, buffer, primitive);
        }
        else if (attributeName.compare("COLOR_0") == 0) {
            ReadColorToVertexField<&GltfHelper::Vertex::Color0>(accessor, bufferView, buffer, primitive);
        }
        else {
            return;  // Ignore unsupported vertex accessors like TEXCOORD_1.
        }
    }

    // Reads index data from a glTF primitive into a GltfHelper Primitive. glTF indices may be 8bit, 16bit or 32bit integers.
    // This will coalesce indices from the source type(s) into a 32bit integer.
    template <typename TSrcIndex>
    void ReadIndices(const tinygltf::Accessor& accessor, const tinygltf::BufferView& bufferView, const tinygltf::Buffer& buffer,
                     GltfHelper::Primitive& primitive)
    {
        if (bufferView.target != TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER &&
            bufferView.target != 0)  // Allow 0 (not specified) even though spec doesn't seem to allow this (BoomBox GLB fails)
        {
            throw std::runtime_error("Accessor for indices uses bufferview with invalid 'target' type.");
        }

        constexpr size_t ComponentSizeBytes = sizeof(TSrcIndex);
        if (bufferView.byteStride != 0 && bufferView.byteStride != ComponentSizeBytes)  // Index buffer must be packed per glTF spec.
        {
            throw std::runtime_error("Accessor for indices uses bufferview with invalid 'byteStride'.");
        }

        ValidateAccessor(accessor, bufferView, buffer, ComponentSizeBytes, ComponentSizeBytes);

        if ((accessor.count % 3) != 0)  // Since only triangles are supported, enforce that the number of indices is divisible by 3.
        {
            throw std::runtime_error("Unexpected number of indices for triangle primitive");
        }

        const TSrcIndex* indexBuffer = reinterpret_cast<const TSrcIndex*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
        for (uint32_t i = 0; i < accessor.count; i++) {
            primitive.Indices.push_back(*(indexBuffer + i));
        }
    }

    // Reads index data from a glTF primitive into a GltfHelper Primitive.
    void LoadIndexAccessor(const tinygltf::Model& gltfModel, const tinygltf::Accessor& accessor, GltfHelper::Primitive& primitive)
    {
        if (accessor.type != TINYGLTF_TYPE_SCALAR) {
            throw std::runtime_error("Accessor for indices specifies invalid 'type'.");
        }

        if (accessor.bufferView == -1) {
            throw std::runtime_error("Index accessor without bufferView is currently not supported.");
        }

        const tinygltf::BufferView& bufferView = gltfModel.bufferViews.at(accessor.bufferView);
        const tinygltf::Buffer& buffer = gltfModel.buffers.at(bufferView.buffer);

        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            ReadIndices<uint8_t>(accessor, bufferView, buffer, primitive);
        }
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            ReadIndices<uint16_t>(accessor, bufferView, buffer, primitive);
        }
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            ReadIndices<uint32_t>(accessor, bufferView, buffer, primitive);
        }
        else {
            throw std::runtime_error("Accessor for indices specifies invalid 'componentType'.");
        }
    }
}  // namespace

namespace GltfHelper
{
    XrMatrix4x4f ReadNodeLocalTransform(const tinygltf::Node& gltfNode)
    {
        // A node may specify either a 4x4 matrix or TRS (Translation-Rotation-Scale) values, but not both.
        if (gltfNode.matrix.size() == 16) {
            constexpr XrMatrix4x4f identityMatrix = Matrix::Identity;
            return Double4x4ToXrMatrix4x4f(identityMatrix, gltfNode.matrix);
        }
        else {
            // No matrix is present, so construct a matrix from the TRS values (each one is optional).
            XrVector3f translation = DoublesToXrVector3f(XrVector3f{0, 0, 0}, gltfNode.translation);
            XrQuaternionf rotation = DoublesToXrQuaternionf(XrQuaternionf{0, 0, 0, 1}, gltfNode.rotation);
            XrVector3f scale = DoublesToXrVector3f(XrVector3f{1, 1, 1}, gltfNode.scale);
            return Matrix::FromTranslationRotationScale(translation, rotation, scale);
        }
    }

    Primitive ReadPrimitive(const tinygltf::Model& gltfModel, const tinygltf::Primitive& gltfPrimitive)
    {
        if (gltfPrimitive.mode != TINYGLTF_MODE_TRIANGLES) {
            throw std::runtime_error("Unsupported primitive mode. Only TINYGLTF_MODE_TRIANGLES is supported.");
        }

        Primitive primitive;

        // glTF vertex data is stored in an attribute dictionary. Loop through each attribute and insert it into the GltfHelper primitive.
        for (const auto& attribute : gltfPrimitive.attributes) {
            LoadAttributeAccessor(gltfModel, attribute.first /* attribute name */, attribute.second /* accessor index */, primitive);
        }

        if (gltfPrimitive.indices != -1) {
            // If indices are specified for the glTF primitive, read them into the GltfHelper Primitive.
            LoadIndexAccessor(gltfModel, gltfModel.accessors.at(gltfPrimitive.indices), primitive);
        }
        else {
            // When indices is not defined, the primitives should be rendered without indices using drawArrays()
            // This is the equivalent to having an index in sequence for each vertex.
            const uint32_t vertexCount = (uint32_t)primitive.Vertices.size();
            if ((vertexCount % 3) != 0) {
                throw std::runtime_error("Non-indexed triangle-based primitive must have number of vertices divisible by 3.");
            }

            primitive.Indices.reserve(primitive.Indices.size() + vertexCount);
            for (uint32_t i = 0; i < vertexCount; i++) {
                primitive.Indices.push_back(i);
            }
        }

        // If normals are missing, compute flat normals. Normals must be computed before tangents.
        if (gltfPrimitive.attributes.find("NORMAL") == std::end(gltfPrimitive.attributes)) {
            ComputeTriangleNormals(primitive);
        }

        // If tangents are missing, compute tangents.
        if (gltfPrimitive.attributes.find("TANGENT") == std::end(gltfPrimitive.attributes)) {
            ComputeTriangleTangents(primitive);
        }

        // If colors are missing, set to default.
        if (gltfPrimitive.attributes.find("COLOR_0") == std::end(gltfPrimitive.attributes)) {
            for (GltfHelper::Vertex& vertex : primitive.Vertices) {
                vertex.Color0 = {1, 1, 1, 1};
            }
        }

        return primitive;
    }

    const Primitive& PrimitiveCache::ReadPrimitive(const tinygltf::Primitive& gltfPrimitive)
    {
        PrimitiveAttributesVec attributesVec{};
        for (auto const& attr : gltfPrimitive.attributes) {
            attributesVec.push_back(std::make_pair(attr.first, attr.second));
        }
        PrimitiveKey key = std::make_pair(attributesVec, gltfPrimitive.indices);
        auto primitiveIt = m_primitiveCache.find(key);
        if (primitiveIt != m_primitiveCache.end()) {
            return primitiveIt->second;
        }

        Primitive primitive = GltfHelper::ReadPrimitive(m_model, gltfPrimitive);
        return m_primitiveCache.emplace(key, std::move(primitive)).first->second;
    }

    Material ReadMaterial(const tinygltf::Model& gltfModel, const tinygltf::Material& gltfMaterial)
    {
        // Read an optional VEC4 parameter if available, otherwise use the default.
        auto readParameterFactorAsColor4 = [](const tinygltf::ParameterMap& parameters, const char* name, const XrColor4f& defaultValue) {
            auto c = parameters.find(name);
            return (c != parameters.end() && c->second.number_array.size() == 4)
                       ? XrColor4f{(float)c->second.number_array[0], (float)c->second.number_array[1], (float)c->second.number_array[2],
                                   (float)c->second.number_array[3]}
                       : defaultValue;
        };

        // Read an optional VEC3 parameter if available, otherwise use the default.
        auto readParameterFactorAsVec3 = [](const tinygltf::ParameterMap& parameters, const char* name, const XrVector3f& defaultValue) {
            auto c = parameters.find(name);
            return (c != parameters.end() && c->second.number_array.size() == 3)
                       ? XrVector3f{(float)c->second.number_array[0], (float)c->second.number_array[1], (float)c->second.number_array[2]}
                       : defaultValue;
        };

        // Read an optional scalar parameter if available, otherwise use the default.
        auto readParameterFactorAsScalar = [](const tinygltf::ParameterMap& parameters, const char* name, double defaultValue) {
            auto c = parameters.find(name);
            return (c != parameters.end() && c->second.has_number_value) ? c->second.number_value : defaultValue;
        };

        // Read an optional boolean parameter if available, otherwise use the default.
        auto readParameterFactorAsBoolean = [](const tinygltf::ParameterMap& parameters, const char* name, bool defaultValue) {
            auto c = parameters.find(name);
            return c != parameters.end() ? c->second.bool_value : defaultValue;
        };

        // Read an optional boolean parameter if available, otherwise use the default.
        auto readParameterFactorAsString = [](const tinygltf::ParameterMap& parameters, const char* name, const char* defaultValue) {
            auto c = parameters.find(name);
            return c != parameters.end() ? c->second.string_value : defaultValue;
        };

        // Read a specific texture from a tinygltf material parameter map.
        auto loadTextureFromParameter = [&](const tinygltf::ParameterMap& parameterMap, const char* textureName) {
            Material::Texture texture{};

            const auto& textureIt = parameterMap.find(textureName);
            if (textureIt != std::end(parameterMap)) {
                const int textureIndex = (int)textureIt->second.json_double_value.at("index");
                const tinygltf::Texture& gltfTexture = gltfModel.textures.at(textureIndex);
                if (gltfTexture.source != -1) {
                    texture.Image = &gltfModel.images.at(gltfTexture.source);
                }

                if (gltfTexture.sampler != -1) {
                    texture.Sampler = &gltfModel.samplers.at(gltfTexture.sampler);
                }
            }

            return texture;
        };

        // Read a scalar value from a tinygltf material parameter map.
        auto loadScalarFromParameter = [&](const tinygltf::ParameterMap& parameterMap, const char* name, const char* scalarField,
                                           double defaultValue) {
            const auto& textureIt = parameterMap.find(name);
            if (textureIt != std::end(parameterMap)) {
                const auto& jsonDoubleValues = textureIt->second.json_double_value;
                const auto& jsonDoubleIt = jsonDoubleValues.find(scalarField);
                if (jsonDoubleIt != std::end(jsonDoubleValues)) {
                    return jsonDoubleIt->second;
                }
            }

            return defaultValue;
        };

        //
        // Read all of the optional material fields from the tinygltf object model and store them in a GltfHelper Material object
        // coalesced with proper defaults when needed.
        //
        Material material;

        material.BaseColorTexture = loadTextureFromParameter(gltfMaterial.values, "baseColorTexture");
        material.BaseColorFactor = readParameterFactorAsColor4(gltfMaterial.values, "baseColorFactor", XrColor4f{1, 1, 1, 1});

        material.MetallicRoughnessTexture = loadTextureFromParameter(gltfMaterial.values, "metallicRoughnessTexture");
        material.MetallicFactor = (float)readParameterFactorAsScalar(gltfMaterial.values, "metallicFactor", 1);
        material.RoughnessFactor = (float)readParameterFactorAsScalar(gltfMaterial.values, "roughnessFactor", 1);

        material.EmissiveTexture = loadTextureFromParameter(gltfMaterial.additionalValues, "emissiveTexture");
        material.EmissiveFactor = readParameterFactorAsVec3(gltfMaterial.additionalValues, "emissiveFactor", XrVector3f{0, 0, 0});

        material.NormalTexture = loadTextureFromParameter(gltfMaterial.additionalValues, "normalTexture");
        material.NormalScale = (float)loadScalarFromParameter(gltfMaterial.additionalValues, "normalTexture", "scale", 1.0);

        material.OcclusionTexture = loadTextureFromParameter(gltfMaterial.additionalValues, "occlusionTexture");
        material.OcclusionStrength = (float)loadScalarFromParameter(gltfMaterial.additionalValues, "occlusionTexture", "strength", 1.0);

        auto alphaMode = readParameterFactorAsString(gltfMaterial.additionalValues, "alphaMode", "OPAQUE");
        material.AlphaMode = alphaMode == "MASK"    ? AlphaModeType::Mask
                             : alphaMode == "BLEND" ? AlphaModeType::Blend
                                                    : AlphaModeType::Opaque;
        material.DoubleSided = readParameterFactorAsBoolean(gltfMaterial.additionalValues, "doubleSided", false);
        material.AlphaCutoff = (float)readParameterFactorAsScalar(gltfMaterial.additionalValues, "alphaCutoff", 0.5f);

        return material;
    }

    static bool CaseInsensitiveCompare(char a, char b)
    {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    }

    static bool IsKTX2(const tinygltf::Image& image)
    {
        if (!image.mimeType.empty()) {
            return image.mimeType == "image/ktx2";
        }
        if (!image.name.empty()) {
            static const std::string ext{".ktx2"};
            return image.name.size() >= ext.size() &&
                   std::equal(ext.begin(), ext.end(), image.name.end() - ext.size(), image.name.end(), CaseInsensitiveCompare);
        }
        return false;
    }

    bool PassThroughKTX2(tinygltf::Image* image, const int image_idx, std::string* err, std::string* warn, int req_width, int req_height,
                         const unsigned char* bytes, int size, void* /* user_data */) noexcept
    {
        if (image == nullptr || bytes == nullptr) {
            if (err) {
                (*err) += "PassThroughKTX2 received nullptr image or bytes for image[" + std::to_string(image_idx) + "] name = \"" +
                          image->name + "\".\n";
            }
            return false;
        }

        if (!IsKTX2(*image)) {
            // forward to base implementation if the image isn't ktx2
            return tinygltf::LoadImageData(image, image_idx, err, warn, req_width, req_height, bytes, size, nullptr);
        }

        image->image = std::vector<unsigned char>(bytes, bytes + size);

        image->as_is = true;
        return true;
    }

    Conformance::Image::Image ReadImageAsRGBA(const tinygltf::Image& image, bool sRGB,
                                              span<const Conformance::Image::FormatParams> supportedFormats,
                                              std::vector<uint8_t>& tempBuffer)
    {
        namespace Image = Conformance::Image;

        if (image.as_is) {
            throw std::logic_error("ReadImageAsRGBA called on un-decoded image");
        }

        assert(image.component >= 3);
        assert(image.component <= 4);

        auto colorSpaceType = sRGB ? Image::ColorSpaceType::sRGB : Image::ColorSpaceType::Linear;
        auto formatParams = FindRawFormat((Image::Channels)image.component, colorSpaceType, supportedFormats);

        auto metadata = Image::ImageLevelMetadata::MakeUncompressed(image.width, image.height);

        if (image.width < 1 || image.height < 1) {
            // The image vector (image.image) will be populated if the image was successfully loaded by glTF.
            throw std::runtime_error("Image has zero or negative dimension");
        }

        if ((size_t)(image.width * image.height * image.component) != image.image.size()) {
            throw std::runtime_error("Invalid image buffer size");
        }

        // Not supported: STBI_grey (DXGI_FORMAT_R8_UNORM?) and STBI_grey_alpha.
        if (image.component == 3 && formatParams.channels == Image::Channels::RGBA) {
            // Convert RGB to RGBA.
            tempBuffer.resize(image.width * image.height * 4);
            for (int y = 0; y < image.height; ++y) {
                const uint8_t* src = image.image.data() + y * image.width * 3;
                uint8_t* dest = tempBuffer.data() + y * image.width * 4;
                for (int x = image.width - 1; x >= 0; --x, src += 3, dest += 4) {
                    dest[0] = src[0];
                    dest[1] = src[1];
                    dest[2] = src[2];
                    dest[3] = 255;
                }
            }

            std::vector<Image::ImageLevel> imageLevels = {Image::ImageLevel{metadata, tempBuffer}};
            return Image::Image{formatParams, imageLevels};
        }
        else if (image.component == formatParams.channels) {
            // Already same channel count, no conversion needed
            // static_assert(sizeof(decltype(image.image)::value_type) == sizeof(uint8_t)); // C++17
            return Conformance::Image::Image{formatParams, {{metadata, span<uint8_t>{(uint8_t*)image.image.data(), image.image.size()}}}};
        }
        else {
            throw std::runtime_error("Unexpected number of image components");
        }
    }

    Conformance::Image::Image DecodeImageKTX2(const tinygltf::Image& image, bool sRGB,
                                              span<const Conformance::Image::FormatParams> supportedFormats,
                                              std::vector<uint8_t>& tempBuffer)
    {
        if (!IsKTX2(image)) {
            throw std::logic_error("DecodeImageKTX2 called on un-decoded image");
        }

        if (!image.as_is) {
            throw std::logic_error("DecodeImageKTX2 called on non-as-is image");
        }

        return Conformance::Image::Image::LoadAndTranscodeKTX2(image.image, sRGB, supportedFormats, tempBuffer, image.name.c_str());
    }

    Conformance::Image::Image DecodeImage(const tinygltf::Image& image, bool sRGB,
                                          span<const Conformance::Image::FormatParams> supportedFormats, std::vector<uint8_t>& tempBuffer)
    {
        if (!image.as_is) {
            return ReadImageAsRGBA(image, sRGB, supportedFormats, tempBuffer);
        }
        if (IsKTX2(image)) {
            return DecodeImageKTX2(image, sRGB, supportedFormats, tempBuffer);
        }
        throw std::logic_error("Unknown as-is image type: IsKTX2 returned false.");
    }
}  // namespace GltfHelper
