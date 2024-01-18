// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "PbrCommon.h"

#include "common/xr_linear.h"

#include <openxr/openxr.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stddef.h>
#include <stdexcept>
#include <string>

namespace Pbr
{
    namespace Internal
    {
        // for later consolidation
        void ThrowIf(bool cond, const char* msg)
        {
            if (cond) {
                std::stringstream ss;
                ss << std::hex << "Error in PBR renderer: " << msg;
                throw std::runtime_error(ss.str().c_str());
            }
        }
    }  // namespace Internal

    static inline float ChannelFromSRGB(float srgb)
    {
        if (srgb < 0.04045f)
            return srgb / 12.92f;
        return std::pow((srgb + .055f) / 1.055f, 2.4f);
    }

    RGBAColor FromSRGB(XrColor4f color)
    {
        RGBAColor linearColor{};
        linearColor.r = ChannelFromSRGB(color.r);
        linearColor.g = ChannelFromSRGB(color.g);
        linearColor.b = ChannelFromSRGB(color.b);
        linearColor.a = color.a;
        return linearColor;
    }

    // Based on code from DirectXTK
    PrimitiveBuilder& PrimitiveBuilder::AddSphere(float diameter, uint32_t tessellation, Pbr::NodeIndex_t transformIndex,
                                                  RGBAColor vertexColor)
    {
        if (tessellation < 3) {
            throw std::out_of_range("tessellation parameter out of range");
        }

        const uint32_t verticalSegments = tessellation;
        const uint32_t horizontalSegments = tessellation * 2;

        const float radius = diameter / 2;

        const uint32_t startVertexIndex = (uint32_t)Vertices.size();

        // Create rings of vertices at progressively higher latitudes.
        for (uint32_t i = 0; i <= verticalSegments; i++) {
            const float v = 1 - (float)i / verticalSegments;

            const float latitude = (i * MATH_PI / verticalSegments) - (MATH_PI * 0.5f);
            float dy = std::sin(latitude);
            float dxz = std::cos(latitude);

            // Create a single ring of vertices at this latitude.
            for (uint32_t j = 0; j <= horizontalSegments; j++) {
                const float longitude = j * (MATH_PI * 2.f) / horizontalSegments;
                float dx = std::sin(longitude);
                float dz = std::cos(longitude);
                dx *= dxz;
                dz *= dxz;

                // Compute tangent at 90 degrees along longitude.
                // todo: is this supposed to be PI/2?
                float tdx = std::sin(longitude + MATH_PI);
                float tdz = std::cos(longitude + MATH_PI);
                tdx *= dxz;
                tdz *= dxz;

                const float u = (float)j / horizontalSegments;

                Pbr::Vertex vert;
                vert.Normal = {dx, dy, dz};
                XrVector3f_Scale(&vert.Position, &vert.Normal, radius);
                vert.Tangent = {tdx, 0, tdz, 0};
                vert.TexCoord0 = {u, v};

                vert.Color0 = vertexColor;
                vert.ModelTransformIndex = transformIndex;
                Vertices.push_back(vert);
            }
        }

        // Fill the index buffer with triangles joining each pair of latitude rings.
        const uint32_t stride = horizontalSegments + 1;
        for (uint32_t i = 0; i < verticalSegments; i++) {
            for (uint32_t j = 0; j <= horizontalSegments; j++) {
                uint32_t nextI = i + 1;
                uint32_t nextJ = (j + 1) % stride;

                Indices.push_back(startVertexIndex + (i * stride + j));
                Indices.push_back(startVertexIndex + (nextI * stride + j));
                Indices.push_back(startVertexIndex + (i * stride + nextJ));

                Indices.push_back(startVertexIndex + (i * stride + nextJ));
                Indices.push_back(startVertexIndex + (nextI * stride + j));
                Indices.push_back(startVertexIndex + (nextI * stride + nextJ));
            }
        }

        return *this;
    }

    // Based on code from DirectXTK
    PrimitiveBuilder& PrimitiveBuilder::AddCube(XrVector3f sideLengths, XrVector3f translation, Pbr::NodeIndex_t transformIndex,
                                                RGBAColor vertexColor)
    {
        // A box has six faces, each one pointing in a different direction.
        const int FaceCount = 6;

        static const XrVector3f faceNormals[FaceCount] = {
            {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
        };

        static const XrVector2f textureCoordinates[4] = {
            {1, 0},
            {1, 1},
            {0, 1},
            {0, 0},
        };

        // Create each face in turn.
        const XrVector3f sideLengthHalfVector = {sideLengths.x / 2, sideLengths.y / 2, sideLengths.z / 2};

        for (int i = 0; i < FaceCount; i++) {
            XrVector3f normal = faceNormals[i];

            // Get two vectors perpendicular both to the face normal and to each other.
            XrVector3f basis = (i >= 4) ? XrVector3f{0, 0, 1} : XrVector3f{0, 1, 0};

            XrVector3f side1;
            XrVector3f_Cross(&side1, &normal, &basis);
            XrVector3f side2;
            XrVector3f_Cross(&side2, &normal, &side1);

            // Six indices (two triangles) per face.
            size_t vbase = Vertices.size();
            Indices.push_back((uint32_t)vbase + 0);
            Indices.push_back((uint32_t)vbase + 1);
            Indices.push_back((uint32_t)vbase + 2);

            Indices.push_back((uint32_t)vbase + 0);
            Indices.push_back((uint32_t)vbase + 2);
            Indices.push_back((uint32_t)vbase + 3);

            XrVector3f positions[4];
            for (int j = 0; j < 4; j++) {
                // const XrVector3f positions[4] = {{(normal - side1 - side2) * sideLengthHalfVector},
                //                                 {(normal - side1 + side2) * sideLengthHalfVector},
                //                                 {(normal + side1 + side2) * sideLengthHalfVector},
                //                                 {(normal + side1 - side2) * sideLengthHalfVector}};
                XrVector3f offset;
                if ((j % 2) == 0) {
                    XrVector3f_Add(&offset, &side1, &side2);
                }
                else {
                    XrVector3f_Sub(&offset, &side1, &side2);
                }
                XrVector3f offsetNormal;
                if (j >= 2) {
                    XrVector3f_Sub(&offsetNormal, &normal, &offset);
                }
                else {
                    XrVector3f_Add(&offsetNormal, &normal, &offset);
                }
                positions[j].x = offsetNormal.x * sideLengthHalfVector.x;
                positions[j].y = offsetNormal.y * sideLengthHalfVector.y;
                positions[j].z = offsetNormal.z * sideLengthHalfVector.z;
            }

            for (int j = 0; j < 4; j++) {
                Pbr::Vertex vert;
                XrVector3f_Add(&vert.Position, &positions[j], &translation);
                vert.Normal = normal;
                // 1. might be wrong, just getting it building
                vert.Tangent = {side1.x, side1.y, side1.z, 1.};  // TODO arbitrarily picked side 1
                vert.TexCoord0 = textureCoordinates[j];
                vert.Color0 = vertexColor;
                vert.ModelTransformIndex = transformIndex;
                Vertices.push_back(vert);
            }
        }

        return *this;
    }

    PrimitiveBuilder& PrimitiveBuilder::AddCube(XrVector3f sideLengths, Pbr::NodeIndex_t transformIndex, RGBAColor vertexColor)
    {
        return AddCube(sideLengths, {0, 0, 0}, transformIndex, vertexColor);
    }

    PrimitiveBuilder& PrimitiveBuilder::AddCube(float sideLength, Pbr::NodeIndex_t transformIndex, RGBAColor vertexColor)
    {
        return AddCube(XrVector3f{sideLength, sideLength, sideLength}, transformIndex, vertexColor);
    }

    PrimitiveBuilder& PrimitiveBuilder::AddQuad(XrVector2f sideLengths, XrVector2f textureCoord, Pbr::NodeIndex_t transformIndex,
                                                RGBAColor vertexColor)
    {
        const XrVector2f halfSideLength = {sideLengths.x / 2, sideLengths.y / 2};
        const std::array<XrVector3f, 4> vertices = {{{-halfSideLength.x, -halfSideLength.y, 0},   // LB
                                                     {-halfSideLength.x, halfSideLength.y, 0},    // LT
                                                     {halfSideLength.x, halfSideLength.y, 0},     // RT
                                                     {halfSideLength.x, -halfSideLength.y, 0}}};  // RB
        const XrVector2f uvs[4] = {
            {0, textureCoord.y},
            {0, 0},
            {textureCoord.x, 0},
            {textureCoord.x, textureCoord.y},
        };

        // Two triangles.
        auto vbase = static_cast<uint32_t>(Vertices.size());
        Indices.push_back(vbase + 0);
        Indices.push_back(vbase + 1);
        Indices.push_back(vbase + 2);
        Indices.push_back(vbase + 0);
        Indices.push_back(vbase + 2);
        Indices.push_back(vbase + 3);

        Pbr::Vertex vert;
        vert.Normal = {0, 0, 1};
        vert.Tangent = {1, 0, 0, 0};
        vert.Color0 = vertexColor;
        vert.ModelTransformIndex = transformIndex;
        for (size_t j = 0; j < vertices.size(); j++) {
            vert.Position = vertices[j];
            vert.TexCoord0 = uvs[j];
            Vertices.push_back(vert);
        }
        return *this;
    }
}  // namespace Pbr
