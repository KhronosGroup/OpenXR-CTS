// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "VkResources.h"
#include "VkTexture.h"

#include "../PbrCommon.h"
#include "../PbrMaterial.h"
#include "../PbrSharedState.h"

#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

#include <vulkan/vulkan_core.h>

#include <array>
#include <memory>
#include <stddef.h>
#include <string>
#include <vector>

namespace Pbr
{
    struct VulkanResources;
    struct VulkanTextureBundle;

    /// A VulkanMaterial contains the metallic roughness parameters and textures.
    /// Primitives specify which VulkanMaterial to use when being rendered.
    struct VulkanMaterial final : public Material
    {
        /// Create a uninitialized material. Textures and shader coefficients must be set.
        VulkanMaterial(Pbr::VulkanResources const& pbrResources);
        ~VulkanMaterial() override = default;

        /// Create a clone of this material. Shares the texture and sampler heap with this material.
        std::shared_ptr<VulkanMaterial> Clone(Pbr::VulkanResources const& pbrResources) const;

        /// Create a flat (no texture) material.
        static std::shared_ptr<VulkanMaterial> CreateFlat(VulkanResources& pbrResources, RGBAColor baseColorFactor,
                                                          float roughnessFactor = 1.0f, float metallicFactor = 0.0f,
                                                          RGBColor emissiveFactor = RGB::Black);

        /// Set a Metallic-Roughness texture.
        void SetTexture(ShaderSlots::PSMaterial slot, std::shared_ptr<VulkanTextureBundle> textureView,
                        std::shared_ptr<Conformance::ScopedVkSampler> sampler);

        /// Get the material constant buffer for binding
        VkDescriptorBufferInfo GetMaterialConstantBuffer();

        /// Get the combined image sampler descriptors for binding
        std::vector<VkDescriptorImageInfo> GetTextureDescriptors();

        /// Update the material constant buffer
        void UpdateBuffer();

        std::string Name;
        bool Hidden{false};

    private:
        static constexpr size_t TextureCount = ShaderSlots::NumMaterialSlots;
        std::array<std::shared_ptr<VulkanTextureBundle>, TextureCount> m_textures;
        std::array<std::shared_ptr<Conformance::ScopedVkSampler>, TextureCount> m_samplers;
        Conformance::StructuredBuffer<ConstantBufferData> m_constantBuffer;
    };
}  // namespace Pbr
