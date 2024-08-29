// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalPipelineStates.h"
#include "MetalResources.h"

#include "../PbrCommon.h"

#include "utilities/throw_helpers.h"

#include <type_traits>

namespace Pbr
{

    MetalPipelineStateBundle MetalPipelineStates::GetOrCreatePipelineState(const MetalResources& pbrResources,
                                                                           MTL::PixelFormat colorRenderTargetFormat,
                                                                           MTL::PixelFormat depthRenderTargetFormat, BlendState blendState,
                                                                           DepthDirection depthDirection)
    {
        const PipelineStateKey state{colorRenderTargetFormat, depthRenderTargetFormat, blendState, depthDirection};
        auto iter = m_pipelineStates.find(state);
        if (iter != m_pipelineStates.end()) {
            return iter->second;
        }
        auto renderingPipelineDesc = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
        auto depthStencilDesc = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());

        renderingPipelineDesc->setVertexFunction(m_vertexFunction.get());
        renderingPipelineDesc->setFragmentFunction(m_fragmentFunction.get());
        renderingPipelineDesc->setVertexDescriptor(m_vertexDescriptor.get());

        renderingPipelineDesc->colorAttachments()->object(0)->setPixelFormat(colorRenderTargetFormat);
        renderingPipelineDesc->setDepthAttachmentPixelFormat(depthRenderTargetFormat);

        bool enableBlending = blendState == BlendState::AlphaBlended;
        renderingPipelineDesc->colorAttachments()->object(0)->setBlendingEnabled(enableBlending);
        if (enableBlending) {
            renderingPipelineDesc->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
            renderingPipelineDesc->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
            renderingPipelineDesc->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
            renderingPipelineDesc->colorAttachments()->object(0)->setSourceAlphaBlendFactor(MTL::BlendFactorZero);
            renderingPipelineDesc->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(MTL::BlendFactorZero);
            renderingPipelineDesc->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
            renderingPipelineDesc->colorAttachments()->object(0)->setWriteMask(MTL::ColorWriteMaskAll);

            // disable depth writing if alpha blending is enabled
            depthStencilDesc->setDepthWriteEnabled(false);
        }
        else {
            renderingPipelineDesc->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorOne);
            renderingPipelineDesc->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorZero);
            renderingPipelineDesc->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
            renderingPipelineDesc->colorAttachments()->object(0)->setSourceAlphaBlendFactor(MTL::BlendFactorZero);
            renderingPipelineDesc->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(MTL::BlendFactorZero);
            renderingPipelineDesc->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
            renderingPipelineDesc->colorAttachments()->object(0)->setWriteMask(MTL::ColorWriteMaskAll);

            depthStencilDesc->setDepthWriteEnabled(true);
        }

        MTL::CompareFunction depthCompareFunction =
            (depthDirection == DepthDirection::Forward) ? MTL::CompareFunctionLess : MTL::CompareFunctionGreater;
        depthStencilDesc->setDepthCompareFunction(depthCompareFunction);

        MetalPipelineStateBundle bundle;
        NS::Error* pError = nullptr;
        bundle.m_renderPipelineState =
            NS::TransferPtr(pbrResources.GetDevice()->newRenderPipelineState(renderingPipelineDesc.get(), &pError));
        XRC_CHECK_THROW(bundle.m_renderPipelineState);
        bundle.m_depthStencilState = NS::TransferPtr(pbrResources.GetDevice()->newDepthStencilState(depthStencilDesc.get()));
        m_pipelineStates.emplace(state, bundle);
        return bundle;
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_METAL)
