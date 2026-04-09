// Copyright (c) 2022-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#pragma once
#ifdef XR_USE_GRAPHICS_API_VULKAN

#include "gltf_model.h"

#include "common/xr_linear.h"
#include "pbr/Vulkan/VkModel.h"

#include <vulkan/vulkan_core.h>

namespace Pbr
{
    struct VulkanResources;
}  // namespace Pbr

namespace Conformance
{
    struct CmdBuffer;

    class VulkanGLTF : public RenderableGltfModelInstanceBase<Pbr::VulkanModelInstance, Pbr::VulkanResources>
    {
    public:
        using RenderableGltfModelInstanceBase::RenderableGltfModelInstanceBase;

        void Render(CmdBuffer& directCommandBuffer, Pbr::VulkanResources& resources, VkRenderPass renderPass,
                    VkSampleCountFlagBits sampleCount);
    };
}  // namespace Conformance
#endif
