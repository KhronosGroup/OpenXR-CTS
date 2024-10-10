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
#include "../PbrTexture.h"
#include "../D3DCommon.h"

#include "stb_image.h"

#include "utilities/throw_helpers.h"

#include <d3dx12.h>
#include <tinygltf/tiny_gltf.h>

using namespace DirectX;

namespace Pbr
{
    namespace D3D12Texture
    {
        namespace Image = Conformance::Image;

        Conformance::D3D12ResourceWithSRVDesc LoadTextureImage(Pbr::D3D12Resources& pbrResources,
                                                               ID3D12GraphicsCommandList* copyCommandList,
                                                               StagingResources stagingResources, bool sRGB,
                                                               _In_reads_bytes_(fileSize) const uint8_t* fileData, uint32_t fileSize)
        {
            StbiLoader::OwningImage<StbiLoader::stbi_unique_ptr> owningImage =
                StbiLoader::LoadTextureImage(pbrResources.GetSupportedFormats(), sRGB, fileData, fileSize);
            return CreateTexture(pbrResources, copyCommandList, stagingResources, owningImage.image);
        }

        /// Creates a texture array with support for multiple mip levels and compressed textures
        Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureArray(D3D12Resources& pbrResources, ID3D12GraphicsCommandList* copyCommandList,
                                                                  StagingResources stagingResources, span<const Image::Image*> imageArray)
        {
            Microsoft::WRL::ComPtr<ID3D12Device> device = pbrResources.GetDevice();

            uint16_t arraySize = imageArray.size();
            assert(arraySize > 0);

            uint16_t mipLevels = imageArray[0]->levels.size();
            assert(mipLevels > 0);

            uint16_t baseMipWidth = imageArray[0]->levels[0].metadata.physicalDimensions.width;
            uint16_t baseMipHeight = imageArray[0]->levels[0].metadata.physicalDimensions.height;
            Image::FormatParams formatParams = imageArray[0]->format;
            DXGI_FORMAT format = ToDXGIFormat(formatParams);

            // consistency check
            for (auto arrayLayer : imageArray) {
                assert(arrayLayer->levels.size() == mipLevels);
                assert(arrayLayer->levels[0].metadata.physicalDimensions.width == baseMipWidth);
                assert(arrayLayer->levels[0].metadata.physicalDimensions.height == baseMipHeight);
                (void)arrayLayer;
            }

            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> imageUploadBuffers;
            Microsoft::WRL::ComPtr<ID3D12Resource> image = Conformance::D3D12CreateImage(
                device.Get(), baseMipWidth, baseMipHeight, arraySize, mipLevels, format, D3D12_HEAP_TYPE_DEFAULT);

            D3D12_RESOURCE_DESC imageDesc = image->GetDesc();
            assert(imageDesc.DepthOrArraySize == arraySize);
            // TODO: maybe call GetCopyableFootprints only once, as all out fields accept arrays
            for (int arrayIndex = 0; arrayIndex < arraySize; arrayIndex++) {
                Image::Image const& arrayLayer = *imageArray[arrayIndex];

                for (int mipLevel = 0; mipLevel < mipLevels; mipLevel++) {
                    auto levelData = arrayLayer.levels[mipLevel];

                    UINT subresourceIndex = D3D12CalcSubresource(mipLevel, arrayIndex, 0, imageDesc.MipLevels, arraySize);

                    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
                    UINT rowCount;
                    UINT64 rowSize;
                    UINT64 uploadBufferSize;
                    device->GetCopyableFootprints(&imageDesc, subresourceIndex, 1, 0, &footprint, &rowCount, &rowSize, &uploadBufferSize);

                    // useful resource: https://www.gamedev.net/forums/topic/677932-getcopyablefootprints-question/
                    assert((size_t)rowCount == (size_t)(levelData.metadata.RowCount()));

                    assert((size_t)rowSize ==
                           (size_t)(levelData.metadata.RowSizeInBlocksOrPixels() * arrayLayer.format.BytesPerBlockOrPixel()));

                    Microsoft::WRL::ComPtr<ID3D12Resource> imageUpload =
                        Conformance::D3D12CreateBuffer(device.Get(), (uint32_t)uploadBufferSize, D3D12_HEAP_TYPE_UPLOAD);
                    *(stagingResources++) = imageUpload;

                    D3D12_SUBRESOURCE_DATA initData{};
                    initData.pData = levelData.data.data();
                    initData.RowPitch = (levelData.metadata.physicalDimensions.width / levelData.metadata.blockSize.width) *
                                        arrayLayer.format.BytesPerBlockOrPixel();
                    initData.SlicePitch = levelData.data.size();

                    // this does a row-by-row memcpy internally or we would have used our own CopyWithStride
                    Internal::ThrowIf(!UpdateSubresources(copyCommandList, image.Get(), imageUpload.Get(), 0, 1, uploadBufferSize,
                                                          &footprint, &rowCount, &rowSize, &initData),
                                      "Call to UpdateSubresources helper failed");
                }
            }

            return image;
        }

        Conformance::D3D12ResourceWithSRVDesc CreateFlatCubeTexture(D3D12Resources& pbrResources,
                                                                    ID3D12GraphicsCommandList* copyCommandList,
                                                                    StagingResources stagingResources, RGBAColor color, bool sRGB)
        {
            // Each side is a 1x1 pixel (RGBA) image.
            const std::array<uint8_t, 4> rgbaColor = LoadRGBAUI4(color);

            auto formatParams = Image::FormatParams::R8G8B8A8(sRGB);
            auto metadata = Image::ImageLevelMetadata::MakeUncompressed(1, 1);
            auto face = Image::Image{formatParams, {{metadata, rgbaColor}}};

            std::array<Image::Image const*, 6> faces;
            faces.fill(&face);

            Microsoft::WRL::ComPtr<ID3D12Resource> texture = CreateTextureArray(pbrResources, copyCommandList, stagingResources, faces);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = ToDXGIFormat(formatParams);
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

            return Conformance::D3D12ResourceWithSRVDesc{std::move(texture), srvDesc};
        }
        Conformance::D3D12ResourceWithSRVDesc CreateTexture(D3D12Resources& pbrResources, ID3D12GraphicsCommandList* copyCommandList,
                                                            StagingResources stagingResources, const Image::Image& image)
        {
            Image::Image const* imageArray[] = {&image};
            Microsoft::WRL::ComPtr<ID3D12Resource> texture =
                CreateTextureArray(pbrResources, copyCommandList, stagingResources, imageArray);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = ToDXGIFormat(image.format);
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
