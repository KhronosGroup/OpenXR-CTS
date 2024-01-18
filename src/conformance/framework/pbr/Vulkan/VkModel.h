// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once
#include "VkResources.h"

#include "../GlslBuffers.h"
#include "../PbrHandles.h"
#include "../PbrModel.h"

#include "common/xr_linear.h"
#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

#include <vulkan/vulkan_core.h>

#include <stdint.h>
#include <vector>

namespace Pbr
{
    struct VulkanPrimitive;
    struct VulkanResources;

    class VulkanModelInstance final : public ModelInstance
    {
    public:
        VulkanModelInstance(Pbr::VulkanResources& pbrResources, std::shared_ptr<const Model> model);

        /// Render the model.
        void Render(Pbr::VulkanResources& pbrResources, Conformance::CmdBuffer& directCommandBuffer, VkRenderPass renderPass,
                    VkSampleCountFlagBits sampleCount, XrMatrix4x4f modelToWorld);

    private:
        void AllocateDescriptorSets(Pbr::VulkanResources& pbrResources, uint32_t numSets);
        /// Update the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::VulkanResources& pbrResources);

        Glsl::ModelConstantBuffer m_modelBuffer;
        Conformance::StructuredBuffer<Glsl::ModelConstantBuffer> m_modelConstantBuffer;

        Conformance::StructuredBuffer<XrMatrix4x4f> m_modelTransformsStructuredBuffer;
        Conformance::ScopedVkDescriptorPool m_descriptorPool;
        std::vector<VkDescriptorSet> m_descriptorSets;
    };
}  // namespace Pbr
