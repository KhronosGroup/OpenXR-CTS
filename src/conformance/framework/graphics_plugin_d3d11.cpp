// Copyright (c) 2019-2024, The Khronos Group Inc.
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

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "conformance_framework.h"
#include "graphics_plugin.h"
#include "graphics_plugin_d3d11_gltf.h"
#include "graphics_plugin_impl_helpers.h"
#include "swapchain_image_data.h"

#include "common/xr_linear.h"
#include "common/xr_dependencies.h"
#include "pbr/D3D11/D3D11Resources.h"
#include "pbr/D3D11/D3D11Texture.h"
#include "utilities/Geometry.h"
#include "utilities/d3d_common.h"
#include "utilities/swapchain_parameters.h"
#include "utilities/throw_helpers.h"

#include <D3Dcompiler.h>
#include <DirectXColors.h>
#include <catch2/catch_test_macros.hpp>
#include <d3d11.h>
#include <openxr/openxr_platform.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <algorithm>
#include <array>
#include <windows.h>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace Conformance
{

    struct D3D11Mesh
    {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11Buffer> vertexBuffer;
        ComPtr<ID3D11Buffer> indexBuffer;
        UINT numIndices;

        D3D11Mesh(ComPtr<ID3D11Device> d3d11Device, span<const uint16_t> indices, span<const Geometry::Vertex> vertices)
            : device(d3d11Device), numIndices((UINT)indices.size())
        {

            const D3D11_SUBRESOURCE_DATA vertexBufferData{vertices.data()};
            const CD3D11_BUFFER_DESC vertexBufferDesc((UINT)(vertices.size() * sizeof(Geometry::Vertex)), D3D11_BIND_VERTEX_BUFFER);
            XRC_CHECK_THROW_HRCMD(d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, vertexBuffer.ReleaseAndGetAddressOf()));

            const D3D11_SUBRESOURCE_DATA indexBufferData{indices.data()};
            const CD3D11_BUFFER_DESC indexBufferDesc((UINT)(indices.size() * sizeof(decltype(indices)::element_type)),
                                                     D3D11_BIND_INDEX_BUFFER);
            XRC_CHECK_THROW_HRCMD(d3d11Device->CreateBuffer(&indexBufferDesc, &indexBufferData, indexBuffer.ReleaseAndGetAddressOf()));
        }
    };

    struct D3D11FallbackDepthTexture
    {
    public:
        D3D11FallbackDepthTexture() = default;

        void Reset()
        {
            m_texture = nullptr;
            m_xrImage.texture = nullptr;
        }
        bool Allocated() const
        {
            return m_texture != nullptr;
        }

        void Allocate(ID3D11Device* d3d11Device, UINT width, UINT height, UINT arraySize)
        {
            Reset();
            D3D11_TEXTURE2D_DESC depthDesc{};
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.ArraySize = arraySize;
            depthDesc.MipLevels = 1;
            depthDesc.Format = kDefaultDepthFormatTypeless;
            depthDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
            depthDesc.SampleDesc.Count = 1;
            XRC_CHECK_THROW_HRCMD(d3d11Device->CreateTexture2D(&depthDesc, nullptr, m_texture.ReleaseAndGetAddressOf()));
            m_xrImage.texture = m_texture.Get();
        }

        const XrSwapchainImageD3D11KHR& GetTexture() const
        {
            return m_xrImage;
        }

    private:
        ComPtr<ID3D11Texture2D> m_texture{};
        XrSwapchainImageD3D11KHR m_xrImage{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, NULL, nullptr};
    };

    class D3D11SwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageD3D11KHR>
    {
    public:
        D3D11SwapchainImageData(ComPtr<ID3D11Device> device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo,
                                XrSwapchain depthSwapchain, const XrSwapchainCreateInfo& depthCreateInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, capacity, createInfo, depthSwapchain, depthCreateInfo)
            , m_device(std::move(device))

        {
        }
        D3D11SwapchainImageData(ComPtr<ID3D11Device> device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, capacity, createInfo)
            , m_device(std::move(device))
            , m_internalDepthTextures(capacity)
        {
        }

        void Reset() override
        {
            m_internalDepthTextures.clear();
            m_device = nullptr;
            SwapchainImageDataBase::Reset();
        }

        const XrSwapchainImageD3D11KHR& GetFallbackDepthSwapchainImage(uint32_t i) override
        {
            if (!m_internalDepthTextures[i].Allocated()) {
                m_internalDepthTextures[i].Allocate(m_device.Get(), this->Width(), this->Height(), this->ArraySize());
            }

            return m_internalDepthTextures[i].GetTexture();
        }

    private:
        ComPtr<ID3D11Device> m_device;
        std::vector<D3D11FallbackDepthTexture> m_internalDepthTextures;
    };

    struct D3D11GraphicsPlugin : public IGraphicsPlugin
    {
    public:
        D3D11GraphicsPlugin(std::shared_ptr<IPlatformPlugin>);

        ~D3D11GraphicsPlugin() override;

        bool Initialize() override;

        bool IsInitialized() const override;

        void Shutdown() override;

        std::string DescribeGraphics() const override;

        std::vector<std::string> GetInstanceExtensions() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

        void Flush() override;

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

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex, XrColor4f color) override;

        MeshHandle MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx) override;

        GLTFModelHandle LoadGLTF(std::shared_ptr<tinygltf::Model> tinygltfModel) override;
        std::shared_ptr<Pbr::Model> GetPbrModel(GLTFModelHandle handle) const override;
        GLTFModelInstanceHandle CreateGLTFModelInstance(GLTFModelHandle handle) override;
        Pbr::ModelInstance& GetModelInstance(GLTFModelInstanceHandle handle) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        const RenderParams& params) override;

    private:
        ComPtr<ID3D11RenderTargetView> CreateRenderTargetView(D3D11SwapchainImageData& swapchainData, uint32_t imageIndex,
                                                              uint32_t imageArrayIndex) const;
        ComPtr<ID3D11DepthStencilView> CreateDepthStencilView(D3D11SwapchainImageData& swapchainData, uint32_t imageIndex,
                                                              uint32_t imageArrayIndex) const;

        bool initialized{false};
        XrGraphicsBindingD3D11KHR graphicsBinding;
        ComPtr<ID3D11Device> d3d11Device;
        ComPtr<ID3D11DeviceContext> d3d11DeviceContext;

        // Resources needed for rendering cubes, meshes and glTFs
        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> pixelShader;
        ComPtr<ID3D11InputLayout> inputLayout;
        ComPtr<ID3D11Buffer> modelCBuffer;
        ComPtr<ID3D11Buffer> viewProjectionCBuffer;

        MeshHandle m_cubeMesh;
        VectorWithGenerationCountedHandles<D3D11Mesh, MeshHandle> m_meshes;
        // This is fine to be a shared_ptr because Model doesn't directly hold any graphics state.
        VectorWithGenerationCountedHandles<std::shared_ptr<Pbr::Model>, GLTFModelHandle> m_gltfModels;
        VectorWithGenerationCountedHandles<D3D11GLTF, GLTFModelInstanceHandle> m_gltfInstances;

        std::unique_ptr<Pbr::D3D11Resources> m_pbrResources;

        SwapchainImageDataMap<D3D11SwapchainImageData> m_swapchainImageDataMap;
    };

    D3D11GraphicsPlugin::D3D11GraphicsPlugin(std::shared_ptr<IPlatformPlugin>)
        : initialized(false), graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR}, d3d11Device(), d3d11DeviceContext()
    {
    }

    D3D11GraphicsPlugin::~D3D11GraphicsPlugin()
    {
        D3D11GraphicsPlugin::ShutdownDevice();
        D3D11GraphicsPlugin::Shutdown();
    }

    bool D3D11GraphicsPlugin::Initialize()
    {
        if (initialized)
            return false;

        // To do.
        initialized = true;
        return initialized;
    }

    bool D3D11GraphicsPlugin::IsInitialized() const
    {
        return initialized;
    }

    void D3D11GraphicsPlugin::Shutdown()
    {
        if (initialized) {
            // To do.
            initialized = false;
        }
    }

    std::string D3D11GraphicsPlugin::DescribeGraphics() const
    {
        return std::string("D3D11");
    }

    std::vector<std::string> D3D11GraphicsPlugin::GetInstanceExtensions() const
    {
        return {XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
    }

    bool D3D11GraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                                               uint32_t deviceCreationFlags)
    {
        try {
            XrGraphicsRequirementsD3D11KHR graphicsRequirements{
                XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR, nullptr, {0, 0}, D3D_FEATURE_LEVEL_11_0};

            // Create the D3D11 device for the adapter associated with the system.
            if (checkGraphicsRequirements) {

                auto xrGetD3D11GraphicsRequirementsKHR =
                    GetInstanceExtensionFunction<PFN_xrGetD3D11GraphicsRequirementsKHR>(instance, "xrGetD3D11GraphicsRequirementsKHR");

                XrResult result = xrGetD3D11GraphicsRequirementsKHR(instance, systemId, &graphicsRequirements);
                XRC_CHECK_THROW(ValidateResultAllowed("xrGetD3D11GraphicsRequirementsKHR", result));
                if (XR_FAILED(result)) {
                    // Log result?
                    return false;
                }
            }

            const ComPtr<IDXGIAdapter1> adapter = GetDXGIAdapter(graphicsRequirements.adapterLuid);

            // Create a list of feature levels which are both supported by the OpenXR runtime and this application.
            std::vector<D3D_FEATURE_LEVEL> featureLevels = {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
                                                            D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
            featureLevels.erase(std::remove_if(featureLevels.begin(), featureLevels.end(),
                                               [&](D3D_FEATURE_LEVEL fl) { return (fl < graphicsRequirements.minFeatureLevel); }),
                                featureLevels.end());

            if (featureLevels.empty()) {
                // Log result?
                return false;
            }

            // Create the device
            UINT creationFlags = deviceCreationFlags | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if !defined(NDEBUG)
            creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

            // Create the Direct3D 11 API device object and a corresponding context.
            const D3D_DRIVER_TYPE driverType = ((adapter == nullptr) ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN);

        TryAgain:
            HRESULT hr = D3D11CreateDevice(adapter.Get(), driverType, 0, creationFlags, featureLevels.data(), (UINT)featureLevels.size(),
                                           D3D11_SDK_VERSION, d3d11Device.ReleaseAndGetAddressOf(), nullptr,
                                           d3d11DeviceContext.ReleaseAndGetAddressOf());
            if (FAILED(hr)) {
                if (creationFlags & D3D11_CREATE_DEVICE_DEBUG)  // This can fail if debug functionality is not installed.
                {
                    creationFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
                    goto TryAgain;
                }

                // If the initialization fails, fall back to the WARP device.
                // For more information on WARP, see: http://go.microsoft.com/fwlink/?LinkId=286690
                hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, 0, creationFlags, featureLevels.data(), (UINT)featureLevels.size(),
                                       D3D11_SDK_VERSION, d3d11Device.ReleaseAndGetAddressOf(), nullptr,
                                       d3d11DeviceContext.ReleaseAndGetAddressOf());

                if (FAILED(hr)) {
                    // Log it?
                    return false;
                }
            }

            graphicsBinding.device = d3d11Device.Get();

            // Initialize resources needed to render cubes
            {
                const ComPtr<ID3DBlob> vertexShaderBytes = CompileShader(ShaderHlsl, "MainVS", "vs_5_0");
                XRC_CHECK_THROW_HRCMD(d3d11Device->CreateVertexShader(vertexShaderBytes->GetBufferPointer(),
                                                                      vertexShaderBytes->GetBufferSize(), nullptr,
                                                                      vertexShader.ReleaseAndGetAddressOf()));

                const ComPtr<ID3DBlob> pixelShaderBytes = CompileShader(ShaderHlsl, "MainPS", "ps_5_0");
                XRC_CHECK_THROW_HRCMD(d3d11Device->CreatePixelShader(pixelShaderBytes->GetBufferPointer(),
                                                                     pixelShaderBytes->GetBufferSize(), nullptr,
                                                                     pixelShader.ReleaseAndGetAddressOf()));

                const std::array<D3D11_INPUT_ELEMENT_DESC, 2> vertexDesc{{
                    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
                }};

                XRC_CHECK_THROW_HRCMD(d3d11Device->CreateInputLayout(vertexDesc.data(), (UINT)vertexDesc.size(),
                                                                     vertexShaderBytes->GetBufferPointer(),
                                                                     vertexShaderBytes->GetBufferSize(), &inputLayout));

                const CD3D11_BUFFER_DESC modelConstantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
                XRC_CHECK_THROW_HRCMD(d3d11Device->CreateBuffer(&modelConstantBufferDesc, nullptr, modelCBuffer.ReleaseAndGetAddressOf()));

                const CD3D11_BUFFER_DESC viewProjectionConstantBufferDesc(sizeof(ViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
                XRC_CHECK_THROW_HRCMD(
                    d3d11Device->CreateBuffer(&viewProjectionConstantBufferDesc, nullptr, viewProjectionCBuffer.ReleaseAndGetAddressOf()));

                m_cubeMesh = MakeCubeMesh();

                m_pbrResources = std::make_unique<Pbr::D3D11Resources>(d3d11Device.Get());
                m_pbrResources->SetLight({0.0f, 0.7071067811865475f, 0.7071067811865475f}, Pbr::RGB::White);

                // Read the BRDF Lookup Table used by the PBR system into a DirectX texture.
                std::vector<byte> brdfLutFileData = ReadFileBytes("brdf_lut.png");
                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brdfLutResourceView =
                    Pbr::D3D11Texture::LoadTextureImage(d3d11Device.Get(), brdfLutFileData.data(), (uint32_t)brdfLutFileData.size());
                m_pbrResources->SetBrdfLut(brdfLutResourceView.Get());
            }

            return true;
        }
        catch (...) {
            // Log it?
        }

        return false;
    }

    void D3D11GraphicsPlugin::ClearSwapchainCache()
    {
        m_swapchainImageDataMap.Reset();
    }

    void D3D11GraphicsPlugin::Flush()
    {
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-flush
        if (d3d11DeviceContext)
            d3d11DeviceContext->Flush();
    }

    void D3D11GraphicsPlugin::ShutdownDevice()
    {
        graphicsBinding = XrGraphicsBindingD3D11KHR{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};

        vertexShader.Reset();
        pixelShader.Reset();
        inputLayout.Reset();
        modelCBuffer.Reset();
        viewProjectionCBuffer.Reset();
        m_swapchainImageDataMap.Reset();

        m_cubeMesh = {};
        m_meshes.clear();
        m_gltfInstances.clear();
        m_gltfModels.clear();
        m_pbrResources.reset();

        d3d11DeviceContext.Reset();
        d3d11Device.Reset();
    }

    const XrBaseInStructure* D3D11GraphicsPlugin::GetGraphicsBinding() const
    {
        if (graphicsBinding.device) {
            return reinterpret_cast<const XrBaseInStructure*>(&graphicsBinding);
        }
        return nullptr;
    }

    void D3D11GraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, uint32_t arraySlice, const RGBAImage& image)
    {
        D3D11_TEXTURE2D_DESC rgbaImageDesc{};
        rgbaImageDesc.Width = image.width;
        rgbaImageDesc.Height = image.height;
        rgbaImageDesc.MipLevels = 1;
        rgbaImageDesc.ArraySize = 1;

        D3D11SwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImage);
        int64_t imageFormat = swapchainData->GetCreateInfo().format;
        XRC_CHECK_THROW(imageFormat == GetSRGBA8Format());

        rgbaImageDesc.Format = (DXGI_FORMAT)imageFormat;
        rgbaImageDesc.SampleDesc.Count = 1;
        rgbaImageDesc.SampleDesc.Quality = 0;
        rgbaImageDesc.Usage = D3D11_USAGE_DEFAULT;
        rgbaImageDesc.BindFlags = 0;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = image.pixels.data();
        initData.SysMemPitch = image.width * sizeof(uint32_t);
        initData.SysMemSlicePitch = initData.SysMemPitch * image.height;

        ComPtr<ID3D11Texture2D> texture2D;
        XRC_CHECK_THROW_HRCMD(d3d11Device->CreateTexture2D(&rgbaImageDesc, &initData, &texture2D));

        ID3D11Texture2D* const destTexture = reinterpret_cast<const XrSwapchainImageD3D11KHR*>(swapchainImage)->texture;

        D3D11_TEXTURE2D_DESC destDesc;
        destTexture->GetDesc(&destDesc);

        const UINT destSubResource = D3D11CalcSubresource(0, arraySlice, destDesc.MipLevels);
        const D3D11_BOX sourceRegion{0, 0, 0, rgbaImageDesc.Width, rgbaImageDesc.Height, 1};
        d3d11DeviceContext->CopySubresourceRegion(destTexture, destSubResource, 0 /* X */, 0 /* Y */, 0 /* Z */, texture2D.Get(), 0,
                                                  &sourceRegion);
    }

    std::string D3D11GraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        return GetDxgiImageFormatName(imageFormat);
    }

    bool D3D11GraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        return IsDxgiImageFormatKnown(imageFormat);
    }

    bool D3D11GraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                               int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        return GetDxgiSwapchainCreateTestParameters(imageFormat, swapchainTestParameters);
    }

    bool D3D11GraphicsPlugin::ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* tp,
                                                      XrSwapchain swapchain, uint32_t* imageCount) const noexcept(false)
    {
        // OK to use CHECK and REQUIRE in here because this is always called from within a test.
        *imageCount = 0;  // Zero until set below upon success.

        std::vector<XrSwapchainImageD3D11KHR> swapchainImageVector;
        uint32_t countOutput;

        XrResult result = xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr);
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput > 0);

        swapchainImageVector.resize(countOutput, XrSwapchainImageD3D11KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});

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
        swapchainImageVector.resize(countOutput, XrSwapchainImageD3D11KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        result = xrEnumerateSwapchainImages(swapchain, countOutput, &countOutput,
                                            reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput == swapchainImageVector.size());
        REQUIRE(ValidateStructVectorType(swapchainImageVector, XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR));

        for (const XrSwapchainImageD3D11KHR& image : swapchainImageVector) {
            D3D11_TEXTURE2D_DESC desc;
            image.texture->GetDesc(&desc);

            // Verify that the format is the typeless version of the requested format.
            CHECK(desc.Format == tp->expectedCreatedImageFormat);

            // Anything else from desc to check?
        }

        *imageCount = countOutput;
        return true;
    }

    bool D3D11GraphicsPlugin::ValidateSwapchainImageState(XrSwapchain /*swapchain*/, uint32_t /*index*/, int64_t /*imageFormat*/) const
    {
        // No resource state in D3D11
        return true;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t D3D11GraphicsPlugin::SelectColorSwapchainFormat(const int64_t* formatArray, size_t count) const
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
    int64_t D3D11GraphicsPlugin::SelectDepthSwapchainFormat(const int64_t* formatArray, size_t count) const
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

    int64_t D3D11GraphicsPlugin::GetSRGBA8Format() const
    {
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    ISwapchainImageData* D3D11GraphicsPlugin::AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {
        auto typedResult = std::make_unique<D3D11SwapchainImageData>(d3d11Device, uint32_t(size), swapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    inline ISwapchainImageData* D3D11GraphicsPlugin::AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo)
    {

        auto typedResult = std::make_unique<D3D11SwapchainImageData>(d3d11Device, uint32_t(size), colorSwapchainCreateInfo, depthSwapchain,
                                                                     depthSwapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    ComPtr<ID3D11RenderTargetView> D3D11GraphicsPlugin::CreateRenderTargetView(D3D11SwapchainImageData& swapchainData, uint32_t imageIndex,
                                                                               uint32_t imageArrayIndex) const
    {

        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        ComPtr<ID3D11RenderTargetView> renderTargetView;
        const CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(
            (swapchainData.SampleCount() > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY,
            (DXGI_FORMAT)swapchainData.GetCreateInfo().format, 0 /* mipSlice */, imageArrayIndex, 1 /* arraySize */);

        ID3D11Texture2D* const colorTexture = swapchainData.GetTypedImage(imageIndex).texture;

        XRC_CHECK_THROW_HRCMD(
            d3d11Device->CreateRenderTargetView(colorTexture, &renderTargetViewDesc, renderTargetView.ReleaseAndGetAddressOf()));
        return renderTargetView;
    }

    ComPtr<ID3D11DepthStencilView> D3D11GraphicsPlugin::CreateDepthStencilView(D3D11SwapchainImageData& swapchainData, uint32_t imageIndex,
                                                                               uint32_t imageArrayIndex) const
    {

        // Clear depth buffer.
        const ComPtr<ID3D11Texture2D> depthStencilTexture = swapchainData.GetDepthImageForColorIndex(imageIndex).texture;
        ComPtr<ID3D11DepthStencilView> depthStencilView;
        const XrSwapchainCreateInfo* depthCreateInfo = swapchainData.GetDepthCreateInfo();
        DXGI_FORMAT depthSwapchainFormatDX = GetDepthStencilFormatOrDefault(depthCreateInfo);
        uint32_t depthArraySize = 1;
        if (depthCreateInfo != nullptr) {
            depthArraySize = depthCreateInfo->arraySize;
        }
        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(
            (swapchainData.DepthSampleCount() > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY,
            depthSwapchainFormatDX, 0 /* mipSlice */, imageArrayIndex, depthArraySize);
        XRC_CHECK_THROW_HRCMD(
            d3d11Device->CreateDepthStencilView(depthStencilTexture.Get(), &depthStencilViewDesc, depthStencilView.GetAddressOf()));
        return depthStencilView;
    }

    void D3D11GraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                              XrColor4f color)
    {

        D3D11SwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        // Clear color buffer.
        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        ComPtr<ID3D11RenderTargetView> renderTargetView = CreateRenderTargetView(*swapchainData, imageIndex, imageArrayIndex);
        FLOAT bg[] = {color.r, color.g, color.b, color.a};
        d3d11DeviceContext->ClearRenderTargetView(renderTargetView.Get(), bg);

        // Clear depth buffer.
        ComPtr<ID3D11DepthStencilView> depthStencilView = CreateDepthStencilView(*swapchainData, imageIndex, imageArrayIndex);
        d3d11DeviceContext->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    inline MeshHandle D3D11GraphicsPlugin::MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx)
    {
        auto handle = m_meshes.emplace_back(d3d11Device, idx, vtx);
        return handle;
    }

    GLTFModelHandle D3D11GraphicsPlugin::LoadGLTF(std::shared_ptr<tinygltf::Model> tinygltfModel)
    {
        std::shared_ptr<Pbr::Model> pbrModel = Gltf::FromGltfObject(*m_pbrResources, *tinygltfModel);
        auto handle = m_gltfModels.emplace_back(std::move(pbrModel));
        return handle;
    }

    std::shared_ptr<Pbr::Model> D3D11GraphicsPlugin::GetPbrModel(GLTFModelHandle handle) const
    {
        return m_gltfModels[handle];
    }

    GLTFModelInstanceHandle D3D11GraphicsPlugin::CreateGLTFModelInstance(GLTFModelHandle handle)
    {
        auto pbrModelInstance = Pbr::D3D11ModelInstance(*m_pbrResources, GetPbrModel(handle));
        auto instanceHandle = m_gltfInstances.emplace_back(std::move(pbrModelInstance));
        return instanceHandle;
    }

    Pbr::ModelInstance& D3D11GraphicsPlugin::GetModelInstance(GLTFModelInstanceHandle handle)
    {
        return m_gltfInstances[handle].GetModelInstance();
    }

    void D3D11GraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                         const XrSwapchainImageBaseHeader* colorSwapchainImage, const RenderParams& params)
    {
        D3D11SwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        CD3D11_VIEWPORT viewport((float)layerView.subImage.imageRect.offset.x, (float)layerView.subImage.imageRect.offset.y,
                                 (float)layerView.subImage.imageRect.extent.width, (float)layerView.subImage.imageRect.extent.height);
        d3d11DeviceContext->RSSetViewports(1, &viewport);

        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        ComPtr<ID3D11RenderTargetView> renderTargetView =
            CreateRenderTargetView(*swapchainData, imageIndex, layerView.subImage.imageArrayIndex);

        ComPtr<ID3D11DepthStencilView> depthStencilView =
            CreateDepthStencilView(*swapchainData, imageIndex, layerView.subImage.imageArrayIndex);
        std::array<ID3D11RenderTargetView*, 1> renderTargets{{renderTargetView.Get()}};
        d3d11DeviceContext->OMSetRenderTargets((UINT)renderTargets.size(), renderTargets.data(), depthStencilView.Get());

        const XMMATRIX spaceToView = XMMatrixInverse(nullptr, LoadXrPose(layerView.pose));
        XrMatrix4x4f projectionMatrix;
        XrMatrix4x4f_CreateProjectionFov(&projectionMatrix, GRAPHICS_D3D, layerView.fov, 0.05f, 100.0f);

        // Set shaders and constant buffers.
        ViewProjectionConstantBuffer viewProjection;
        XMStoreFloat4x4(&viewProjection.ViewProjection, XMMatrixTranspose(spaceToView * LoadXrMatrix(projectionMatrix)));
        d3d11DeviceContext->UpdateSubresource(viewProjectionCBuffer.Get(), 0, nullptr, &viewProjection, 0, 0);

        std::array<ID3D11Buffer*, 2> constantBuffers{{modelCBuffer.Get(), viewProjectionCBuffer.Get()}};
        d3d11DeviceContext->VSSetConstantBuffers(0, (UINT)constantBuffers.size(), constantBuffers.data());
        d3d11DeviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
        d3d11DeviceContext->PSSetShader(pixelShader.Get(), nullptr, 0);

        // Set cube primitive data.
        d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d11DeviceContext->IASetInputLayout(inputLayout.Get());
        MeshHandle lastMeshHandle;

        const auto drawMesh = [&, this](const MeshDrawable mesh) {
            D3D11Mesh& d3dMesh = m_meshes[mesh.handle];
            if (mesh.handle != lastMeshHandle) {
                // We are now rendering a new mesh
                // Set primitive data.

                const UINT strides[] = {sizeof(Geometry::Vertex)};
                const UINT offsets[] = {0};
                std::array<ID3D11Buffer*, 1> vertexBuffers{{d3dMesh.vertexBuffer.Get()}};
                d3d11DeviceContext->IASetVertexBuffers(0, (UINT)vertexBuffers.size(), vertexBuffers.data(), strides, offsets);
                d3d11DeviceContext->IASetIndexBuffer(d3dMesh.indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

                lastMeshHandle = mesh.handle;
            }

            // Compute and update the model transform.
            ModelConstantBuffer model;
            XMStoreFloat4x4(&model.Model, XMMatrixTranspose(XMMatrixScaling(mesh.params.scale.x, mesh.params.scale.y, mesh.params.scale.z) *
                                                            LoadXrPose(mesh.params.pose)));
            d3d11DeviceContext->UpdateSubresource(modelCBuffer.Get(), 0, nullptr, &model, 0, 0);

            // Draw the mesh.
            d3d11DeviceContext->DrawIndexed(d3dMesh.numIndices, 0, 0);
        };

        // Render each cube
        for (const Cube& cube : params.cubes) {
            drawMesh(MeshDrawable{m_cubeMesh, cube.params.pose, cube.params.scale});
        }

        // Render each mesh
        for (const auto& mesh : params.meshes) {
            drawMesh(mesh);
        }

        // Render each gltf
        for (const auto& gltfDrawable : params.glTFs) {
            D3D11GLTF& gltf = m_gltfInstances[gltfDrawable.handle];
            // Compute and update the model transform.

            XrMatrix4x4f modelToWorld;
            XrMatrix4x4f_CreateTranslationRotationScale(&modelToWorld, &gltfDrawable.params.pose.position,
                                                        &gltfDrawable.params.pose.orientation, &gltfDrawable.params.scale);
            XrMatrix4x4f viewMatrix;
            XrVector3f unitScale = {1, 1, 1};
            XrMatrix4x4f_CreateTranslationRotationScale(&viewMatrix, &layerView.pose.position, &layerView.pose.orientation, &unitScale);
            XrMatrix4x4f viewMatrixInverse;
            XrMatrix4x4f_Invert(&viewMatrixInverse, &viewMatrix);
            m_pbrResources->SetViewProjection(LoadXrMatrix(viewMatrixInverse), LoadXrMatrix(projectionMatrix));

            gltf.Render(d3d11DeviceContext, *m_pbrResources, modelToWorld);
        }
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D11(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<D3D11GraphicsPlugin>(platformPlugin);
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_D3D11
