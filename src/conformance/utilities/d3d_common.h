// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#if defined(XR_USE_GRAPHICS_API_D3D11) || defined(XR_USE_GRAPHICS_API_D3D12)
#include <openxr/openxr.h>

#include "common/xr_linear.h"
#include "swapchain_parameters.h"

#include <DirectXColors.h>
#include <D3Dcompiler.h>

#include <dxgi.h>
#include <d3dcommon.h>
#include <DirectXMath.h>
#include <wrl.h>

#include <string>
#include <map>

namespace Conformance
{
    struct ModelConstantBuffer
    {
        DirectX::XMFLOAT4X4 Model;
    };
    struct ViewProjectionConstantBuffer
    {
        DirectX::XMFLOAT4X4 ViewProjection;
    };

    // Separate entry points for the vertex and pixel shader functions.
    constexpr char ShaderHlsl[] = R"_(
    struct PSVertex {
        float4 Pos : SV_POSITION;
        float3 Color : COLOR0;
    };
    struct Vertex {
        float3 Pos : POSITION;
        float3 Color : COLOR0;
    };
    cbuffer ModelConstantBuffer : register(b0) {
        float4x4 Model;
    };
    cbuffer ViewProjectionConstantBuffer : register(b1) {
        float4x4 ViewProjection;
    };

    PSVertex MainVS(Vertex input) {
       PSVertex output;
       output.Pos = mul(mul(float4(input.Pos, 1), Model), ViewProjection);
       output.Color = input.Color;
       return output;
    }

    float4 MainPS(PSVertex input) : SV_TARGET {
        return float4(input.Color, 1);
    }
    )_";

    DirectX::XMMATRIX XM_CALLCONV LoadXrPose(const XrPosef& pose);
    DirectX::XMMATRIX XM_CALLCONV LoadXrMatrix(const XrMatrix4x4f& matrix);

    Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const char* hlsl, const char* entrypoint, const char* shaderTarget);
    Microsoft::WRL::ComPtr<IDXGIAdapter1> GetDXGIAdapter(LUID adapterId) noexcept(false);

    typedef std::map<int64_t, SwapchainCreateTestParameters> SwapchainTestMap;
    SwapchainTestMap& GetDxgiSwapchainTestMap();

    /// Implementation for IGraphicsPlugin::GetSwapchainCreateTestParameters for D3D11/12
    bool GetDxgiSwapchainCreateTestParameters(int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters);

    /// Implementation for GetImageFormatName for d3d plugins
    std::string GetDxgiImageFormatName(int64_t imageFormat);

    /// Implementation for IsImageFormatKnown for d3d plugins
    bool IsDxgiImageFormatKnown(int64_t imageFormat);

    static constexpr DXGI_FORMAT kDefaultDepthFormat = DXGI_FORMAT_D32_FLOAT;
    static constexpr DXGI_FORMAT kDefaultDepthFormatTypeless = DXGI_FORMAT_R32_TYPELESS;

    /// Either get the format from the depth swapchain create info, or return the default format
    DXGI_FORMAT GetDepthStencilFormatOrDefault(const XrSwapchainCreateInfo* createInfo);

}  // namespace Conformance

#endif
