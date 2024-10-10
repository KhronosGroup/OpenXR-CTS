// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

//
// Functions to load glTF 2.0 content into a renderable Pbr::Model.
//

#pragma once

#include "IGltfBuilder.h"
#include "PbrModel.h"

#include <map>
#include <memory>
#include <stdint.h>
#include <type_traits>

namespace Pbr
{
    class Model;
    struct IGltfBuilder;
}  // namespace Pbr

namespace tinygltf
{
    class Model;
}

namespace Gltf
{
    // Maps a glTF material to a PrimitiveBuilder. This optimization combines all primitives which use
    // the same material into a single primitive for reduced draw calls. Each primitive's vertex specifies
    // which node it corresponds to any appropriate node transformation be happen in the shader.
    using PrimitiveBuilderMap = std::map<int, Pbr::PrimitiveBuilder>;
    class ModelBuilder
    {
    public:
        ModelBuilder() = default;
        ModelBuilder(const ModelBuilder&) = delete;
        ModelBuilder(ModelBuilder&& other) = default;
        ModelBuilder& operator=(ModelBuilder&& other) = default;
        ~ModelBuilder() = default;

        ModelBuilder(std::shared_ptr<const tinygltf::Model> gltfModel);
        ModelBuilder(const uint8_t* buffer, uint32_t bufferBytes);

        template <typename Container>
        ModelBuilder(Pbr::IGltfBuilder& gltfBuilder, const Container& buffer)
            : ModelBuilder(gltfBuilder, buffer.data(), static_cast<uint32_t>(buffer.size()))
        {
        }

        std::shared_ptr<Pbr::Model> Build(Pbr::IGltfBuilder& gltfBuilder);

    private:
        void SharedInit();

    private:
        std::shared_ptr<Pbr::Model> m_pbrModel;
        std::shared_ptr<const tinygltf::Model> m_gltfModel;
        PrimitiveBuilderMap m_primitiveBuilderMap;
    };
}  // namespace Gltf
