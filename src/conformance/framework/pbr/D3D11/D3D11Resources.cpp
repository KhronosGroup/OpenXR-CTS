// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "D3D11Resources.h"

#include "D3D11Primitive.h"
#include "D3D11Texture.h"
#include "D3D11TextureCache.h"

#include "../../gltf/GltfHelper.h"
#include "../PbrMaterial.h"

#include "utilities/throw_helpers.h"

#include <tinygltf/tiny_gltf.h>

#include <PbrPixelShader_hlsl.h>
#include <PbrVertexShader_hlsl.h>

#include <type_traits>

using namespace DirectX;

namespace
{
    struct SceneConstantBuffer
    {
        DirectX::XMFLOAT4X4 ViewProjection;
        DirectX::XMFLOAT4 EyePosition;
        DirectX::XMFLOAT3 LightDirection{};
        float _pad0;
        DirectX::XMFLOAT3 LightDiffuseColor{};
        float _pad1;
        uint32_t NumSpecularMipLevels{1};
        float _pad2[3];
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

    const D3D11_INPUT_ELEMENT_DESC s_vertexDesc[6] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TRANSFORMINDEX", 0, DXGI_FORMAT_R16_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
}  // namespace

namespace Pbr
{
    using ImageKey = std::tuple<const tinygltf::Image*, bool>;  // Item1 is a pointer to the image, Item2 is sRGB.

    struct D3D11Resources::Impl
    {
        void Initialize(_In_ ID3D11Device* device)
        {
            XRC_CHECK_THROW_HRCMD(device->CreateInputLayout(s_vertexDesc, ARRAYSIZE(s_vertexDesc), g_PbrVertexShader,
                                                            sizeof(g_PbrVertexShader), Resources.InputLayout.ReleaseAndGetAddressOf()));

            // Set up pixel shader.
            XRC_CHECK_THROW_HRCMD(device->CreatePixelShader(g_PbrPixelShader, sizeof(g_PbrPixelShader), nullptr,
                                                            Resources.PbrPixelShader.ReleaseAndGetAddressOf()));

            XRC_CHECK_THROW_HRCMD(device->CreateVertexShader(g_PbrVertexShader, sizeof(g_PbrVertexShader), nullptr,
                                                             Resources.PbrVertexShader.ReleaseAndGetAddressOf()));

            // Set up the scene constant buffer.
            const CD3D11_BUFFER_DESC pbrConstantBufferDesc(sizeof(SceneConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
            XRC_CHECK_THROW_HRCMD(
                device->CreateBuffer(&pbrConstantBufferDesc, nullptr, Resources.SceneConstantBuffer.ReleaseAndGetAddressOf()));

            // Samplers for environment map and BRDF.
            Resources.EnvironmentMapSampler = D3D11Texture::CreateSampler(device);
            Resources.BrdfSampler = D3D11Texture::CreateSampler(device);

            CD3D11_BLEND_DESC blendStateDesc(D3D11_DEFAULT);
            XRC_CHECK_THROW_HRCMD(device->CreateBlendState(&blendStateDesc, Resources.DefaultBlendState.ReleaseAndGetAddressOf()));

            D3D11_RENDER_TARGET_BLEND_DESC rtBlendDesc;
            rtBlendDesc.BlendEnable = TRUE;
            rtBlendDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            rtBlendDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            rtBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
            rtBlendDesc.SrcBlendAlpha = D3D11_BLEND_ZERO;
            rtBlendDesc.DestBlendAlpha = D3D11_BLEND_ONE;
            rtBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
                blendStateDesc.RenderTarget[i] = rtBlendDesc;
            }
            XRC_CHECK_THROW_HRCMD(device->CreateBlendState(&blendStateDesc, Resources.AlphaBlendState.ReleaseAndGetAddressOf()));

            for (bool doubleSided : {false, true}) {
                for (bool wireframe : {false, true}) {
                    for (bool frontCounterClockwise : {false, true}) {
                        CD3D11_RASTERIZER_DESC rasterizerDesc(D3D11_DEFAULT);
                        rasterizerDesc.CullMode = doubleSided ? D3D11_CULL_NONE : D3D11_CULL_BACK;
                        rasterizerDesc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
                        rasterizerDesc.FrontCounterClockwise = frontCounterClockwise;
                        XRC_CHECK_THROW_HRCMD(device->CreateRasterizerState(
                            &rasterizerDesc,
                            Resources.RasterizerStates[doubleSided][wireframe][frontCounterClockwise].ReleaseAndGetAddressOf()));
                    }
                }
            }

            for (bool reverseZ : {false, true}) {
                for (bool noWrite : {false, true}) {
                    CD3D11_DEPTH_STENCIL_DESC depthStencilDesc(CD3D11_DEFAULT{});
                    depthStencilDesc.DepthFunc = reverseZ ? D3D11_COMPARISON_GREATER : D3D11_COMPARISON_LESS;
                    depthStencilDesc.DepthWriteMask = noWrite ? D3D11_DEPTH_WRITE_MASK_ZERO : D3D11_DEPTH_WRITE_MASK_ALL;
                    XRC_CHECK_THROW_HRCMD(device->CreateDepthStencilState(
                        &depthStencilDesc, Resources.DepthStencilStates[reverseZ][noWrite].ReleaseAndGetAddressOf()));
                }
            }

            Resources.SolidColorTextureCache = D3D11TextureCache{device};
        }

        struct DeviceResources
        {
            Microsoft::WRL::ComPtr<ID3D11SamplerState> BrdfSampler;
            Microsoft::WRL::ComPtr<ID3D11SamplerState> EnvironmentMapSampler;
            Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
            Microsoft::WRL::ComPtr<ID3D11VertexShader> PbrVertexShader;
            Microsoft::WRL::ComPtr<ID3D11PixelShader> PbrPixelShader;
            Microsoft::WRL::ComPtr<ID3D11Buffer> SceneConstantBuffer;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> BrdfLut;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SpecularEnvironmentMap;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> DiffuseEnvironmentMap;
            Microsoft::WRL::ComPtr<ID3D11BlendState> AlphaBlendState;
            Microsoft::WRL::ComPtr<ID3D11BlendState> DefaultBlendState;
            Microsoft::WRL::ComPtr<ID3D11RasterizerState>
                RasterizerStates[2][2][2];  // Three dimensions for [DoubleSide][Wireframe][FrontCounterClockWise]
            Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilStates[2][2];  // Two dimensions for [ReverseZ][NoWrite]
            mutable D3D11TextureCache SolidColorTextureCache;
        };
        PrimitiveCollection<D3D11Primitive> Primitives;

        DeviceResources Resources;
        SceneConstantBuffer SceneBuffer;

        struct LoaderResources
        {
            // Create D3D cache for reuse of texture views and samplers when possible.
            std::map<ImageKey, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> imageMap;
            std::map<const tinygltf::Sampler*, Microsoft::WRL::ComPtr<ID3D11SamplerState>> samplerMap;
        };
        LoaderResources loaderResources;
    };

    D3D11Resources::D3D11Resources(_In_ ID3D11Device* device) : m_impl(std::make_unique<Impl>())
    {
        m_impl->Initialize(device);
    }

    D3D11Resources::D3D11Resources(D3D11Resources&& resources) = default;

    D3D11Resources::~D3D11Resources() = default;

    // Create a DirectX texture view from a tinygltf Image.
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> D3D11LoadGLTFImage(_In_ ID3D11Device* device, const tinygltf::Image& image,
                                                                               bool sRGB)
    {
        // First convert the image to RGBA if it isn't already.
        std::vector<uint8_t> tempBuffer;
        const uint8_t* rgbaBuffer = GltfHelper::ReadImageAsRGBA(image, &tempBuffer);
        if (rgbaBuffer == nullptr) {
            return nullptr;
        }

        const DXGI_FORMAT format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        return Pbr::D3D11Texture::CreateTexture(device, rgbaBuffer, image.width * image.height * 4, image.width, image.height, format);
    }

    static D3D11_FILTER D3D11ConvertFilter(int glMinFilter, int glMagFilter)
    {
        const D3D11_FILTER_TYPE minFilter = glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST                  ? D3D11_FILTER_TYPE_POINT
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR                 ? D3D11_FILTER_TYPE_LINEAR
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ? D3D11_FILTER_TYPE_POINT
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  ? D3D11_FILTER_TYPE_LINEAR
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  ? D3D11_FILTER_TYPE_POINT
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   ? D3D11_FILTER_TYPE_LINEAR
                                                                                                            : D3D11_FILTER_TYPE_POINT;
        const D3D11_FILTER_TYPE mipFilter = glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST                  ? D3D11_FILTER_TYPE_POINT
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR                 ? D3D11_FILTER_TYPE_POINT
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ? D3D11_FILTER_TYPE_POINT
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  ? D3D11_FILTER_TYPE_POINT
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  ? D3D11_FILTER_TYPE_LINEAR
                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   ? D3D11_FILTER_TYPE_LINEAR
                                                                                                            : D3D11_FILTER_TYPE_POINT;
        const D3D11_FILTER_TYPE magFilter = glMagFilter == TINYGLTF_TEXTURE_FILTER_NEAREST  ? D3D11_FILTER_TYPE_POINT
                                            : glMagFilter == TINYGLTF_TEXTURE_FILTER_LINEAR ? D3D11_FILTER_TYPE_LINEAR
                                                                                            : D3D11_FILTER_TYPE_POINT;

        const D3D11_FILTER filter = D3D11_ENCODE_BASIC_FILTER(minFilter, magFilter, mipFilter, D3D11_FILTER_REDUCTION_TYPE_STANDARD);
        return filter;
    }

    // Create a DirectX sampler state from a tinygltf Sampler.
    static Microsoft::WRL::ComPtr<ID3D11SamplerState> D3D11CreateGLTFSampler(_In_ ID3D11Device* device, const tinygltf::Sampler& sampler)
    {
        D3D11_SAMPLER_DESC samplerDesc{};

        samplerDesc.Filter = D3D11ConvertFilter(sampler.minFilter, sampler.magFilter);
        samplerDesc.AddressU = sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE     ? D3D11_TEXTURE_ADDRESS_CLAMP
                               : sampler.wrapS == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? D3D11_TEXTURE_ADDRESS_MIRROR
                                                                                        : D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = sampler.wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE     ? D3D11_TEXTURE_ADDRESS_CLAMP
                               : sampler.wrapT == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? D3D11_TEXTURE_ADDRESS_MIRROR
                                                                                        : D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
        XRC_CHECK_THROW_HRCMD(device->CreateSamplerState(&samplerDesc, samplerState.ReleaseAndGetAddressOf()));
        return samplerState;
    }

    /* IResources implementations */
    std::shared_ptr<Material> D3D11Resources::CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor, float metallicFactor,
                                                                 RGBColor emissiveFactor)
    {
        return D3D11Material::CreateFlat(*this, baseColorFactor, roughnessFactor, metallicFactor, emissiveFactor);
    }
    std::shared_ptr<Material> D3D11Resources::CreateMaterial()
    {
        return std::make_shared<D3D11Material>(*this);
    }
    std::shared_ptr<ITexture> D3D11Resources::CreateSolidColorTexture(RGBAColor color)
    {
        // TODO maybe unused
        auto ret = std::make_shared<Pbr::D3D11TextureAndSampler>();
        ret->srv = CreateTypedSolidColorTexture(color);
        return ret;
    }

    void D3D11Resources::LoadTexture(const std::shared_ptr<Material>& material, Pbr::ShaderSlots::PSMaterial slot,
                                     const tinygltf::Image* image, const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA)
    {
        auto pbrMaterial = std::dynamic_pointer_cast<D3D11Material>(material);
        if (!pbrMaterial) {
            throw std::logic_error("Wrong type of material");
        }
        // Find or load the image referenced by the texture.
        const ImageKey imageKey = std::make_tuple(image, sRGB);
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView =
            image != nullptr ? m_impl->loaderResources.imageMap[imageKey] : CreateTypedSolidColorTexture(defaultRGBA);
        if (!textureView)  // If not cached, load the image and store it in the texture cache.
        {
            // TODO: Generate mipmaps if sampler's minification filter (minFilter) uses mipmapping.
            // TODO: If texture is not power-of-two and (sampler has wrapping=repeat/mirrored_repeat OR minFilter uses
            // mipmapping), resize to power-of-two.
            textureView = D3D11LoadGLTFImage(GetDevice().Get(), *image, sRGB);
            m_impl->loaderResources.imageMap[imageKey] = textureView;
        }

        // Find or create the sampler referenced by the texture.
        Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState = m_impl->loaderResources.samplerMap[sampler];
        if (!samplerState)  // If not cached, create the sampler and store it in the sampler cache.
        {
            samplerState = sampler != nullptr ? D3D11CreateGLTFSampler(GetDevice().Get(), *sampler)
                                              : Pbr::D3D11Texture::CreateSampler(GetDevice().Get(), D3D11_TEXTURE_ADDRESS_WRAP);
            m_impl->loaderResources.samplerMap[sampler] = samplerState;
        }

        pbrMaterial->SetTexture(slot, textureView.Get(), samplerState.Get());
    }
    void D3D11Resources::DropLoaderCaches()
    {
        m_impl->loaderResources = {};
    }

    void D3D11Resources::SetBrdfLut(_In_ ID3D11ShaderResourceView* brdfLut)
    {
        m_impl->Resources.BrdfLut = brdfLut;
    }

    void D3D11Resources::CreateDeviceDependentResources(_In_ ID3D11Device* device)
    {
        m_impl->Initialize(device);
    }

    void D3D11Resources::ReleaseDeviceDependentResources()
    {
        m_impl->Resources = {};
        m_impl->loaderResources = {};
        m_impl->Primitives.clear();
    }

    Microsoft::WRL::ComPtr<ID3D11Device> D3D11Resources::GetDevice() const
    {
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        m_impl->Resources.SceneConstantBuffer->GetDevice(device.ReleaseAndGetAddressOf());
        return device;
    }

    void D3D11Resources::SetLight(DirectX::XMFLOAT3 direction, RGBColor diffuseColor)
    {
        m_impl->SceneBuffer.LightDirection = direction;
        m_impl->SceneBuffer.LightDiffuseColor = {diffuseColor.x, diffuseColor.y, diffuseColor.z};
    }

    void XM_CALLCONV D3D11Resources::SetViewProjection(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection)
    {
        XMStoreFloat4x4(&m_impl->SceneBuffer.ViewProjection, XMMatrixTranspose(XMMatrixMultiply(view, projection)));
        XMStoreFloat4(&m_impl->SceneBuffer.EyePosition, XMMatrixInverse(nullptr, view).r[3]);
    }

    void D3D11Resources::SetEnvironmentMap(_In_ ID3D11ShaderResourceView* specularEnvironmentMap,
                                           _In_ ID3D11ShaderResourceView* diffuseEnvironmentMap)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        diffuseEnvironmentMap->GetDesc(&desc);
        if (desc.ViewDimension != D3D_SRV_DIMENSION_TEXTURECUBE) {
            throw std::logic_error("Diffuse Resource View Type is not D3D_SRV_DIMENSION_TEXTURECUBE");
        }

        specularEnvironmentMap->GetDesc(&desc);
        if (desc.ViewDimension != D3D_SRV_DIMENSION_TEXTURECUBE) {
            throw std::logic_error("Specular Resource View Type is not D3D_SRV_DIMENSION_TEXTURECUBE");
        }

        m_impl->SceneBuffer.NumSpecularMipLevels = desc.TextureCube.MipLevels;
        m_impl->Resources.SpecularEnvironmentMap = specularEnvironmentMap;
        m_impl->Resources.DiffuseEnvironmentMap = diffuseEnvironmentMap;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> D3D11Resources::CreateTypedSolidColorTexture(RGBAColor color) const
    {
        return m_impl->Resources.SolidColorTextureCache.CreateTypedSolidColorTexture(color);
    }

    void D3D11Resources::Bind(_In_ ID3D11DeviceContext* context) const
    {
        context->UpdateSubresource(m_impl->Resources.SceneConstantBuffer.Get(), 0, nullptr, &m_impl->SceneBuffer, 0, 0);

        context->VSSetShader(m_impl->Resources.PbrVertexShader.Get(), nullptr, 0);
        context->PSSetShader(m_impl->Resources.PbrPixelShader.Get(), nullptr, 0);

        ID3D11Buffer* psBuffers[] = {m_impl->Resources.SceneConstantBuffer.Get()};
        context->PSSetConstantBuffers(Pbr::ShaderSlots::ConstantBuffers::Scene, _countof(psBuffers), psBuffers);

        context->IASetInputLayout(m_impl->Resources.InputLayout.Get());

        static_assert(ShaderSlots::DiffuseTexture == ShaderSlots::SpecularTexture + 1, "Diffuse must follow Specular slot");
        static_assert(ShaderSlots::SpecularTexture == ShaderSlots::Brdf + 1, "Specular must follow BRDF slot");
        ID3D11ShaderResourceView* shaderResources[] = {m_impl->Resources.BrdfLut.Get(), m_impl->Resources.SpecularEnvironmentMap.Get(),
                                                       m_impl->Resources.DiffuseEnvironmentMap.Get()};
        context->PSSetShaderResources(Pbr::ShaderSlots::Brdf, _countof(shaderResources), shaderResources);
        ID3D11SamplerState* samplers[] = {m_impl->Resources.BrdfSampler.Get(), m_impl->Resources.EnvironmentMapSampler.Get()};
        context->PSSetSamplers(ShaderSlots::Brdf, _countof(samplers), samplers);
    }

    void D3D11Resources::BindConstantBuffers(_In_ ID3D11DeviceContext* context, ID3D11Buffer* modelConstantBuffer) const
    {
        ID3D11Buffer* vsBuffers[] = {m_impl->Resources.SceneConstantBuffer.Get(), modelConstantBuffer};
        context->VSSetConstantBuffers(Pbr::ShaderSlots::ConstantBuffers::Scene, _countof(vsBuffers), vsBuffers);
        // PSSetConstantBuffers is done in Bind because it is not model-dependent
    }

    PrimitiveHandle D3D11Resources::MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                                  const std::shared_ptr<Pbr::Material>& material)
    {
        auto typedMaterial = std::dynamic_pointer_cast<Pbr::D3D11Material>(material);
        if (!typedMaterial) {
            throw std::logic_error("Got the wrong type of material");
        }
        return m_impl->Primitives.emplace_back(*this, primitiveBuilder, typedMaterial, false);
    }

    D3D11Primitive& D3D11Resources::GetPrimitive(PrimitiveHandle p)
    {
        return m_impl->Primitives[p];
    }

    const D3D11Primitive& D3D11Resources::GetPrimitive(PrimitiveHandle p) const
    {
        return m_impl->Primitives[p];
    }

    void D3D11Resources::SetFillMode(FillMode mode)
    {
        m_sharedState.SetFillMode(mode);
    }

    FillMode D3D11Resources::GetFillMode() const
    {
        return m_sharedState.GetFillMode();
    }

    void D3D11Resources::SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder)
    {
        m_sharedState.SetFrontFaceWindingOrder(windingOrder);
    }

    FrontFaceWindingOrder D3D11Resources::GetFrontFaceWindingOrder() const
    {
        return m_sharedState.GetFrontFaceWindingOrder();
    }

    void D3D11Resources::SetDepthDirection(DepthDirection depthDirection)
    {
        m_sharedState.SetDepthDirection(depthDirection);
    }

    void D3D11Resources::SetBlendState(_In_ ID3D11DeviceContext* context, bool enabled) const
    {
        context->OMSetBlendState(enabled ? m_impl->Resources.AlphaBlendState.Get() : m_impl->Resources.DefaultBlendState.Get(), nullptr,
                                 0xFFFFFF);
    }

    void D3D11Resources::SetRasterizerState(_In_ ID3D11DeviceContext* context, bool doubleSided) const
    {
        context->RSSetState(
            m_impl->Resources
                .RasterizerStates[doubleSided ? 1 : 0][m_sharedState.GetFillMode() == FillMode::Wireframe ? 1 : 0]
                                 [m_sharedState.GetFrontFaceWindingOrder() == FrontFaceWindingOrder::CounterClockWise ? 1 : 0]
                .Get());
    }

    void D3D11Resources::SetDepthStencilState(_In_ ID3D11DeviceContext* context, bool disableDepthWrite) const
    {
        context->OMSetDepthStencilState(
            m_impl->Resources
                .DepthStencilStates[m_sharedState.GetDepthDirection() == DepthDirection::Reversed ? 1 : 0][disableDepthWrite ? 1 : 0]
                .Get(),
            1);
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
