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

#include <map>

#include "Common.h"
#include "RuntimeFailure.h"
#include "IGraphicsValidator.h"

#if defined(XR_USE_GRAPHICS_API_D3D11)

namespace Conformance
{
#if !defined(MISSING_DIRECTX_COLORS)
    // Map type to typeless.
    const std::unordered_map<DXGI_FORMAT, DXGI_FORMAT> g_typelessMap{
        {DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_TYPELESS},
        {DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_TYPELESS},
        {DXGI_FORMAT_R32G32B32A32_SINT, DXGI_FORMAT_R32G32B32A32_TYPELESS},
        {DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32_TYPELESS},
        {DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_TYPELESS},
        {DXGI_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32_TYPELESS},
        {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_TYPELESS},
        {DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_TYPELESS},
        {DXGI_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_R16G16B16A16_TYPELESS},
        {DXGI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R16G16B16A16_TYPELESS},
        {DXGI_FORMAT_R16G16B16A16_SINT, DXGI_FORMAT_R16G16B16A16_TYPELESS},
        {DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32_TYPELESS},
        {DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_TYPELESS},
        {DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32_TYPELESS},
        {DXGI_FORMAT_D32_FLOAT_S8X24_UINT, DXGI_FORMAT_R32G8X24_TYPELESS},
        {DXGI_FORMAT_X32_TYPELESS_G8X24_UINT, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS},
        {DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_TYPELESS},
        {DXGI_FORMAT_R10G10B10A2_UINT, DXGI_FORMAT_R10G10B10A2_TYPELESS},
        {DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R10G10B10A2_TYPELESS},
        {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_TYPELESS},
        {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_TYPELESS},
        {DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_TYPELESS},
        {DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_TYPELESS},
        {DXGI_FORMAT_R8G8B8A8_SINT, DXGI_FORMAT_R8G8B8A8_TYPELESS},
        {DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16_TYPELESS},
        {DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_TYPELESS},
        {DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R16G16_TYPELESS},
        {DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16_TYPELESS},
        {DXGI_FORMAT_R16G16_SINT, DXGI_FORMAT_R16G16_TYPELESS},
        {DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_TYPELESS},
        {DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_TYPELESS},
        {DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_TYPELESS},
        {DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32_TYPELESS},
        {DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24G8_TYPELESS},
        {DXGI_FORMAT_X24_TYPELESS_G8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS},
        {DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_TYPELESS},
        {DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8_TYPELESS},
        {DXGI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R8G8_TYPELESS},
        {DXGI_FORMAT_R8G8_SINT, DXGI_FORMAT_R8G8_TYPELESS},
        {DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_TYPELESS},
        {DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R16_TYPELESS},
        {DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_TYPELESS},
        {DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_TYPELESS},
        {DXGI_FORMAT_R16_SNORM, DXGI_FORMAT_R16_TYPELESS},
        {DXGI_FORMAT_R16_SINT, DXGI_FORMAT_R16_TYPELESS},
        {DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_TYPELESS},
        {DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8_TYPELESS},
        {DXGI_FORMAT_R8_SNORM, DXGI_FORMAT_R8_TYPELESS},
        {DXGI_FORMAT_R8_SINT, DXGI_FORMAT_R8_TYPELESS},
        {DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_R8_TYPELESS},
        {DXGI_FORMAT_R1_UNORM, DXGI_FORMAT_R8_TYPELESS},
        /*
        DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
        DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
        DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
        */
        {DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_TYPELESS},
        {DXGI_FORMAT_BC1_UNORM_SRGB, DXGI_FORMAT_BC1_TYPELESS},
        {DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_TYPELESS},
        {DXGI_FORMAT_BC2_UNORM_SRGB, DXGI_FORMAT_BC2_TYPELESS},
        {DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_TYPELESS},
        {DXGI_FORMAT_BC3_UNORM_SRGB, DXGI_FORMAT_BC3_TYPELESS},
        {DXGI_FORMAT_BC4_UNORM, DXGI_FORMAT_BC4_TYPELESS},
        {DXGI_FORMAT_BC4_SNORM, DXGI_FORMAT_BC4_TYPELESS},
        {DXGI_FORMAT_BC5_UNORM, DXGI_FORMAT_BC5_TYPELESS},
        {DXGI_FORMAT_BC5_SNORM, DXGI_FORMAT_BC5_TYPELESS},
        /*
        DXGI_FORMAT_B5G6R5_UNORM = 85,
        DXGI_FORMAT_B5G5R5A1_UNORM = 86,
        DXGI_FORMAT_B8G8R8A8_UNORM = 87,
        DXGI_FORMAT_B8G8R8X8_UNORM = 88,
        DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
        */
        {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_TYPELESS},
        {DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, DXGI_FORMAT_B8G8R8X8_TYPELESS},
        {DXGI_FORMAT_BC6H_UF16, DXGI_FORMAT_BC6H_TYPELESS},
        {DXGI_FORMAT_BC6H_SF16, DXGI_FORMAT_BC6H_TYPELESS},
        {DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_BC7_TYPELESS},
        {DXGI_FORMAT_BC7_UNORM_SRGB, DXGI_FORMAT_BC7_TYPELESS},
        /*
        DXGI_FORMAT_AYUV = 100,
        DXGI_FORMAT_Y410 = 101,
        DXGI_FORMAT_Y416 = 102,
        DXGI_FORMAT_NV12 = 103,
        DXGI_FORMAT_P010 = 104,
        DXGI_FORMAT_P016 = 105,
        DXGI_FORMAT_420_OPAQUE = 106,
        DXGI_FORMAT_YUY2 = 107,
        DXGI_FORMAT_Y210 = 108,
        DXGI_FORMAT_Y216 = 109,
        DXGI_FORMAT_NV11 = 110,
        DXGI_FORMAT_AI44 = 111,
        DXGI_FORMAT_IA44 = 112,
        DXGI_FORMAT_P8 = 113,
        DXGI_FORMAT_A8P8 = 114,
        DXGI_FORMAT_B4G4R4A4_UNORM = 115,

        DXGI_FORMAT_P208 = 130,
        DXGI_FORMAT_V208 = 131,
        DXGI_FORMAT_V408 = 132,
        */
    };
#endif

    struct D3D11GraphicsValidator : IGraphicsValidator
    {
        void ValidateSwapchainFormats(ConformanceHooksBase* conformanceHooks, uint32_t count, uint64_t* formats) const override
        {
            // TODO: Do not allow types with typeless equivalents.
            (void)conformanceHooks;
            (void)count;
            (void)formats;
        }

        void ValidateSwapchainImageStructs(ConformanceHooksBase* conformanceHooks, uint64_t swapchainFormat, uint32_t count,
                                           XrSwapchainImageBaseHeader* images) const override
        {
#if !defined(MISSING_DIRECTX_COLORS)
            const auto it = g_typelessMap.find((DXGI_FORMAT)swapchainFormat);
            const DXGI_FORMAT expectedTextureFormat = it == g_typelessMap.end() ? (DXGI_FORMAT)swapchainFormat : it->second;
#endif

            const XrSwapchainImageD3D11KHR* const d3d11Images = reinterpret_cast<const XrSwapchainImageD3D11KHR*>(images);
            for (uint32_t i = 0; i < count; i++) {
                if (d3d11Images[i].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
                    conformanceHooks->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
                                                         "xrEnumerateSwapchainImages failed due to image header structure not D3D11: %d",
                                                         d3d11Images[i].type);
                }

#if 0
                D3D11_TEXTURE2D_DESC desc;
                d3d11Images[i].texture->GetDesc(&desc);

#if !defined(MISSING_DIRECTX_COLORS)
                if (desc.Format != expectedTextureFormat) {
                    conformanceHooks->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
                                                         "xrEnumerateSwapchainImages failed: ID3D11Texture2D format is not expected format %d: Swapchain : %d", expectedTextureFormat, desc.Format);
                }
#endif
#endif  // 0

                // TODO: Confirm texture is for same device(context)?
            }
        }

        void ValidateUsageFlags(ConformanceHooksBase* conformanceHooks, uint64_t usageFlags, uint32_t count,
                                XrSwapchainImageBaseHeader* images) const override
        {
            const XrSwapchainImageD3D11KHR* const d3d11Images = reinterpret_cast<const XrSwapchainImageD3D11KHR*>(images);
            for (uint32_t i = 0; i < count; i++) {
                if (d3d11Images[i].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
                    // This will already have caused a conformance failure above.
                    continue;
                }

                D3D11_TEXTURE2D_DESC desc;
                d3d11Images[i].texture->GetDesc(&desc);

                const bool hasColorUsageFlag = (usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) != 0;
                const bool hasColorBindFlag = ((desc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0);
                if (hasColorUsageFlag && !hasColorBindFlag) {
                    conformanceHooks->ConformanceFailure(
                        XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
                        "xrEnumerateSwapchainImages failed: XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT set but D3D11_BIND_RENDER_TARGET not set on texture");
                }

                const bool hasDepthUsageFlag = (usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
                const bool hasDepthBindFlag = ((desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);
                if (hasDepthUsageFlag && !hasDepthBindFlag) {
                    conformanceHooks->ConformanceFailure(
                        XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
                        "xrEnumerateSwapchainImages failed: XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT set but D3D11_BIND_DEPTH_STENCIL not set on texture");
                }

                const bool hasSampledUsageFlag = (usageFlags & XR_SWAPCHAIN_USAGE_SAMPLED_BIT) != 0;
                const bool hasSampledBindFlag = ((desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0);
                if (hasSampledUsageFlag && !hasSampledBindFlag) {
                    conformanceHooks->ConformanceFailure(
                        XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
                        "xrEnumerateSwapchainImages failed: XR_SWAPCHAIN_USAGE_SAMPLED_BIT set but D3D11_BIND_SHADER_RESOURCE not set on texture");
                }

                const bool hasUnorderedUsageFlag = (usageFlags & XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT) != 0;
                const bool hasUnorderedBindFlag = ((desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0);
                if (hasUnorderedUsageFlag && !hasUnorderedBindFlag) {
                    conformanceHooks->ConformanceFailure(
                        XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
                        "xrEnumerateSwapchainImages failed: XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT set but D3D11_BIND_UNORDERED_ACCESS not set on texture");
                }
            }
        }
    };

    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator_D3D11()
    {
        return std::make_shared<D3D11GraphicsValidator>();
    }

}  // namespace Conformance

#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
