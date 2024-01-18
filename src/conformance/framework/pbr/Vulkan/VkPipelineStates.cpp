// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_VULKAN)

#include "VkPipelineStates.h"

#include "../PbrSharedState.h"

#include "utilities/array_size.h"
#include "utilities/vulkan_scoped_handle.h"
#include "utilities/vulkan_utils.h"

#include <nonstd/span.hpp>

#include <array>
#include <type_traits>
#include <utility>

namespace Pbr
{
    Conformance::Pipeline& VulkanPipelines::GetOrCreatePipeline(VkRenderPass renderPass, VkSampleCountFlagBits sampleCount,
                                                                FillMode fillMode, FrontFaceWindingOrder frontFaceWindingOrder,
                                                                BlendState blendState, DoubleSided doubleSided,
                                                                DepthDirection depthDirection)
    {
        const PipelineStateKey state{renderPass, sampleCount, fillMode, frontFaceWindingOrder, blendState, doubleSided, depthDirection};
        auto iter = m_pipelines.find(state);
        if (iter != m_pipelines.end()) {
            return iter->second;
        }

        static_assert(std::is_same<PipelineStateKey, std::tuple<VkRenderPass, VkSampleCountFlagBits, FillMode, FrontFaceWindingOrder,
                                                                BlendState, DoubleSided, DepthDirection>>::value,
                      "This function copies all fields to the desc and must be updated if the fieldset is changed");

        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};
        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = (uint32_t)Conformance::ArraySize(dynamicStates);
        dynamicState.pDynamicStates = dynamicStates;

        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount = (uint32_t)m_vertexInputBindDesc.size();
        vi.pVertexBindingDescriptions = m_vertexInputBindDesc.data();
        vi.vertexAttributeDescriptionCount = (uint32_t)m_vertexAttrDesc.size();
        vi.pVertexAttributeDescriptions = m_vertexAttrDesc.data();

        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.primitiveRestartEnable = VK_FALSE;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = (fillMode == FillMode::Wireframe) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        rs.cullMode = (doubleSided == DoubleSided::DoubleSided) ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rs.frontFace =
            (frontFaceWindingOrder == FrontFaceWindingOrder::CounterClockWise) ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        rs.depthClampEnable = VK_FALSE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.depthBiasEnable = VK_FALSE;
        rs.depthBiasConstantFactor = 0;
        rs.depthBiasClamp = 0;
        rs.depthBiasSlopeFactor = 0;
        rs.lineWidth = 1.0f;

        VkPipelineColorBlendAttachmentState attachState{};

        if (blendState == BlendState::AlphaBlended) {
            attachState.blendEnable = 1;
            attachState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachState.colorBlendOp = VK_BLEND_OP_ADD;
            attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachState.alphaBlendOp = VK_BLEND_OP_ADD;
            attachState.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }
        else {
            attachState.blendEnable = 0;
            attachState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachState.colorBlendOp = VK_BLEND_OP_ADD;
            attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachState.alphaBlendOp = VK_BLEND_OP_ADD;
            attachState.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = 1;
        cb.pAttachments = &attachState;
        cb.logicOpEnable = VK_FALSE;
        cb.logicOp = VK_LOGIC_OP_NO_OP;
        cb.blendConstants[0] = 1.0f;
        cb.blendConstants[1] = 1.0f;
        cb.blendConstants[2] = 1.0f;
        cb.blendConstants[3] = 1.0f;

        // Use dynamic scissor and viewport
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount = 1;
        vp.scissorCount = 1;

        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = blendState != BlendState::AlphaBlended;
        ds.depthCompareOp = (depthDirection == DepthDirection::Reversed) ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable = VK_FALSE;
        ds.front.failOp = VK_STENCIL_OP_KEEP;
        ds.front.passOp = VK_STENCIL_OP_KEEP;
        ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
        ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
        ds.back = ds.front;
        ds.minDepthBounds = 0.0f;
        ds.maxDepthBounds = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = sampleCount;

        VkGraphicsPipelineCreateInfo pipeInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

        pipeInfo.stageCount = (uint32_t)m_pbrShader.shaderInfo.size();
        pipeInfo.pStages = m_pbrShader.shaderInfo.data();

        pipeInfo.pVertexInputState = &vi;
        pipeInfo.pInputAssemblyState = &ia;
        pipeInfo.pTessellationState = nullptr;
        pipeInfo.pViewportState = &vp;
        pipeInfo.pRasterizationState = &rs;
        pipeInfo.pMultisampleState = &ms;
        pipeInfo.pDepthStencilState = &ds;
        pipeInfo.pColorBlendState = &cb;
        if (dynamicState.dynamicStateCount > 0) {
            pipeInfo.pDynamicState = &dynamicState;
        }
        pipeInfo.layout = m_layout->get();
        pipeInfo.renderPass = renderPass;
        pipeInfo.subpass = 0;

        Conformance::Pipeline& pipeline = m_pipelines.emplace(state, Conformance::Pipeline()).first->second;
        pipeline.Create(m_device, pipeInfo);

        return pipeline;
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)
