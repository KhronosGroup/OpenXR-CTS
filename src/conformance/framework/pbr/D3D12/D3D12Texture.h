// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "D3D12Resources.h"

#include "../PbrCommon.h"

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <openxr/openxr.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

namespace Pbr
{
    namespace D3D12Texture
    {
        Conformance::D3D12ResourceWithSRVDesc LoadTextureImage(D3D12Resources& pbrResources, ID3D12GraphicsCommandList* copyCommandList,
                                                               StagingResources stagingResources, bool sRGB,
                                                               _In_reads_bytes_(fileSize) const uint8_t* fileData, uint32_t fileSize);

        Conformance::D3D12ResourceWithSRVDesc CreateFlatCubeTexture(D3D12Resources& pbrResources,
                                                                    ID3D12GraphicsCommandList* copyCommandList,
                                                                    StagingResources stagingResources, RGBAColor color, bool sRGB);

        Conformance::D3D12ResourceWithSRVDesc CreateTexture(D3D12Resources& pbrResources, ID3D12GraphicsCommandList* copyCommandList,
                                                            StagingResources stagingResources, const Conformance::Image::Image& image);

        D3D12_SAMPLER_DESC DefaultSamplerDesc();
        void CreateSampler(_In_ ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor,
                           D3D12_TEXTURE_ADDRESS_MODE addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    }  // namespace D3D12Texture
}  // namespace Pbr
