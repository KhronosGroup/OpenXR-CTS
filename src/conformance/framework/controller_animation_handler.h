
// Copyright 2023-2025 The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "pbr/PbrCommon.h"

#include <openxr/openxr.h>

#include <vector>
#include <memory>

namespace Pbr
{
    class Model;
    class ModelInstance;
}  // namespace Pbr

namespace Conformance
{
    /// For XR_MSFT_controller_model
    class ControllerAnimationHandler
    {
    public:
        ControllerAnimationHandler() = default;
        ControllerAnimationHandler(const Pbr::Model& model, std::vector<XrControllerModelNodePropertiesMSFT>&& properties);

        void Init(const Pbr::Model& model, std::vector<XrControllerModelNodePropertiesMSFT>&& properties);
        void UpdateControllerParts(const std::vector<XrControllerModelNodeStateMSFT>& nodeStates, Pbr::ModelInstance& pbrModelInstance);

    private:
        static Pbr::NodeIndex_t FindPbrNodeIndex(const Pbr::Model& model, const char* parentNodeName, const char* nodeName);
        std::vector<Pbr::NodeIndex_t> m_nodeIndices;
        std::vector<XrControllerModelNodePropertiesMSFT> m_nodeProperties;
        std::vector<XrControllerModelNodeStateMSFT> m_nodeStates;
    };

    /// For XR_EXT_render_model
    /// Handles display and animation of a single render model.
    class RenderModelAnimationHandler
    {
    public:
        RenderModelAnimationHandler() = default;
        RenderModelAnimationHandler(std::shared_ptr<Pbr::Model> model, std::vector<XrRenderModelAssetNodePropertiesEXT> nodeProperties);
        void UpdateNodes(std::vector<XrRenderModelNodeStateEXT>&& nodeStates, Pbr::ModelInstance& pbrModelInstance);
        size_t GetNumberOfAnimatableNodes() const
        {
            return m_nodeIndices.size();
        }

    private:
        std::shared_ptr<Pbr::Model> m_pbrModel;
        std::vector<Pbr::NodeIndex_t> m_nodeIndices;
        std::vector<XrRenderModelAssetNodePropertiesEXT> m_nodeProperties;
        std::vector<XrRenderModelNodeStateEXT> m_nodeStates;
    };
}  // namespace Conformance
