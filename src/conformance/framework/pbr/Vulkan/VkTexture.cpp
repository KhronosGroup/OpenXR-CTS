// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_VULKAN)

#include "VkTexture.h"

#include "VkCommon.h"
#include "VkResources.h"
#include "stb_image.h"

#include "../PbrCommon.h"

#include "common/vulkan_debug_object_namer.hpp"
#include "utilities/throw_helpers.h"
#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <stdlib.h>

namespace Pbr
{
    namespace VulkanTexture
    {
        std::array<uint8_t, 4> LoadRGBAUI4(RGBAColor color)
        {
            return std::array<uint8_t, 4>{(uint8_t)(color.r * 255.), (uint8_t)(color.g * 255.), (uint8_t)(color.b * 255.),
                                          (uint8_t)(color.a * 255.)};
        }

        VulkanTextureBundle LoadTextureImage(Pbr::VulkanResources& pbrResources, const uint8_t* fileData, uint32_t fileSize)
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

            return CreateTexture(pbrResources, rgbaData.get(), DesiredComponentCount, w, h, VK_FORMAT_R8G8B8A8_UNORM);
        }

        /// Creates a texture and fills all array members with the data in rgba
        VulkanTextureBundle CreateTextureArrayRepeat(VulkanResources& pbrResources, const VulkanDebugObjectNamer& namer, const char* name,
                                                     const uint8_t* rgba, uint32_t elemSize, uint32_t width, uint32_t height, bool cubemap,
                                                     VkFormat format)
        {
            VkDevice device = pbrResources.GetDevice();
            const Conformance::MemoryAllocator& memAllocator = pbrResources.GetMemoryAllocator();
            const Conformance::CmdBuffer& copyCmdBuffer = pbrResources.GetCopyCommandBuffer();

            VulkanTextureBundle bundle{};

            uint32_t layerCount = cubemap ? 6 : 1;

            bundle.width = width;
            bundle.height = height;
            bundle.mipLevels = 1;
            bundle.layerCount = layerCount;

            // Create a staging buffer
            VkBufferCreateInfo bufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferCreateInfo.size = static_cast<VkDeviceSize>(width) * height * elemSize;

            Conformance::BufferAndMemory stagingBuffer;
            stagingBuffer.Create(device, memAllocator, bufferCreateInfo);
            XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_BUFFER, (uint64_t)stagingBuffer.buf, "CTS texture array staging buffer"));
            stagingBuffer.Update<uint8_t>(device, {rgba, static_cast<size_t>(bufferCreateInfo.size)}, 0);

            // create image
            VkImage image{VK_NULL_HANDLE};
            VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            imageInfo.flags = cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = layerCount;
            imageInfo.format = format;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            XRC_CHECK_THROW_VKCMD(vkCreateImage(device, &imageInfo, nullptr, &image));
            XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE, (uint64_t)image, name));

            bundle.image = Conformance::ScopedVkImage(image, device);

            VkDeviceMemory imageMemory;
            VkMemoryRequirements memRequirements{};
            vkGetImageMemoryRequirements(device, bundle.image.get(), &memRequirements);
            memAllocator.Allocate(memRequirements, &imageMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)imageMemory, name));
            XRC_CHECK_THROW_VKCMD(vkBindImageMemory(device, bundle.image.get(), imageMemory, 0));

            bundle.deviceMemory = Conformance::ScopedVkDeviceMemory(imageMemory, device);

            // Switch the destination image to TRANSFER_DST_OPTIMAL
            VkImageMemoryBarrier imgBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            imgBarrier.srcAccessMask = 0;
            imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgBarrier.image = bundle.image.get();
            imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount};
            vkCmdPipelineBarrier(copyCmdBuffer.buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                 nullptr, 1, &imgBarrier);

            for (uint32_t layer = 0; layer < layerCount; ++layer) {
                VkBufferImageCopy region;
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = layer;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = {0, 0, 0};
                region.imageExtent = {width, height, 1};

                vkCmdCopyBufferToImage(copyCmdBuffer.buf, stagingBuffer.buf, bundle.image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                       &region);
            }

            // Switch the destination image to SHADER_READ_ONLY_OPTIMAL
            imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgBarrier.image = bundle.image.get();
            imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount};
            vkCmdPipelineBarrier(copyCmdBuffer.buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                                 nullptr, 1, &imgBarrier);

            pbrResources.DestroyAfterRender(stagingBuffer);

            return bundle;
        }

        VulkanTextureBundle CreateFlatCubeTexture(VulkanResources& pbrResources, RGBAColor color, VkFormat format)
        {
            // Each side is a 1x1 pixel (RGBA) image.
            const std::array<uint8_t, 4> rgbaColor = LoadRGBAUI4(color);
            const VulkanDebugObjectNamer& namer = pbrResources.GetDebugNamer();
            VulkanTextureBundle textureBundle =
                CreateTextureArrayRepeat(pbrResources, namer, "CTS PBR 2D color image", rgbaColor.data(), 4, 1, 1, true, format);
            assert(textureBundle.image != VK_NULL_HANDLE);

            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = textureBundle.image.get();
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            viewInfo.format = format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 6;
            VkImageView view;
            XRC_CHECK_THROW_VKCMD(vkCreateImageView(pbrResources.GetDevice(), &viewInfo, nullptr, &view));
            XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)view, "CTS PBR 2D color image view"));

            textureBundle.view.adopt(view, pbrResources.GetDevice());

            return textureBundle;
        }

        VulkanTextureBundle CreateTexture(VulkanResources& pbrResources, const uint8_t* rgba, int elemSize, int width, int height,
                                          VkFormat format)
        {
            const VulkanDebugObjectNamer& namer = pbrResources.GetDebugNamer();
            VulkanTextureBundle textureBundle =
                CreateTextureArrayRepeat(pbrResources, namer, "CTS PBR 2D color image", rgba, elemSize, width, height, false, format);
            assert(textureBundle.image != VK_NULL_HANDLE);

            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = textureBundle.image.get();
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            VkImageView view;
            XRC_CHECK_THROW_VKCMD(vkCreateImageView(pbrResources.GetDevice(), &viewInfo, nullptr, &view));
            XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)view, "CTS PBR 2D color image view"));

            textureBundle.view.adopt(view, pbrResources.GetDevice());

            return textureBundle;
        }

        VkSamplerCreateInfo DefaultSamplerCreateInfo()
        {
            VkSamplerCreateInfo info{};

            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            info.magFilter = VK_FILTER_LINEAR;
            info.minFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.mipLodBias = 0.0f;
            info.anisotropyEnable = false;
            info.maxAnisotropy = 16.0f;
            info.compareEnable = false;
            info.minLod = 0.0f;
            info.maxLod = VK_LOD_CLAMP_NONE;
            info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
            info.unnormalizedCoordinates = false;

            return info;
        }

        VkSampler CreateSampler(VkDevice device, VkSamplerAddressMode addressMode)
        {
            VkSamplerCreateInfo info = DefaultSamplerCreateInfo();

            info.addressModeU = info.addressModeV = info.addressModeW = addressMode;

            VkSampler destSampler;
            XRC_CHECK_THROW_VKCMD(vkCreateSampler(device, &info, NULL, &destSampler));
            return destSampler;
        }
    }  // namespace VulkanTexture
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
