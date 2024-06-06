// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

//
// Shared data types and functions used throughout the Pbr rendering library.
//

#pragma once

#include <openxr/openxr.h>

#include <array>
#include <set>
#include <stddef.h>
#include <stdint.h>
#include <tuple>
#include <vector>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#endif

namespace tinygltf
{
    struct Image;
}
namespace Pbr
{
    namespace Internal
    {
#ifdef _WIN32
        void ThrowIfFailed(HRESULT hr);
#endif
        void ThrowIf(bool cond, const char* msg);
    }  // namespace Internal

    using NodeIndex_t = uint16_t;  // This type must align with the type used in the Pbr shaders.

    // Indicates an invalid node index (similar to std::variant_npos)
    static constexpr Pbr::NodeIndex_t NodeIndex_npos = static_cast<Pbr::NodeIndex_t>(-1);
    static constexpr Pbr::NodeIndex_t RootNodeIndex = 0;

    // These colors are in linear color space unless otherwise specified.
    using RGBAColor = XrColor4f;
    using RGBColor = XrVector3f;

    using ImageKey = std::tuple<const tinygltf::Image*, bool>;  // Item1 is a pointer to the image, Item2 is sRGB.

    // // DirectX::Colors are in sRGB color space.
    // RGBAColor FromSRGB(DirectX::XMVECTOR color);

    namespace RGBA
    {
        constexpr RGBAColor White{1, 1, 1, 1};
        constexpr RGBAColor Black{0, 0, 0, 1};
        constexpr RGBAColor FlatNormal{0.5f, 0.5f, 1, 1};
        constexpr RGBAColor Transparent{0, 0, 0, 0};
    }  // namespace RGBA

    namespace RGB
    {
        constexpr RGBColor White{1, 1, 1};
        constexpr RGBColor Black{0, 0, 0};
    }  // namespace RGB

    // Vertex structure used by the PBR shaders.
    struct Vertex
    {
        XrVector3f Position;
        XrVector3f Normal;
        XrVector4f Tangent;
        XrColor4f Color0;
        XrVector2f TexCoord0;
        NodeIndex_t ModelTransformIndex;  // Index into the node transforms
    };

    struct PrimitiveBuilder
    {
        std::vector<Pbr::Vertex> Vertices;
        std::vector<uint32_t> Indices;
        std::set<NodeIndex_t> NodeIndices;

        PrimitiveBuilder& AddSphere(float diameter, uint32_t tessellation, Pbr::NodeIndex_t transformIndex = Pbr::RootNodeIndex,
                                    RGBAColor vertexColor = RGBA::White);
        PrimitiveBuilder& AddCube(float sideLength, Pbr::NodeIndex_t transformIndex = Pbr::RootNodeIndex,
                                  RGBAColor vertexColor = RGBA::White);
        PrimitiveBuilder& AddCube(XrVector3f sideLengths, Pbr::NodeIndex_t transformIndex = Pbr::RootNodeIndex,
                                  RGBAColor vertexColor = RGBA::White);
        PrimitiveBuilder& AddCube(XrVector3f sideLengths, XrVector3f translation, Pbr::NodeIndex_t transformIndex = Pbr::RootNodeIndex,
                                  RGBAColor vertexColor = RGBA::White);
        PrimitiveBuilder& AddQuad(XrVector2f sideLengths, XrVector2f textureCoord = {1, 1},
                                  Pbr::NodeIndex_t transformIndex = Pbr::RootNodeIndex, RGBAColor vertexColor = RGBA::White);

        std::vector<NodeIndex_t> NodeIndicesVector() const;
    };
}  // namespace Pbr
