// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "D3D11Texture.h"

#include "D3D11Resources.h"
#include "../D3DCommon.h"
#include "../PbrTexture.h"

#include "stb_image.h"

#include "utilities/throw_helpers.h"

#include <memory>
#include <stdexcept>

using namespace DirectX;

namespace Pbr
{
    namespace D3D11Texture
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureImage(const D3D11Resources& pbrResources, bool sRGB,
                                                                          _In_reads_bytes_(fileSize) const uint8_t* fileData,
                                                                          uint32_t fileSize)
        {
            StbiLoader::OwningImage<StbiLoader::stbi_unique_ptr> owningImage =
                StbiLoader::LoadTextureImage(pbrResources.GetSupportedFormats(), sRGB, fileData, fileSize);
            return CreateTexture(pbrResources, owningImage.image);
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateFlatCubeTexture(const D3D11Resources& pbrResources, RGBAColor color,
                                                                               DXGI_FORMAT format)
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = 1;
            desc.Height = 1;
            desc.MipLevels = 1;
            desc.ArraySize = 6;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

            // Each side is a 1x1 pixel (RGBA) image.
            const std::array<uint8_t, 4> rgbaColor = LoadRGBAUI4(color);
            D3D11_SUBRESOURCE_DATA initData[6];
            for (int i = 0; i < _countof(initData); i++) {
                initData[i].pSysMem = rgbaColor.data();
                initData[i].SysMemPitch = initData[i].SysMemSlicePitch = 4;
            }

            Microsoft::WRL::ComPtr<ID3D11Texture2D> cubeTexture;
            XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateTexture2D(&desc, initData, cubeTexture.ReleaseAndGetAddressOf()));

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
            XRC_CHECK_THROW_HRCMD(
                pbrResources.GetDevice()->CreateShaderResourceView(cubeTexture.Get(), &srvDesc, textureView.ReleaseAndGetAddressOf()));

            return textureView;
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateTexture(const D3D11Resources& pbrResources,
                                                                       const Conformance::Image::Image& image)
        {
            auto dxgiFormat = ToDXGIFormat(image.format);
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = image.levels[0].metadata.physicalDimensions.width;
            desc.Height = image.levels[0].metadata.physicalDimensions.height;
            desc.MipLevels = image.levels.size();
            desc.ArraySize = 1;
            desc.Format = dxgiFormat;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            auto subData = std::vector<D3D11_SUBRESOURCE_DATA>{};
            subData.reserve(image.levels.size());
            for (auto& level : image.levels) {
                D3D11_SUBRESOURCE_DATA levelData{};
                levelData.pSysMem = level.data.data();
                levelData.SysMemPitch =
                    (level.metadata.physicalDimensions.width / level.metadata.blockSize.width) * image.format.BytesPerBlockOrPixel();
                levelData.SysMemSlicePitch = level.data.size();
                subData.push_back(levelData);
            }

            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
            XRC_CHECK_THROW_HRCMD(pbrResources.GetDevice()->CreateTexture2D(&desc, subData.data(), texture2D.ReleaseAndGetAddressOf()));

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
            XRC_CHECK_THROW_HRCMD(
                pbrResources.GetDevice()->CreateShaderResourceView(texture2D.Get(), &srvDesc, textureView.ReleaseAndGetAddressOf()));

            return textureView;
        }

        Microsoft::WRL::ComPtr<ID3D11SamplerState> CreateSampler(_In_ ID3D11Device* device, D3D11_TEXTURE_ADDRESS_MODE addressMode)
        {
            CD3D11_SAMPLER_DESC samplerDesc(CD3D11_DEFAULT{});
            samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = addressMode;

            Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
            XRC_CHECK_THROW_HRCMD(device->CreateSamplerState(&samplerDesc, samplerState.ReleaseAndGetAddressOf()));
            return samplerState;
        }
    }  // namespace D3D11Texture
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D11)
