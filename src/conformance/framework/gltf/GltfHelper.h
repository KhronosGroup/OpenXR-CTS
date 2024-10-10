// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

// GltfHelper provides additional glTF parsing functionality, built on top of tinygltf.
// This library has no rendering dependencies and can be used for any purpose, such as
// format transcoding or by a rendering engine.

#pragma once

#include <utilities/image.h>

#include "common/xr_linear.h"

#include <nonstd/span.hpp>
#include <openxr/openxr.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tinygltf
{
    class Node;
    class Model;
    struct Primitive;
    struct Material;
    struct Image;
    struct Sampler;
}  // namespace tinygltf

namespace GltfHelper
{
    using nonstd::span;

    // Vertex data.
    struct Vertex
    {
        XrVector3f Position;
        XrVector3f Normal;
        XrVector4f Tangent;
        XrVector2f TexCoord0;
        XrColor4f Color0;
        // Note: This implementation does not currently support TexCoord1 attributes.
    };

    // A primitive is a collection of vertices and indices.
    struct Primitive
    {
        std::vector<Vertex> Vertices;
        std::vector<uint32_t> Indices;
    };

    enum class AlphaModeType
    {
        Opaque,
        Mask,
        Blend
    };

    // Metallic-roughness material definition.
    struct Material
    {
        struct Texture
        {
            const tinygltf::Image* Image;
            const tinygltf::Sampler* Sampler;
        };

        Texture BaseColorTexture;
        Texture MetallicRoughnessTexture;
        Texture EmissiveTexture;
        Texture NormalTexture;
        Texture OcclusionTexture;

        XrColor4f BaseColorFactor;
        float MetallicFactor;
        float RoughnessFactor;
        XrVector3f EmissiveFactor;

        float NormalScale;
        float OcclusionStrength;

        AlphaModeType AlphaMode;
        float AlphaCutoff;
        bool DoubleSided;
    };

    class PrimitiveCache
    {
    public:
        explicit PrimitiveCache(const tinygltf::Model& gltfModel) : m_model(gltfModel)
        {
        }
        const Primitive& ReadPrimitive(const tinygltf::Primitive& gltfPrimitive);

    private:
        using PrimitiveAttributesVec = std::vector<std::pair<std::string, int>>;  // first is name, second is accessor
        using PrimitiveKey = std::pair<PrimitiveAttributesVec, int>;              // first is attributes, second is indices
        std::reference_wrapper<const tinygltf::Model> m_model;
        std::map<PrimitiveKey, Primitive> m_primitiveCache{};
    };

    // Reads the "transform" or "TRS" data for a Node as an XrMatrix4x4f.
    XrMatrix4x4f ReadNodeLocalTransform(const tinygltf::Node& gltfNode);

    // Parses the primitive attributes and indices from the glTF accessors/bufferviews/buffers into a common simplified data structure, the Primitive.
    Primitive ReadPrimitive(const tinygltf::Model& gltfModel, const tinygltf::Primitive& gltfPrimitive);

    // Parses the material values into a simplified data structure, the Material.
    Material ReadMaterial(const tinygltf::Model& gltfModel, const tinygltf::Material& gltfMaterial);

    // Passes image data throguh as-is if the image name ends in ".ktx2" or the mime type is "image/ktx2". Otherwise forwards to tinygltf.
    bool PassThroughKTX2(tinygltf::Image* image, const int image_idx, std::string* err, std::string* warn, int req_width, int req_height,
                         const unsigned char* bytes, int size, void* user_data) noexcept;

    /// For images identified as KTX2, pass their data through as-is, setting the as-is flag.
    bool PassThroughKTX2(tinygltf::Image* image, const int image_idx, std::string* err, std::string* warn, int req_width, int req_height,
                         const unsigned char* bytes, int size, void* /* user_data */) noexcept;

    /// Converts the image to RGBA if necessary. Requires a temporary buffer only if it needs to be converted.
    Conformance::Image::Image DecodeImage(const tinygltf::Image& image, bool sRGB,
                                          span<const Conformance::Image::FormatParams> supportedFormats, std::vector<uint8_t>& tempBuffer);

    /// Used in DecodeImage. Decode an image that is in RGBA format and not as-is.
    Conformance::Image::Image ReadImageAsRGBA(const tinygltf::Image& image, bool sRGB,
                                              span<const Conformance::Image::FormatParams> supportedFormats,
                                              std::vector<uint8_t>& tempBuffer);

    /// Used in DecodeImage. Decode an image that is as-is, and has been identified as KTX2.
    Conformance::Image::Image DecodeImageKTX2(const tinygltf::Image& image, bool sRGB,
                                              span<const Conformance::Image::FormatParams> supportedFormats,
                                              std::vector<uint8_t>& tempBuffer);
}  // namespace GltfHelper
