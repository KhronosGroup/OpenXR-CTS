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

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <map>
#include <tuple>
#include <utility>

namespace Pbr
{

    struct MetalResources;

    struct MetalPipelineStateBundle
    {
        NS::SharedPtr<MTL::RenderPipelineState> m_renderPipelineState;
        NS::SharedPtr<MTL::DepthStencilState> m_depthStencilState;
    };

    /// A factory/cache for pipeline state objects that differ in a few dimensions.
    class MetalPipelineStates
    {
    public:
        /// Note: Make sure your shaders are global/static!
        MetalPipelineStates(MTL::Function* vertexFunction, MTL::Function* fragmentFunction, MTL::VertexDescriptor* vertexDescriptor)
            : m_vertexFunction(NS::RetainPtr(vertexFunction))
            , m_fragmentFunction(NS::RetainPtr(fragmentFunction))
            , m_vertexDescriptor(NS::RetainPtr(vertexDescriptor))
        {
        }

        MetalPipelineStateBundle GetOrCreatePipelineState(const MetalResources& pbrResources, MTL::PixelFormat colorRenderTargetFormat,
                                                          MTL::PixelFormat depthRenderTargetFormat, BlendState blendState,
                                                          DepthDirection depthDirection);

    private:
        using PipelineStateKey = std::tuple<MTL::PixelFormat, MTL::PixelFormat, BlendState, DepthDirection>;
        NS::SharedPtr<MTL::Function> m_vertexFunction;
        NS::SharedPtr<MTL::Function> m_fragmentFunction;
        NS::SharedPtr<MTL::VertexDescriptor> m_vertexDescriptor;

        std::map<PipelineStateKey, MetalPipelineStateBundle> m_pipelineStates;
    };
}  // namespace Pbr
