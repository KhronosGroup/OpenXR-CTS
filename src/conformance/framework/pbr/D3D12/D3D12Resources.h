// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include "../IResources.h"
#include "../PbrCommon.h"
#include "../PbrHandles.h"
#include "../PbrSharedState.h"

#include "utilities/d3d12_utils.h"
#include "utilities/throw_helpers.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

namespace Pbr
{
    struct Primitive;
    using Duration = std::chrono::high_resolution_clock::duration;
    struct D3D12Primitive;
    struct D3D12Material;

    struct D3D12TextureAndSampler : public ITexture
    {
        ~D3D12TextureAndSampler() = default;
        /// Required
        Conformance::D3D12ResourceWithSRVDesc texture;

        /// Optional
        D3D12_SAMPLER_DESC sampler;
        bool samplerSet;
    };

    // Global PBR resources required for rendering a scene.
    struct D3D12Resources final : public IResources
    {
        D3D12Resources(_In_ ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& basePipelineStateDesc);
        D3D12Resources(D3D12Resources&&);

        ~D3D12Resources() override;

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
        void SetBrdfLut(_In_ Conformance::D3D12ResourceWithSRVDesc brdfLut);

        // Create device-dependent resources.
        void CreateDeviceDependentResources(_In_ ID3D12Device* device);

        // Release device-dependent resources.
        void ReleaseDeviceDependentResources();

        // Get the D3D12Device that the PBR resources are associated with.
        Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const;

        // Create a new copy command list, which can later be executed with ExecuteCopyCommandList
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CreateCopyCommandList() const;

        // Execute a copy command list on the internal copy queue, which can be waited on using GetFenceAndValue
        void ExecuteCopyCommandList(ID3D12GraphicsCommandList* cmdList,
                                    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> destroyAfterCopy = {}) const;

        /// Create a command list, apply the functor to it, close it, and execute it.
        /// Functor must take a single argument of type ID3D12GraphicsCommandList* or compatible.
        template <typename F>
        void WithCopyCommandList(F&& commandListFunctor) const
        {
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList = CreateCopyCommandList();

            commandListFunctor(cmdList.Get());

            XRC_CHECK_THROW_HRCMD(cmdList->Close());
            ExecuteCopyCommandList(cmdList.Get());
        }

        // Get a pipeline state matching some parameters as well as the current settings inside D3D12Resources.
        Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreatePipelineState(DXGI_FORMAT colorRenderTargetFormat,
                                                                             DXGI_FORMAT depthRenderTargetFormat, BlendState blendState,
                                                                             DoubleSided doubleSided);

        // Set the directional light.
        void SetLight(DirectX::XMFLOAT3 direction, RGBColor diffuseColor);

        // Set the specular and diffuse image-based lighting (IBL) maps. ShaderResourceViews must be TextureCubes.
        void SetEnvironmentMap(_In_ Conformance::D3D12ResourceWithSRVDesc specularEnvironmentMap,
                               _In_ Conformance::D3D12ResourceWithSRVDesc diffuseEnvironmentMap);

        // Set the current view and projection matrices.
        void XM_CALLCONV SetViewProjection(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection) const;

        // Many 1x1 pixel colored textures are used in the PBR system. This is used to create textures backed by a cache to reduce the
        // number of textures created.
        Conformance::D3D12ResourceWithSRVDesc CreateTypedSolidColorTexture(RGBAColor color);

        // Bind the the PBR resources to the current context.
        void Bind(_In_ ID3D12GraphicsCommandList* directCommandList) const;

        // Get the fence to wait on before executing any command list built on this Resources.
        std::pair<ID3D12Fence*, uint64_t> GetFenceAndValue() const;

        // Set and update the model to world constant buffer value.
        void XM_CALLCONV SetModelToWorld(DirectX::FXMMATRIX modelToWorld) const;

        D3D12Primitive& GetPrimitive(PrimitiveHandle p);
        const D3D12Primitive& GetPrimitive(PrimitiveHandle p) const;

        // Set or get the shading and fill modes.
        void SetFillMode(FillMode mode);
        FillMode GetFillMode() const;
        void SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder);
        FrontFaceWindingOrder GetFrontFaceWindingOrder() const;
        void SetDepthDirection(DepthDirection depthDirection);

    private:
        void SetTransforms(D3D12_CPU_DESCRIPTOR_HANDLE transformDescriptor);
        void GetTransforms(D3D12_CPU_DESCRIPTOR_HANDLE destTransformDescriptor);
        void GetGlobalTexturesAndSamplers(D3D12_CPU_DESCRIPTOR_HANDLE destTextureDescriptors,
                                          D3D12_CPU_DESCRIPTOR_HANDLE destSamplerDescriptors);
        // Bind a material's descriptors according to the root signature.
        void BindDescriptorHeaps(_In_ ID3D12GraphicsCommandList* directCommandList, ID3D12DescriptorHeap* srvDescriptorHeap,
                                 ID3D12DescriptorHeap* samplerDescriptorHeap) const;

        friend struct D3D12Material;
        friend class D3D12Model;
        friend struct D3D12Primitive;

        struct Impl;
        std::unique_ptr<Impl> m_impl;

        SharedState m_sharedState;
    };
}  // namespace Pbr
