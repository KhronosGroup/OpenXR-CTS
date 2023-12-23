// Copyright 2023, The Khronos Group, Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once
#include "../PbrSharedState.h"

#include <d3d12.h>
#include <nonstd/span.hpp>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <map>
#include <tuple>
#include <utility>

namespace Pbr
{
    using nonstd::span;

    /// A factory/cache for pipeline state objects that differ in a few dimensions.
    class D3D12PipelineStates
    {
    public:
        /// Note: Make sure your shaders are global/static!
        D3D12PipelineStates(Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature,
                            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& basePipelineStateDesc,
                            span<const D3D12_INPUT_ELEMENT_DESC> inputLayout, span<const unsigned char> pbrVS,
                            span<const unsigned char> pbrPS)
            : m_rootSignature(std::move(rootSignature))
            , m_basePipelineStateDesc(basePipelineStateDesc)
            , m_inputLayout(inputLayout)
            , m_pbrVS(pbrVS)
            , m_pbrPS(pbrPS)
        {
            m_basePipelineStateDesc.pRootSignature = m_rootSignature.Get();

            m_basePipelineStateDesc.InputLayout.pInputElementDescs = m_inputLayout.data();
            m_basePipelineStateDesc.InputLayout.NumElements = (UINT)m_inputLayout.size();
        }

        Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreatePipelineState(DXGI_FORMAT colorRenderTargetFormat,
                                                                             DXGI_FORMAT depthRenderTargetFormat, FillMode fillMode,
                                                                             FrontFaceWindingOrder frontFaceWindingOrder,
                                                                             BlendState blendState, DoubleSided doubleSided,
                                                                             DepthDirection depthDirection);

        // void DropStates()
        // {
        //     m_pipelineStates.clear();
        // }
        // void Reset(Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr)
        // {
        //     DropStates();
        //     m_rootSignature = std::move(rootSignature);
        //     m_basePipelineStateDesc.pRootSignature = m_rootSignature.Get();
        // }

    private:
        using PipelineStateKey =
            std::tuple<DXGI_FORMAT, DXGI_FORMAT, FillMode, FrontFaceWindingOrder, BlendState, DoubleSided, DepthDirection>;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC m_basePipelineStateDesc;
        span<const D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
        span<const unsigned char> m_pbrVS;
        span<const unsigned char> m_pbrPS;

        std::map<PipelineStateKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_pipelineStates;
    };
}  // namespace Pbr
