// Copyright (c) 2022-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#ifdef XR_USE_GRAPHICS_API_VULKAN

#include "graphics_plugin_vulkan_gltf.h"

#include "conformance_framework.h"
#include "graphics_plugin_vulkan_gltf.h"
#include "report.h"

#include "pbr/GltfLoader.h"
#include "pbr/Vulkan/VkModel.h"
#include "pbr/Vulkan/VkPrimitive.h"
#include "pbr/Vulkan/VkResources.h"
#include "utilities/throw_helpers.h"

#include <memory>

namespace Conformance
{
    struct CmdBuffer;

    void VulkanGLTF::Render(CmdBuffer& directCommandBuffer, Pbr::VulkanResources& resources, XrMatrix4x4f& modelToWorld,
                            VkRenderPass renderPass, VkSampleCountFlagBits sampleCount) const
    {
        if (!GetModel()) {
            return;
        }

        resources.SetFillMode(GetFillMode());
        resources.SetModelToWorld(modelToWorld);
        // resources.Bind(directCommandBuffer);
        GetModel()->Render(resources, directCommandBuffer, renderPass, sampleCount);
    }

}  // namespace Conformance
#endif
