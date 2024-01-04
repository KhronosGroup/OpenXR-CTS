
// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "pbr/PbrCommon.h"
#include "pbr/PbrModel.h"

#include <openxr/openxr.h>

#include <memory>
#include <vector>

namespace Pbr
{
    class Model;
}  // namespace Pbr

namespace Conformance
{
    class ControllerAnimationHandler
    {
    public:
        ControllerAnimationHandler() = default;
        ControllerAnimationHandler(std::shared_ptr<Pbr::Model> model, std::vector<XrControllerModelNodePropertiesMSFT> properties);
        void UpdateControllerParts(std::vector<XrControllerModelNodeStateMSFT> nodeStates);

    private:
        std::shared_ptr<Pbr::Model> m_pbrModel;
        std::vector<Pbr::NodeIndex_t> m_nodeIndices;
        std::vector<XrControllerModelNodePropertiesMSFT> m_nodeProperties;
        std::vector<XrControllerModelNodeStateMSFT> m_nodeStates;
    };
}  // namespace Conformance
