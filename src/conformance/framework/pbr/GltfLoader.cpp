// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "GltfLoader.h"

#include "IResources.h"
#include "PbrCommon.h"
#include "PbrMaterial.h"
#include "PbrModel.h"
#include "PbrSharedState.h"

#include "../gltf/GltfHelper.h"

#include "common/xr_linear.h"

#include <openxr/openxr.h>
#include <tinygltf/tiny_gltf.h>

#include <cstdint>
#include <limits>
#include <map>
#include <stddef.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    // Maps a glTF material to a PrimitiveBuilder. This optimization combines all primitives which use
    // the same material into a single primitive for reduced draw calls. Each primitive's vertex specifies
    // which node it corresponds to any appropriate node transformation be happen in the shader.
    using PrimitiveBuilderMap = std::map<int, Pbr::PrimitiveBuilder>;

    // Load a glTF node from the tinygltf object model. This will process the node's mesh (if specified) and then recursively load the child
    // nodes too.
    void LoadNode(Pbr::NodeIndex_t parentNodeIndex, const tinygltf::Model& gltfModel, int nodeId,
                  GltfHelper::PrimitiveCache& primitiveCache, PrimitiveBuilderMap& primitiveBuilderMap, Pbr::Model& model)
    {
        const tinygltf::Node& gltfNode = gltfModel.nodes.at(nodeId);

        // Read the local transform for this node and add it into the Pbr Model.
        const XrMatrix4x4f nodeLocalTransform = GltfHelper::ReadNodeLocalTransform(gltfNode);
        const Pbr::NodeIndex_t transformIndex = model.AddNode(nodeLocalTransform, parentNodeIndex, gltfNode.name);

        if (gltfNode.mesh != -1)  // Load the node's optional mesh when specified.
        {
            // A glTF mesh is composed of primitives.
            const tinygltf::Mesh& gltfMesh = gltfModel.meshes.at(gltfNode.mesh);
            for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives) {
                // Read the primitive data from the glTF buffers.
                const GltfHelper::Primitive primitive = primitiveCache.ReadPrimitive(gltfPrimitive);

                // Insert or append the primitive into the PBR primitive builder. Primitives which use the same
                // material are appended to reduce the number of draw calls.
                Pbr::PrimitiveBuilder& primitiveBuilder = primitiveBuilderMap[gltfPrimitive.material];

                // Use the starting offset for vertices and indices since multiple glTF primitives can
                // be put into the same primitive builder.
                const uint32_t startVertex = (uint32_t)primitiveBuilder.Vertices.size();
                const uint32_t startIndex = (uint32_t)primitiveBuilder.Indices.size();

                // Convert the GltfHelper vertices into the PBR vertex format.
                primitiveBuilder.Vertices.resize(startVertex + primitive.Vertices.size());
                for (size_t i = 0; i < primitive.Vertices.size(); i++) {
                    const GltfHelper::Vertex& vertex = primitive.Vertices[i];
                    Pbr::Vertex pbrVertex;
                    pbrVertex.Position = vertex.Position;
                    pbrVertex.Normal = vertex.Normal;
                    pbrVertex.Tangent = vertex.Tangent;
                    pbrVertex.Color0 = vertex.Color0;
                    pbrVertex.TexCoord0 = vertex.TexCoord0;
                    pbrVertex.ModelTransformIndex = transformIndex;

                    primitiveBuilder.Vertices[i + startVertex] = pbrVertex;
                }

                // Insert indices with reverse winding order.
                primitiveBuilder.Indices.resize(startIndex + primitive.Indices.size());
                for (size_t i = 0; i < primitive.Indices.size(); i += 3) {
                    primitiveBuilder.Indices[startIndex + i + 0] = startVertex + primitive.Indices[i + 0];
                    primitiveBuilder.Indices[startIndex + i + 1] = startVertex + primitive.Indices[i + 2];
                    primitiveBuilder.Indices[startIndex + i + 2] = startVertex + primitive.Indices[i + 1];
                }
            }
        }

        // Recursively load all children.
        for (const int childNodeId : gltfNode.children) {
            LoadNode(transformIndex, gltfModel, childNodeId, primitiveCache, primitiveBuilderMap, model);
        }
    }
}  // namespace

namespace Gltf
{
    void PopulateFromGltfObject(Pbr::Model& model, Pbr::IResources& pbrResources, const tinygltf::Model& gltfModel)
    {
        // Empty the model to ensure we're starting from scratch
        model.Clear();

        // Read and transform mesh/node data. Primitives with the same material are merged to reduce draw calls.
        PrimitiveBuilderMap primitiveBuilderMap;
        GltfHelper::PrimitiveCache primitiveCache{gltfModel};
        {
            const int defaultSceneId = (gltfModel.defaultScene == -1) ? 0 : gltfModel.defaultScene;
            const tinygltf::Scene& defaultScene = gltfModel.scenes.at(defaultSceneId);

            // Process the root scene nodes. The children will be processed recursively.
            for (const int rootNodeId : defaultScene.nodes) {
                LoadNode(Pbr::RootNodeIndex, gltfModel, rootNodeId, primitiveCache, primitiveBuilderMap, model);
            }
        }

        // Load the materials referenced by the primitives
        std::map<int, std::shared_ptr<Pbr::Material>> materialMap;
        {
            // primitiveBuilderMap is grouped by material. Loop through the referenced materials and load their resources. This will only
            // load materials which are used by the active scene.
            for (const auto& primitiveBuilderPair : primitiveBuilderMap) {
                std::shared_ptr<Pbr::Material> pbrMaterial;

                const int materialIndex = primitiveBuilderPair.first;
                if (materialIndex == -1)  // No material was referenced. Make up a material for it.
                {
                    // Default material is a grey material, 50% roughness, non-metallic.
                    pbrMaterial = pbrResources.CreateFlatMaterial({0.5f, 0.5f, 0.5f, 0.5f}, 0.5f);
                }
                else {
                    const tinygltf::Material& gltfMaterial = gltfModel.materials.at(materialIndex);

                    const GltfHelper::Material material = GltfHelper::ReadMaterial(gltfModel, gltfMaterial);
                    pbrMaterial = pbrResources.CreateMaterial();

                    pbrMaterial->Name = gltfMaterial.name;

                    auto loadTexture = [&](Pbr::ShaderSlots::PSMaterial slot, const GltfHelper::Material::Texture& texture, bool sRGB,
                                           Pbr::RGBAColor defaultRGBA) {
                        pbrResources.LoadTexture(pbrMaterial, slot, texture.Image, texture.Sampler, sRGB, defaultRGBA);
                    };
                    loadTexture(Pbr::ShaderSlots::BaseColor, material.BaseColorTexture, true /* sRGB */, Pbr::RGBA::White);
                    loadTexture(Pbr::ShaderSlots::MetallicRoughness, material.MetallicRoughnessTexture, false /* sRGB */, Pbr::RGBA::White);
                    loadTexture(Pbr::ShaderSlots::Emissive, material.EmissiveTexture, true /* sRGB */, Pbr::RGBA::White);
                    loadTexture(Pbr::ShaderSlots::Normal, material.NormalTexture, false /* sRGB */, Pbr::RGBA::FlatNormal);
                    loadTexture(Pbr::ShaderSlots::Occlusion, material.OcclusionTexture, false /* sRGB */, Pbr::RGBA::White);

                    pbrMaterial->SetDoubleSided(material.DoubleSided ? Pbr::DoubleSided::DoubleSided : Pbr::DoubleSided::NotDoubleSided);
                    pbrMaterial->SetAlphaBlended(material.AlphaMode == GltfHelper::AlphaModeType::Blend ? Pbr::BlendState::AlphaBlended
                                                                                                        : Pbr::BlendState::NotAlphaBlended);

                    Pbr::Material::ConstantBufferData& parameters = pbrMaterial->Parameters();
                    parameters.BaseColorFactor = material.BaseColorFactor;
                    parameters.MetallicFactor = material.MetallicFactor;
                    parameters.RoughnessFactor = material.RoughnessFactor;
                    parameters.EmissiveFactor = material.EmissiveFactor;
                    parameters.OcclusionStrength = material.OcclusionStrength;
                    parameters.NormalScale = material.NormalScale;
                    parameters.AlphaCutoff =
                        material.AlphaMode == GltfHelper::AlphaModeType::Mask ? material.AlphaCutoff : std::numeric_limits<float>::lowest();
                }

                materialMap.insert(std::make_pair(materialIndex, std::move(pbrMaterial)));
            }
        }

        // Convert the primitive builders into primitives with their respective material and add it into the Pbr Model.
        for (const auto& primitiveBuilderPair : primitiveBuilderMap) {
            const Pbr::PrimitiveBuilder& primitiveBuilder = primitiveBuilderPair.second;
            const std::shared_ptr<Pbr::Material>& material = materialMap.find(primitiveBuilderPair.first)->second;
            auto handle = pbrResources.MakePrimitive(primitiveBuilder, material);
            model.AddPrimitive(handle);
        }

        pbrResources.DropLoaderCaches();
    }

    void PopulateFromGltfBinary(Pbr::Model& model, Pbr::IResources& pbrResources, const uint8_t* buffer, uint32_t bufferBytes)
    {
        // Parse the GLB buffer data into a tinygltf model object.
        tinygltf::Model gltfModel;
        std::string errorMessage;
        tinygltf::TinyGLTF loader;
        if (!loader.LoadBinaryFromMemory(&gltfModel, &errorMessage, nullptr /*warn*/, buffer, bufferBytes, ".")) {
            const auto msg =
                std::string("\r\nFailed to load gltf model (") + std::to_string(bufferBytes) + " bytes). Error: " + errorMessage;
            throw std::runtime_error(msg.c_str());
        }

        PopulateFromGltfObject(model, pbrResources, gltfModel);
    }
}  // namespace Gltf
