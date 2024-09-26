// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include "MetalPrimitive.h"
#include "MetalTextureCache.h"
#include "MetalPipelineStates.h"

#include "../IGltfBuilder.h"
#include "../PbrCommon.h"
#include "../PbrHandles.h"
#include "../PbrSharedState.h"

#include <utilities/image.h>
#include "utilities/metal_utils.h"

#include <nonstd/span.hpp>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <chrono>
#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

namespace Pbr
{
    using nonstd::span;

    struct Primitive;
    class MetalPipelineStates;

    using Duration = std::chrono::high_resolution_clock::duration;
    struct MetalMaterial;

    struct SceneConstantBuffer
    {
        simd::float4x4 ViewProjection;
        simd::float4 EyePosition;
        simd::float3 LightDirection{};
        simd::float3 LightDiffuseColor{};
        simd::uint1 NumSpecularMipLevels{1};
    };

    static_assert(std::is_standard_layout<SceneConstantBuffer>::value, "Must be standard layout");
    static_assert(sizeof(float) == 4, "Single precision floats");
    static_assert((sizeof(SceneConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");
    static_assert(sizeof(SceneConstantBuffer) == 128, "Size must be the same as known");
    static_assert(offsetof(SceneConstantBuffer, ViewProjection) == 0, "Offsets must match shader");
    static_assert(offsetof(SceneConstantBuffer, EyePosition) == 64, "Offsets must match shader");
    static_assert(offsetof(SceneConstantBuffer, LightDirection) == 80, "Offsets must match shader");
    static_assert(offsetof(SceneConstantBuffer, LightDiffuseColor) == 96, "Offsets must match shader");
    static_assert(offsetof(SceneConstantBuffer, NumSpecularMipLevels) == 112, "Offsets must match shader");

    struct ModelConstantBuffer
    {
        simd::float4x4 ModelToWorld;
    };

    static_assert((sizeof(ModelConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");

    struct MetalTextureAndSampler : public ITexture
    {
        ~MetalTextureAndSampler() = default;
        /// Required
        NS::SharedPtr<MTL::Texture> mtlTexture;

        /// Optional
        NS::SharedPtr<MTL::SamplerState> mtlSamplerState;
    };

    /// Global PBR resources required for rendering a scene.
    struct MetalResources final : public IGltfBuilder
    {
        explicit MetalResources(MTL::Device* mtlDevice);
        MetalResources(MetalResources&&);

        ~MetalResources() override;

        std::shared_ptr<Material> CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor = 1.0f, float metallicFactor = 0.0f,
                                                     RGBColor emissiveFactor = RGB::Black) override;
        std::shared_ptr<Material> CreateMaterial() override;
        std::shared_ptr<ITexture> CreateSolidColorTexture(RGBAColor color, bool sRGB);

        void LoadTexture(const std::shared_ptr<Material>& pbrMaterial, Pbr::ShaderSlots::PSMaterial slot, const tinygltf::Image* image,
                         const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA) override;
        PrimitiveHandle MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                      const std::shared_ptr<Pbr::Material>& material) override;
        void DropLoaderCaches() override;

        /// Sets the Bidirectional Reflectance Distribution Function Lookup Table texture, required by the shader to compute surface
        /// reflectance from the IBL.
        void SetBrdfLut(MTL::Texture* brdfLut);

        /// Create device-dependent resources.
        void CreateDeviceDependentResources(MTL::Device* device);

        /// Release device-dependent resources.
        void ReleaseDeviceDependentResources();

        /// Get the MTLDevice that the PBR resources are associated with.
        NS::SharedPtr<MTL::Device> GetDevice() const;

        /// Get a pipeline state matching some parameters as well as the current settings inside MetalResources.
        MetalPipelineStateBundle GetOrCreatePipelineState(MTL::PixelFormat colorRenderTargetFormat,
                                                          MTL::PixelFormat depthRenderTargetFormat, BlendState blendState) const;

        /// Set the directional light.
        void SetLight(const XrVector3f& direction, RGBColor diffuseColor);

        /// Set the specular and diffuse image-based lighting (IBL) maps. ShaderResourceViews must be TextureCubes.
        void SetEnvironmentMap(MTL::Texture* specularEnvironmentMap, MTL::Texture* diffuseEnvironmentMap);

        /// Set the current view and projection matrices.
        void SetViewProjection(const XrMatrix4x4f& view, const XrMatrix4x4f& projection);

        /// Many 1x1 pixel colored textures are used in the PBR system. This is used to create textures backed by a cache to reduce the
        /// number of textures created.
        NS::SharedPtr<MTL::Texture> CreateTypedSolidColorTexture(RGBAColor color, bool sRGB) const;

        /// Get the cached list of texture formats supported by the device
        /// Note: these formats are not guaranteed to support cubemap
        span<const Conformance::Image::FormatParams> GetSupportedFormats() const;

        /// Bind the the PBR resources to the current RenderCommandEncoder.
        void Bind(MTL::RenderCommandEncoder* renderCommandEncoder) const;

        /// Set and update the model to world constant buffer value.
        void SetModelToWorld(const XrMatrix4x4f& modelToWorld) const;

        /// Get the MetalPrimitive from a primitive handle.
        MetalPrimitive& GetPrimitive(PrimitiveHandle p);

        /// Get the MetalPrimitive from a primitive handle, const overload.
        const MetalPrimitive& GetPrimitive(PrimitiveHandle p) const;

        /// Set or get the shading and fill modes.
        void SetFillMode(FillMode mode);
        FillMode GetFillMode() const;
        void SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder);
        FrontFaceWindingOrder GetFrontFaceWindingOrder() const;
        void SetDepthDirection(DepthDirection depthDirection);

    private:
        friend struct MetalMaterial;

        SharedState m_sharedState;

        NS::SharedPtr<MTL::Device> m_device;

        struct DeviceResources
        {
            NS::SharedPtr<MTL::SamplerState> BrdfSampler;
            NS::SharedPtr<MTL::SamplerState> EnvironmentMapSampler;
            NS::SharedPtr<MTL::VertexDescriptor> VertexDescriptor;
            NS::SharedPtr<MTL::Function> PbrVertexShader;
            NS::SharedPtr<MTL::Function> PbrPixelShader;
            NS::SharedPtr<MTL::Texture> BrdfLut;
            NS::SharedPtr<MTL::Texture> SpecularEnvironmentMap;
            NS::SharedPtr<MTL::Texture> DiffuseEnvironmentMap;
            std::unique_ptr<MetalPipelineStates> PipelineStates;
            mutable MetalTextureCache SolidColorTextureCache;

            std::vector<Conformance::Image::FormatParams> SupportedTextureFormats;
        };
        PrimitiveCollection<MetalPrimitive> m_Primitives;

        DeviceResources m_Resources;
        mutable SceneConstantBuffer m_SceneBuffer;
        mutable ModelConstantBuffer m_ModelBuffer;

        using ImageKey = std::tuple<const tinygltf::Image*, bool>;
        struct LoaderResources
        {
            /// Create cache for reuse of texture views and samplers when possible.
            std::map<ImageKey, NS::SharedPtr<MTL::Texture>> imageMap;
            std::map<const tinygltf::Sampler*, NS::SharedPtr<MTL::SamplerState>> samplerMap;
        };
        LoaderResources m_LoaderResources;
    };
}  // namespace Pbr
