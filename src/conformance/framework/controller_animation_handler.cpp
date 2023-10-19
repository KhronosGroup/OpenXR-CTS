
// Copyright 2023, The Khronos Group, Inc.
//
// Based in part on code that is:
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "controller_animation_handler.h"

#include "common/xr_linear.h"
#include "pbr/PbrCommon.h"
#include "pbr/PbrModel.h"

#include <openxr/openxr.h>

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <stddef.h>

using namespace std::literals::chrono_literals;

namespace Conformance
{
    ControllerAnimationHandler::ControllerAnimationHandler(std::shared_ptr<Pbr::Model> model,
                                                           std::vector<XrControllerModelNodePropertiesMSFT> properties)
        : m_pbrModel(model), m_nodeProperties(properties)

    {
        // Compute the index of each node reported by runtime to be animated.
        // The order of m_nodeIndices exactly matches the order of the nodes properties and states.
        m_nodeIndices.resize(m_nodeProperties.size(), Pbr::NodeIndex_npos);
        for (size_t i = 0; i < m_nodeProperties.size(); ++i) {
            const auto& nodeProperty = m_nodeProperties[i];
            Pbr::NodeIndex_t parentNodeIndex;
            if (m_pbrModel->FindFirstNode(&parentNodeIndex, nodeProperty.parentNodeName)) {
                Pbr::NodeIndex_t targetNodeIndex;
                if (m_pbrModel->FindFirstNode(&targetNodeIndex, nodeProperty.nodeName, &parentNodeIndex)) {
                    m_nodeIndices[i] = targetNodeIndex;
                }
            }
        }
    }

    // Update transforms of nodes for the animatable parts in the controller model
    void ControllerAnimationHandler::UpdateControllerParts(std::vector<XrControllerModelNodeStateMSFT> nodeStates)
    {
        m_nodeStates = nodeStates;

        assert(m_nodeStates.size() == m_nodeIndices.size());
        const size_t end = std::min(m_nodeStates.size(), m_nodeIndices.size());
        for (size_t i = 0; i < end; i++) {
            const Pbr::NodeIndex_t nodeIndex = m_nodeIndices[i];
            if (nodeIndex != Pbr::NodeIndex_npos) {
                Pbr::Node& node = m_pbrModel->GetNode(nodeIndex);
                XrMatrix4x4f nodeTransform;
                XrVector3f unitScale = {1, 1, 1};
                XrMatrix4x4f_CreateTranslationRotationScale(&nodeTransform, &m_nodeStates[i].nodePose.position,
                                                            &m_nodeStates[i].nodePose.orientation, &unitScale);
                node.SetTransform(nodeTransform);
            }
        }
    }
}  // namespace Conformance
