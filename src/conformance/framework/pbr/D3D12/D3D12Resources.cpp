// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "D3D12Resources.h"

#include "D3D12PipelineStates.h"
#include "D3D12Primitive.h"
#include "D3D12Texture.h"
#include "D3D12TextureCache.h"

#include "../../RGBAImage.h"
#include "../../gltf/GltfHelper.h"
#include "../PbrMaterial.h"

#include "utilities/d3d12_queue_wrapper.h"
#include "utilities/d3d12_utils.h"
#include "utilities/destruction_queue.h"
#include "utilities/throw_helpers.h"

#include <d3dx12.h>
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

    struct ModelConstantBuffer
    {
        DirectX::XMFLOAT4X4 ModelToWorld;
    };

    static_assert((sizeof(ModelConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");

    const D3D12_INPUT_ELEMENT_DESC s_vertexDesc[6] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TRANSFORMINDEX", 0, DXGI_FORMAT_R16_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    const CD3DX12_DESCRIPTOR_RANGE s_constantBufferDesc = CD3DX12_DESCRIPTOR_RANGE{D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0};

}  // namespace

namespace Pbr
{
    using ImageKey = std::tuple<const tinygltf::Image*, bool>;  // Item1 is a pointer to the image, Item2 is sRGB.

    namespace RootSig
    {
        enum RootParamIndex
        {
            SceneConstantBuffer,
            ModelConstantBuffer,
            MaterialConstantBuffer,
            TransformsBuffer,
            TextureSRVs,
            TextureSamplers,
            RootParameterCount,

        };

        static Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSig(_In_ ID3D12Device* device)
        {
            // root signature has one parameter for each RootParamIndex
            CD3DX12_ROOT_PARAMETER rootParams[RootParameterCount] = {};

            // constant buffers
            rootParams[SceneConstantBuffer].InitAsConstantBufferView(ShaderSlots::ConstantBuffers::Scene, 0, D3D12_SHADER_VISIBILITY_ALL);
            rootParams[ModelConstantBuffer].InitAsConstantBufferView(ShaderSlots::ConstantBuffers::Model, 0,
                                                                     D3D12_SHADER_VISIBILITY_VERTEX);
            rootParams[MaterialConstantBuffer].InitAsConstantBufferView(ShaderSlots::ConstantBuffers::Material, 0,
                                                                        D3D12_SHADER_VISIBILITY_PIXEL);

            // transform register index overlaps with textures, but that's fine because their visibility is disjoint
            // preferrring DescriptorTable over ShaderResourceView because root ShaderResourceView doesn't let you specify stride
            CD3DX12_DESCRIPTOR_RANGE vsrvRange =
                CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ShaderSlots::NumVSResourceViews, 0);
            rootParams[TransformsBuffer].InitAsDescriptorTable(1, &vsrvRange, D3D12_SHADER_VISIBILITY_VERTEX);

            // textures and samplers are out-of-line in descriptor tables
            CD3DX12_DESCRIPTOR_RANGE psrvRange = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ShaderSlots::NumTextures, 0);
            CD3DX12_DESCRIPTOR_RANGE sRange = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ShaderSlots::NumSamplers, 0);
            rootParams[TextureSRVs].InitAsDescriptorTable(1, &psrvRange, D3D12_SHADER_VISIBILITY_PIXEL);
            rootParams[TextureSamplers].InitAsDescriptorTable(1, &sRange, D3D12_SHADER_VISIBILITY_PIXEL);

            D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

            CD3DX12_ROOT_SIGNATURE_DESC rsigDesc = {};
            rsigDesc.Init(static_cast<UINT>(std::size(rootParams)), rootParams, 0, nullptr, rootSignatureFlags);

            Microsoft::WRL::ComPtr<ID3DBlob> rootSigBlob;
            Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
            Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
            XRC_CHECK_THROW_HRCMD(D3D12SerializeRootSignature(&rsigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
                                                              rootSigBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf()));

            XRC_CHECK_THROW_HRCMD(device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
                                                              __uuidof(ID3D12RootSignature),
                                                              reinterpret_cast<void**>(rootSig.ReleaseAndGetAddressOf())));

            return rootSig;
        }
    }  // namespace RootSig

    struct D3D12Resources::Impl
    {
        // TODO: make this a constructor
        void Initialize(_In_ ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& basePipelineStateDesc)
        {
            Resources.Device = device;

            Resources.CopyQueue = std::make_unique<Conformance::D3D12QueueWrapper>(device, D3D12_COMMAND_LIST_TYPE_COPY);

            XRC_CHECK_THROW_HRCMD(
                device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, __uuidof(ID3D12CommandAllocator),
                                               reinterpret_cast<void**>(Resources.CopyAllocator.ReleaseAndGetAddressOf())));

            Resources.RootSignature = RootSig::CreateRootSig(device);
            Resources.PipelineStates = std::make_unique<D3D12PipelineStates>(Resources.RootSignature, basePipelineStateDesc, s_vertexDesc,
                                                                             g_PbrVertexShader, g_PbrPixelShader);

            // Set up the constant buffers.
            static_assert((sizeof(SceneConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");
            Resources.SceneConstantBuffer.Allocate(device);

            static_assert((sizeof(ModelConstantBuffer) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");
            Resources.ModelConstantBuffer.Allocate(device);

            D3D12_DESCRIPTOR_HEAP_DESC transformHeapDesc;
            transformHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            transformHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            transformHeapDesc.NumDescriptors = ShaderSlots::NumVSResourceViews;
            transformHeapDesc.NodeMask = 1;

            XRC_CHECK_THROW_HRCMD(device->CreateDescriptorHeap(&transformHeapDesc, IID_PPV_ARGS(&Resources.TransformHeap)));

            D3D12_DESCRIPTOR_HEAP_DESC textureHeapDesc;
            textureHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            textureHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            textureHeapDesc.NumDescriptors = ShaderSlots::NumTextures - ShaderSlots::NumMaterialSlots;
            textureHeapDesc.NodeMask = 1;

            XRC_CHECK_THROW_HRCMD(device->CreateDescriptorHeap(&textureHeapDesc, IID_PPV_ARGS(&Resources.TextureHeap)));
            UINT textureDescriptorSize = device->GetDescriptorHandleIncrementSize(textureHeapDesc.Type);
            CD3DX12_CPU_DESCRIPTOR_HANDLE textureBaseHandle(Resources.TextureHeap->GetCPUDescriptorHandleForHeapStart());
            Resources.BrdfLutTextureDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(  //
                textureBaseHandle, ShaderSlots::Brdf - ShaderSlots::NumMaterialSlots, textureDescriptorSize);
            Resources.SpecularEnvMapTextureDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(  //
                textureBaseHandle, ShaderSlots::SpecularTexture - ShaderSlots::NumMaterialSlots, textureDescriptorSize);
            Resources.DiffuseEnvMapTextureDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(  //
                textureBaseHandle, ShaderSlots::DiffuseTexture - ShaderSlots::NumMaterialSlots, textureDescriptorSize);

            D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc;
            samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            samplerHeapDesc.NumDescriptors = ShaderSlots::NumSamplers - ShaderSlots::NumMaterialSlots;
            samplerHeapDesc.NodeMask = 1;

            XRC_CHECK_THROW_HRCMD(device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&Resources.SamplerHeap)));
            UINT samplerDescriptorSize = device->GetDescriptorHandleIncrementSize(samplerHeapDesc.Type);
            CD3DX12_CPU_DESCRIPTOR_HANDLE samplerBaseHandle(Resources.SamplerHeap->GetCPUDescriptorHandleForHeapStart());
            Resources.BrdfSamplerDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(  //
                samplerBaseHandle, ShaderSlots::Brdf - ShaderSlots::NumMaterialSlots, samplerDescriptorSize);
            Resources.EnvironmentMapSamplerDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(  //
                samplerBaseHandle, ShaderSlots::EnvironmentMapSampler - ShaderSlots::NumMaterialSlots, samplerDescriptorSize);

            D3D12Texture::CreateSampler(device, Resources.BrdfSamplerDescriptor);
            D3D12Texture::CreateSampler(device, Resources.EnvironmentMapSamplerDescriptor);

            Resources.SolidColorTextureCache = D3D12TextureCache{device};
        }

        /// Things we might want per frame eventually
        struct FrameDeviceResources
        {
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> MainHeap;
            Microsoft::WRL::ComPtr<ID3D12Resource> ConstantBufferUploadHeap;

            void Allocate(const Microsoft::WRL::ComPtr<ID3D12Device>& /*device*/)
            {
                // D3d12x
            }
        };

        struct DeviceResources
        {
            Microsoft::WRL::ComPtr<ID3D12Device> Device;

            std::unique_ptr<Conformance::D3D12QueueWrapper> CopyQueue;
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CopyAllocator;
            Conformance::DestructionQueue<Microsoft::WRL::ComPtr<ID3D12Resource>> DestructionQueue;
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> TransformHeap;
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> TextureHeap;
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SamplerHeap;
            Microsoft::WRL::ComPtr<ID3D12Resource> BrdfLutTexture;
            Microsoft::WRL::ComPtr<ID3D12Resource> SpecularEnvMapTexture;
            Microsoft::WRL::ComPtr<ID3D12Resource> DiffuseEnvMapTexture;
            D3D12_CPU_DESCRIPTOR_HANDLE BrdfLutTextureDescriptor;
            D3D12_CPU_DESCRIPTOR_HANDLE SpecularEnvMapTextureDescriptor;
            D3D12_CPU_DESCRIPTOR_HANDLE DiffuseEnvMapTextureDescriptor;
            D3D12_CPU_DESCRIPTOR_HANDLE BrdfSamplerDescriptor;
            D3D12_CPU_DESCRIPTOR_HANDLE EnvironmentMapSamplerDescriptor;
            Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
            Conformance::D3D12BufferWithUpload<SceneConstantBuffer> SceneConstantBuffer;
            Conformance::D3D12BufferWithUpload<ModelConstantBuffer> ModelConstantBuffer;
            std::unique_ptr<D3D12PipelineStates> PipelineStates{};
            mutable D3D12TextureCache SolidColorTextureCache;
        };
        PrimitiveCollection<D3D12Primitive> Primitives;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC BasePipelineStateDesc;
        DeviceResources Resources;
        SceneConstantBuffer SceneBuffer;
        ModelConstantBuffer ModelBuffer;

        struct LoaderResources
        {
            // Create D3D cache for reuse of texture views and samplers when possible.
            std::map<ImageKey, std::shared_ptr<Conformance::D3D12ResourceWithSRVDesc>> imageMap;
            std::map<const tinygltf::Sampler*, std::shared_ptr<D3D12_SAMPLER_DESC>> samplerMap;
        };
        LoaderResources loaderResources;
    };

    D3D12Resources::D3D12Resources(_In_ ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& basePipelineStateDesc)
        : m_impl(std::make_unique<Impl>())
    {
        m_impl->Initialize(device, basePipelineStateDesc);
    }

    D3D12Resources::D3D12Resources(D3D12Resources&& resources) = default;

    D3D12Resources::~D3D12Resources() = default;

    /* IResources implementations */
    std::shared_ptr<Material> D3D12Resources::CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor, float metallicFactor,
                                                                 RGBColor emissiveFactor)
    {
        return D3D12Material::CreateFlat(*this, baseColorFactor, roughnessFactor, metallicFactor, emissiveFactor);
    }
    std::shared_ptr<Material> D3D12Resources::CreateMaterial()
    {
        return std::make_shared<D3D12Material>(*this);
    }
    std::shared_ptr<ITexture> D3D12Resources::CreateSolidColorTexture(RGBAColor color)
    {
        // TODO maybe unused
        auto ret = std::make_shared<Pbr::D3D12TextureAndSampler>();
        ret->texture = CreateTypedSolidColorTexture(color);
        return ret;
    }

    // Create a DirectX texture view from a tinygltf Image.
    static Conformance::D3D12ResourceWithSRVDesc LoadGLTFImage(D3D12Resources& pbrResources, const tinygltf::Image& image, bool sRGB)
    {
        // First convert the image to RGBA if it isn't already.
        std::vector<uint8_t> tempBuffer;
        const uint8_t* rgbaBuffer = GltfHelper::ReadImageAsRGBA(image, &tempBuffer);
        Internal::ThrowIf(rgbaBuffer == nullptr, "Failed to read image");

        const DXGI_FORMAT format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        return D3D12Texture::CreateTexture(pbrResources, rgbaBuffer, 4, image.width, image.height, format);
    }

    static D3D12_FILTER ConvertFilter(int glMinFilter, int glMagFilter)
    {
        const D3D12_FILTER_TYPE minFilter = glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST
                                                ? D3D12_FILTER_TYPE_POINT
                                                : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR
                                                      ? D3D12_FILTER_TYPE_LINEAR
                                                      : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
                                                            ? D3D12_FILTER_TYPE_POINT
                                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST
                                                                  ? D3D12_FILTER_TYPE_LINEAR
                                                                  : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR
                                                                        ? D3D12_FILTER_TYPE_POINT
                                                                        : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
                                                                              ? D3D12_FILTER_TYPE_LINEAR
                                                                              : D3D12_FILTER_TYPE_POINT;
        const D3D12_FILTER_TYPE mipFilter = glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST
                                                ? D3D12_FILTER_TYPE_POINT
                                                : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR
                                                      ? D3D12_FILTER_TYPE_POINT
                                                      : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
                                                            ? D3D12_FILTER_TYPE_POINT
                                                            : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST
                                                                  ? D3D12_FILTER_TYPE_POINT
                                                                  : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR
                                                                        ? D3D12_FILTER_TYPE_LINEAR
                                                                        : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
                                                                              ? D3D12_FILTER_TYPE_LINEAR
                                                                              : D3D12_FILTER_TYPE_POINT;
        const D3D12_FILTER_TYPE magFilter =
            glMagFilter == TINYGLTF_TEXTURE_FILTER_NEAREST
                ? D3D12_FILTER_TYPE_POINT
                : glMagFilter == TINYGLTF_TEXTURE_FILTER_LINEAR ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;

        const D3D12_FILTER filter = D3D12_ENCODE_BASIC_FILTER(minFilter, magFilter, mipFilter, D3D12_FILTER_REDUCTION_TYPE_STANDARD);
        return filter;
    }

    // Create a DirectX sampler state from a tinygltf Sampler.
    static D3D12_SAMPLER_DESC CreateGLTFSampler(_In_ ID3D12Device* /*device*/, const tinygltf::Sampler& sampler)
    {
        D3D12_SAMPLER_DESC samplerDesc{};

        samplerDesc.Filter = ConvertFilter(sampler.minFilter, sampler.magFilter);
        samplerDesc.AddressU = sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE
                                   ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP
                                   : sampler.wrapS == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? D3D12_TEXTURE_ADDRESS_MODE_MIRROR
                                                                                            : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = sampler.wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE
                                   ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP
                                   : sampler.wrapT == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? D3D12_TEXTURE_ADDRESS_MODE_MIRROR
                                                                                            : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        return samplerDesc;
    }
    void D3D12Resources::LoadTexture(const std::shared_ptr<Material>& material, Pbr::ShaderSlots::PSMaterial slot,
                                     const tinygltf::Image* image, const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA)
    {
        auto pbrMaterial = std::dynamic_pointer_cast<D3D12Material>(material);
        if (!pbrMaterial) {
            throw std::logic_error("Wrong type of material");
        }
        // Find or load the image referenced by the texture.
        const ImageKey imageKey = std::make_tuple(image, sRGB);
        std::shared_ptr<Conformance::D3D12ResourceWithSRVDesc> textureView =
            image != nullptr ? m_impl->loaderResources.imageMap[imageKey]
                             : std::make_shared<Conformance::D3D12ResourceWithSRVDesc>(CreateTypedSolidColorTexture(defaultRGBA));
        if (!textureView)  // If not cached, load the image and store it in the texture cache.
        {
            // TODO: Generate mipmaps if sampler's minification filter (minFilter) uses mipmapping.
            // TODO: If texture is not power-of-two and (sampler has wrapping=repeat/mirrored_repeat OR minFilter uses
            // mipmapping), resize to power-of-two.
            textureView = std::make_shared<Conformance::D3D12ResourceWithSRVDesc>(LoadGLTFImage(*this, *image, sRGB));
            m_impl->loaderResources.imageMap[imageKey] = textureView;
        }

        // Find or create the sampler referenced by the texture.
        std::shared_ptr<D3D12_SAMPLER_DESC> samplerState = m_impl->loaderResources.samplerMap[sampler];
        if (!samplerState)  // If not cached, create the sampler and store it in the sampler cache.
        {
            samplerState = std::make_shared<D3D12_SAMPLER_DESC>(sampler != nullptr ? CreateGLTFSampler(GetDevice().Get(), *sampler)
                                                                                   : D3D12Texture::DefaultSamplerDesc());
            m_impl->loaderResources.samplerMap[sampler] = samplerState;
        }

        pbrMaterial->SetTexture(GetDevice().Get(), slot, *textureView, samplerState.get());
    }
    void D3D12Resources::DropLoaderCaches()
    {
        m_impl->loaderResources = {};
    }

    void D3D12Resources::SetBrdfLut(_In_ Conformance::D3D12ResourceWithSRVDesc brdfLut)
    {
        m_impl->Resources.BrdfLutTexture = brdfLut.resource;

        GetDevice()->CreateShaderResourceView(m_impl->Resources.BrdfLutTexture.Get(), &brdfLut.srvDesc,
                                              m_impl->Resources.BrdfLutTextureDescriptor);
    }

    void D3D12Resources::CreateDeviceDependentResources(_In_ ID3D12Device* device)
    {
        m_impl->Initialize(device, m_impl->BasePipelineStateDesc);
    }

    void D3D12Resources::ReleaseDeviceDependentResources()
    {
        m_impl->Resources = {};
        m_impl->loaderResources = {};
        m_impl->Primitives.clear();
    }

    Microsoft::WRL::ComPtr<ID3D12Device> D3D12Resources::GetDevice() const
    {
        return m_impl->Resources.Device;
    }

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> D3D12Resources::CreateCopyCommandList() const
    {
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
        XRC_CHECK_THROW_HRCMD(GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_impl->Resources.CopyAllocator.Get(),
                                                             nullptr, __uuidof(ID3D12GraphicsCommandList),
                                                             reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));
        return cmdList;
    }

    void D3D12Resources::ExecuteCopyCommandList(ID3D12GraphicsCommandList* cmdList,
                                                std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> destroyAfterCopy) const
    {
        Internal::ThrowIf(!m_impl->Resources.CopyQueue->ExecuteCommandList(cmdList), "ExecuteCommandList failed");
        m_impl->Resources.DestructionQueue.PushResources(m_impl->Resources.CopyQueue->GetSignaledFenceValue(), std::move(destroyAfterCopy));
    }

    void D3D12Resources::SetTransforms(D3D12_CPU_DESCRIPTOR_HANDLE transformDescriptor)
    {
        GetDevice()->CopyDescriptorsSimple(ShaderSlots::NumVSResourceViews,
                                           m_impl->Resources.TransformHeap->GetCPUDescriptorHandleForHeapStart(), transformDescriptor,
                                           D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    void D3D12Resources::GetTransforms(D3D12_CPU_DESCRIPTOR_HANDLE destTransformDescriptor)
    {
        GetDevice()->CopyDescriptorsSimple(ShaderSlots::NumVSResourceViews, destTransformDescriptor,
                                           m_impl->Resources.TransformHeap->GetCPUDescriptorHandleForHeapStart(),
                                           D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    void D3D12Resources::GetGlobalTexturesAndSamplers(D3D12_CPU_DESCRIPTOR_HANDLE destTextureDescriptors,
                                                      D3D12_CPU_DESCRIPTOR_HANDLE destSamplerDescriptors)
    {
        GetDevice()->CopyDescriptorsSimple(ShaderSlots::NumTextures - ShaderSlots::NumMaterialSlots, destTextureDescriptors,
                                           m_impl->Resources.TextureHeap->GetCPUDescriptorHandleForHeapStart(),
                                           D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        GetDevice()->CopyDescriptorsSimple(ShaderSlots::NumSamplers - ShaderSlots::NumMaterialSlots, destSamplerDescriptors,
                                           m_impl->Resources.SamplerHeap->GetCPUDescriptorHandleForHeapStart(),
                                           D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> D3D12Resources::GetOrCreatePipelineState(DXGI_FORMAT colorRenderTargetFormat,
                                                                                         DXGI_FORMAT depthRenderTargetFormat,
                                                                                         BlendState blendState, DoubleSided doubleSided)
    {
        return m_impl->Resources.PipelineStates->GetOrCreatePipelineState(
            colorRenderTargetFormat, depthRenderTargetFormat, m_sharedState.GetFillMode(), m_sharedState.GetFrontFaceWindingOrder(),
            blendState, doubleSided, m_sharedState.GetDepthDirection());
    }

    void D3D12Resources::SetLight(DirectX::XMFLOAT3 direction, RGBColor diffuseColor)
    {
        m_impl->SceneBuffer.LightDirection = direction;
        m_impl->SceneBuffer.LightDiffuseColor = {diffuseColor.x, diffuseColor.y, diffuseColor.z};
    }

    void XM_CALLCONV D3D12Resources::SetModelToWorld(DirectX::FXMMATRIX modelToWorld) const
    {
        XMStoreFloat4x4(&m_impl->ModelBuffer.ModelToWorld, XMMatrixTranspose(modelToWorld));
        WithCopyCommandList(
            [&](ID3D12GraphicsCommandList* cmdList) { m_impl->Resources.ModelConstantBuffer.AsyncUpload(cmdList, &m_impl->ModelBuffer); });
    }

    void XM_CALLCONV D3D12Resources::SetViewProjection(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection) const
    {
        XMStoreFloat4x4(&m_impl->SceneBuffer.ViewProjection, XMMatrixTranspose(XMMatrixMultiply(view, projection)));
        XMStoreFloat4(&m_impl->SceneBuffer.EyePosition, XMMatrixInverse(nullptr, view).r[3]);
    }

    void D3D12Resources::SetEnvironmentMap(_In_ Conformance::D3D12ResourceWithSRVDesc specularEnvironmentMap,
                                           _In_ Conformance::D3D12ResourceWithSRVDesc diffuseEnvironmentMap)
    {
        if (diffuseEnvironmentMap.srvDesc.ViewDimension != D3D12_SRV_DIMENSION_TEXTURECUBE) {
            throw std::logic_error("Diffuse Resource View Type is not D3D_SRV_DIMENSION_TEXTURECUBE");
        }

        if (specularEnvironmentMap.srvDesc.ViewDimension != D3D12_SRV_DIMENSION_TEXTURECUBE) {
            throw std::logic_error("Specular Resource View Type is not D3D_SRV_DIMENSION_TEXTURECUBE");
        }
        auto desc = specularEnvironmentMap.resource->GetDesc();
        m_impl->SceneBuffer.NumSpecularMipLevels = desc.MipLevels;
        m_impl->Resources.SpecularEnvMapTexture = specularEnvironmentMap.resource;
        m_impl->Resources.DiffuseEnvMapTexture = diffuseEnvironmentMap.resource;

        GetDevice()->CreateShaderResourceView(m_impl->Resources.SpecularEnvMapTexture.Get(), &specularEnvironmentMap.srvDesc,
                                              m_impl->Resources.SpecularEnvMapTextureDescriptor);
        GetDevice()->CreateShaderResourceView(m_impl->Resources.DiffuseEnvMapTexture.Get(), &diffuseEnvironmentMap.srvDesc,
                                              m_impl->Resources.DiffuseEnvMapTextureDescriptor);
    }

    Conformance::D3D12ResourceWithSRVDesc D3D12Resources::CreateTypedSolidColorTexture(RGBAColor color)
    {
        return m_impl->Resources.SolidColorTextureCache.CreateTypedSolidColorTexture(*this, color);
    }

    void D3D12Resources::Bind(_In_ ID3D12GraphicsCommandList* directCommandList) const
    {
        directCommandList->SetGraphicsRootSignature(m_impl->Resources.RootSignature.Get());

        WithCopyCommandList(
            [&](ID3D12GraphicsCommandList* cmdList) { m_impl->Resources.SceneConstantBuffer.AsyncUpload(cmdList, &m_impl->SceneBuffer); });

        directCommandList->SetGraphicsRootConstantBufferView(ShaderSlots::ConstantBuffers::Scene,
                                                             m_impl->Resources.SceneConstantBuffer.GetResource()->GetGPUVirtualAddress());
        directCommandList->SetGraphicsRootConstantBufferView(ShaderSlots::ConstantBuffers::Model,
                                                             m_impl->Resources.ModelConstantBuffer.GetResource()->GetGPUVirtualAddress());
    }

    void D3D12Resources::BindDescriptorHeaps(_In_ ID3D12GraphicsCommandList* directCommandList, ID3D12DescriptorHeap* srvDescriptorHeap,
                                             ID3D12DescriptorHeap* samplerDescriptorHeap) const
    {
        using RootSig::RootParamIndex;

        static_assert(ShaderSlots::DiffuseTexture == ShaderSlots::SpecularTexture + 1, "Diffuse must follow Specular slot");
        static_assert(ShaderSlots::SpecularTexture == ShaderSlots::Brdf + 1, "Specular must follow BRDF slot");

        ID3D12DescriptorHeap* descriptorHeaps[] = {srvDescriptorHeap, samplerDescriptorHeap};
        directCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

        auto srvDescriptorSize = GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // count is defined by InitAsDescriptorTable
        directCommandList->SetGraphicsRootDescriptorTable(RootParamIndex::TransformsBuffer,
                                                          srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        directCommandList->SetGraphicsRootDescriptorTable(
            RootParamIndex::TextureSRVs, CD3DX12_GPU_DESCRIPTOR_HANDLE(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
                                                                       ShaderSlots::NumVSResourceViews, srvDescriptorSize));
        directCommandList->SetGraphicsRootDescriptorTable(RootParamIndex::TextureSamplers,
                                                          samplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    }

    std::pair<ID3D12Fence*, uint64_t> D3D12Resources::GetFenceAndValue() const
    {
        return std::make_pair(m_impl->Resources.CopyQueue->GetFence().Get(), m_impl->Resources.CopyQueue->GetSignaledFenceValue());
    }

    PrimitiveHandle D3D12Resources::MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                                  const std::shared_ptr<Pbr::Material>& material)
    {
        auto typedMaterial = std::dynamic_pointer_cast<Pbr::D3D12Material>(material);
        if (!typedMaterial) {
            throw std::logic_error("Got the wrong type of material");
        }
        return m_impl->Primitives.emplace_back(*this, primitiveBuilder, typedMaterial);
    }

    D3D12Primitive& D3D12Resources::GetPrimitive(PrimitiveHandle p)
    {
        return m_impl->Primitives[p];
    }

    const D3D12Primitive& D3D12Resources::GetPrimitive(PrimitiveHandle p) const
    {
        return m_impl->Primitives[p];
    }

    void D3D12Resources::SetFillMode(FillMode mode)
    {
        m_sharedState.SetFillMode(mode);
    }

    FillMode D3D12Resources::GetFillMode() const
    {
        return m_sharedState.GetFillMode();
    }

    void D3D12Resources::SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder)
    {
        m_sharedState.SetFrontFaceWindingOrder(windingOrder);
    }

    FrontFaceWindingOrder D3D12Resources::GetFrontFaceWindingOrder() const
    {
        return m_sharedState.GetFrontFaceWindingOrder();
    }

    void D3D12Resources::SetDepthDirection(DepthDirection depthDirection)
    {
        m_sharedState.SetDepthDirection(depthDirection);
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
