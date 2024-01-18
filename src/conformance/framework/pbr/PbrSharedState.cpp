// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "PbrSharedState.h"

namespace Pbr
{
    void SharedState::SetFillMode(FillMode mode)
    {
        m_fill = mode;
    }

    FillMode SharedState::GetFillMode() const
    {
        return m_fill;
    }

    void SharedState::SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder)
    {
        m_windingOrder = windingOrder;
    }

    FrontFaceWindingOrder SharedState::GetFrontFaceWindingOrder() const
    {
        return m_windingOrder;
    }

    void SharedState::SetDepthDirection(DepthDirection depthDirection)
    {
        m_depthDirection = depthDirection;
    }

    DepthDirection SharedState::GetDepthDirection() const
    {
        return m_depthDirection;
    }
}  // namespace Pbr
