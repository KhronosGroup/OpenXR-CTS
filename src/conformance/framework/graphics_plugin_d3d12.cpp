// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if defined(XR_USE_GRAPHICS_API_D3D12) && !defined(MISSING_DIRECTX_COLORS)

#include "graphics_plugin.h"
#include "common/xr_linear.h"
#include "conformance_framework.h"
#include "graphics_plugin_impl_helpers.h"
#include "swapchain_image_data.h"
#include "utilities/Geometry.h"
#include "utilities/align_to.h"
#include "utilities/array_size.h"
#include "utilities/d3d_common.h"
#include "utilities/swapchain_parameters.h"
#include "utilities/throw_helpers.h"

#include <catch2/catch_test_macros.hpp>

#include <D3Dcompiler.h>
#include <DirectXColors.h>
#include <dxgiformat.h>
#include <windows.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <openxr/openxr_platform.h>

#include <algorithm>
#include <array>
#include <functional>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace Conformance
{
    ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* d3d12Device, uint32_t size, D3D12_HEAP_TYPE heapType)
    {
        D3D12_RESOURCE_STATES d3d12ResourceState;
        if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
            d3d12ResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
            size = AlignTo<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(size);
        }
        else {
            d3d12ResourceState = D3D12_RESOURCE_STATE_COMMON;
        }

        D3D12_HEAP_PROPERTIES heapProp{};
        heapProp.Type = heapType;
        heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC buffDesc{};
        buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffDesc.Alignment = 0;
        buffDesc.Width = size;
        buffDesc.Height = 1;
        buffDesc.DepthOrArraySize = 1;
        buffDesc.MipLevels = 1;
        buffDesc.Format = DXGI_FORMAT_UNKNOWN;
        buffDesc.SampleDesc.Count = 1;
        buffDesc.SampleDesc.Quality = 0;
        buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ComPtr<ID3D12Resource> buffer;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &buffDesc, d3d12ResourceState, nullptr,
                                                                   __uuidof(ID3D12Resource),
                                                                   reinterpret_cast<void**>(buffer.ReleaseAndGetAddressOf())));
        return buffer;
    }

    struct D3D12Mesh
    {
        ComPtr<ID3D12Device> device;
        uint32_t vertexBufferSizeBytes;
        ComPtr<ID3D12Resource> vertexBuffer;
        uint32_t indexBufferSizeBytes;

        ComPtr<ID3D12Resource> indexBuffer;
        UINT numIndices;

        D3D12Mesh(ComPtr<ID3D12Device> d3d12Device, span<const uint16_t> indices, span<const Geometry::Vertex> vertices,
                  const std::function<bool(ID3D12CommandList* cmdList)>& ExecuteCommandList)
            : device(d3d12Device), numIndices((UINT)indices.size())
        {

            ComPtr<ID3D12CommandAllocator> commandAllocator;

            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                                      reinterpret_cast<void**>(commandAllocator.ReleaseAndGetAddressOf())));
            XRC_CHECK_THROW_HRCMD(commandAllocator->SetName(L"CTS mesh upload command allocator"));

            ComPtr<ID3D12GraphicsCommandList> cmdList;
            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
                                                                 __uuidof(ID3D12GraphicsCommandList),
                                                                 reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));
            XRC_CHECK_THROW_HRCMD(cmdList->SetName(L"CTS mesh upload command list"));

            vertexBufferSizeBytes = (uint32_t)vertices.size_bytes();
            ComPtr<ID3D12Resource> vertexBufferUpload;
            vertexBuffer = CreateBuffer(d3d12Device.Get(), vertexBufferSizeBytes, D3D12_HEAP_TYPE_DEFAULT);
            XRC_CHECK_THROW_HRCMD(vertexBuffer->SetName(L"CTS mesh vertex buffer"));
            {
                vertexBufferUpload = CreateBuffer(d3d12Device.Get(), vertexBufferSizeBytes, D3D12_HEAP_TYPE_UPLOAD);
                XRC_CHECK_THROW_HRCMD(vertexBufferUpload->SetName(L"CTS mesh vertex buffer upload"));

                void* data;
                const D3D12_RANGE readRange{0, 0};
                XRC_CHECK_THROW_HRCMD(vertexBufferUpload->Map(0, &readRange, &data));
                memcpy(data, vertices.data(), vertexBufferSizeBytes);
                vertexBufferUpload->Unmap(0, nullptr);

                cmdList->CopyBufferRegion(vertexBuffer.Get(), 0, vertexBufferUpload.Get(), 0, vertexBufferSizeBytes);
            }

            indexBufferSizeBytes = (uint32_t)indices.size_bytes();
            ComPtr<ID3D12Resource> indexBufferUpload;
            indexBuffer = CreateBuffer(d3d12Device.Get(), indexBufferSizeBytes, D3D12_HEAP_TYPE_DEFAULT);
            XRC_CHECK_THROW_HRCMD(indexBuffer->SetName(L"CTS mesh index buffer"));
            {
                indexBufferUpload = CreateBuffer(d3d12Device.Get(), indexBufferSizeBytes, D3D12_HEAP_TYPE_UPLOAD);
                XRC_CHECK_THROW_HRCMD(indexBufferUpload->SetName(L"CTS mesh index buffer upload"));
                void* data;
                const D3D12_RANGE readRange{0, 0};
                XRC_CHECK_THROW_HRCMD(indexBufferUpload->Map(0, &readRange, &data));
                memcpy(data, indices.data(), indexBufferSizeBytes);
                indexBufferUpload->Unmap(0, nullptr);

                cmdList->CopyBufferRegion(indexBuffer.Get(), 0, indexBufferUpload.Get(), 0, indexBufferSizeBytes);
            }

            XRC_CHECK_THROW_HRCMD(cmdList->Close());
            XRC_CHECK_THROW(ExecuteCommandList(cmdList.Get()));
        }
    };

    struct D3D12FallbackDepthTexture
    {
    public:
        D3D12FallbackDepthTexture() = default;

        void Reset()
        {
            m_texture = nullptr;
            m_xrImage.texture = nullptr;
        }
        bool Allocated() const
        {
            return m_texture != nullptr;
        }

        void Allocate(ID3D12Device* d3d12Device, UINT width, UINT height, uint16_t arraySize, UINT sampleCount,
                      D3D12_RESOURCE_DIMENSION dimension, uint64_t alignment, D3D12_TEXTURE_LAYOUT layout)
        {
            Reset();

            D3D12_HEAP_PROPERTIES heapProp{};
            heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_RESOURCE_DESC depthDesc{};
            depthDesc.Dimension = dimension;
            depthDesc.Alignment = alignment;
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.DepthOrArraySize = arraySize;
            depthDesc.MipLevels = 1;
            depthDesc.Format = kDefaultDepthFormatTypeless;
            depthDesc.SampleDesc.Count = sampleCount;
            depthDesc.Layout = layout;
            depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE clearValue{};
            clearValue.DepthStencil.Depth = 1.0f;
            clearValue.Format = kDefaultDepthFormat;

            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommittedResource(
                &heapProp, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, __uuidof(ID3D12Resource),
                reinterpret_cast<void**>(m_texture.ReleaseAndGetAddressOf())));
            XRC_CHECK_THROW_HRCMD(m_texture->SetName(L"CTS fallback depth tex"));
            m_xrImage.texture = m_texture.Get();
        }

        const XrSwapchainImageD3D12KHR& GetTexture() const
        {
            return m_xrImage;
        }

    private:
        ComPtr<ID3D12Resource> m_texture{};
        XrSwapchainImageD3D12KHR m_xrImage{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, nullptr, nullptr};
    };

    class D3D12SwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageD3D12KHR>
    {
        void init()
        {
            XRC_CHECK_THROW_HRCMD(
                m_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                      reinterpret_cast<void**>(commandAllocator.ReleaseAndGetAddressOf())));
            XRC_CHECK_THROW_HRCMD(commandAllocator->SetName(L"CTS swapchain command allocator"));

            viewProjectionCBuffer = CreateBuffer(m_d3d12Device.Get(), sizeof(ViewProjectionConstantBuffer), D3D12_HEAP_TYPE_UPLOAD);
            XRC_CHECK_THROW_HRCMD(viewProjectionCBuffer->SetName(L"CTS view proj cbuffer"));
        }

    public:
        D3D12SwapchainImageData(ComPtr<ID3D12Device> d3d12Device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR, capacity, createInfo)
            , m_d3d12Device(std::move(d3d12Device))
            , m_internalDepthTextures(capacity)
        {
            init();
        }

        D3D12SwapchainImageData(ComPtr<ID3D12Device> d3d12Device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo,
                                XrSwapchain depthSwapchain, const XrSwapchainCreateInfo& depthCreateInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR, capacity, createInfo, depthSwapchain, depthCreateInfo)
            , m_d3d12Device(std::move(d3d12Device))
            , m_internalDepthTextures(capacity)
        {
            init();
        }

        void Reset() override
        {
            m_internalDepthTextures.clear();
            m_d3d12Device = nullptr;
            SwapchainImageDataBase::Reset();
        }

        const XrSwapchainImageD3D12KHR& GetFallbackDepthSwapchainImage(uint32_t i) override
        {
            if (!m_internalDepthTextures[i].Allocated()) {
                XRC_CHECK_THROW(GetTypedImage(i).texture != nullptr);
                /// @todo we should not need to do this introspection, I think
                D3D12_RESOURCE_DESC colorDesc = GetTypedImage(i).texture->GetDesc();
                m_internalDepthTextures[i].Allocate(m_d3d12Device.Get(), this->Width(), this->Height(), (uint16_t)this->ArraySize(),
                                                    this->SampleCount(), colorDesc.Dimension, colorDesc.Alignment, colorDesc.Layout);
            }

            return m_internalDepthTextures[i].GetTexture();
        }

        ID3D12CommandAllocator* GetCommandAllocator() const
        {
            return commandAllocator.Get();
        }

        uint64_t GetFrameFenceValue() const
        {
            return fenceValue;
        }
        void SetFrameFenceValue(uint64_t fenceVal)
        {
            fenceValue = fenceVal;
        }

        void ResetCommandAllocator()
        {
            XRC_CHECK_THROW_HRCMD(commandAllocator->Reset());
        }

        void RequestModelCBuffer(uint32_t requiredSize)
        {
            if (!modelCBuffer || (requiredSize > modelCBuffer->GetDesc().Width)) {
                modelCBuffer = CreateBuffer(m_d3d12Device.Get(), requiredSize, D3D12_HEAP_TYPE_UPLOAD);
                XRC_CHECK_THROW_HRCMD(modelCBuffer->SetName(L"CTS model cbuffer"));
            }
        }

        ID3D12Resource* GetModelCBuffer() const
        {
            return modelCBuffer.Get();
        }

        ID3D12Resource* GetViewProjectionCBuffer() const
        {
            return viewProjectionCBuffer.Get();
        }

    private:
        ComPtr<ID3D12Device> m_d3d12Device;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ComPtr<ID3D12Resource> modelCBuffer;
        ComPtr<ID3D12Resource> viewProjectionCBuffer;
        uint64_t fenceValue = 0;
        std::vector<D3D12FallbackDepthTexture> m_internalDepthTextures;
    };

    struct D3D12GraphicsPlugin : public IGraphicsPlugin
    {
        D3D12GraphicsPlugin(std::shared_ptr<IPlatformPlugin>);

        ~D3D12GraphicsPlugin() override;

        bool Initialize() override;

        bool IsInitialized() const override;

        void Shutdown() override;

        std::string DescribeGraphics() const override;

        std::vector<std::string> GetInstanceExtensions() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

        void ClearSwapchainCache() override;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, uint32_t arraySlice, const RGBAImage& image) override;

        std::string GetImageFormatName(int64_t imageFormat) const override;

        bool IsImageFormatKnown(int64_t imageFormat) const override;

        bool GetSwapchainCreateTestParameters(XrInstance instance, XrSession session, XrSystemId systemId, int64_t imageFormat,
                                              SwapchainCreateTestParameters* swapchainTestParameters) override;

        bool ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                     uint32_t* imageCount) const override;
        bool ValidateSwapchainImageState(XrSwapchain swapchain, uint32_t index, int64_t imageFormat) const override;

        int64_t SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        // Format required by RGBAImage type.
        int64_t GetSRGBA8Format() const override;

        ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(size_t size,
                                                                          const XrSwapchainCreateInfo& colorSwapchainCreateInfo,
                                                                          XrSwapchain depthSwapchain,
                                                                          const XrSwapchainCreateInfo& depthSwapchainCreateInfo) override;

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                             XrColor4f bgColor = DarkSlateGrey) override;

        MeshHandle MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        const RenderParams& params) override;

        void Flush() override;

    protected:
        D3D12_CPU_DESCRIPTOR_HANDLE CreateRenderTargetView(ID3D12Resource* colorTexture, uint32_t imageArrayIndex,
                                                           int64_t colorSwapchainFormat);
        D3D12_CPU_DESCRIPTOR_HANDLE CreateDepthStencilView(ID3D12Resource* depthStencilTexture, uint32_t imageArrayIndex,
                                                           DXGI_FORMAT depthSwapchainFormat);

        ID3D12PipelineState* GetOrCreatePipelineState(DXGI_FORMAT colorSwapchainFormat, DXGI_FORMAT dsvSwapchainFormat);
        bool ExecuteCommandList(ID3D12CommandList* cmdList) const;
        void CpuWaitForFence(uint64_t fenceVal) const;
        void WaitForGpu() const;

    protected:
        bool initialized = false;
        XrGraphicsBindingD3D12KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
        ComPtr<ID3D12Device> d3d12Device;
        ComPtr<ID3D12CommandQueue> d3d12CmdQueue;
        ComPtr<ID3D12Fence> fence;
        mutable uint64_t fenceValue = 0;
        HANDLE fenceEvent = INVALID_HANDLE_VALUE;

        SwapchainImageDataMap<D3D12SwapchainImageData> m_swapchainImageDataMap;
        const XrSwapchainImageBaseHeader* lastSwapchainImage = nullptr;

        // Resources needed for rendering cubes
        const ComPtr<ID3DBlob> vertexShaderBytes;
        const ComPtr<ID3DBlob> pixelShaderBytes;
        ComPtr<ID3D12RootSignature> rootSignature;
        std::map<std::pair<DXGI_FORMAT, DXGI_FORMAT>, ComPtr<ID3D12PipelineState>> pipelineStates;
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        ComPtr<ID3D12DescriptorHeap> dsvHeap;

        MeshHandle m_cubeMesh;
        VectorWithGenerationCountedHandles<D3D12Mesh, MeshHandle> m_meshes;
    };

    D3D12GraphicsPlugin::D3D12GraphicsPlugin(std::shared_ptr<IPlatformPlugin>)
        : vertexShaderBytes(CompileShader(ShaderHlsl, "MainVS", "vs_5_1")), pixelShaderBytes(CompileShader(ShaderHlsl, "MainPS", "ps_5_1"))
    {
    }

    D3D12GraphicsPlugin::~D3D12GraphicsPlugin()
    {
        ShutdownDevice();
        Shutdown();
    }

    bool D3D12GraphicsPlugin::Initialize()
    {
        if (initialized)
            return false;

        // To do.
        initialized = true;
        return initialized;
    }

    bool D3D12GraphicsPlugin::IsInitialized() const
    {
        return initialized;
    }

    void D3D12GraphicsPlugin::Shutdown()
    {
        if (initialized) {
            // To do.
            initialized = false;
        }
    }

    std::string D3D12GraphicsPlugin::DescribeGraphics() const
    {
        return std::string("D3D12");
    }

    std::vector<std::string> D3D12GraphicsPlugin::GetInstanceExtensions() const
    {
        return {XR_KHR_D3D12_ENABLE_EXTENSION_NAME};
    }

    const XrBaseInStructure* D3D12GraphicsPlugin::GetGraphicsBinding() const
    {
        if (graphicsBinding.device && graphicsBinding.queue) {
            return reinterpret_cast<const XrBaseInStructure*>(&graphicsBinding);
        }
        return nullptr;
    }

    bool D3D12GraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                                               uint32_t /*deviceCreationFlags*/)
    {
        try {
            XrGraphicsRequirementsD3D12KHR graphicsRequirements{
                XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR, nullptr, {0, 0}, D3D_FEATURE_LEVEL_11_0};

            // Create the D3D12 device for the adapter associated with the system.
            if (checkGraphicsRequirements) {

                auto xrGetD3D12GraphicsRequirementsKHR =
                    GetInstanceExtensionFunction<PFN_xrGetD3D12GraphicsRequirementsKHR>(instance, "xrGetD3D12GraphicsRequirementsKHR");

                XrResult result = xrGetD3D12GraphicsRequirementsKHR(instance, systemId, &graphicsRequirements);
                XRC_CHECK_THROW(ValidateResultAllowed("xrGetD3D12GraphicsRequirementsKHR", result));
                if (FAILED(result)) {
                    // Log result?
                    return false;
                }
            }

            const ComPtr<IDXGIAdapter1> adapter = GetDXGIAdapter(graphicsRequirements.adapterLuid);

            // Create a list of feature levels which are both supported by the OpenXR runtime and this application.
            std::vector<D3D_FEATURE_LEVEL> featureLevels = {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
                                                            D3D_FEATURE_LEVEL_11_0};
            featureLevels.erase(std::remove_if(featureLevels.begin(), featureLevels.end(),
                                               [&](D3D_FEATURE_LEVEL fl) { return (fl < graphicsRequirements.minFeatureLevel); }),
                                featureLevels.end());

            if (featureLevels.empty()) {
                // Log result?
                return false;
            }

#if !defined(NDEBUG)
            ComPtr<ID3D12Debug> debugCtrl;
            if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugCtrl))) {
                debugCtrl->EnableDebugLayer();
            }
#endif

            XRC_CHECK_THROW_HRCMD(D3D12CreateDevice(adapter.Get(), featureLevels.back(), __uuidof(ID3D12Device),
                                                    reinterpret_cast<void**>(d3d12Device.ReleaseAndGetAddressOf())));
            XRC_CHECK_THROW_HRCMD(d3d12Device->SetName(L"CTS device"));

            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue),
                                                                  reinterpret_cast<void**>(d3d12CmdQueue.ReleaseAndGetAddressOf())));
            XRC_CHECK_THROW_HRCMD(d3d12CmdQueue->SetName(L"CTS direct cmd queue"));

            {
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
                heapDesc.NumDescriptors = 1;
                heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                XRC_CHECK_THROW_HRCMD(d3d12Device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap),
                                                                        reinterpret_cast<void**>(rtvHeap.ReleaseAndGetAddressOf())));
                XRC_CHECK_THROW_HRCMD(rtvHeap->SetName(L"CTS RTV heap"));
            }
            {
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
                heapDesc.NumDescriptors = 1;
                heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                XRC_CHECK_THROW_HRCMD(d3d12Device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap),
                                                                        reinterpret_cast<void**>(dsvHeap.ReleaseAndGetAddressOf())));
                XRC_CHECK_THROW_HRCMD(dsvHeap->SetName(L"CTS DSV heap"));
            }

            D3D12_ROOT_PARAMETER rootParams[2];
            rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParams[0].Descriptor.ShaderRegister = 0;
            rootParams[0].Descriptor.RegisterSpace = 0;
            rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
            rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParams[1].Descriptor.ShaderRegister = 1;
            rootParams[1].Descriptor.RegisterSpace = 0;
            rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

            D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
            rootSignatureDesc.NumParameters = (UINT)ArraySize(rootParams);
            rootSignatureDesc.pParameters = rootParams;
            rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr<ID3DBlob> rootSignatureBlob;
            ComPtr<ID3DBlob> error;
            XRC_CHECK_THROW_HRCMD(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
                                                              rootSignatureBlob.ReleaseAndGetAddressOf(), error.ReleaseAndGetAddressOf()));

            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
                                                                   rootSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature),
                                                                   reinterpret_cast<void**>(rootSignature.ReleaseAndGetAddressOf())));
            XRC_CHECK_THROW_HRCMD(rootSignature->SetName(L"CTS root signature"));

            /// @todo not sure why we are doing this.
            XrSwapchainCreateInfo emptyCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
            auto initImages = std::make_shared<D3D12SwapchainImageData>(d3d12Device.Get(), 1, emptyCreateInfo);

            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                                           reinterpret_cast<void**>(fence.ReleaseAndGetAddressOf())));
            XRC_CHECK_THROW_HRCMD(fence->SetName(L"CTS fence"));

            fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
            XRC_CHECK_THROW(fenceEvent != nullptr);

            m_cubeMesh = MakeCubeMesh();

            graphicsBinding.device = d3d12Device.Get();
            graphicsBinding.queue = d3d12CmdQueue.Get();

            return true;
        }
        catch (...) {
            // Log it?
        }

        return false;
    }

    void D3D12GraphicsPlugin::ClearSwapchainCache()
    {
        m_swapchainImageDataMap.Reset();
        lastSwapchainImage = nullptr;
    }

    void D3D12GraphicsPlugin::ShutdownDevice()
    {
        graphicsBinding = XrGraphicsBindingD3D12KHR{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
        d3d12CmdQueue.Reset();
        fence.Reset();
        if (fenceEvent != INVALID_HANDLE_VALUE) {
            ::CloseHandle(fenceEvent);
            fenceEvent = INVALID_HANDLE_VALUE;
        }
        rootSignature.Reset();
        pipelineStates.clear();
        m_cubeMesh = {};
        m_meshes.clear();
        rtvHeap.Reset();
        dsvHeap.Reset();
        m_swapchainImageDataMap.Reset();

        d3d12Device.Reset();
        lastSwapchainImage = nullptr;
    }

    void D3D12GraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, uint32_t arraySlice, const RGBAImage& image)
    {
        D3D12_HEAP_PROPERTIES heapProp{};
        heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        ID3D12Resource* const destTexture = reinterpret_cast<const XrSwapchainImageD3D12KHR*>(swapchainImage)->texture;
        const D3D12_RESOURCE_DESC rgbaImageDesc = destTexture->GetDesc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint64_t requiredSize = 0;
        uint64_t rowSizeInBytes = 0;
        d3d12Device->GetCopyableFootprints(&rgbaImageDesc, 0, 1, 0, &layout, nullptr, &rowSizeInBytes, &requiredSize);

        ComPtr<ID3D12Resource> uploadBuffer = CreateBuffer(d3d12Device.Get(), (uint32_t)(requiredSize), D3D12_HEAP_TYPE_UPLOAD);
        XRC_CHECK_THROW_HRCMD(uploadBuffer->SetName(L"CTS RGBA upload buffer"));
        {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(image.pixels.data());
            const uint32_t imageRowPitch = image.width * sizeof(uint32_t);
            uint8_t* dst;
            const D3D12_RANGE readRange{0, 0};
            XRC_CHECK_THROW_HRCMD(uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&dst)));
            for (int y = 0; y < image.height; ++y) {
                memcpy(dst, src, imageRowPitch);

                src += imageRowPitch;
                dst += layout.Footprint.RowPitch;
            }
            const D3D12_RANGE writeRange{0, (SIZE_T)requiredSize};
            uploadBuffer->Unmap(0, &writeRange);
        }

        D3D12SwapchainImageData* swapchainData = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImage).first;

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, swapchainData->GetCommandAllocator(),
                                                             nullptr, __uuidof(ID3D12GraphicsCommandList),
                                                             reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));
        XRC_CHECK_THROW_HRCMD(cmdList->SetName(L"CTS copy rgba command list"));

        D3D12_TEXTURE_COPY_LOCATION srcLocation;
        srcLocation.pResource = uploadBuffer.Get();
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint = layout;

        D3D12_TEXTURE_COPY_LOCATION dstLocation;
        dstLocation.pResource = destTexture;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = D3D11CalcSubresource(0, arraySlice, rgbaImageDesc.MipLevels);

        cmdList->CopyTextureRegion(&dstLocation, 0 /* X */, 0 /* Y */, 0 /* Z */, &srcLocation, nullptr);

        XRC_CHECK_THROW_HRCMD(cmdList->Close());
        XRC_CHECK_THROW(ExecuteCommandList(cmdList.Get()));

        WaitForGpu();
    }

    std::string D3D12GraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        return GetDxgiImageFormatName(imageFormat);
    }

    bool D3D12GraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        return IsDxgiImageFormatKnown(imageFormat);
    }

    bool D3D12GraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                               int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        return GetDxgiSwapchainCreateTestParameters(imageFormat, swapchainTestParameters);
    }

    bool D3D12GraphicsPlugin::ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* tp,
                                                      XrSwapchain swapchain, uint32_t* imageCount) const noexcept(false)
    {
        // OK to use CHECK and REQUIRE in here because this is always called from within a test.
        *imageCount = 0;  // Zero until set below upon success.

        std::vector<XrSwapchainImageD3D12KHR> swapchainImageVector;
        uint32_t countOutput;

        XrResult result = xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr);
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput > 0);

        swapchainImageVector.resize(countOutput, XrSwapchainImageD3D12KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});

        // Exercise XR_ERROR_SIZE_INSUFFICIENT
        if (countOutput >= 2) {  // Need at least two in order to exercise XR_ERROR_SIZE_INSUFFICIENT
            result = xrEnumerateSwapchainImages(swapchain, 1, &countOutput,
                                                reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
            CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
            CHECK(result == XR_ERROR_SIZE_INSUFFICIENT);
            CHECK(countOutput == swapchainImageVector.size());
            // Contents of swapchainImageVector is undefined, so nothing to validate about the output.
        }

        countOutput = (uint32_t)swapchainImageVector.size();  // Restore countOutput if it was (mistakenly) modified.
        swapchainImageVector.clear();                         // Who knows what the runtime may have mistakely written into our vector.
        swapchainImageVector.resize(countOutput, XrSwapchainImageD3D12KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
        result = xrEnumerateSwapchainImages(swapchain, countOutput, &countOutput,
                                            reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput == swapchainImageVector.size());
        REQUIRE(ValidateStructVectorType(swapchainImageVector, XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR));

        for (const XrSwapchainImageD3D12KHR& image : swapchainImageVector) {
            D3D12_RESOURCE_DESC desc = image.texture->GetDesc();

            // Verify that the format is the typeless version of the requested format.
            CHECK(desc.Format == tp->expectedCreatedImageFormat);

            // Anything else from desc to check?
        }

        *imageCount = countOutput;
        return true;
    }

    bool D3D12GraphicsPlugin::ExecuteCommandList(ID3D12CommandList* cmdList) const
    {
        bool success;
        __try {
            ID3D12CommandList* cmdLists[] = {cmdList};
            d3d12CmdQueue->ExecuteCommandLists((UINT)ArraySize(cmdLists), cmdLists);
            success = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            success = false;
        }

        ++fenceValue;
        XRC_CHECK_THROW_HRCMD(d3d12CmdQueue->Signal(fence.Get(), fenceValue));

        return success;
    }

    bool D3D12GraphicsPlugin::ValidateSwapchainImageState(XrSwapchain swapchain, uint32_t index, int64_t imageFormat) const
    {
        // OK to use CHECK and REQUIRE in here because this is always called from within a test.
        std::vector<XrSwapchainImageD3D12KHR> swapchainImageVector;
        uint32_t countOutput;

        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr));
        swapchainImageVector.resize(countOutput, XrSwapchainImageD3D12KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, countOutput, &countOutput,
                                                         reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data())));

        ComPtr<ID3D12CommandAllocator> commandAllocator;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                                  reinterpret_cast<void**>(commandAllocator.ReleaseAndGetAddressOf())));
        XRC_CHECK_THROW_HRCMD(commandAllocator->SetName(L"CTS ValidateSwapchainImageState cmd alloc"));

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
                                                             __uuidof(ID3D12GraphicsCommandList),
                                                             reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));
        XRC_CHECK_THROW_HRCMD(commandAllocator->SetName(L"CTS ValidateSwapchainImageState cmd list"));

        const XrSwapchainImageD3D12KHR& image = swapchainImageVector[index];
        const bool isColorFormat = GetDxgiSwapchainTestMap()[imageFormat].colorFormat;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = image.texture;
        barrier.Transition.StateBefore = isColorFormat ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &barrier);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        cmdList->ResourceBarrier(1, &barrier);

        XRC_CHECK_THROW_HRCMD(cmdList->Close());

        BOOL oldBreakOnError = FALSE;
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(d3d12Device.As<ID3D12InfoQueue>(&infoQueue))) {
            oldBreakOnError = infoQueue->GetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        }

        const bool success = ExecuteCommandList(cmdList.Get());

        if (infoQueue) {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, oldBreakOnError);
        }

        WaitForGpu();

        return success;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t D3D12GraphicsPlugin::SelectColorSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of supported color swapchain formats.
        const std::array<DXGI_FORMAT, 4> f{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM,
                                           DXGI_FORMAT_B8G8R8A8_UNORM};

        const int64_t* formatArrayEnd = formatArray + count;
        auto it = std::find_first_of(formatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t D3D12GraphicsPlugin::SelectDepthSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of supported depth swapchain formats.
        const std::array<DXGI_FORMAT, 4> f{DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D16_UNORM,
                                           DXGI_FORMAT_D32_FLOAT_S8X24_UINT};

        const int64_t* formatArrayEnd = formatArray + count;
        auto it = std::find_first_of(formatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    int64_t D3D12GraphicsPlugin::GetSRGBA8Format() const
    {
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    ISwapchainImageData* D3D12GraphicsPlugin::AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {
        auto typedResult = std::make_unique<D3D12SwapchainImageData>(d3d12Device.Get(), uint32_t(size), swapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    ISwapchainImageData* D3D12GraphicsPlugin::AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo)
    {

        auto typedResult = std::make_unique<D3D12SwapchainImageData>(d3d12Device.Get(), uint32_t(size), colorSwapchainCreateInfo,
                                                                     depthSwapchain, depthSwapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsPlugin::CreateRenderTargetView(ID3D12Resource* colorTexture, uint32_t imageArrayIndex,
                                                                            int64_t colorSwapchainFormat)
    {
        const D3D12_RESOURCE_DESC colorTextureDesc = colorTexture->GetDesc();

        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
        renderTargetViewDesc.Format = (DXGI_FORMAT)colorSwapchainFormat;
        if (colorTextureDesc.DepthOrArraySize > 1) {
            if (colorTextureDesc.SampleDesc.Count > 1) {
                renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                renderTargetViewDesc.Texture2DMSArray.ArraySize = 1;
                renderTargetViewDesc.Texture2DMSArray.FirstArraySlice = imageArrayIndex;
            }
            else {
                renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                renderTargetViewDesc.Texture2DArray.ArraySize = 1;
                renderTargetViewDesc.Texture2DArray.FirstArraySlice = imageArrayIndex;
            }
        }
        else {
            if (colorTextureDesc.SampleDesc.Count > 1) {
                renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            }
            else {
                renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            }
        }
        d3d12Device->CreateRenderTargetView(colorTexture, &renderTargetViewDesc, renderTargetView);

        return renderTargetView;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsPlugin::CreateDepthStencilView(ID3D12Resource* depthStencilTexture, uint32_t imageArrayIndex,
                                                                            DXGI_FORMAT depthSwapchainFormat)
    {
        const D3D12_RESOURCE_DESC depthStencilTextureDesc = depthStencilTexture->GetDesc();

        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
        depthStencilViewDesc.Format = depthSwapchainFormat;
        if (depthStencilTextureDesc.DepthOrArraySize > 1) {
            if (depthStencilTextureDesc.SampleDesc.Count > 1) {
                depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                depthStencilViewDesc.Texture2DMSArray.ArraySize = 1;
                depthStencilViewDesc.Texture2DMSArray.FirstArraySlice = imageArrayIndex;
            }
            else {
                depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                depthStencilViewDesc.Texture2DArray.ArraySize = 1;
                depthStencilViewDesc.Texture2DArray.FirstArraySlice = imageArrayIndex;
            }
        }
        else {
            if (depthStencilTextureDesc.SampleDesc.Count > 1) {
                depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            }
            else {
                depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            }
        }
        d3d12Device->CreateDepthStencilView(depthStencilTexture, &depthStencilViewDesc, depthStencilView);

        return depthStencilView;
    }

    void D3D12GraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                              XrColor4f bgColor)
    {

        D3D12SwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        ID3D12Resource* const colorTexture = swapchainData->GetTypedImage(imageIndex).texture;

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, swapchainData->GetCommandAllocator(),
                                                             nullptr, __uuidof(ID3D12GraphicsCommandList),
                                                             reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));
        XRC_CHECK_THROW_HRCMD(cmdList->SetName(L"CTS ClearImageSlice cmd list"));

        // Clear color buffer.
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView =
            CreateRenderTargetView(colorTexture, imageArrayIndex, swapchainData->GetCreateInfo().format);
        // TODO: Do not clear to a color when using a pass-through view configuration.
        FLOAT bg[] = {bgColor.r, bgColor.g, bgColor.b, bgColor.a};
        cmdList->ClearRenderTargetView(renderTargetView, bg, 0, nullptr);

        // Clear depth buffer.
        ID3D12Resource* depthStencilTexture = swapchainData->GetDepthImageForColorIndex(imageIndex).texture;

        const XrSwapchainCreateInfo* depthCreateInfo = swapchainData->GetDepthCreateInfo();
        DXGI_FORMAT depthSwapchainFormat = GetDepthStencilFormatOrDefault(depthCreateInfo);

        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = CreateDepthStencilView(depthStencilTexture, imageArrayIndex, depthSwapchainFormat);
        cmdList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        XRC_CHECK_THROW_HRCMD(cmdList->Close());
        XRC_CHECK_THROW(ExecuteCommandList(cmdList.Get()));
    }

    inline MeshHandle D3D12GraphicsPlugin::MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx)

    {
        auto handle = m_meshes.emplace_back(d3d12Device, idx, vtx, [&](ID3D12CommandList* cmdList) -> bool {
            bool success = ExecuteCommandList(cmdList);
            // Must wait in here so that we don't try to clean up the "upload"-related objects before they're finished.
            WaitForGpu();
            return success;
        });

        return handle;
    }

    void D3D12GraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                         const XrSwapchainImageBaseHeader* colorSwapchainImage, const RenderParams& params)
    {
        if (params.cubes.empty() && params.meshes.empty()) {
            return;
        }
        D3D12SwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, swapchainData->GetCommandAllocator(),
                                                             nullptr, __uuidof(ID3D12GraphicsCommandList),
                                                             reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));
        XRC_CHECK_THROW_HRCMD(cmdList->SetName(L"CTS RenderView command list"));

        const XrSwapchainCreateInfo* depthCreateInfo = swapchainData->GetDepthCreateInfo();
        DXGI_FORMAT depthSwapchainFormat = GetDepthStencilFormatOrDefault(depthCreateInfo);

        ID3D12PipelineState* pipelineState =
            GetOrCreatePipelineState((DXGI_FORMAT)swapchainData->GetCreateInfo().format, depthSwapchainFormat);
        cmdList->SetPipelineState(pipelineState);
        cmdList->SetGraphicsRootSignature(rootSignature.Get());

        ID3D12Resource* const colorTexture = swapchainData->GetTypedImage(imageIndex).texture;

        const D3D12_VIEWPORT viewport = {(float)layerView.subImage.imageRect.offset.x,
                                         (float)layerView.subImage.imageRect.offset.y,
                                         (float)layerView.subImage.imageRect.extent.width,
                                         (float)layerView.subImage.imageRect.extent.height,
                                         0,
                                         1};
        cmdList->RSSetViewports(1, &viewport);

        const D3D12_RECT scissorRect = {layerView.subImage.imageRect.offset.x, layerView.subImage.imageRect.offset.y,
                                        layerView.subImage.imageRect.offset.x + layerView.subImage.imageRect.extent.width,
                                        layerView.subImage.imageRect.offset.y + layerView.subImage.imageRect.extent.height};
        cmdList->RSSetScissorRects(1, &scissorRect);

        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView =
            CreateRenderTargetView(colorTexture, layerView.subImage.imageArrayIndex, swapchainData->GetCreateInfo().format);

        ID3D12Resource* depthStencilTexture = swapchainData->GetDepthImageForColorIndex(imageIndex).texture;

        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView =
            CreateDepthStencilView(depthStencilTexture, layerView.subImage.imageArrayIndex, depthSwapchainFormat);

        D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = {renderTargetView};
        cmdList->OMSetRenderTargets((UINT)ArraySize(renderTargets), renderTargets, true, &depthStencilView);

        const XMMATRIX spaceToView = XMMatrixInverse(nullptr, LoadXrPose(layerView.pose));
        XrMatrix4x4f projectionMatrix;
        XrMatrix4x4f_CreateProjectionFov(&projectionMatrix, GRAPHICS_D3D, layerView.fov, 0.05f, 100.0f);

        // Set shaders and constant buffers.
        ID3D12Resource* viewProjectionCBuffer = swapchainData->GetViewProjectionCBuffer();
        ViewProjectionConstantBuffer viewProjection;
        XMStoreFloat4x4(&viewProjection.ViewProjection, XMMatrixTranspose(spaceToView * LoadXrMatrix(projectionMatrix)));
        {
            void* data;
            const D3D12_RANGE readRange{0, 0};
            XRC_CHECK_THROW_HRCMD(viewProjectionCBuffer->Map(0, &readRange, &data));
            memcpy(data, &viewProjection, sizeof(viewProjection));
            viewProjectionCBuffer->Unmap(0, nullptr);
        }

        cmdList->SetGraphicsRootConstantBufferView(1, viewProjectionCBuffer->GetGPUVirtualAddress());

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        constexpr uint32_t modelCBufferSize = AlignTo<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(sizeof(ModelConstantBuffer));
        swapchainData->RequestModelCBuffer(static_cast<uint32_t>(modelCBufferSize * (params.cubes.size() + params.meshes.size())));
        ID3D12Resource* modelCBuffer = swapchainData->GetModelCBuffer();

        // Render each cube
        uint32_t offset = 0;
        MeshHandle lastMeshHandle;

        const auto drawMesh = [&, this](const MeshDrawable mesh) {
            D3D12Mesh& d3dMesh = m_meshes[mesh.handle];
            if (mesh.handle != lastMeshHandle) {
                // We are now rendering a new mesh
                // Set primitive data.
                const D3D12_VERTEX_BUFFER_VIEW vertexBufferView[] = {
                    {d3dMesh.vertexBuffer->GetGPUVirtualAddress(), d3dMesh.vertexBufferSizeBytes, sizeof(Geometry::Vertex)}};
                cmdList->IASetVertexBuffers(0, (UINT)ArraySize(vertexBufferView), vertexBufferView);

                D3D12_INDEX_BUFFER_VIEW indexBufferView{d3dMesh.indexBuffer->GetGPUVirtualAddress(), d3dMesh.indexBufferSizeBytes,
                                                        DXGI_FORMAT_R16_UINT};
                cmdList->IASetIndexBuffer(&indexBufferView);

                lastMeshHandle = mesh.handle;
            }
            // Compute and update the model transform.
            ModelConstantBuffer model;

            XMStoreFloat4x4(&model.Model, XMMatrixTranspose(XMMatrixScaling(mesh.params.scale.x, mesh.params.scale.y, mesh.params.scale.z) *
                                                            LoadXrPose(mesh.params.pose)));
            {
                uint8_t* data;
                const D3D12_RANGE readRange{0, 0};
                XRC_CHECK_THROW_HRCMD(modelCBuffer->Map(0, &readRange, reinterpret_cast<void**>(&data)));
                memcpy(data + offset, &model, sizeof(model));
                const D3D12_RANGE writeRange{offset, offset + modelCBufferSize};
                modelCBuffer->Unmap(0, &writeRange);
            }

            cmdList->SetGraphicsRootConstantBufferView(0, modelCBuffer->GetGPUVirtualAddress() + offset);

            // Draw the mesh.
            cmdList->DrawIndexedInstanced(d3dMesh.numIndices, 1, 0, 0, 0);

            offset += modelCBufferSize;
        };

        // Render each cube
        for (const Cube& cube : params.cubes) {
            drawMesh(MeshDrawable{m_cubeMesh, cube.params.pose, cube.params.scale});
        }

        // Render each mesh
        for (const auto& mesh : params.meshes) {
            drawMesh(mesh);
        }

        XRC_CHECK_THROW_HRCMD(cmdList->Close());
        XRC_CHECK_THROW(ExecuteCommandList(cmdList.Get()));

        // TODO: Track down exactly why this wait is needed.
        // On some drivers and/or hardware the test is generating the same image for the left and right eye,
        // and generating images that fail the interactive tests. This did not seem to be the case several
        // months ago, so it likely a driver change that flipped a race condition the other direction.
        WaitForGpu();
    }

    void D3D12GraphicsPlugin::Flush()
    {
        if (fence) {
            WaitForGpu();
        }
    }

    ID3D12PipelineState* D3D12GraphicsPlugin::GetOrCreatePipelineState(DXGI_FORMAT colorSwapchainFormat, DXGI_FORMAT dsvSwapchainFormat)
    {
        auto iter = pipelineStates.find({colorSwapchainFormat, dsvSwapchainFormat});
        if (iter != pipelineStates.end()) {
            return iter->second.Get();
        }

        const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
        pipelineStateDesc.pRootSignature = rootSignature.Get();
        pipelineStateDesc.VS = {vertexShaderBytes->GetBufferPointer(), vertexShaderBytes->GetBufferSize()};
        pipelineStateDesc.PS = {pixelShaderBytes->GetBufferPointer(), pixelShaderBytes->GetBufferSize()};
        {
            pipelineStateDesc.BlendState.AlphaToCoverageEnable = false;
            pipelineStateDesc.BlendState.IndependentBlendEnable = false;

            for (size_t i = 0; i < ArraySize(pipelineStateDesc.BlendState.RenderTarget); ++i) {
                pipelineStateDesc.BlendState.RenderTarget[i].BlendEnable = false;

                pipelineStateDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
                pipelineStateDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
                pipelineStateDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;

                pipelineStateDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
                pipelineStateDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
                pipelineStateDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;

                pipelineStateDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
                pipelineStateDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            }
        }
        pipelineStateDesc.SampleMask = 0xFFFFFFFF;
        {
            pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
            pipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE;
            pipelineStateDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            pipelineStateDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            pipelineStateDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            pipelineStateDesc.RasterizerState.DepthClipEnable = TRUE;
            pipelineStateDesc.RasterizerState.MultisampleEnable = FALSE;
            pipelineStateDesc.RasterizerState.AntialiasedLineEnable = FALSE;
            pipelineStateDesc.RasterizerState.ForcedSampleCount = 0;
            pipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        }
        {
            pipelineStateDesc.DepthStencilState.DepthEnable = TRUE;
            pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
            pipelineStateDesc.DepthStencilState.StencilEnable = FALSE;
            pipelineStateDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
            pipelineStateDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
            pipelineStateDesc.DepthStencilState.FrontFace = pipelineStateDesc.DepthStencilState.BackFace = {
                D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
        }
        {
            pipelineStateDesc.InputLayout.pInputElementDescs = inputElementDescs;
            pipelineStateDesc.InputLayout.NumElements = (UINT)ArraySize(inputElementDescs);
        }
        pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
        pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateDesc.NumRenderTargets = 1;
        pipelineStateDesc.RTVFormats[0] = colorSwapchainFormat;
        pipelineStateDesc.DSVFormat = dsvSwapchainFormat;
        pipelineStateDesc.SampleDesc = {1, 0};
        pipelineStateDesc.NodeMask = 0;
        pipelineStateDesc.CachedPSO = {nullptr, 0};
        pipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        ComPtr<ID3D12PipelineState> pipelineState;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateGraphicsPipelineState(&pipelineStateDesc, __uuidof(ID3D12PipelineState),
                                                                       reinterpret_cast<void**>(pipelineState.ReleaseAndGetAddressOf())));
        XRC_CHECK_THROW_HRCMD(pipelineState->SetName(L"CTS pipeline state"));
        ID3D12PipelineState* pipelineStateRaw = pipelineState.Get();

        pipelineStates.emplace(std::make_pair(colorSwapchainFormat, dsvSwapchainFormat), std::move(pipelineState));

        return pipelineStateRaw;
    }

    void D3D12GraphicsPlugin::CpuWaitForFence(uint64_t fenceVal) const
    {
        if (fence->GetCompletedValue() < fenceVal) {
            XRC_CHECK_THROW_HRCMD(fence->SetEventOnCompletion(fenceVal, fenceEvent));
            const uint32_t retVal = WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
            if (retVal != WAIT_OBJECT_0) {
                XRC_CHECK_THROW_HRCMD(E_FAIL);
            }
        }
    }

    void D3D12GraphicsPlugin::WaitForGpu() const
    {
        CpuWaitForFence(fenceValue);
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D12(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<D3D12GraphicsPlugin>(platformPlugin);
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_D3D12
