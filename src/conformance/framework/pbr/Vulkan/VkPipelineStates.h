// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "../PbrSharedState.h"

#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

#include <nonstd/span.hpp>
#include <vulkan/vulkan_core.h>

#include <map>
#include <memory>
#include <stdint.h>
#include <tuple>

namespace Pbr
{
    using nonstd::span;

    /// A factory/cache for pipeline state objects that differ in a few dimensions.
    class VulkanPipelines
    {
    public:
        /// Note: Make sure your shaders are global/static!
        VulkanPipelines(VkDevice device, std::shared_ptr<Conformance::ScopedVkPipelineLayout> layout,
                        span<const VkVertexInputAttributeDescription> vertexAttrDesc,
                        span<const VkVertexInputBindingDescription> vertexInputBindDesc, span<const uint32_t> pbrVS,
                        span<const uint32_t> pbrPS)
            : m_device(device), m_layout(layout), m_vertexAttrDesc(vertexAttrDesc), m_vertexInputBindDesc(vertexInputBindDesc)
        {
            m_pbrShader.Init(m_device);
            m_pbrShader.LoadVertexShader(pbrVS);
            m_pbrShader.LoadFragmentShader(pbrPS);
        }

        Conformance::Pipeline& GetOrCreatePipeline(VkRenderPass renderPass, VkSampleCountFlagBits sampleCount, FillMode fillMode,
                                                   FrontFaceWindingOrder frontFaceWindingOrder, BlendState blendState,
                                                   DoubleSided doubleSided, DepthDirection depthDirection);

        void DropStates()
        {
            m_pipelines.clear();
        }

    private:
        VkDevice m_device;
        using PipelineStateKey =
            std::tuple<VkRenderPass, VkSampleCountFlagBits, FillMode, FrontFaceWindingOrder, BlendState, DoubleSided, DepthDirection>;
        std::shared_ptr<Conformance::ScopedVkPipelineLayout> m_layout;
        span<const VkVertexInputAttributeDescription> m_vertexAttrDesc;
        span<const VkVertexInputBindingDescription> m_vertexInputBindDesc;
        Conformance::ShaderProgram m_pbrShader;

        std::map<PipelineStateKey, Conformance::Pipeline> m_pipelines;
    };
}  // namespace Pbr
