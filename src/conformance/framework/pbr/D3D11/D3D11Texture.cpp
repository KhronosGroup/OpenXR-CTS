// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include "D3D11Texture.h"

#include "stb_image.h"

#include "utilities/throw_helpers.h"

#include <memory>
#include <stdexcept>

using namespace DirectX;

namespace Pbr
{
    namespace D3D11Texture
    {
        std::array<uint8_t, 4> LoadRGBAUI4(RGBAColor color)
        {
            return std::array<uint8_t, 4>{(uint8_t)(color.r * 255.), (uint8_t)(color.g * 255.), (uint8_t)(color.b * 255.),
                                          (uint8_t)(color.a * 255.)};
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureImage(_In_ ID3D11Device* device,
                                                                          _In_reads_bytes_(fileSize) const uint8_t* fileData,
                                                                          uint32_t fileSize)
        {
            auto freeImageData = [](unsigned char* ptr) { ::free(ptr); };
            using stbi_unique_ptr = std::unique_ptr<unsigned char, decltype(freeImageData)>;

            constexpr uint32_t DesiredComponentCount = 4;

            int w, h, c;
            // If c == 3, a component will be padded with 1.0f
            stbi_unique_ptr rgbaData(stbi_load_from_memory(fileData, fileSize, &w, &h, &c, DesiredComponentCount), freeImageData);
            if (!rgbaData) {
                throw std::runtime_error("Failed to load image file data.");
            }

            return CreateTexture(device, rgbaData.get(), w * h * DesiredComponentCount, w, h, DXGI_FORMAT_R8G8B8A8_UNORM);
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateFlatCubeTexture(_In_ ID3D11Device* device, RGBAColor color,
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
            XRC_CHECK_THROW_HRCMD(device->CreateTexture2D(&desc, initData, cubeTexture.ReleaseAndGetAddressOf()));

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
            XRC_CHECK_THROW_HRCMD(device->CreateShaderResourceView(cubeTexture.Get(), &srvDesc, textureView.ReleaseAndGetAddressOf()));

            return textureView;
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateTexture(_In_ ID3D11Device* device,
                                                                       _In_reads_bytes_(size) const uint8_t* rgba, uint32_t size, int width,
                                                                       int height, DXGI_FORMAT format)
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA initData{};
            initData.pSysMem = rgba;
            initData.SysMemPitch = size / height;
            initData.SysMemSlicePitch = size;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
            XRC_CHECK_THROW_HRCMD(device->CreateTexture2D(&desc, &initData, texture2D.ReleaseAndGetAddressOf()));

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = desc.MipLevels - 1;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
            XRC_CHECK_THROW_HRCMD(device->CreateShaderResourceView(texture2D.Get(), &srvDesc, textureView.ReleaseAndGetAddressOf()));

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
