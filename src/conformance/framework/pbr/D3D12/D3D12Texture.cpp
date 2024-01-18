// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "D3D12Texture.h"

#include "D3D12Resources.h"
#include "stb_image.h"

#include "utilities/throw_helpers.h"

#include <d3dx12.h>
#include <tinygltf/tiny_gltf.h>

using namespace DirectX;

namespace Pbr
{
    namespace D3D12Texture
    {
        std::array<uint8_t, 4> LoadRGBAUI4(RGBAColor color)
        {
            return std::array<uint8_t, 4>{(uint8_t)(color.r * 255.), (uint8_t)(color.g * 255.), (uint8_t)(color.b * 255.),
                                          (uint8_t)(color.a * 255.)};
        }

        Conformance::D3D12ResourceWithSRVDesc LoadTextureImage(Pbr::D3D12Resources& pbrResources,
                                                               _In_reads_bytes_(fileSize) const uint8_t* fileData, uint32_t fileSize)
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

            return CreateTexture(pbrResources, rgbaData.get(), DesiredComponentCount, w, h, DXGI_FORMAT_R8G8B8A8_UNORM);
        }

        /// Creates a texture and fills all array members with the data in rgba
        Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureArrayRepeat(D3D12Resources& pbrResources,
                                                                        _In_reads_bytes_(elemSize* width* height) const uint8_t* rgba,
                                                                        int elemSize, int width, int height, int arraySize,
                                                                        DXGI_FORMAT format)
        {
            Microsoft::WRL::ComPtr<ID3D12Device> device = pbrResources.GetDevice();

            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList = pbrResources.CreateCopyCommandList();

            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> imageUploadBuffers;
            Microsoft::WRL::ComPtr<ID3D12Resource> image =
                Conformance::D3D12CreateImage(device.Get(), width, height, arraySize, format, D3D12_HEAP_TYPE_DEFAULT);

            D3D12_RESOURCE_DESC imageDesc = image->GetDesc();
            assert(imageDesc.DepthOrArraySize == arraySize);
            imageUploadBuffers.reserve(arraySize);
            // TODO: maybe call GetCopyableFootprints only once, as all out fields accept arrays
            // TODO: put the upload buffer in a staging resources vector and make async
            for (int arrayIndex = 0; arrayIndex < arraySize; arrayIndex++) {
                UINT subresourceIndex = D3D12CalcSubresource(0, arrayIndex, 0, imageDesc.MipLevels, arraySize);

                D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
                UINT rowCount;
                UINT64 rowSize;
                UINT64 uploadBufferSize;
                device->GetCopyableFootprints(&imageDesc, subresourceIndex, 1, 0, &footprint, &rowCount, &rowSize, &uploadBufferSize);

                // doesn't hold for compressed textures, see: https://www.gamedev.net/forums/topic/677932-getcopyablefootprints-question/
                assert((size_t)rowCount == (size_t)height);

                // assert this for now, probably doesn't hold for e.g. compressed textures
                assert(rowSize == width * elemSize);

                Microsoft::WRL::ComPtr<ID3D12Resource> imageUpload =
                    Conformance::D3D12CreateBuffer(device.Get(), (uint32_t)uploadBufferSize, D3D12_HEAP_TYPE_UPLOAD);
                imageUploadBuffers.push_back(imageUpload);

                D3D12_SUBRESOURCE_DATA initData{};
                initData.pData = rgba;
                initData.RowPitch = elemSize * width;
                initData.SlicePitch = elemSize * width * height;

                // this does a row-by-row memcpy internally or we would have used our own CopyWithStride
                Internal::ThrowIf(!UpdateSubresources(cmdList.Get(), image.Get(), imageUpload.Get(), 0, 1, uploadBufferSize, &footprint,
                                                      &rowCount, &rowSize, &initData),
                                  "Call to UpdateSubresources helper failed");
            }

            XRC_CHECK_THROW_HRCMD(cmdList->Close());
            pbrResources.ExecuteCopyCommandList(cmdList.Get(), std::move(imageUploadBuffers));

            return image;
        }

        Conformance::D3D12ResourceWithSRVDesc CreateFlatCubeTexture(D3D12Resources& pbrResources, RGBAColor color, DXGI_FORMAT format)
        {
            // Each side is a 1x1 pixel (RGBA) image.
            const std::array<uint8_t, 4> rgbaColor = D3D12Texture::LoadRGBAUI4(color);
            Microsoft::WRL::ComPtr<ID3D12Resource> texture = CreateTextureArrayRepeat(pbrResources, rgbaColor.data(), 4, 1, 1, 6, format);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

            return Conformance::D3D12ResourceWithSRVDesc{std::move(texture), srvDesc};
        }

        Conformance::D3D12ResourceWithSRVDesc CreateTexture(D3D12Resources& pbrResources,
                                                            _In_reads_bytes_(elemSize* width* height) const uint8_t* rgba, int elemSize,
                                                            int width, int height, DXGI_FORMAT format)
        {
            Microsoft::WRL::ComPtr<ID3D12Resource> texture =
                CreateTextureArrayRepeat(pbrResources, rgba, elemSize, width, height, 1, format);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.TextureCube.MipLevels = 1;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

            return Conformance::D3D12ResourceWithSRVDesc{std::move(texture), srvDesc};
        }

        D3D12_SAMPLER_DESC DefaultSamplerDesc()
        {
            D3D12_SAMPLER_DESC samplerDesc;

            samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.MipLODBias = 0.0f;
            samplerDesc.MaxAnisotropy = 16;
            samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            samplerDesc.BorderColor[0] = 1.0f;
            samplerDesc.BorderColor[1] = 1.0f;
            samplerDesc.BorderColor[2] = 1.0f;
            samplerDesc.BorderColor[3] = 1.0f;
            samplerDesc.MinLOD = 0.0f;
            samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

            return samplerDesc;
        }

        void CreateSampler(_In_ ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor, D3D12_TEXTURE_ADDRESS_MODE addressMode)
        {
            D3D12_SAMPLER_DESC samplerDesc = DefaultSamplerDesc();

            samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = addressMode;

            device->CreateSampler(&samplerDesc, destDescriptor);
        }
    }  // namespace D3D12Texture
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
