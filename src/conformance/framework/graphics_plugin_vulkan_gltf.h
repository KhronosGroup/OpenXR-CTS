// Copyright (c) 2022-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#pragma once
#ifdef XR_USE_GRAPHICS_API_VULKAN
#include "gltf.h"
#include "gltf_model.h"

#include "common/xr_linear.h"
#include "gltf/GltfHelper.h"
#include "pbr/PbrSharedState.h"
#include "pbr/Vulkan/VkModel.h"
#include "pbr/Vulkan/VkResources.h"

#include <vulkan/vulkan_core.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Pbr
{
    class VulkanModel;
    struct VulkanResources;
}  // namespace Pbr

namespace Conformance
{
    struct CmdBuffer;

    class VulkanGLTF : public GltfModelBase<Pbr::VulkanModel, Pbr::VulkanResources>
    {
    public:
        using GltfModelBase::GltfModelBase;

        void Render(CmdBuffer& directCommandBuffer, Pbr::VulkanResources& resources, XrMatrix4x4f& modelToWorld, VkRenderPass renderPass,
                    VkSampleCountFlagBits sampleCount) const;
    };
}  // namespace Conformance
#endif
