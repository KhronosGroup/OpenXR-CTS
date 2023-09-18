// Copyright (c) 2017-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11) || defined(XR_USE_GRAPHICS_API_D3D12)

#include "d3d_common.h"

#include "common/xr_linear.h"
#include "swapchain_format_data.h"
#include "swapchain_parameters.h"
#include "throw_helpers.h"

#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <dxgiformat.h>
#include <winerror.h>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace Conformance
{
    XMMATRIX XM_CALLCONV LoadXrPose(const XrPosef& pose)
    {
        return XMMatrixAffineTransformation(DirectX::g_XMOne, DirectX::g_XMZero,
                                            XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&pose.orientation)),
                                            XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&pose.position)));
    }

    XMMATRIX XM_CALLCONV LoadXrMatrix(const XrMatrix4x4f& matrix)
    {
        // XrMatrix4x4f has same memory layout as DirectX Math (Row-major,post-multiplied = column-major,pre-multiplied)
        return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&matrix));
    }

    ComPtr<ID3DBlob> CompileShader(const char* hlsl, const char* entrypoint, const char* shaderTarget)
    {
        ComPtr<ID3DBlob> compiled;
        ComPtr<ID3DBlob> errMsgs;
        DWORD flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;

#if !defined(NDEBUG)
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        HRESULT hr = D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, entrypoint, shaderTarget, flags, 0, compiled.GetAddressOf(),
                                errMsgs.GetAddressOf());
        if (FAILED(hr)) {
            std::string errMsg((const char*)errMsgs->GetBufferPointer(), errMsgs->GetBufferSize());
            // Log(Fmt("D3DCompile failed %X: %s", hr, errMsg.c_str()));
            XRC_CHECK_THROW_HRESULT(hr, "D3DCompile");
        }

        return compiled;
    }

    bool operator==(LUID luid, uint64_t id)
    {
        return ((((uint64_t)luid.HighPart << 32) | (uint64_t)luid.LowPart) == id);
    }

    // If adapterId is 0 then use the first adapter we find, the default adapter.
    ComPtr<IDXGIAdapter1> GetDXGIAdapter(LUID adapterId) noexcept(false)
    {
        ComPtr<IDXGIAdapter1> dxgiAdapter;
        ComPtr<IDXGIFactory1> dxgiFactory;

        HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory.ReleaseAndGetAddressOf()));
        XRC_CHECK_THROW_HRESULT(hr, "GetAdapter: CreateDXGIFactory1");

        for (UINT adapterIndex = 0;; adapterIndex++) {
            // EnumAdapters1 will fail with DXGI_ERROR_NOT_FOUND when there are no more adapters to enumerate.
            hr = dxgiFactory->EnumAdapters1(adapterIndex, dxgiAdapter.ReleaseAndGetAddressOf());

            if (hr == DXGI_ERROR_NOT_FOUND || !dxgiAdapter) {
                XRC_THROW("Could not find graphics adapter with indicated LUID");
            }
            DXGI_ADAPTER_DESC1 adapterDesc;
            hr = dxgiAdapter->GetDesc1(&adapterDesc);
            XRC_CHECK_THROW_HRESULT(hr, "dxgiAdapter->GetDesc1");

            if ((adapterId == 0) || memcmp(&adapterDesc.AdapterLuid, &adapterId, sizeof(adapterId)) == 0) {
                return dxgiAdapter;
            }
        }
    }

    SwapchainTestMap& GetDxgiSwapchainTestMap()
    {
        static SwapchainTestMap dxgiSwapchainTestMap{

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32B32A32_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32B32A32_FLOAT).ExpectedFormat(DXGI_FORMAT_R32G32B32A32_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32B32A32_UINT).ExpectedFormat(DXGI_FORMAT_R32G32B32A32_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32B32A32_SINT).ExpectedFormat(DXGI_FORMAT_R32G32B32A32_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32B32_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32B32_FLOAT).ExpectedFormat(DXGI_FORMAT_R32G32B32_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32B32_UINT).ExpectedFormat(DXGI_FORMAT_R32G32B32_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32B32_SINT).ExpectedFormat(DXGI_FORMAT_R32G32B32_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16B16A16_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16B16A16_FLOAT).ExpectedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16B16A16_UINT).ExpectedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16B16A16_SINT).ExpectedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16B16A16_UNORM).ExpectedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16B16A16_SNORM).ExpectedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32_FLOAT).ExpectedFormat(DXGI_FORMAT_R32G32_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32_UINT).ExpectedFormat(DXGI_FORMAT_R32G32_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G32_SINT).ExpectedFormat(DXGI_FORMAT_R32G32_TYPELESS).Build(),

            // 32bit channel, 8bit channel, 24bit ignored. All typeless.
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32G8X24_TYPELESS).Typeless().Build(),
            // 32bit float depth, 8 bit uint stencil, 24bit ignored.
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_D32_FLOAT_S8X24_UINT).ExpectedFormat(DXGI_FORMAT_R32G8X24_TYPELESS).DepthStencil().Build(),
            // 32bit float red, 8bit ignored, 24bit ignored. Not typeless because used parts are typed?
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS).Typeless().Build(),
            // typeless unused 32bit component, 8bit uint green, and 24bit unused. Not typeless because used parts are typed?
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_X32_TYPELESS_G8X24_UINT).ExpectedFormat(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R10G10B10A2_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R10G10B10A2_UNORM).ExpectedFormat(DXGI_FORMAT_R10G10B10A2_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R10G10B10A2_UINT).ExpectedFormat(DXGI_FORMAT_R10G10B10A2_TYPELESS).Build(),

            // This doesn't have a typeless equivalent, so it's created as-is by the runtime.
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R11G11B10_FLOAT).NotMutable().Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8B8A8_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8B8A8_UNORM).ExpectedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).ExpectedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8B8A8_UINT).ExpectedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8B8A8_SINT).ExpectedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8B8A8_SNORM).ExpectedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16_FLOAT).ExpectedFormat(DXGI_FORMAT_R16G16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16_UINT).ExpectedFormat(DXGI_FORMAT_R16G16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16_SINT).ExpectedFormat(DXGI_FORMAT_R16G16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16_UNORM).ExpectedFormat(DXGI_FORMAT_R16G16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16G16_SNORM).ExpectedFormat(DXGI_FORMAT_R16G16_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32_FLOAT).ExpectedFormat(DXGI_FORMAT_R32_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_D32_FLOAT).ExpectedFormat(DXGI_FORMAT_R32_TYPELESS).Depth().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32_UINT).ExpectedFormat(DXGI_FORMAT_R32_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R32_SINT).ExpectedFormat(DXGI_FORMAT_R32_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R24G8_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_D24_UNORM_S8_UINT).ExpectedFormat(DXGI_FORMAT_R24G8_TYPELESS).Depth().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R24_UNORM_X8_TYPELESS).ExpectedFormat(DXGI_FORMAT_R24G8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_X24_TYPELESS_G8_UINT).ExpectedFormat(DXGI_FORMAT_R24G8_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8_UINT).ExpectedFormat(DXGI_FORMAT_R8G8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8_SINT).ExpectedFormat(DXGI_FORMAT_R8G8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8_UNORM).ExpectedFormat(DXGI_FORMAT_R8G8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8_SNORM).ExpectedFormat(DXGI_FORMAT_R8G8_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16_FLOAT).ExpectedFormat(DXGI_FORMAT_R16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_D16_UNORM).ExpectedFormat(DXGI_FORMAT_R16_TYPELESS).Depth().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16_UINT).ExpectedFormat(DXGI_FORMAT_R16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16_SINT).ExpectedFormat(DXGI_FORMAT_R16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16_UNORM).ExpectedFormat(DXGI_FORMAT_R16_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R16_SNORM).ExpectedFormat(DXGI_FORMAT_R16_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8_UINT).ExpectedFormat(DXGI_FORMAT_R8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8_SINT).ExpectedFormat(DXGI_FORMAT_R8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8_UNORM).ExpectedFormat(DXGI_FORMAT_R8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8_SNORM).ExpectedFormat(DXGI_FORMAT_R8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_A8_UNORM).ExpectedFormat(DXGI_FORMAT_R8_TYPELESS).Build(),

            // These don't have typeless equivalents, so they are created as-is by the runtime.
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R1_UNORM).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R9G9B9E5_SHAREDEXP).NotMutable().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R8G8_B8G8_UNORM).NotMutable().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_G8R8_G8B8_UNORM).NotMutable().Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC1_TYPELESS).Compressed().Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC1_UNORM).Compressed().ExpectedFormat(DXGI_FORMAT_BC1_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC1_UNORM_SRGB).Compressed().ExpectedFormat(DXGI_FORMAT_BC1_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC2_TYPELESS).Compressed().Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC2_UNORM).Compressed().ExpectedFormat(DXGI_FORMAT_BC2_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC2_UNORM_SRGB).Compressed().ExpectedFormat(DXGI_FORMAT_BC2_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC3_TYPELESS).Compressed().Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC3_UNORM).Compressed().ExpectedFormat(DXGI_FORMAT_BC3_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC3_UNORM_SRGB).Compressed().ExpectedFormat(DXGI_FORMAT_BC3_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC4_TYPELESS).Compressed().Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC4_UNORM).Compressed().ExpectedFormat(DXGI_FORMAT_BC4_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC4_SNORM).Compressed().ExpectedFormat(DXGI_FORMAT_BC4_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC5_TYPELESS).Compressed().Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC5_UNORM).Compressed().ExpectedFormat(DXGI_FORMAT_BC5_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC5_SNORM).Compressed().ExpectedFormat(DXGI_FORMAT_BC5_TYPELESS).Build(),

            // These don't have typeless equivalents, so they are created as-is by the runtime.
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B5G6R5_UNORM).NotMutable().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B5G5R5A1_UNORM).NotMutable().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM).NotMutable().Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B8G8R8A8_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B8G8R8A8_UNORM).ExpectedFormat(DXGI_FORMAT_B8G8R8A8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB).ExpectedFormat(DXGI_FORMAT_B8G8R8A8_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B8G8R8X8_TYPELESS).Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B8G8R8X8_UNORM).ExpectedFormat(DXGI_FORMAT_B8G8R8X8_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B8G8R8X8_UNORM_SRGB).ExpectedFormat(DXGI_FORMAT_B8G8R8X8_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC6H_TYPELESS).Compressed().Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC6H_UF16).Compressed().ExpectedFormat(DXGI_FORMAT_BC6H_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC6H_SF16).Compressed().ExpectedFormat(DXGI_FORMAT_BC6H_TYPELESS).Build(),

            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC7_TYPELESS).Compressed().Typeless().Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC7_UNORM).Compressed().ExpectedFormat(DXGI_FORMAT_BC7_TYPELESS).Build(),
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_BC7_UNORM_SRGB).Compressed().ExpectedFormat(DXGI_FORMAT_BC7_TYPELESS).Build(),

            // This doesn't have a typeless equivalent, so it's created as-is by the runtime.
            XRC_SWAPCHAIN_FORMAT(DXGI_FORMAT_B4G4R4A4_UNORM).NotMutable().Build(),

        };

        return dxgiSwapchainTestMap;
    }

    bool GetDxgiSwapchainCreateTestParameters(int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
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

        const SwapchainTestMap& dxgiSwapchainTestMap = GetDxgiSwapchainTestMap();
        SwapchainTestMap::const_iterator it = dxgiSwapchainTestMap.find(imageFormat);

        // Verify that the image format is known. If it's not known then this test needs to be
        // updated to recognize new DXGI formats.
        XRC_CHECK_THROW_MSG(it != dxgiSwapchainTestMap.end(), "Unknown DXGI image format.");

        // Verify that imageFormat is not a typeless type. Only regular types are allowed to
        // be returned by the runtime for enumerated image formats.
        XRC_CHECK_THROW_MSG(!it->second.mutableFormat,
                            "Typeless DXGI image formats must not be enumerated by runtimes: " + it->second.imageFormatName);

        // We may now proceed with creating swapchains with the format.
        *swapchainTestParameters = it->second;
        return true;
    }

    std::string GetDxgiImageFormatName(int64_t imageFormat)
    {
        const SwapchainTestMap& dxgiSwapchainTestMap = GetDxgiSwapchainTestMap();
        SwapchainTestMap::const_iterator it = dxgiSwapchainTestMap.find(imageFormat);

        if (it != dxgiSwapchainTestMap.end()) {
            return it->second.imageFormatName;
        }

        return std::string("unknown");
    }

    bool IsDxgiImageFormatKnown(int64_t imageFormat)
    {
        SwapchainTestMap& dxgiSwapchainTestMap = GetDxgiSwapchainTestMap();
        SwapchainTestMap::const_iterator it = dxgiSwapchainTestMap.find(imageFormat);

        return (it != dxgiSwapchainTestMap.end());
    }

    DXGI_FORMAT GetDepthStencilFormatOrDefault(const XrSwapchainCreateInfo* createInfo)
    {
        if (createInfo) {
            return (DXGI_FORMAT)createInfo->format;
        }
        return kDefaultDepthFormat;
    }
}  // namespace Conformance

#endif
