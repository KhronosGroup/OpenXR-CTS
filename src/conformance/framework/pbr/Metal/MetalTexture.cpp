// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalTexture.h"

#include "stb_image.h"

#include <tinygltf/tiny_gltf.h>

namespace Pbr
{
    namespace MetalTexture
    {
        std::array<uint8_t, 4> LoadRGBAUI4(RGBAColor color)
        {
            return std::array<uint8_t, 4>{(uint8_t)(color.r * 255.), (uint8_t)(color.g * 255.), (uint8_t)(color.b * 255.),
                                          (uint8_t)(color.a * 255.)};
        }

        NS::SharedPtr<MTL::Texture> LoadTextureImage(MetalResources& pbrResources, const uint8_t* fileData, uint32_t fileSize,
                                                     const NS::String* label)
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

            return CreateTexture(pbrResources, rgbaData.get(), DesiredComponentCount, w, h, MTL::PixelFormatRGBA8Unorm, label);
        }

        NS::SharedPtr<MTL::Texture> CreateFlatCubeTexture(MetalResources& pbrResources, RGBAColor color, MTL::PixelFormat format,
                                                          const NS::String* label)
        {
            NS::SharedPtr<MTL::TextureDescriptor> desc = NS::RetainPtr(MTL::TextureDescriptor::textureCubeDescriptor(format, 1, false));

            NS::SharedPtr<MTL::Texture> texture = NS::TransferPtr(pbrResources.GetDevice()->newTexture(desc.get()));

            // Each side is a 1x1 pixel (RGBA) image.
            const std::array<uint8_t, 4> rgbaColor = LoadRGBAUI4(color);

            for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex) {
                MTL::Region region(0, 0, 1, 1);
                texture->replaceRegion(region, 0, faceIndex, rgbaColor.data(), 4 /*bytesPerRow*/, 0 /*bytesPerImage*/);
            }

            texture->setLabel(label);

            return texture;
        }

        NS::SharedPtr<MTL::Texture> CreateTexture(MetalResources& pbrResources, const uint8_t* rgba, int elemSize, int width, int height,
                                                  MTL::PixelFormat format, const NS::String* label)
        {
            return CreateTexture(pbrResources.GetDevice().get(), rgba, elemSize, width, height, format, label);
        }

        NS::SharedPtr<MTL::Texture> CreateTexture(MTL::Device* device, const uint8_t* rgba, int elemSize, int width, int height,
                                                  MTL::PixelFormat format, const NS::String* label)
        {
            NS::SharedPtr<MTL::TextureDescriptor> desc =
                NS::RetainPtr(MTL::TextureDescriptor::texture2DDescriptor(format, width, height, false));

            NS::SharedPtr<MTL::Texture> texture = NS::TransferPtr(device->newTexture(desc.get()));

            MTL::Region region(0, 0, width, height);
            texture->replaceRegion(region, 0, rgba, elemSize * width);

            texture->setLabel(label);

            return texture;
        }

        NS::SharedPtr<MTL::SamplerDescriptor> DefaultSamplerDesc()
        {
            NS::SharedPtr<MTL::SamplerDescriptor> desc = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
            desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
            desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
            desc->setMipFilter(MTL::SamplerMipFilterLinear);
            desc->setMaxAnisotropy(16);
            desc->setSAddressMode(MTL::SamplerAddressModeRepeat);
            desc->setTAddressMode(MTL::SamplerAddressModeRepeat);
            desc->setRAddressMode(MTL::SamplerAddressModeRepeat);
            desc->setBorderColor(MTL::SamplerBorderColorOpaqueWhite);
            desc->setCompareFunction(MTL::CompareFunctionLessEqual);
            return desc;
        }

        NS::SharedPtr<MTL::SamplerState> CreateSampler(MTL::Device* device, MTL::SamplerAddressMode addressMode)
        {
            auto desc = DefaultSamplerDesc();
            desc->setSAddressMode(addressMode);
            desc->setTAddressMode(addressMode);
            desc->setRAddressMode(addressMode);
            NS::SharedPtr<MTL::SamplerState> samplerState = NS::TransferPtr(device->newSamplerState(desc.get()));
            return samplerState;
        }
    }  // namespace MetalTexture
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_METAL)
