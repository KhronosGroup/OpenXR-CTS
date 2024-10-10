// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_VULKAN)

#include "VkResources.h"

#include "VkCommon.h"
#include "VkFormats.h"
#include "VkMaterial.h"
#include "VkPipelineStates.h"
#include "VkPrimitive.h"
#include "VkTexture.h"
#include "VkTextureCache.h"

#include "../../gltf/GltfHelper.h"
#include "../GlslBuffers.h"
#include "../PbrCommon.h"
#include "../PbrHandles.h"
#include "../PbrSharedState.h"

#include "common/vulkan_debug_object_namer.hpp"
#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"
#include "utilities/xr_math_operators.h"

#include <nonstd/type.hpp>
#include <tinygltf/tiny_gltf.h>
#include <vulkan/vulkan_core.h>

#include <cassert>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace Pbr
{
    struct Material;
}  // namespace Pbr

// IWYU pragma: begin_keep
const uint32_t g_PbrVertexShader[] = SPV_PREFIX
#include <PbrVertexShader_glsl_spv.h>
    SPV_SUFFIX;

const uint32_t g_PbrPixelShader[] = SPV_PREFIX
#include <PbrPixelShader_glsl_spv.h>
    SPV_SUFFIX;
// IWYU pragma: end_keep

using namespace openxr::math_operators;

namespace
{

    // TODO why are these here instead of in VkPipelines or something?
    static constexpr VkVertexInputAttributeDescription c_attrDesc[6] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Pbr::Vertex, Position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Pbr::Vertex, Normal)},
        {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Pbr::Vertex, Tangent)},
        {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Pbr::Vertex, Color0)},
        {4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Pbr::Vertex, TexCoord0)},
        {5, 0, VK_FORMAT_R16_UINT, offsetof(Pbr::Vertex, ModelTransformIndex)},
    };

    static constexpr VkVertexInputBindingDescription c_bindingDesc[1] = {
        {0, sizeof(Pbr::Vertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };

    std::vector<Conformance::Image::FormatParams> MakeSupportedFormatsList(VkPhysicalDevice physicalDevice)
    {
        std::vector<Conformance::Image::FormatParams> supported;
        for (auto& format : Pbr::GetVkFormatMap()) {
            VkImageFormatProperties formatProperties;
            VkResult ret = vkGetPhysicalDeviceImageFormatProperties(
                physicalDevice, format.second, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &formatProperties);

            if (ret == VK_ERROR_FORMAT_NOT_SUPPORTED) {
                continue;
            }
            else {
                XRC_CHECK_THROW_VKCMD(ret);
            }

            supported.push_back(format.first);
        }
        return supported;
    }
}  // namespace

namespace Pbr
{
    using ImageKey = std::tuple<const tinygltf::Image*, bool>;  // Item1 is a pointer to the image, Item2 is sRGB.

    namespace PipelineLayout
    {
        enum BindingSection
        {
            SceneConstantBuffer,
            ModelConstantBuffer,
            MaterialConstantBuffer,
            TransformsBuffer,
            MaterialTextures,
            GlobalTextures,
            BindingSectionCount,
        };

        class VulkanDescriptorSetLayout
        {
        public:
            VulkanDescriptorSetLayout() = default;
            std::array<VkDescriptorSetLayoutBinding, BindingCount> m_bindings = {};
            std::array<VkDescriptorPoolSize, BindingCount> m_poolSizes = {};
            std::array<bool, BindingCount> m_writtenBindings = {};
            std::array<size_t, BindingSectionCount> m_sectionOffsets = {};
            std::array<size_t, BindingSectionCount> m_sectionSizes = {};

            void AssertFullyInitialized()
            {
                for (int i = 0; i < BindingSectionCount; i++) {
                    if (m_sectionSizes[i] == 0) {
                        throw std::logic_error("VulkanDescriptorSetLayout: Not all layout sections were written");
                    }
                }
                for (auto writtenBinding : m_writtenBindings) {
                    if (!writtenBinding) {
                        // this is legal but we aren't intentionally doing sparse bindings
                        throw std::logic_error("VulkanDescriptorSetLayout: Not all bindings were written");
                    }
                }
            };

            void SetBindings(BindingSection section, uint32_t bindIndex, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags,
                             uint32_t count = 1)
            {
                // section ranges for indexing
                m_sectionOffsets[section] = bindIndex;
                m_sectionSizes[section] = count;

                for (uint32_t i = 0; i < count; i++) {
                    // descriptor pool layout
                    m_bindings[bindIndex + i].binding = bindIndex + i;
                    m_bindings[bindIndex + i].descriptorType = descriptorType;
                    m_bindings[bindIndex + i].descriptorCount = 1;
                    m_bindings[bindIndex + i].stageFlags = stageFlags;
                    m_bindings[bindIndex + i].pImmutableSamplers = nullptr;

                    // pool sizes for allocation
                    m_poolSizes[bindIndex + i].type = descriptorType;
                    m_poolSizes[bindIndex + i].descriptorCount = 1;

                    m_writtenBindings[bindIndex + i] = true;
                }
            };

            VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device)
            {
                AssertFullyInitialized();

                VkDescriptorSetLayoutCreateInfo layoutInfo{};
                layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layoutInfo.bindingCount = (uint32_t)m_bindings.size();
                layoutInfo.pBindings = m_bindings.data();

                VkDescriptorSetLayout descriptorSetLayout;
                XRC_CHECK_THROW_VKCMD(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));

                return descriptorSetLayout;
            }

            VkDescriptorPool CreateDescriptorPool(VkDevice device, uint32_t maxSets)
            {
                AssertFullyInitialized();

                VkDescriptorPoolCreateInfo descriptorPoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
                descriptorPoolInfo.maxSets = maxSets;
                // maxSets is not a multiplier on pool sizes, so we need to scale them too
                std::array<VkDescriptorPoolSize, BindingCount> poolSizesScaled = m_poolSizes;
                for (VkDescriptorPoolSize& poolSize : poolSizesScaled) {
                    poolSize.descriptorCount *= maxSets;
                }
                descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizesScaled.size();
                descriptorPoolInfo.pPoolSizes = poolSizesScaled.data();

                VkDescriptorPool descriptorPool;
                XRC_CHECK_THROW_VKCMD(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
                return descriptorPool;
            };
        };

        class VulkanWriteDescriptorSetsBuilder
        {
        private:
            VulkanDescriptorSetLayout m_layout;
            std::unique_ptr<VulkanWriteDescriptorSets> m_wds{};
            std::array<bool, BindingCount> m_boundBindings{};

        public:
            VulkanWriteDescriptorSetsBuilder(VulkanDescriptorSetLayout layout, VkDescriptorSet dstSet) : m_layout(layout)
            {
                m_layout.AssertFullyInitialized();
                m_wds = std::make_unique<VulkanWriteDescriptorSets>();

                for (size_t i = 0; i < m_layout.m_bindings.size(); ++i) {
                    m_wds->writeDescriptorSets[i] = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    m_wds->writeDescriptorSets[i].dstSet = dstSet;
                    m_wds->writeDescriptorSets[i].dstBinding = m_layout.m_bindings[i].binding;
                    m_wds->writeDescriptorSets[i].descriptorCount = m_layout.m_bindings[i].descriptorCount;
                    m_wds->writeDescriptorSets[i].descriptorType = m_layout.m_bindings[i].descriptorType;
                }
            }

            void BindBuffers(BindingSection section, span<const VkDescriptorBufferInfo> bufferInfos)
            {
                const size_t sectionOffset = m_layout.m_sectionOffsets[section];
                const size_t sectionSize = m_layout.m_sectionSizes[section];
                assert(bufferInfos.size() <= sectionSize);
                (void)sectionSize;

                for (size_t indexInSection = 0; indexInSection < bufferInfos.size(); ++indexInSection) {
                    size_t bindingIndex = sectionOffset + indexInSection;
                    assert((m_wds->writeDescriptorSets[bindingIndex].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ||
                           (m_wds->writeDescriptorSets[bindingIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) ||
                           (m_wds->writeDescriptorSets[bindingIndex].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                           (m_wds->writeDescriptorSets[bindingIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC));
                    assert(bufferInfos[indexInSection].buffer != VK_NULL_HANDLE);
                    m_wds->BindBuffer(bindingIndex, bufferInfos[indexInSection]);
                    m_boundBindings[bindingIndex] = true;
                }
            }

            void BindImages(BindingSection section, span<const VkDescriptorImageInfo> imageInfos)
            {
                const size_t sectionOffset = m_layout.m_sectionOffsets[section];
                const size_t sectionSize = m_layout.m_sectionSizes[section];
                assert(sectionSize == imageInfos.size());
                (void)sectionSize;

                for (size_t indexInSection = 0; indexInSection < imageInfos.size(); ++indexInSection) {
                    size_t bindingIndex = sectionOffset + indexInSection;
                    assert((m_wds->writeDescriptorSets[bindingIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ||
                           (m_wds->writeDescriptorSets[bindingIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
                           (m_wds->writeDescriptorSets[bindingIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
                    assert(imageInfos[indexInSection].imageView != VK_NULL_HANDLE);
                    assert(imageInfos[indexInSection].imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
                    assert(imageInfos[indexInSection].sampler != VK_NULL_HANDLE);
                    m_wds->BindImage(bindingIndex, imageInfos[indexInSection]);
                    m_boundBindings[bindingIndex] = true;
                }
            }

            std::unique_ptr<VulkanWriteDescriptorSets> Build()
            {
                for (auto boundBinding : m_boundBindings) {
                    if (!boundBinding) {
                        // this is legal but we aren't intentionally doing sparse bindings
                        throw std::logic_error("VulkanDescriptorSetLayout: Not all bindings were bound");
                    }
                }
                return std::move(m_wds);
            }
        };

        void SetupBindings(VulkanDescriptorSetLayout& layoutBuilder)
        {
            // constant buffers
            layoutBuilder.SetBindings(SceneConstantBuffer, ShaderSlots::ConstantBuffers::Scene, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            layoutBuilder.SetBindings(ModelConstantBuffer, ShaderSlots::ConstantBuffers::Model, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_VERTEX_BIT);
            layoutBuilder.SetBindings(MaterialConstantBuffer, ShaderSlots::ConstantBuffers::Material, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT);

            // transform buffer
            layoutBuilder.SetBindings(TransformsBuffer, (int)ShaderSlots::GLSL::VSResourceViewsOffset + (int)ShaderSlots::Transforms,
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,  //
                                      ShaderSlots::NumVSResourceViews);

            // combined textures and samplers
            layoutBuilder.SetBindings(MaterialTextures, ShaderSlots::GLSL::MaterialTexturesOffset,              //
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,  //
                                      ShaderSlots::NumMaterialSlots);
            layoutBuilder.SetBindings(GlobalTextures, ShaderSlots::GLSL::GlobalTexturesOffset,                  //
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,  //
                                      (int)ShaderSlots::NumTextures - (int)ShaderSlots::NumMaterialSlots);
        }

        // very basic for now, can grow if needed
        VkPipelineLayout CreatePipelineLayout(VkDevice device, const VkDescriptorSetLayout& descriptorSetLayout)
        {

            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            pipelineLayoutCreateInfo.setLayoutCount = 1;
            pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

            VkPipelineLayout pipelineLayout;
            XRC_CHECK_THROW_VKCMD(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

            return pipelineLayout;
        }
    }  // namespace PipelineLayout

    struct VulkanResources::Impl
    {
        void Initialize(const VulkanDebugObjectNamer& objnamer, VkPhysicalDevice physicalDevice_, VkDevice device_,
                        uint32_t queueFamilyIndex)
        {
            device = device_;
            allocator.Init(physicalDevice_, device);

            Internal::ThrowIf(!copyCmdBuffer.Init(objnamer, device_, queueFamilyIndex), "Failed to create command buffer");
            copyCmdBuffer.Begin();

            PipelineLayout::SetupBindings(VulkanLayout);

            Resources.DescriptorSetLayout =
                std::make_shared<Conformance::ScopedVkDescriptorSetLayout>(VulkanLayout.CreateDescriptorSetLayout(device), device);
            Resources.PipelineLayout = std::make_shared<Conformance::ScopedVkPipelineLayout>(
                PipelineLayout::CreatePipelineLayout(device, Resources.DescriptorSetLayout->get()), device);

            Resources.Pipelines = std::make_unique<VulkanPipelines>(device, Resources.PipelineLayout, c_attrDesc, c_bindingDesc,
                                                                    g_PbrVertexShader, g_PbrPixelShader);

            // Set up the scene constant buffer.
            Resources.SceneBuffer.Init(device, allocator);
            Resources.SceneBuffer.Create(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            XRC_CHECK_THROW_VKCMD(objnamer.SetName(VK_OBJECT_TYPE_BUFFER, (uint64_t)Resources.SceneBuffer.buf, "CTS pbr scene buffer"));

            Resources.BrdfSampler.adopt(VulkanTexture::CreateSampler(device), device);
            Resources.EnvironmentMapSampler.adopt(VulkanTexture::CreateSampler(device), device);

            Resources.SupportedTextureFormats = MakeSupportedFormatsList(physicalDevice_);
        }

        void Reset()
        {
            allocator.Reset();

            Resources.BrdfSampler.reset();
            Resources.EnvironmentMapSampler.reset();

            device = VK_NULL_HANDLE;
        }

        struct DeviceResources
        {
            VkDevice device;

            std::shared_ptr<VulkanTextureBundle> BrdfLut;
            std::shared_ptr<VulkanTextureBundle> SpecularEnvironmentMap;
            std::shared_ptr<VulkanTextureBundle> DiffuseEnvironmentMap;
            mutable VulkanTextureCache SolidColorTextureCache;

            Conformance::StructuredBuffer<Glsl::SceneConstantBuffer> SceneBuffer;
            Conformance::ScopedVkSampler BrdfSampler;
            Conformance::ScopedVkSampler EnvironmentMapSampler;
            std::shared_ptr<Conformance::ScopedVkDescriptorSetLayout> DescriptorSetLayout;
            std::shared_ptr<Conformance::ScopedVkPipelineLayout> PipelineLayout;
            std::unique_ptr<VulkanPipelines> Pipelines{};

            std::vector<Conformance::Image::FormatParams> SupportedTextureFormats;

            std::vector<Conformance::BufferAndMemory> StagingBuffers;
        };

        VulkanDebugObjectNamer namer;
        VkDevice device{VK_NULL_HANDLE};
        Conformance::MemoryAllocator allocator{};
        Conformance::CmdBuffer copyCmdBuffer{};

        PrimitiveCollection<VulkanPrimitive> Primitives;

        DeviceResources Resources;
        Glsl::SceneConstantBuffer SceneBuffer;
        PipelineLayout::VulkanDescriptorSetLayout VulkanLayout{};

        struct LoaderResources
        {
            // Create cache for reuse of textures (images, samplers, etc) when possible.
            std::map<ImageKey, std::shared_ptr<VulkanTextureBundle>> imageMap;
            std::map<const tinygltf::Sampler*, std::shared_ptr<Conformance::ScopedVkSampler>> samplerMap;
        };
        LoaderResources loaderResources;
    };

    VulkanResources::VulkanResources(const VulkanDebugObjectNamer& namer, VkPhysicalDevice physicalDevice, VkDevice device,
                                     uint32_t queueFamilyIndex)
        : m_impl(std::make_unique<Impl>())
    {
        m_impl->Initialize(namer, physicalDevice, device, queueFamilyIndex);
    }

    VulkanResources::VulkanResources(VulkanResources&& resources) noexcept = default;

    VulkanResources::~VulkanResources()
    {
        // stagingBuffers are queued to be cleared in Wait(). If Wait() has not been called, clear them here.
        for (auto stagingBuffer : m_impl->Resources.StagingBuffers) {
            stagingBuffer.Reset(GetDevice());
        }
        m_impl->Resources.StagingBuffers.clear();
    }

    /* IGltfBuilder implementations */
    std::shared_ptr<Material> VulkanResources::CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor, float metallicFactor,
                                                                  RGBColor emissiveFactor)
    {
        return VulkanMaterial::CreateFlat(*this, baseColorFactor, roughnessFactor, metallicFactor, emissiveFactor);
    }
    std::shared_ptr<Material> VulkanResources::CreateMaterial()
    {
        return std::make_shared<VulkanMaterial>(*this);
    }

    // Create a Vulkan texture from a tinygltf Image.
    static VulkanTextureBundle LoadGLTFImage(VulkanResources& pbrResources, const tinygltf::Image& image, bool sRGB)
    {
        // First convert the image to RGBA if it isn't already.
        std::vector<uint8_t> tempBuffer;
        Conformance::Image::Image decodedImage = GltfHelper::DecodeImage(image, sRGB, pbrResources.GetSupportedFormats(), tempBuffer);

        return VulkanTexture::CreateTexture(pbrResources, decodedImage);
    }

    static VkFilter ConvertMinFilter(int glMinFilter)
    {
        return glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST                  ? VK_FILTER_NEAREST
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR                 ? VK_FILTER_LINEAR
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ? VK_FILTER_NEAREST
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  ? VK_FILTER_LINEAR
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  ? VK_FILTER_NEAREST
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   ? VK_FILTER_LINEAR
                                                                               : VK_FILTER_NEAREST;
    }

    static VkSamplerMipmapMode ConvertMipFilter(int glMinFilter)
    {
        return glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST                  ? VK_SAMPLER_MIPMAP_MODE_NEAREST
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR                 ? VK_SAMPLER_MIPMAP_MODE_NEAREST
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  ? VK_SAMPLER_MIPMAP_MODE_NEAREST
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  ? VK_SAMPLER_MIPMAP_MODE_LINEAR
               : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                                                               : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    static VkFilter ConvertMagFilter(int glMagFilter)
    {
        return glMagFilter == TINYGLTF_TEXTURE_FILTER_NEAREST  ? VK_FILTER_NEAREST
               : glMagFilter == TINYGLTF_TEXTURE_FILTER_LINEAR ? VK_FILTER_LINEAR
                                                               : VK_FILTER_NEAREST;
    }

    /// Create a Vulkan sampler from a tinygltf Sampler.
    static VkSampler CreateGLTFSampler(VkDevice device, const tinygltf::Sampler& sampler)
    {
        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

        info.minFilter = ConvertMinFilter(sampler.minFilter);
        info.mipmapMode = ConvertMipFilter(sampler.minFilter);
        info.magFilter = ConvertMagFilter(sampler.magFilter);
        info.addressModeU = sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE     ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                            : sampler.wrapS == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                                                                                     : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = sampler.wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE     ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                            : sampler.wrapT == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                                                                                     : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.maxAnisotropy = 1;
        info.compareOp = VK_COMPARE_OP_ALWAYS;
        info.minLod = 0;
        info.maxLod = VK_LOD_CLAMP_NONE;

        VkSampler destSampler;
        XRC_CHECK_THROW_VKCMD(vkCreateSampler(device, &info, nullptr, &destSampler));
        return destSampler;
    }

    void VulkanResources::LoadTexture(const std::shared_ptr<Material>& material, Pbr::ShaderSlots::PSMaterial slot,
                                      const tinygltf::Image* image, const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA)
    {
        auto pbrMaterial = std::dynamic_pointer_cast<VulkanMaterial>(material);
        if (!pbrMaterial) {
            throw std::logic_error("Wrong type of material");
        }
        // Find or load the image referenced by the texture.
        const ImageKey imageKey = std::make_tuple(image, sRGB);
        std::shared_ptr<VulkanTextureBundle> textureView =
            image != nullptr ? m_impl->loaderResources.imageMap[imageKey] : CreateTypedSolidColorTexture(defaultRGBA, sRGB);
        if (!textureView)  // If not cached, load the image and store it in the texture cache.
        {
            // TODO: Generate mipmaps if sampler's minification filter (minFilter) uses mipmapping.
            // TODO: If texture is not power-of-two and (sampler has wrapping=repeat/mirrored_repeat OR minFilter uses
            // mipmapping), resize to power-of-two.
            textureView = std::make_shared<VulkanTextureBundle>(LoadGLTFImage(*this, *image, sRGB));
            m_impl->loaderResources.imageMap[imageKey] = textureView;
        }

        // Find or create the sampler referenced by the texture.
        std::shared_ptr<Conformance::ScopedVkSampler> vkSampler = m_impl->loaderResources.samplerMap[sampler];
        if (!vkSampler)  // If not cached, create the sampler and store it in the sampler cache.
        {

            VkSampler raw = sampler != nullptr ? CreateGLTFSampler(GetDevice(), *sampler) : VulkanTexture::CreateSampler(GetDevice());
            vkSampler = std::make_shared<Conformance::ScopedVkSampler>(Conformance::ScopedVkSampler(raw, GetDevice()));
            m_impl->loaderResources.samplerMap[sampler] = vkSampler;
        }

        pbrMaterial->SetTexture(slot, textureView, vkSampler);
    }

    void VulkanResources::DropLoaderCaches()
    {
        m_impl->loaderResources = {};
    }

    void VulkanResources::SetBrdfLut(std::shared_ptr<VulkanTextureBundle> brdfLut)
    {
        m_impl->Resources.BrdfLut = std::move(brdfLut);
    }

    std::unique_ptr<VulkanWriteDescriptorSets> VulkanResources::BuildWriteDescriptorSets(
        VkDescriptorBufferInfo modelConstantBuffer, VkDescriptorBufferInfo materialConstantBuffer, VkDescriptorBufferInfo transformBuffer,
        span<VkDescriptorImageInfo> materialCombinedImageSamplers, VkDescriptorSet dstSet)
    {
        PipelineLayout::VulkanWriteDescriptorSetsBuilder builder(m_impl->VulkanLayout, dstSet);

        // SceneConstantBuffer
        VkDescriptorBufferInfo sceneConstantBuffer[] = {
            m_impl->Resources.SceneBuffer.MakeDescriptor(),
        };
        builder.BindBuffers(PipelineLayout::SceneConstantBuffer, sceneConstantBuffer);

        // ModelConstantBuffer
        builder.BindBuffers(PipelineLayout::ModelConstantBuffer, {&modelConstantBuffer, 1});

        // MaterialConstantBuffer
        builder.BindBuffers(PipelineLayout::MaterialConstantBuffer, {&materialConstantBuffer, 1});

        // TransformsBuffer
        builder.BindBuffers(PipelineLayout::TransformsBuffer, {&transformBuffer, 1});

        // MaterialTextures
        builder.BindImages(PipelineLayout::MaterialTextures, materialCombinedImageSamplers);

        // GlobalTextures
        VkDescriptorImageInfo globalTextures[] = {
            {m_impl->Resources.BrdfSampler.get(), m_impl->Resources.BrdfLut->view.get(),  //
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {m_impl->Resources.EnvironmentMapSampler.get(), m_impl->Resources.DiffuseEnvironmentMap->view.get(),  //
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {m_impl->Resources.EnvironmentMapSampler.get(), m_impl->Resources.SpecularEnvironmentMap->view.get(),  //
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        };
        builder.BindImages(PipelineLayout::GlobalTextures, globalTextures);

        return builder.Build();
    }

    Conformance::Pipeline& VulkanResources::GetOrCreatePipeline(VkRenderPass renderPass, VkSampleCountFlagBits sampleCount,
                                                                BlendState blendState, DoubleSided doubleSided)
    {
        return m_impl->Resources.Pipelines->GetOrCreatePipeline(renderPass, sampleCount, m_sharedState.GetFillMode(),
                                                                m_sharedState.GetFrontFaceWindingOrder(), blendState, doubleSided,
                                                                m_sharedState.GetDepthDirection());
    }

    void VulkanResources::SetLight(XrVector3f direction, RGBColor diffuseColor)
    {
        m_impl->SceneBuffer.LightDirection = direction;
        m_impl->SceneBuffer.LightDiffuseColor = diffuseColor;
    }

    void VulkanResources::SetViewProjection(XrMatrix4x4f view, XrMatrix4x4f projection) const
    {
        m_impl->SceneBuffer.ViewProjection = projection * view;

        XrMatrix4x4f inv = Matrix::InvertRigidBody(view);
        m_impl->SceneBuffer.EyePosition = {inv.m[12], inv.m[13], inv.m[14]};
    }

    void VulkanResources::SetEnvironmentMap(std::shared_ptr<VulkanTextureBundle> specularEnvironmentMap,
                                            std::shared_ptr<VulkanTextureBundle> diffuseEnvironmentMap)
    {
        m_impl->SceneBuffer.NumSpecularMipLevels = specularEnvironmentMap->mipLevels;
        m_impl->Resources.SpecularEnvironmentMap = std::move(specularEnvironmentMap);
        m_impl->Resources.DiffuseEnvironmentMap = std::move(diffuseEnvironmentMap);
    }

    std::shared_ptr<VulkanTextureBundle> VulkanResources::CreateTypedSolidColorTexture(RGBAColor color, bool sRGB)
    {
        return m_impl->Resources.SolidColorTextureCache.CreateTypedSolidColorTexture(*this, color, sRGB);
    }

    span<const Conformance::Image::FormatParams> VulkanResources::GetSupportedFormats() const
    {
        if (m_impl->Resources.SupportedTextureFormats.size() == 0) {
            throw std::logic_error("SupportedTextureFormats empty or not yet populated");
        }
        return m_impl->Resources.SupportedTextureFormats;
    }

    void VulkanResources::UpdateBuffer() const
    {
        m_impl->Resources.SceneBuffer.Update({&m_impl->SceneBuffer, 1});
    }

    PrimitiveHandle VulkanResources::MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                                   const std::shared_ptr<Pbr::Material>& material)
    {
        auto typedMaterial = std::dynamic_pointer_cast<Pbr::VulkanMaterial>(material);
        if (!typedMaterial) {
            throw std::logic_error("Got the wrong type of material");
        }
        return m_impl->Primitives.emplace_back(*this, primitiveBuilder, typedMaterial);
    }

    VulkanPrimitive& VulkanResources::GetPrimitive(PrimitiveHandle p)
    {
        return m_impl->Primitives[p];
    }

    const VulkanPrimitive& VulkanResources::GetPrimitive(PrimitiveHandle p) const
    {
        return m_impl->Primitives[p];
    }

    void VulkanResources::SetFillMode(FillMode mode)
    {
        m_sharedState.SetFillMode(mode);
    }

    FillMode VulkanResources::GetFillMode() const
    {
        return m_sharedState.GetFillMode();
    }

    void VulkanResources::SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder)
    {
        m_sharedState.SetFrontFaceWindingOrder(windingOrder);
    }

    FrontFaceWindingOrder VulkanResources::GetFrontFaceWindingOrder() const
    {
        return m_sharedState.GetFrontFaceWindingOrder();
    }

    void VulkanResources::SetDepthDirection(DepthDirection depthDirection)
    {
        m_sharedState.SetDepthDirection(depthDirection);
    }

    VkDevice VulkanResources::GetDevice() const
    {
        return m_impl->device;
    }

    const Conformance::MemoryAllocator& VulkanResources::GetMemoryAllocator() const
    {
        return m_impl->allocator;
    }

    const Conformance::CmdBuffer& VulkanResources::GetCopyCommandBuffer() const
    {
        return m_impl->copyCmdBuffer;
    }

    VkPipelineLayout VulkanResources::GetPipelineLayout() const
    {
        return m_impl->Resources.PipelineLayout->get();
    }

    void VulkanResources::SubmitFrameResources(VkQueue queue) const
    {
        m_impl->copyCmdBuffer.End();
        m_impl->copyCmdBuffer.Exec(queue);
    }

    void VulkanResources::Wait() const
    {
        m_impl->copyCmdBuffer.Wait();
        m_impl->copyCmdBuffer.Clear();
        m_impl->copyCmdBuffer.Begin();

        for (auto stagingBuffer : m_impl->Resources.StagingBuffers) {
            stagingBuffer.Reset(GetDevice());
        }
        m_impl->Resources.StagingBuffers.clear();
    }

    const VulkanDebugObjectNamer& VulkanResources::GetDebugNamer() const
    {
        return m_impl->namer;
    }

    VkDescriptorSetLayout VulkanResources::GetDescriptorSetLayout() const
    {
        return m_impl->Resources.DescriptorSetLayout->get();
    }

    VkDescriptorPool VulkanResources::MakeDescriptorPool(uint32_t maxSets) const
    {
        return m_impl->VulkanLayout.CreateDescriptorPool(GetDevice(), maxSets);
    }

    void VulkanResources::DestroyAfterRender(Conformance::BufferAndMemory buffer) const
    {
        m_impl->Resources.StagingBuffers.push_back(buffer);
    }

}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
