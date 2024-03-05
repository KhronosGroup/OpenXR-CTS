// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>

namespace Conformance
{
    struct BodyInMotion
    {
        XrSpaceVelocity velocity;
        XrPosef pose;
        XrTime updateTime;
        XrTime createTime;

        // precondition: velocity.velocityFlags must have VALID linear and angular velocity
        void doSimulationStep(XrVector3f acceleration, XrTime predictedDisplayTime);
    };
}  // namespace Conformance
