// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "D3D12PipelineStates.h"

#include "../PbrCommon.h"

#include "utilities/throw_helpers.h"

#include <type_traits>

namespace Pbr
{
    Microsoft::WRL::ComPtr<ID3D12PipelineState> D3D12PipelineStates::GetOrCreatePipelineState(
        DXGI_FORMAT colorRenderTargetFormat, DXGI_FORMAT depthRenderTargetFormat, FillMode fillMode,
        FrontFaceWindingOrder frontFaceWindingOrder, BlendState blendState, DoubleSided doubleSided, DepthDirection depthDirection)
    {
        const PipelineStateKey state{
            colorRenderTargetFormat, depthRenderTargetFormat, fillMode, frontFaceWindingOrder, blendState, doubleSided, depthDirection};
        auto iter = m_pipelineStates.find(state);
        if (iter != m_pipelineStates.end()) {
            return iter->second;
        }
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        XRC_CHECK_THROW_HRCMD(
            m_rootSignature->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(device.ReleaseAndGetAddressOf())));

        static_assert(std::is_same<PipelineStateKey, std::tuple<DXGI_FORMAT, DXGI_FORMAT, FillMode, FrontFaceWindingOrder, BlendState,
                                                                DoubleSided, DepthDirection>>::value,
                      "This function copies all fields to the desc and must be updated if the fieldset is changed");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = m_basePipelineStateDesc;

        pipelineStateDesc.VS = {m_pbrVS.data(), m_pbrVS.size()};
        pipelineStateDesc.PS = {m_pbrPS.data(), m_pbrPS.size()};

        for (UINT i = 0; i < pipelineStateDesc.NumRenderTargets; ++i) {
            pipelineStateDesc.RTVFormats[i] = colorRenderTargetFormat;
        }

        pipelineStateDesc.DSVFormat = depthRenderTargetFormat;

        pipelineStateDesc.RasterizerState.CullMode =
            (doubleSided == DoubleSided::DoubleSided) ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;

        pipelineStateDesc.RasterizerState.FillMode = (fillMode == FillMode::Wireframe) ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;

        pipelineStateDesc.RasterizerState.FrontCounterClockwise = (frontFaceWindingOrder == FrontFaceWindingOrder::CounterClockWise);

        pipelineStateDesc.DepthStencilState.DepthFunc =
            (depthDirection == DepthDirection::Reversed) ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;

        if (blendState == BlendState::AlphaBlended) {
            D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc{};
            rtBlendDesc.BlendEnable = TRUE;
            rtBlendDesc.LogicOpEnable = FALSE;
            rtBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;

            rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
            rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
            rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
            rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;

            rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
                pipelineStateDesc.BlendState.RenderTarget[i] = rtBlendDesc;
            }
            pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        }
        else {
            // already set up by default
        }

        pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        XRC_CHECK_THROW_HRCMD(device->CreateGraphicsPipelineState(&pipelineStateDesc, __uuidof(ID3D12PipelineState),
                                                                  reinterpret_cast<void**>(pipelineState.ReleaseAndGetAddressOf())));

        m_pipelineStates.emplace(state, pipelineState);

        return pipelineState;
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_D3D12)
