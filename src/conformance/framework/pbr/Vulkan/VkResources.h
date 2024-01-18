// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include "VkCommon.h"

#include "../IResources.h"
#include "../PbrCommon.h"
#include "../PbrHandles.h"
#include "../PbrSharedState.h"

#include "common/vulkan_debug_object_namer.hpp"
#include "common/xr_linear.h"
#include "utilities/vulkan_utils.h"

#include <nonstd/span.hpp>
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>

#include <array>
#include <chrono>
#include <memory>
#include <stddef.h>
#include <stdint.h>

class VulkanDebugObjectNamer;

namespace Conformance
{
    struct BufferAndMemory;
    struct CmdBuffer;
    struct MemoryAllocator;
    struct Pipeline;
}  // namespace Conformance
namespace tinygltf
{
    struct Image;
    struct Sampler;
}  // namespace tinygltf

namespace Pbr
{
    struct Primitive;
    struct Material;
    struct VulkanTextureBundle;

    using Duration = std::chrono::high_resolution_clock::duration;
    struct VulkanPrimitive;
    struct VulkanMaterial;

    struct VulkanTextureAndSampler : public ITexture
    {
        ~VulkanTextureAndSampler() override = default;

        bool samplerSet;
    };

    static constexpr size_t BindingCount = ShaderSlots::NumConstantBuffers + ShaderSlots::NumVSResourceViews + ShaderSlots::NumTextures;

    class VulkanWriteDescriptorSets
    {
    public:
        VulkanWriteDescriptorSets() = default;
        std::array<VkWriteDescriptorSet, BindingCount> writeDescriptorSets{};

    private:
        std::array<VkDescriptorBufferInfo, BindingCount> m_bufferInfos{};
        std::array<VkDescriptorImageInfo, BindingCount> m_imageInfos{};

    public:
        void BindBuffer(size_t bindingIndex, const VkDescriptorBufferInfo& bufferInfo)
        {
            m_bufferInfos[bindingIndex] = bufferInfo;
            writeDescriptorSets[bindingIndex].pBufferInfo = &m_bufferInfos[bindingIndex];
        }

        void BindImage(size_t bindingIndex, const VkDescriptorImageInfo& imageInfo)
        {
            m_imageInfos[bindingIndex] = imageInfo;
            writeDescriptorSets[bindingIndex].pImageInfo = &m_imageInfos[bindingIndex];
        }

        // self-referential
        VulkanWriteDescriptorSets(VulkanWriteDescriptorSets const&) = delete;
        VulkanWriteDescriptorSets(VulkanWriteDescriptorSets&&) = delete;
        VulkanWriteDescriptorSets& operator=(VulkanWriteDescriptorSets const&) = delete;
        VulkanWriteDescriptorSets& operator=(VulkanWriteDescriptorSets&&) = delete;
    };

    /// Global PBR resources required for rendering a scene.
    struct VulkanResources final : public IResources
    {
        VulkanResources(const VulkanDebugObjectNamer& namer, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex);
        VulkanResources(VulkanResources&&) noexcept;

        ~VulkanResources() override;

        std::shared_ptr<Material> CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor = 1.0f, float metallicFactor = 0.0f,
                                                     RGBColor emissiveFactor = RGB::Black) override;
        std::shared_ptr<Material> CreateMaterial() override;
        std::shared_ptr<ITexture> CreateSolidColorTexture(RGBAColor color);

        void LoadTexture(const std::shared_ptr<Material>& pbrMaterial, Pbr::ShaderSlots::PSMaterial slot, const tinygltf::Image* image,
                         const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA) override;
        PrimitiveHandle MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                      const std::shared_ptr<Pbr::Material>& material) override;
        void DropLoaderCaches() override;

        /// Sets the Bidirectional Reflectance Distribution Function Lookup Table texture, required by the shader to compute surface
        /// reflectance from the IBL.
        void SetBrdfLut(std::shared_ptr<VulkanTextureBundle> brdfLut);

        /// Get a pipeline state matching some parameters as well as the current settings inside VkResources
        Conformance::Pipeline& GetOrCreatePipeline(VkRenderPass renderPass, VkSampleCountFlagBits sampleCount, BlendState blendState,
                                                   DoubleSided doubleSided);

        /// Set the directional light.
        void SetLight(XrVector3f direction, RGBColor diffuseColor);

        /// Set the specular and diffuse image-based lighting (IBL) maps. ShaderResourceViews must be TextureCubes.
        void SetEnvironmentMap(std::shared_ptr<VulkanTextureBundle> specularEnvironmentMap,
                               std::shared_ptr<VulkanTextureBundle> diffuseEnvironmentMap);

        /// Set the current view and projection matrices.
        void SetViewProjection(XrMatrix4x4f view, XrMatrix4x4f projection) const;

        /// Many 1x1 pixel colored textures are used in the PBR system. This is used to create textures backed by a cache to reduce the
        /// number of textures created.
        std::shared_ptr<VulkanTextureBundle> CreateTypedSolidColorTexture(RGBAColor color);

        /// Update the scene buffer in GPU memory.
        void UpdateBuffer() const;

        /// Get the VulkanPrimitive from a primitive handle.
        VulkanPrimitive& GetPrimitive(PrimitiveHandle p);

        /// Get the VulkanPrimitive from a primitive handle, const overload.
        const VulkanPrimitive& GetPrimitive(PrimitiveHandle p) const;

        // Set or get the shading and fill modes.
        void SetFillMode(FillMode mode);
        FillMode GetFillMode() const;
        void SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder);
        FrontFaceWindingOrder GetFrontFaceWindingOrder() const;
        void SetDepthDirection(DepthDirection depthDirection);

        VkDevice GetDevice() const;
        const Conformance::MemoryAllocator& GetMemoryAllocator() const;
        const Conformance::CmdBuffer& GetCopyCommandBuffer() const;
        VkPipelineLayout GetPipelineLayout() const;
        void SubmitFrameResources(VkQueue queue) const;
        void Wait() const;
        const VulkanDebugObjectNamer& GetDebugNamer() const;
        VkDescriptorSetLayout GetDescriptorSetLayout() const;
        VkDescriptorPool MakeDescriptorPool(uint32_t maxSets) const;
        void DestroyAfterRender(Conformance::BufferAndMemory buffer) const;

    private:
        std::unique_ptr<VulkanWriteDescriptorSets>
        BuildWriteDescriptorSets(VkDescriptorBufferInfo modelConstantBuffer, VkDescriptorBufferInfo materialConstantBuffer,
                                 VkDescriptorBufferInfo transformBuffer, nonstd::span<VkDescriptorImageInfo> materialCombinedImageSamplers,
                                 VkDescriptorSet dstSet);
        friend struct VulkanMaterial;
        friend struct VulkanPrimitive;

        struct Impl;
        std::unique_ptr<Impl> m_impl;

        SharedState m_sharedState;
    };
}  // namespace Pbr
