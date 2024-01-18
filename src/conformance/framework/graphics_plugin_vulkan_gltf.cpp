// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#ifdef XR_USE_GRAPHICS_API_VULKAN

#include "graphics_plugin_vulkan_gltf.h"

#include "pbr/Vulkan/VkModel.h"
#include "pbr/Vulkan/VkPrimitive.h"
#include "pbr/Vulkan/VkResources.h"

namespace Conformance
{
    struct CmdBuffer;

    void VulkanGLTF::Render(CmdBuffer& directCommandBuffer, Pbr::VulkanResources& resources, const XrMatrix4x4f& modelToWorld,
                            VkRenderPass renderPass, VkSampleCountFlagBits sampleCount)
    {
        resources.SetFillMode(GetFillMode());
        GetModelInstance().Render(resources, directCommandBuffer, renderPass, sampleCount, modelToWorld);
    }

}  // namespace Conformance
#endif
