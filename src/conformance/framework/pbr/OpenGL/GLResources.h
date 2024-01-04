// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include "GLCommon.h"

#include "../IResources.h"
#include "../PbrCommon.h"
#include "../PbrHandles.h"
#include "../PbrSharedState.h"

#include "common/gfxwrapper_opengl.h"
#include "common/xr_linear.h"

#include <openxr/openxr.h>

namespace tinygltf
{
    struct Image;
    struct Sampler;
}  // namespace tinygltf

#include <chrono>
#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

namespace Pbr
{
    struct Primitive;
    struct Material;

    using Duration = std::chrono::high_resolution_clock::duration;
    struct GLPrimitive;
    struct GLMaterial;

    struct GLTextureAndSampler : public ITexture
    {
        ~GLTextureAndSampler() override = default;
        /// Required
        std::shared_ptr<ScopedGLTexture> srv;

        /// Optional
        std::shared_ptr<ScopedGLSampler> sampler;
    };

    // Global PBR resources required for rendering a scene.
    struct GLResources final : public IResources
    {
        explicit GLResources();
        GLResources(GLResources&&) noexcept;

        ~GLResources() override;

        std::shared_ptr<Material> CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor = 1.0f, float metallicFactor = 0.0f,
                                                     RGBColor emissiveFactor = RGB::Black) override;
        std::shared_ptr<Material> CreateMaterial() override;
        std::shared_ptr<ITexture> CreateSolidColorTexture(RGBAColor color);
        void LoadTexture(const std::shared_ptr<Material>& pbrMaterial, Pbr::ShaderSlots::PSMaterial slot, const tinygltf::Image* image,
                         const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA) override;
        PrimitiveHandle MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                      const std::shared_ptr<Pbr::Material>& material) override;
        void DropLoaderCaches() override;

        // Sets the Bidirectional Reflectance Distribution Function Lookup Table texture, required by the shader to compute surface
        // reflectance from the IBL.
        void SetBrdfLut(std::shared_ptr<ScopedGLTexture> brdfLut);

        // Set the directional light.
        void SetLight(XrVector3f direction, RGBColor diffuseColor);

        // Set the specular and diffuse image-based lighting (IBL) maps. ShaderResourceViews must be TextureCubes.
        void SetEnvironmentMap(std::shared_ptr<ScopedGLTexture> specularEnvironmentMap,
                               std::shared_ptr<ScopedGLTexture> diffuseEnvironmentMap);

        // Set the current view and projection matrices.
        void SetViewProjection(XrMatrix4x4f view, XrMatrix4x4f projection) const;

        // Many 1x1 pixel colored textures are used in the PBR system. This is used to create textures backed by a cache to reduce the
        // number of textures created.
        std::shared_ptr<ScopedGLTexture> CreateTypedSolidColorTexture(RGBAColor color) const;

        // Bind the the PBR resources to the current context.
        void Bind() const;

        // Set and update the model to world constant buffer value.
        void SetModelToWorld(XrMatrix4x4f modelToWorld) const;

        GLPrimitive& GetPrimitive(PrimitiveHandle p);
        const GLPrimitive& GetPrimitive(PrimitiveHandle p) const;

        // Set or get the shading and fill modes.
        void SetFillMode(FillMode mode);
        FillMode GetFillMode() const;
        void SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder);
        FrontFaceWindingOrder GetFrontFaceWindingOrder() const;
        void SetDepthDirection(DepthDirection depthDirection);

    private:
        void SetBlendState(bool enabled) const;
        void SetRasterizerState(bool doubleSided) const;
        void SetDepthStencilState(bool disableDepthWrite) const;

        friend struct GLMaterial;

        struct Impl;

        std::unique_ptr<Impl> m_impl;

        SharedState m_sharedState;
    };
}  // namespace Pbr
