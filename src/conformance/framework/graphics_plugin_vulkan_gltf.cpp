// Copyright (c) 2022-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#ifdef XR_USE_GRAPHICS_API_VULKAN

#include "graphics_plugin_vulkan_gltf.h"

#include "common/xr_linear.h"
#include "pbr/Vulkan/VkModel.h"
#include "pbr/Vulkan/VkResources.h"

namespace Conformance
{
    struct CmdBuffer;

    void VulkanGLTF::Render(CmdBuffer& directCommandBuffer, Pbr::VulkanResources& resources, VkRenderPass renderPass,
                            VkSampleCountFlagBits sampleCount)
    {
        resources.SetFillMode(GetFillMode());
        GetModelInstance().Render(resources, directCommandBuffer, renderPass, sampleCount);
    }

}  // namespace Conformance
#endif
