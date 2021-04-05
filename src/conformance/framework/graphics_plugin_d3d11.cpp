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

#if defined(XR_USE_GRAPHICS_API_D3D11) && !defined(MISSING_DIRECTX_COLORS)

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
    // To do: Move this declaration to a header file.
    struct D3D11GraphicsPlugin : public IGraphicsPlugin
    {
    public:
        D3D11GraphicsPlugin(std::shared_ptr<IPlatformPlugin>);

        bool Initialize() override;

        bool IsInitialized() const override;

        void Shutdown() override;

        std::string DescribeGraphics() const override;

        std::vector<std::string> GetInstanceExtensions() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

        void Flush() override;

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
        int64_t GetSRGBA8Format() const override;

        std::shared_ptr<SwapchainImageStructs> AllocateSwapchainImageStructs(size_t size,
                                                                             const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                             int64_t colorSwapchainFormat) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        int64_t colorSwapchainFormat, const std::vector<Cube>& cubes) override;

    protected:
        ComPtr<ID3D11Texture2D> GetDepthStencilTexture(ID3D11Texture2D* colorTexture);

        struct D3D11SwapchainImageStructs : public IGraphicsPlugin::SwapchainImageStructs
        {
            std::vector<XrSwapchainImageD3D11KHR> imageVector;
        };

    protected:
        bool initialized;
        XrGraphicsBindingD3D11KHR graphicsBinding;
        ComPtr<ID3D11Device> d3d11Device;
        ComPtr<ID3D11DeviceContext> d3d11DeviceContext;

        // Resources needed for rendering cubes
        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> pixelShader;
        ComPtr<ID3D11InputLayout> inputLayout;
        ComPtr<ID3D11Buffer> modelCBuffer;
        ComPtr<ID3D11Buffer> viewProjectionCBuffer;
        ComPtr<ID3D11Buffer> cubeVertexBuffer;
        ComPtr<ID3D11Buffer> cubeIndexBuffer;

        // Map color buffer to associated depth buffer. This map is populated on demand.
        std::map<ID3D11Texture2D*, ComPtr<ID3D11Texture2D>> colorToDepthMap;
    };

    D3D11GraphicsPlugin::D3D11GraphicsPlugin(std::shared_ptr<IPlatformPlugin>)
        : initialized(false), graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR}, d3d11Device(), d3d11DeviceContext()
    {
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
                CHECK(ValidateResultAllowed("xrGetD3D11GraphicsRequirementsKHR", result));
                if (FAILED(result)) {
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

                const D3D11_SUBRESOURCE_DATA vertexBufferData{Geometry::c_cubeVertices.data()};
                const CD3D11_BUFFER_DESC vertexBufferDesc((UINT)(Geometry::c_cubeVertices.size() * sizeof(Geometry::c_cubeVertices[0])),
                                                          D3D11_BIND_VERTEX_BUFFER);
                XRC_CHECK_THROW_HRCMD(
                    d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, cubeVertexBuffer.ReleaseAndGetAddressOf()));

                const D3D11_SUBRESOURCE_DATA indexBufferData{Geometry::c_cubeIndices.data()};
                const CD3D11_BUFFER_DESC indexBufferDesc((UINT)(Geometry::c_cubeIndices.size() * sizeof(Geometry::c_cubeIndices[0])),
                                                         D3D11_BIND_INDEX_BUFFER);
                XRC_CHECK_THROW_HRCMD(
                    d3d11Device->CreateBuffer(&indexBufferDesc, &indexBufferData, cubeIndexBuffer.ReleaseAndGetAddressOf()));
            }

            return true;
        }
        catch (...) {
            // Log it?
        }

        return false;
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
        d3d11DeviceContext.Reset();
        d3d11Device.Reset();
        colorToDepthMap.clear();
    }

    const XrBaseInStructure* D3D11GraphicsPlugin::GetGraphicsBinding() const
    {
        if (graphicsBinding.device) {
            return reinterpret_cast<const XrBaseInStructure*>(&graphicsBinding);
        }
        return nullptr;
    }

    void D3D11GraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, int64_t imageFormat, uint32_t arraySlice,
                                            const RGBAImage& image)
    {
        D3D11_TEXTURE2D_DESC rgbaImageDesc{};
        rgbaImageDesc.Width = image.width;
        rgbaImageDesc.Height = image.height;
        rgbaImageDesc.MipLevels = 1;
        rgbaImageDesc.ArraySize = 1;
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
        const SwapchainTestMap& dxgiSwapchainTestMap = GetDxgiSwapchainTestMap();
        SwapchainTestMap::const_iterator it = dxgiSwapchainTestMap.find(imageFormat);

        if (it != dxgiSwapchainTestMap.end()) {
            return it->second.imageFormatName;
        }

        return std::string("unknown");
    }

    bool D3D11GraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        SwapchainTestMap& dxgiSwapchainTestMap = GetDxgiSwapchainTestMap();
        SwapchainTestMap::const_iterator it = dxgiSwapchainTestMap.find(imageFormat);

        return (it != dxgiSwapchainTestMap.end());
    }

    bool D3D11GraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
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

    bool D3D11GraphicsPlugin::ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* tp,
                                                      XrSwapchain swapchain, uint32_t* imageCount) const noexcept(false)
    {
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

    std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
    D3D11GraphicsPlugin::AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& /*swapchainCreateInfo*/)
    {
        // What we are doing below is allocating a subclass of the SwapchainImageStructs struct and
        // using shared_ptr to manage it in a way that the caller doesn't need to know about the
        // graphics implementation behind it.

        auto derivedResult = std::make_shared<D3D11SwapchainImageStructs>();

        derivedResult->imageVector.resize(size, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});

        for (XrSwapchainImageD3D11KHR& image : derivedResult->imageVector) {
            derivedResult->imagePtrVector.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
        }

        // Cast our derived type to the caller-expected type.
        std::shared_ptr<SwapchainImageStructs> result =
            std::static_pointer_cast<SwapchainImageStructs, D3D11SwapchainImageStructs>(derivedResult);

        return result;
    }

    ComPtr<ID3D11Texture2D> D3D11GraphicsPlugin::GetDepthStencilTexture(ID3D11Texture2D* colorTexture)
    {
        // If a depth-stencil view has already been created for this back-buffer, use it.
        auto depthBufferIt = colorToDepthMap.find(colorTexture);
        if (depthBufferIt != colorToDepthMap.end()) {
            return depthBufferIt->second;
        }

        // This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.
        D3D11_TEXTURE2D_DESC colorDesc;
        colorTexture->GetDesc(&colorDesc);

        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = colorDesc.Width;
        depthDesc.Height = colorDesc.Height;
        depthDesc.ArraySize = colorDesc.ArraySize;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        depthDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
        depthDesc.SampleDesc.Count = 1;
        ComPtr<ID3D11Texture2D> depthTexture;
        XRC_CHECK_THROW_HRCMD(d3d11Device->CreateTexture2D(&depthDesc, nullptr, depthTexture.ReleaseAndGetAddressOf()));

        colorToDepthMap.insert(std::make_pair(colorTexture, depthTexture));

        return depthTexture;
    }

    void D3D11GraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                              int64_t colorSwapchainFormat)
    {
        ID3D11Texture2D* const colorTexture = reinterpret_cast<const XrSwapchainImageD3D11KHR*>(colorSwapchainImage)->texture;

        // Clear color buffer.
        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        ComPtr<ID3D11RenderTargetView> renderTargetView;
        const CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, (DXGI_FORMAT)colorSwapchainFormat,
                                                                  0 /* mipSlice */, imageArrayIndex, 1 /* arraySize */);
        // TODO: Do not clear to a color when using a pass-through view configuration.
        XRC_CHECK_THROW_HRCMD(
            d3d11Device->CreateRenderTargetView(colorTexture, &renderTargetViewDesc, renderTargetView.ReleaseAndGetAddressOf()));
        d3d11DeviceContext->ClearRenderTargetView(renderTargetView.Get(), DirectX::Colors::DarkSlateGray);

        // Clear depth buffer.
        const ComPtr<ID3D11Texture2D> depthStencilTexture = GetDepthStencilTexture(colorTexture);
        ComPtr<ID3D11DepthStencilView> depthStencilView;
        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_D32_FLOAT, 0 /* mipSlice */,
                                                            imageArrayIndex, 1 /* arraySize */);
        XRC_CHECK_THROW_HRCMD(
            d3d11Device->CreateDepthStencilView(depthStencilTexture.Get(), &depthStencilViewDesc, depthStencilView.GetAddressOf()));
        d3d11DeviceContext->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    void D3D11GraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                         const XrSwapchainImageBaseHeader* colorSwapchainImage, int64_t colorSwapchainFormat,
                                         const std::vector<Cube>& cubes)
    {
        auto LoadXrPose = [](const XrPosef& pose) -> XMMATRIX {
            return XMMatrixAffineTransformation(DirectX::g_XMOne, DirectX::g_XMZero,
                                                XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&pose.orientation)),
                                                XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&pose.position)));
        };

        auto LoadXrMatrix = [](const XrMatrix4x4f& matrix) -> XMMATRIX {
            // XrMatrix4x4f has same memory layout as DirectX Math (Row-major,post-multiplied = column-major,pre-multiplied)
            return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&matrix));
        };

        ID3D11Texture2D* const colorTexture = reinterpret_cast<const XrSwapchainImageD3D11KHR*>(colorSwapchainImage)->texture;

        CD3D11_VIEWPORT viewport((float)layerView.subImage.imageRect.offset.x, (float)layerView.subImage.imageRect.offset.y,
                                 (float)layerView.subImage.imageRect.extent.width, (float)layerView.subImage.imageRect.extent.height);
        d3d11DeviceContext->RSSetViewports(1, &viewport);

        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        ComPtr<ID3D11RenderTargetView> renderTargetView;
        const CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, (DXGI_FORMAT)colorSwapchainFormat,
                                                                  0 /* mipSlice */, layerView.subImage.imageArrayIndex, 1 /* arraySize */);
        XRC_CHECK_THROW_HRCMD(
            d3d11Device->CreateRenderTargetView(colorTexture, &renderTargetViewDesc, renderTargetView.ReleaseAndGetAddressOf()));

        const ComPtr<ID3D11Texture2D> depthStencilTexture = GetDepthStencilTexture(colorTexture);
        ComPtr<ID3D11DepthStencilView> depthStencilView;
        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_D32_FLOAT, 0 /* mipSlice */,
                                                            layerView.subImage.imageArrayIndex, 1 /* arraySize */);
        XRC_CHECK_THROW_HRCMD(
            d3d11Device->CreateDepthStencilView(depthStencilTexture.Get(), &depthStencilViewDesc, depthStencilView.GetAddressOf()));

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
        const UINT strides[] = {sizeof(Geometry::Vertex)};
        const UINT offsets[] = {0};
        std::array<ID3D11Buffer*, 1> vertexBuffers{{cubeVertexBuffer.Get()}};
        d3d11DeviceContext->IASetVertexBuffers(0, (UINT)vertexBuffers.size(), vertexBuffers.data(), strides, offsets);
        d3d11DeviceContext->IASetIndexBuffer(cubeIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d11DeviceContext->IASetInputLayout(inputLayout.Get());

        // Render each cube
        for (const Cube& cube : cubes) {
            // Compute and update the model transform.
            ModelConstantBuffer model;
            XMStoreFloat4x4(&model.Model,
                            XMMatrixTranspose(XMMatrixScaling(cube.Scale.x, cube.Scale.y, cube.Scale.z) * LoadXrPose(cube.Pose)));
            d3d11DeviceContext->UpdateSubresource(modelCBuffer.Get(), 0, nullptr, &model, 0, 0);

            // Draw the cube.
            d3d11DeviceContext->DrawIndexed((UINT)Geometry::c_cubeIndices.size(), 0, 0);
        }
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D11(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<D3D11GraphicsPlugin>(platformPlugin);
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_D3D11
