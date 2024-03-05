// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "ballistics.h"

#include "common/xr_linear.h"

#include <openxr/openxr.h>
#include <stdexcept>

namespace Conformance
{
    void BodyInMotion::doSimulationStep(XrVector3f acceleration, XrTime predictedDisplayTime)
    {
        if (~this->velocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
            throw std::logic_error("doSimulationStep called without valid linear velocity");
        }

        const XrDuration timeSinceLastTick = predictedDisplayTime - this->updateTime;
        if (timeSinceLastTick <= 0) {
            throw std::logic_error("Unexpected old frame state predictedDisplayTime or future action state lastChangeTime");
        }
        this->updateTime = predictedDisplayTime;

        const float secondSinceLastTick = timeSinceLastTick / (float)1'000'000'000;

        // Apply acceleration to velocity.
        XrVector3f deltaAcceleration;
        XrVector3f_Scale(&deltaAcceleration, &acceleration, secondSinceLastTick);
        XrVector3f_Add(&this->velocity.linearVelocity, &this->velocity.linearVelocity, &deltaAcceleration);

        // Apply velocity to position.
        XrVector3f deltaVelocity;
        XrVector3f_Scale(&deltaVelocity, &this->velocity.linearVelocity, secondSinceLastTick);
        XrVector3f_Add(&this->pose.position, &this->pose.position, &deltaVelocity);

        if (this->velocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
            // Convert angular velocity to quaternion with the appropriate amount of rotation for the delta time.
            XrQuaternionf angularRotation;
            {
                const float radiansPerSecond = XrVector3f_Length(&this->velocity.angularVelocity);
                XrVector3f angularAxis = this->velocity.angularVelocity;
                XrVector3f_Normalize(&angularAxis);
                XrQuaternionf_CreateFromAxisAngle(&angularRotation, &angularAxis, radiansPerSecond * secondSinceLastTick);
            }

            // Update the orientation given the computed angular rotation.
            XrQuaternionf newOrientation;
            XrQuaternionf_Multiply(&newOrientation, &this->pose.orientation, &angularRotation);
            this->pose.orientation = newOrientation;
        }
    };
}  // namespace Conformance
