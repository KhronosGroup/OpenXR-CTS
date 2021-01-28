// Copyright (c) 2019-2021, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ConformanceHooks.h"
#include "CustomHandleState.h"
#include "RuntimeFailure.h"

namespace space
{
    HandleState* GetSpaceState(XrAction handle)
    {
        return GetHandleState({(IntHandle)handle, XR_OBJECT_TYPE_SPACE});
    }
}  // namespace space

/////////////////
// ABI
/////////////////

XrResult ConformanceHooks::xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
{
    VALIDATE_STRUCT_CHAIN(location);

    const XrResult result = ConformanceHooksBase::xrLocateSpace(space, baseSpace, time, location);

    if (XR_SUCCEEDED(result)) {
        if ((location->locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0 &&
            (location->locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) == 0) {
            NONCONFORMANT("Location orientation cannot be tracked but invalid");
        }

        if ((location->locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0 &&
            (location->locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == 0) {
            NONCONFORMANT("Location position cannot be tracked but invalid");
        }

        if ((location->locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
            VALIDATE_QUATERNION(location->pose.orientation);
        }
        if ((location->locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
            VALIDATE_VECTOR3F(location->pose.position);
        }

        XrSpaceVelocity* velocity = reinterpret_cast<XrSpaceVelocity*>(location->next);
        if (velocity != nullptr) {
            VALIDATE_STRUCT_CHAIN(velocity);

            if ((location->locationFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) != 0) {
                VALIDATE_VECTOR3F(velocity->linearVelocity);
            }
            if ((location->locationFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) != 0) {
                VALIDATE_VECTOR3F(velocity->angularVelocity);
            }
        }
    }
    return result;
}
