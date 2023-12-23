// Copyright 2022-2023, The Khronos Group, Inc.
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

#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <chrono>
#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

namespace Pbr
{
    struct Primitive;
    using Duration = std::chrono::high_resolution_clock::duration;
    struct D3D11Primitive;
    struct D3D11Material;

    struct D3D11TextureAndSampler : public ITexture
    {
        ~D3D11TextureAndSampler() = default;
        /// Required
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

        /// Optional
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    };

    // Global PBR resources required for rendering a scene.
    struct D3D11Resources final : public IResources
    {
        explicit D3D11Resources(_In_ ID3D11Device* d3dDevice);
        D3D11Resources(D3D11Resources&&);

        ~D3D11Resources() override;

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
        void SetBrdfLut(_In_ ID3D11ShaderResourceView* brdfLut);

        // Create device-dependent resources.
        void CreateDeviceDependentResources(_In_ ID3D11Device* device);

        // Release device-dependent resources.
        void ReleaseDeviceDependentResources();

        // Get the D3D11Device that the PBR resources are associated with.
        Microsoft::WRL::ComPtr<ID3D11Device> GetDevice() const;

        // Set the directional light.
        void SetLight(DirectX::XMFLOAT3 direction, RGBColor diffuseColor);

        // Set the specular and diffuse image-based lighting (IBL) maps. ShaderResourceViews must be TextureCubes.
        void SetEnvironmentMap(_In_ ID3D11ShaderResourceView* specularEnvironmentMap, _In_ ID3D11ShaderResourceView* diffuseEnvironmentMap);

        // Set the current view and projection matrices.
        void XM_CALLCONV SetViewProjection(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection);

        // Many 1x1 pixel colored textures are used in the PBR system. This is used to create textures backed by a cache to reduce the
        // number of textures created.
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateTypedSolidColorTexture(RGBAColor color) const;

        // Bind the the PBR resources to the current context.
        void Bind(_In_ ID3D11DeviceContext* context) const;

        // Set and update the model to world constant buffer value.
        void XM_CALLCONV SetModelToWorld(DirectX::FXMMATRIX modelToWorld, _In_ ID3D11DeviceContext* context) const;

        D3D11Primitive& GetPrimitive(PrimitiveHandle p);
        const D3D11Primitive& GetPrimitive(PrimitiveHandle p) const;

        // Set or get the shading and fill modes.
        void SetFillMode(FillMode mode);
        FillMode GetFillMode() const;
        void SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder);
        FrontFaceWindingOrder GetFrontFaceWindingOrder() const;
        void SetDepthDirection(DepthDirection depthDirection);

    private:
        void SetBlendState(_In_ ID3D11DeviceContext* context, bool enabled) const;
        void SetRasterizerState(_In_ ID3D11DeviceContext* context, bool doubleSided) const;
        void SetDepthStencilState(_In_ ID3D11DeviceContext* context, bool disableDepthWrite) const;

        friend struct D3D11Material;

        struct Impl;
        std::unique_ptr<Impl> m_impl;

        SharedState m_sharedState;
    };
}  // namespace Pbr
