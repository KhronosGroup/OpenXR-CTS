// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
//
// Shared data types and functions used throughout the Pbr rendering library.
//

#pragma once

#include "../PbrCommon.h"
#include <utilities/image.h>

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <openxr/openxr.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

namespace Pbr
{
    struct D3D11Resources;
    namespace D3D11Texture
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureImage(const D3D11Resources& pbrResources, bool sRGB,
                                                                          _In_reads_bytes_(fileSize) const uint8_t* fileData,
                                                                          uint32_t fileSize);
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateFlatCubeTexture(const D3D11Resources& pbrResources, RGBAColor color,
                                                                               DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateTexture(const D3D11Resources& pbrResources,
                                                                       const Conformance::Image::Image& image);
        Microsoft::WRL::ComPtr<ID3D11SamplerState> CreateSampler(_In_ ID3D11Device* device,
                                                                 D3D11_TEXTURE_ADDRESS_MODE addressMode = D3D11_TEXTURE_ADDRESS_CLAMP);
    }  // namespace D3D11Texture
}  // namespace Pbr
