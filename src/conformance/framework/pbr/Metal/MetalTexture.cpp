// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalTexture.h"
#include "MetalFormats.h"

#include "../PbrTexture.h"

#include "stb_image.h"

#include <tinygltf/tiny_gltf.h>

#include <Foundation/NSTypes.hpp>

namespace Pbr
{
    namespace MetalTexture
    {
        namespace Image = Conformance::Image;

        NS::SharedPtr<MTL::Texture> LoadTextureImage(const MetalResources& pbrResources, bool sRGB, const uint8_t* fileData,
                                                     uint32_t fileSize, const NS::String* label)
        {
            StbiLoader::OwningImage<StbiLoader::stbi_unique_ptr> owningImage =
                StbiLoader::LoadTextureImage(pbrResources.GetSupportedFormats(), sRGB, fileData, fileSize);
            return CreateTexture(pbrResources, owningImage.image, label);
        }

        NS::SharedPtr<MTL::Texture> CreateFlatCubeTexture(const MetalResources& pbrResources, RGBAColor color, MTL::PixelFormat format,
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

        NS::SharedPtr<MTL::Texture> CreateTexture(const MetalResources& pbrResources, const Conformance::Image::Image& image,
                                                  const NS::String* label)
        {
            return CreateTexture(pbrResources.GetDevice().get(), image, label);
        }

        NS::SharedPtr<MTL::Texture> CreateTexture(MTL::Device* device, const Conformance::Image::Image& image, const NS::String* label)
        {
            auto metalFormat = ToMetalFormat(image.format);
            auto baseMipWidth = image.levels[0].metadata.physicalDimensions.width;
            auto baseMipHeight = image.levels[0].metadata.physicalDimensions.height;
            auto mipLevels = image.levels.size();
            NS::SharedPtr<MTL::TextureDescriptor> desc =
                NS::RetainPtr(MTL::TextureDescriptor::texture2DDescriptor(metalFormat, baseMipWidth, baseMipHeight, mipLevels > 1));
            desc->setMipmapLevelCount(mipLevels);

            NS::SharedPtr<MTL::Texture> texture = NS::TransferPtr(device->newTexture(desc.get()));

            for (auto& level : image.levels) {
                MTL::Region region(0, 0, level.metadata.physicalDimensions.width, level.metadata.physicalDimensions.height);
                NS::UInteger bytesPerRow =
                    (level.metadata.physicalDimensions.width / level.metadata.blockSize.width) * image.format.BytesPerBlockOrPixel();
                texture->replaceRegion(region, 0, level.data.data(), bytesPerRow);
            }

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
