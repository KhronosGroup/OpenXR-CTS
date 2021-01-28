// Copyright (c) 2019-2021, The Khronos Group Inc.
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

#include "graphics_plugin.h"

#if defined(XR_USE_GRAPHICS_API_D3D12) && !defined(MISSING_DIRECTX_COLORS)

#include "swapchain_parameters.h"
#include "conformance_framework.h"
#include "Geometry.h"
#include <windows.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr
#include <common/xr_linear.h>
#include <DirectXColors.h>
#include <D3Dcompiler.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <catch2/catch.hpp>

#include "d3d_common.h"

using namespace Microsoft::WRL;
using namespace DirectX;

namespace Conformance
{
    // The equivalent of C++17 std::size. A helper to get the dimension for an array.
    template <typename T, size_t Size>
    constexpr size_t ArraySize(const T (&/*unused*/)[Size]) noexcept
    {
        return Size;
    }

    template <uint32_t alignment>
    constexpr uint32_t AlignTo(uint32_t n)
    {
        static_assert((alignment & (alignment - 1)) == 0, "The alignment must be power-of-two");
        return (n + alignment - 1) & ~(alignment - 1);
    }

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

    struct D3D12GraphicsPlugin : public IGraphicsPlugin
    {
        D3D12GraphicsPlugin(std::shared_ptr<IPlatformPlugin>);

        bool Initialize() override;

        bool IsInitialized() const override;

        void Shutdown() override;

        std::string DescribeGraphics() const override;

        std::vector<std::string> GetInstanceExtensions() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, int64_t imageFormat, uint32_t arraySlice,
                           const RGBAImage& image) override;

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
        int64_t GetRGBA8Format(bool sRGB) const override;

        std::shared_ptr<SwapchainImageStructs> AllocateSwapchainImageStructs(size_t size,
                                                                             const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                             int64_t colorSwapchainFormat) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        int64_t colorSwapchainFormat, const std::vector<Cube>& cubes) override;

    protected:
        D3D12_CPU_DESCRIPTOR_HANDLE CreateRenderTargetView(ID3D12Resource* colorTexture, uint32_t imageArrayIndex,
                                                           int64_t colorSwapchainFormat);
        D3D12_CPU_DESCRIPTOR_HANDLE CreateDepthStencilView(ID3D12Resource* depthStencilTexture, uint32_t imageArrayIndex);

        struct D3D12SwapchainImageStructs : public IGraphicsPlugin::SwapchainImageStructs
        {
            std::vector<XrSwapchainImageBaseHeader*> Create(ID3D12Device* device, uint32_t capacity)
            {
                d3d12Device = device;

                imageVector.resize(capacity, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
                std::vector<XrSwapchainImageBaseHeader*> bases(capacity);
                for (uint32_t i = 0; i < capacity; ++i) {
                    bases[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&imageVector[i]);
                }

                XRC_CHECK_THROW_HRCMD(
                    d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                        reinterpret_cast<void**>(commandAllocator.ReleaseAndGetAddressOf())));

                viewProjectionCBuffer = CreateBuffer(d3d12Device, sizeof(ViewProjectionConstantBuffer), D3D12_HEAP_TYPE_UPLOAD);

                return bases;
            }

            uint32_t ImageIndex(const XrSwapchainImageBaseHeader* swapchainImageHeader)
            {
                auto p = reinterpret_cast<const XrSwapchainImageD3D12KHR*>(swapchainImageHeader);
                return (uint32_t)(p - &imageVector[0]);
            }

            ID3D12Resource* GetDepthStencilTexture(ID3D12Resource* colorTexture)
            {
                if (!depthStencilTexture) {
                    // This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.

                    const D3D12_RESOURCE_DESC colorDesc = colorTexture->GetDesc();

                    D3D12_HEAP_PROPERTIES heapProp{};
                    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
                    heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
                    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

                    D3D12_RESOURCE_DESC depthDesc{};
                    depthDesc.Dimension = colorDesc.Dimension;
                    depthDesc.Alignment = colorDesc.Alignment;
                    depthDesc.Width = colorDesc.Width;
                    depthDesc.Height = colorDesc.Height;
                    depthDesc.DepthOrArraySize = colorDesc.DepthOrArraySize;
                    depthDesc.MipLevels = 1;
                    depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                    depthDesc.SampleDesc.Count = 1;
                    depthDesc.Layout = colorDesc.Layout;
                    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

                    D3D12_CLEAR_VALUE clearValue{};
                    clearValue.DepthStencil.Depth = 1.0f;
                    clearValue.Format = DXGI_FORMAT_D32_FLOAT;

                    XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommittedResource(
                        &heapProp, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                        __uuidof(ID3D12Resource), reinterpret_cast<void**>(depthStencilTexture.ReleaseAndGetAddressOf())));
                }

                return depthStencilTexture.Get();
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
                    modelCBuffer = CreateBuffer(d3d12Device, requiredSize, D3D12_HEAP_TYPE_UPLOAD);
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

            std::vector<XrSwapchainImageD3D12KHR> imageVector;

            ID3D12Device* d3d12Device{nullptr};
            ComPtr<ID3D12CommandAllocator> commandAllocator;
            ComPtr<ID3D12Resource> depthStencilTexture;
            ComPtr<ID3D12Resource> modelCBuffer;
            ComPtr<ID3D12Resource> viewProjectionCBuffer;
            uint64_t fenceValue = 0;
        };

        ID3D12PipelineState* GetOrCreatePipelineState(DXGI_FORMAT swapchainFormat);
        bool ExecuteCommandList(ID3D12CommandList* cmdList) const;
        void CpuWaitForFence(uint64_t fenceVal) const;
        void WaitForGpu() const;

        D3D12SwapchainImageStructs& GetSwapchainImageContext(const XrSwapchainImageBaseHeader* swapchainImage);

    protected:
        bool initialized = false;
        XrGraphicsBindingD3D12KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
        ComPtr<ID3D12Device> d3d12Device;
        ComPtr<ID3D12CommandQueue> d3d12CmdQueue;
        ComPtr<ID3D12Fence> fence;
        mutable uint64_t fenceValue = 0;
        HANDLE fenceEvent = INVALID_HANDLE_VALUE;

        // Not used yet: std::list<std::shared_ptr<D3D12SwapchainImageStructs>> swapchainImageContexts;
        std::map<const XrSwapchainImageBaseHeader*, D3D12SwapchainImageStructs*> swapchainImageContextMap;
        const XrSwapchainImageBaseHeader* lastSwapchainImage = nullptr;

        // Resources needed for rendering cubes
        const ComPtr<ID3DBlob> vertexShaderBytes;
        const ComPtr<ID3DBlob> pixelShaderBytes;
        ComPtr<ID3D12RootSignature> rootSignature;
        std::map<DXGI_FORMAT, ComPtr<ID3D12PipelineState>> pipelineStates;
        ComPtr<ID3D12Resource> cubeVertexBuffer;
        ComPtr<ID3D12Resource> cubeIndexBuffer;
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        ComPtr<ID3D12DescriptorHeap> dsvHeap;
    };

    D3D12GraphicsPlugin::D3D12GraphicsPlugin(std::shared_ptr<IPlatformPlugin>)
        : vertexShaderBytes(CompileShader(ShaderHlsl, "MainVS", "vs_5_1")), pixelShaderBytes(CompileShader(ShaderHlsl, "MainPS", "ps_5_1"))
    {
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
                CHECK(ValidateResultAllowed("xrGetD3D12GraphicsRequirementsKHR", result));
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

            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue),
                                                                  reinterpret_cast<void**>(d3d12CmdQueue.ReleaseAndGetAddressOf())));

            {
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
                heapDesc.NumDescriptors = 1;
                heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                XRC_CHECK_THROW_HRCMD(d3d12Device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap),
                                                                        reinterpret_cast<void**>(rtvHeap.ReleaseAndGetAddressOf())));
            }
            {
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
                heapDesc.NumDescriptors = 1;
                heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                XRC_CHECK_THROW_HRCMD(d3d12Device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap),
                                                                        reinterpret_cast<void**>(dsvHeap.ReleaseAndGetAddressOf())));
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

            D3D12SwapchainImageStructs initializeContext;
            std::vector<XrSwapchainImageBaseHeader*> _ = initializeContext.Create(d3d12Device.Get(), 1);

            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                                           reinterpret_cast<void**>(fence.ReleaseAndGetAddressOf())));
            fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            CHECK(fenceEvent != nullptr);

            ComPtr<ID3D12GraphicsCommandList> cmdList;
            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, initializeContext.GetCommandAllocator(),
                                                                 nullptr, __uuidof(ID3D12GraphicsCommandList),
                                                                 reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));

            ComPtr<ID3D12Resource> cubeVertexBufferUpload;
            cubeVertexBuffer =
                CreateBuffer(d3d12Device.Get(), (uint32_t)(Geometry::c_cubeVertices.size() * sizeof(Geometry::c_cubeVertices[0])),
                             D3D12_HEAP_TYPE_DEFAULT);
            {
                cubeVertexBufferUpload =
                    CreateBuffer(d3d12Device.Get(), (uint32_t)(Geometry::c_cubeVertices.size() * sizeof(Geometry::c_cubeVertices[0])),
                                 D3D12_HEAP_TYPE_UPLOAD);

                void* data;
                const D3D12_RANGE readRange{0, 0};
                XRC_CHECK_THROW_HRCMD(cubeVertexBufferUpload->Map(0, &readRange, &data));
                memcpy(data, Geometry::c_cubeVertices.data(),
                       (uint32_t)(Geometry::c_cubeVertices.size() * sizeof(Geometry::c_cubeVertices[0])));
                cubeVertexBufferUpload->Unmap(0, nullptr);

                cmdList->CopyBufferRegion(cubeVertexBuffer.Get(), 0, cubeVertexBufferUpload.Get(), 0,
                                          Geometry::c_cubeVertices.size() * sizeof(Geometry::c_cubeVertices[0]));
            }

            ComPtr<ID3D12Resource> cubeIndexBufferUpload;
            cubeIndexBuffer =
                CreateBuffer(d3d12Device.Get(), (uint32_t)(Geometry::c_cubeIndices.size() * sizeof(Geometry::c_cubeIndices[0])),
                             D3D12_HEAP_TYPE_DEFAULT);
            {
                cubeIndexBufferUpload =
                    CreateBuffer(d3d12Device.Get(), (uint32_t)(Geometry::c_cubeIndices.size() * sizeof(Geometry::c_cubeIndices[0])),
                                 D3D12_HEAP_TYPE_UPLOAD);

                void* data;
                const D3D12_RANGE readRange{0, 0};
                XRC_CHECK_THROW_HRCMD(cubeIndexBufferUpload->Map(0, &readRange, &data));
                memcpy(data, Geometry::c_cubeIndices.data(), Geometry::c_cubeIndices.size() * sizeof(Geometry::c_cubeIndices[0]));
                cubeIndexBufferUpload->Unmap(0, nullptr);

                cmdList->CopyBufferRegion(cubeIndexBuffer.Get(), 0, cubeIndexBufferUpload.Get(), 0,
                                          Geometry::c_cubeIndices.size() * sizeof(Geometry::c_cubeIndices[0]));
            }

            XRC_CHECK_THROW_HRCMD(cmdList->Close());
            CHECK(ExecuteCommandList(cmdList.Get()));

            WaitForGpu();

            graphicsBinding.device = d3d12Device.Get();
            graphicsBinding.queue = d3d12CmdQueue.Get();

            return true;
        }
        catch (...) {
            // Log it?
        }

        return false;
    }

    void D3D12GraphicsPlugin::ShutdownDevice()
    {
        graphicsBinding = XrGraphicsBindingD3D12KHR{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
        d3d12CmdQueue.Reset();
        d3d12Device.Reset();
        swapchainImageContextMap.clear();
        lastSwapchainImage = nullptr;
    }

    void D3D12GraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, int64_t /*imageFormat*/, uint32_t arraySlice,
                                            const RGBAImage& image)
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

        auto& swapchainContext = GetSwapchainImageContext(swapchainImage);

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, swapchainContext.GetCommandAllocator(),
                                                             nullptr, __uuidof(ID3D12GraphicsCommandList),
                                                             reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));

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
        CHECK(ExecuteCommandList(cmdList.Get()));

        WaitForGpu();
    }

    std::string D3D12GraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        const SwapchainTestMap& dxgiSwapchainTestMap = GetDxgiSwapchainTestMap();
        SwapchainTestMap::const_iterator it = dxgiSwapchainTestMap.find(imageFormat);

        if (it != dxgiSwapchainTestMap.end()) {
            return it->second.imageFormatName;
        }

        return std::string("unknown");
    }

    bool D3D12GraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        SwapchainTestMap& dxgiSwapchainTestMap = GetDxgiSwapchainTestMap();
        SwapchainTestMap::const_iterator it = dxgiSwapchainTestMap.find(imageFormat);

        return (it != dxgiSwapchainTestMap.end());
    }

    bool D3D12GraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                               int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        // Swapchain image format support by the runtime is specified by the xrEnumerateSwapchainFormats function.
        // Runtimes should support R8G8B8A8 and R8G8B8A8 sRGB formats if possible.
        //
        // DXGI resources will be created with their associated TYPELESS format, but the runtime will use the
        // application-specified format for reading the data.
        //
        // With a Direct3D-based graphics API, xrEnumerateSwapchainFormats never returns typeless formats
        // (e.g. DXGI_FORMAT_R8G8B8A8_TYPELESS). Only concrete formats are returned, and only concrete
        // formats may be specified by applications for swapchain creation.

        SwapchainTestMap& dxgiSwapchainTestMap = GetDxgiSwapchainTestMap();
        SwapchainTestMap::iterator it = dxgiSwapchainTestMap.find(imageFormat);

        // Verify that the image format is known. If it's not known then this test needs to be
        // updated to recognize new DXGI formats.
        CAPTURE(imageFormat);
        CHECK_MSG(it != dxgiSwapchainTestMap.end(), "Unknown DXGI image format.");
        if (it == dxgiSwapchainTestMap.end()) {
            return false;
        }

        // Verify that imageFormat is not a typeless type. Only regular types are allowed to
        // be returned by the runtime for enumerated image formats.
        CAPTURE(it->second.imageFormatName);
        CHECK_MSG(!it->second.mutableFormat, "Typeless DXGI image formats must not be enumerated by runtimes.");
        if (it->second.mutableFormat) {
            return false;
        }

        // We may now proceed with creating swapchains with the format.
        SwapchainCreateTestParameters& tp = it->second;
        tp.arrayCountVector = {1, 2};
        if (tp.colorFormat && !tp.compressedFormat) {
            tp.mipCountVector = {1, 2};
        }
        else {
            tp.mipCountVector = {1};
        }

        *swapchainTestParameters = tp;
        return true;
    }

    bool D3D12GraphicsPlugin::ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* tp,
                                                      XrSwapchain swapchain, uint32_t* imageCount) const noexcept(false)
    {
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
        std::vector<XrSwapchainImageD3D12KHR> swapchainImageVector;
        uint32_t countOutput;

        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr));
        swapchainImageVector.resize(countOutput, XrSwapchainImageD3D12KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, countOutput, &countOutput,
                                                         reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data())));

        ComPtr<ID3D12CommandAllocator> commandAllocator;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                                  reinterpret_cast<void**>(commandAllocator.ReleaseAndGetAddressOf())));

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
                                                             __uuidof(ID3D12GraphicsCommandList),
                                                             reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));

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

    int64_t D3D12GraphicsPlugin::GetRGBA8Format(bool sRGB) const
    {
        return sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
    D3D12GraphicsPlugin::AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& /*swapchainCreateInfo*/)
    {
        // What we are doing below is allocating a subclass of the SwapchainImageStructs struct and
        // using shared_ptr to manage it in a way that the caller doesn't need to know about the
        // graphics implementation behind it.

        auto derivedResult = std::make_shared<D3D12SwapchainImageStructs>();

        std::vector<XrSwapchainImageBaseHeader*> bases = derivedResult->Create(d3d12Device.Get(), (uint32_t)(size));

        // Map every swapchainImage base pointer to this context
        for (auto& base : bases) {
            derivedResult->imagePtrVector.push_back(base);
            swapchainImageContextMap[base] = derivedResult.get();
        }

        // Cast our derived type to the caller-expected type.
        std::shared_ptr<SwapchainImageStructs> result =
            std::static_pointer_cast<SwapchainImageStructs, D3D12SwapchainImageStructs>(derivedResult);

        return result;
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

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsPlugin::CreateDepthStencilView(ID3D12Resource* depthStencilTexture, uint32_t imageArrayIndex)
    {
        const D3D12_RESOURCE_DESC depthStencilTextureDesc = depthStencilTexture->GetDesc();

        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
        depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
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
                                              int64_t colorSwapchainFormat)
    {
        auto& swapchainContext = GetSwapchainImageContext(colorSwapchainImage);

        ID3D12Resource* const colorTexture = reinterpret_cast<const XrSwapchainImageD3D12KHR*>(colorSwapchainImage)->texture;
        const D3D12_RESOURCE_DESC colorTextureDesc = colorTexture->GetDesc();

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, swapchainContext.GetCommandAllocator(),
                                                             nullptr, __uuidof(ID3D12GraphicsCommandList),
                                                             reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));

        // Clear color buffer.
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = CreateRenderTargetView(colorTexture, imageArrayIndex, colorSwapchainFormat);
        // TODO: Do not clear to a color when using a pass-through view configuration.
        cmdList->ClearRenderTargetView(renderTargetView, DirectX::Colors::DarkSlateGray, 0, nullptr);

        // Clear depth buffer.
        ID3D12Resource* depthStencilTexture = swapchainContext.GetDepthStencilTexture(colorTexture);
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = CreateDepthStencilView(depthStencilTexture, imageArrayIndex);
        cmdList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        XRC_CHECK_THROW_HRCMD(cmdList->Close());
        CHECK(ExecuteCommandList(cmdList.Get()));
    }

    void D3D12GraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                         const XrSwapchainImageBaseHeader* colorSwapchainImage, int64_t colorSwapchainFormat,
                                         const std::vector<Cube>& cubes)
    {
        auto& swapchainContext = GetSwapchainImageContext(colorSwapchainImage);

        if (!cubes.empty()) {
            ComPtr<ID3D12GraphicsCommandList> cmdList;
            XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, swapchainContext.GetCommandAllocator(),
                                                                 nullptr, __uuidof(ID3D12GraphicsCommandList),
                                                                 reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));

            ID3D12PipelineState* pipelineState = GetOrCreatePipelineState((DXGI_FORMAT)colorSwapchainFormat);
            cmdList->SetPipelineState(pipelineState);
            cmdList->SetGraphicsRootSignature(rootSignature.Get());

            ID3D12Resource* const colorTexture = reinterpret_cast<const XrSwapchainImageD3D12KHR*>(colorSwapchainImage)->texture;
            const D3D12_RESOURCE_DESC colorTextureDesc = colorTexture->GetDesc();

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
                CreateRenderTargetView(colorTexture, layerView.subImage.imageArrayIndex, colorSwapchainFormat);

            ID3D12Resource* depthStencilTexture = swapchainContext.GetDepthStencilTexture(colorTexture);
            D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = CreateDepthStencilView(depthStencilTexture, layerView.subImage.imageArrayIndex);

            D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = {renderTargetView};
            cmdList->OMSetRenderTargets((UINT)ArraySize(renderTargets), renderTargets, true, &depthStencilView);

            const XMMATRIX spaceToView = XMMatrixInverse(nullptr, LoadXrPose(layerView.pose));
            XrMatrix4x4f projectionMatrix;
            XrMatrix4x4f_CreateProjectionFov(&projectionMatrix, GRAPHICS_D3D, layerView.fov, 0.05f, 100.0f);

            // Set shaders and constant buffers.
            ID3D12Resource* viewProjectionCBuffer = swapchainContext.GetViewProjectionCBuffer();
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

            // Set cube primitive data.
            const D3D12_VERTEX_BUFFER_VIEW vertexBufferView[] = {
                {cubeVertexBuffer->GetGPUVirtualAddress(),
                 (uint32_t)(Geometry::c_cubeVertices.size() * sizeof(Geometry::c_cubeVertices[0])), sizeof(Geometry::Vertex)}};
            cmdList->IASetVertexBuffers(0, (UINT)ArraySize(vertexBufferView), vertexBufferView);

            D3D12_INDEX_BUFFER_VIEW indexBufferView{cubeIndexBuffer->GetGPUVirtualAddress(),
                                                    (uint32_t)(Geometry::c_cubeIndices.size() * sizeof(Geometry::c_cubeIndices[0])),
                                                    DXGI_FORMAT_R16_UINT};
            cmdList->IASetIndexBuffer(&indexBufferView);

            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            constexpr uint32_t cubeCBufferSize = AlignTo<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(sizeof(ModelConstantBuffer));
            swapchainContext.RequestModelCBuffer(static_cast<uint32_t>(cubeCBufferSize * cubes.size()));
            ID3D12Resource* modelCBuffer = swapchainContext.GetModelCBuffer();

            // Render each cube
            uint32_t offset = 0;
            for (const Cube& cube : cubes) {
                // Compute and update the model transform.
                ModelConstantBuffer model;
                XMStoreFloat4x4(&model.Model,
                                XMMatrixTranspose(XMMatrixScaling(cube.Scale.x, cube.Scale.y, cube.Scale.z) * LoadXrPose(cube.Pose)));
                {
                    uint8_t* data;
                    const D3D12_RANGE readRange{0, 0};
                    XRC_CHECK_THROW_HRCMD(modelCBuffer->Map(0, &readRange, reinterpret_cast<void**>(&data)));
                    memcpy(data + offset, &model, sizeof(model));
                    const D3D12_RANGE writeRange{offset, offset + cubeCBufferSize};
                    modelCBuffer->Unmap(0, &writeRange);
                }

                cmdList->SetGraphicsRootConstantBufferView(0, modelCBuffer->GetGPUVirtualAddress() + offset);

                // Draw the cube.
                cmdList->DrawIndexedInstanced((uint32_t)Geometry::c_cubeIndices.size(), 1, 0, 0, 0);

                offset += cubeCBufferSize;
            }

            XRC_CHECK_THROW_HRCMD(cmdList->Close());
            CHECK(ExecuteCommandList(cmdList.Get()));

            // TODO: Track down exactly why this wait is needed.
            // On some drivers and/or hardware the test is generating the same image for the left and right eye,
            // and generating images that fail the interactive tests. This did not seem to be the case several
            // months ago, so it likely a driver change that flipped a race condition the other direction.
            WaitForGpu();
        }
    }

    ID3D12PipelineState* D3D12GraphicsPlugin::GetOrCreatePipelineState(DXGI_FORMAT swapchainFormat)
    {
        auto iter = pipelineStates.find(swapchainFormat);
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
        pipelineStateDesc.RTVFormats[0] = swapchainFormat;
        pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipelineStateDesc.SampleDesc = {1, 0};
        pipelineStateDesc.NodeMask = 0;
        pipelineStateDesc.CachedPSO = {nullptr, 0};
        pipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        ComPtr<ID3D12PipelineState> pipelineState;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateGraphicsPipelineState(&pipelineStateDesc, __uuidof(ID3D12PipelineState),
                                                                       reinterpret_cast<void**>(pipelineState.ReleaseAndGetAddressOf())));
        ID3D12PipelineState* pipelineStateRaw = pipelineState.Get();

        pipelineStates.emplace(swapchainFormat, std::move(pipelineState));

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

    D3D12GraphicsPlugin::D3D12SwapchainImageStructs&
    D3D12GraphicsPlugin::GetSwapchainImageContext(const XrSwapchainImageBaseHeader* swapchainImage)
    {
        auto& retContext = *swapchainImageContextMap[swapchainImage];
        if (lastSwapchainImage != swapchainImage) {
            if (lastSwapchainImage != nullptr) {
                swapchainImageContextMap[lastSwapchainImage]->SetFrameFenceValue(fenceValue);
            }
            lastSwapchainImage = swapchainImage;

            CpuWaitForFence(retContext.GetFrameFenceValue());
            retContext.ResetCommandAllocator();
        }
        return retContext;
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D12(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<D3D12GraphicsPlugin>(platformPlugin);
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_D3D12
