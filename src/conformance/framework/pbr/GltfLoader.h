// Copyright 2022-2023, The Khronos Group, Inc.
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

#include "IResources.h"
#include "PbrModel.h"

#include <memory>
#include <stdint.h>
#include <type_traits>

namespace Pbr
{
    class Model;
    struct IResources;
}  // namespace Pbr

namespace tinygltf
{
    class Model;
}

namespace Gltf
{
    // non-templated inner functions:
    void PopulateFromGltfObject(Pbr::Model& model, Pbr::IResources& pbrResources, const tinygltf::Model& gltfModel);
    void PopulateFromGltfBinary(Pbr::Model& model, Pbr::IResources& pbrResources, const uint8_t* buffer, uint32_t bufferBytes);

    // Creates a Pbr Model from tinygltf model.
    template <typename DerivedModel>
    std::shared_ptr<DerivedModel> FromGltfObject(Pbr::IResources& pbrResources, const tinygltf::Model& gltfModel)
    {
        static_assert(std::is_base_of<Pbr::Model, DerivedModel>::value, "DerivedModel not derived from Pbr::Model");

        // Start off with an empty Model.
        auto model = std::make_shared<DerivedModel>();
        PopulateFromGltfObject(*model, pbrResources, gltfModel);
        return model;
    }

    // Creates a Pbr Model from glTF 2.0 GLB file content.
    template <typename DerivedModel>
    std::shared_ptr<DerivedModel> FromGltfBinary(Pbr::IResources& pbrResources, const uint8_t* buffer, uint32_t bufferBytes)
    {
        static_assert(std::is_base_of<Pbr::Model, DerivedModel>::value, "DerivedModel not derived from Pbr::Model");

        // Start off with an empty Model.
        auto model = std::make_shared<DerivedModel>();
        PopulateFromGltfBinary(*model, pbrResources, buffer, bufferBytes);
        return model;
    }

    template <typename DerivedModel, typename Container>
    std::shared_ptr<DerivedModel> FromGltfBinary(Pbr::IResources& pbrResources, const Container& buffer)
    {
        return FromGltfBinary(pbrResources, buffer.data(), static_cast<uint32_t>(buffer.size()));
    }
}  // namespace Gltf
