// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "PbrCommon.h"
#include "PbrHandles.h"
#include "PbrSharedState.h"

#include <memory>

namespace tinygltf
{
    struct Image;
    struct Sampler;
}  // namespace tinygltf

namespace Pbr
{

    struct Material;

    /// The way various APIs track textures is totally distinct, so this base class is just for type erasure.
    /// May also include a sampler
    struct ITexture
    {
        virtual ~ITexture() = default;
    };

    /// TODO add a swapchain length param and ignore it for d3d11?
    struct IGltfBuilder
    {
        virtual ~IGltfBuilder() = default;
        virtual std::shared_ptr<Material> CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor = 1.0f,
                                                             float metallicFactor = 0.0f, RGBColor emissiveFactor = RGB::Black) = 0;

        virtual std::shared_ptr<Material> CreateMaterial() = 0;

        virtual void LoadTexture(const std::shared_ptr<Material>& material, Pbr::ShaderSlots::PSMaterial slot, const tinygltf::Image* image,
                                 const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA) = 0;

        virtual PrimitiveHandle MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                              const std::shared_ptr<Pbr::Material>& material) = 0;

        /// Optional optimization, can call at the end of loading a model to drop per-model caches.
        // If IGltfBuilder is ever one-per-model on all backends, this can be replaced with a destructor.
        virtual void DropLoaderCaches()
        {
        }

    protected:
        // cannot instantiate except from derived class
        IGltfBuilder() = default;
    };
}  // namespace Pbr
