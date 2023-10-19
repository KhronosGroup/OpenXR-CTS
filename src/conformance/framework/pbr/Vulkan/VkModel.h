// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once
#include "VkResources.h"

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

    class VulkanModel final : public Model
    {
    public:
        // Render the model.
        void Render(Pbr::VulkanResources& pbrResources, Conformance::CmdBuffer& directCommandBuffer, VkRenderPass renderPass,
                    VkSampleCountFlagBits sampleCount);

    private:
        void AllocateDescriptorSets(Pbr::VulkanResources& pbrResources, uint32_t numSets);
        // Updated the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::VulkanResources& pbrResources);

        // Temporary buffer holds the world transforms, computed from the node's local transforms.
        mutable std::vector<XrMatrix4x4f> m_modelTransforms;
        mutable Conformance::StructuredBuffer<XrMatrix4x4f> m_modelTransformsStructuredBuffer;
        // mutable Microsoft::WRL::ComPtr<IVulkanDescriptorHeap> m_modelTransformsResourceViewHeap;
        Conformance::ScopedVkDescriptorPool m_descriptorPool;
        std::vector<VkDescriptorSet> m_descriptorSets;

        mutable uint32_t TotalModifyCount{0};
    };
}  // namespace Pbr
