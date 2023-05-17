// Copyright (c) 2019-2023, The Khronos Group Inc.
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

namespace action
{
    HandleState* GetActionState(XrAction handle)
    {
        return GetHandleState({HandleToInt(handle), XR_OBJECT_TYPE_ACTION});
    }

    CustomActionState* GetCustomActionState(XrAction handle)
    {
        return dynamic_cast<CustomActionState*>(GetActionState(handle)->customState.get());
    }
}  // namespace action

/////////////////
// ABI
/////////////////

using namespace action;

XrResult ConformanceHooks::xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action)
{
    const XrResult result = ConformanceHooksBase::xrCreateAction(actionSet, createInfo, action);
    if (XR_SUCCEEDED(result)) {
        // Tag on the custom action state to the generated handle state.
        GetActionState(*action)->customState = std::unique_ptr<CustomActionState>(new CustomActionState(createInfo));
    }
    return result;
}

XrResult ConformanceHooks::xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* data)
{
    VALIDATE_STRUCT_CHAIN(getInfo);
    VALIDATE_STRUCT_CHAIN(data);
    const XrResult result = ConformanceHooksBase::xrGetActionStateBoolean(session, getInfo, data);
    if (XR_SUCCEEDED(result)) {
        CustomActionState* const actionData = GetCustomActionState(getInfo->action);
        NONCONFORMANT_IF(actionData->type != XR_ACTION_TYPE_BOOLEAN_INPUT, "Expected failure due to action type mismatch");
        VALIDATE_XRBOOL32(data->isActive);
        VALIDATE_XRBOOL32(data->currentState);
        VALIDATE_XRBOOL32(data->changedSinceLastSync);

        if (!data->isActive) {
            NONCONFORMANT_IF(data->currentState != XR_FALSE, "currentState must be false when isActive is false");
            NONCONFORMANT_IF(data->changedSinceLastSync != XR_FALSE, "changedSinceLastSync must be false when isActive is false");
            NONCONFORMANT_IF(data->lastChangeTime != 0, "lastChangeTime must be 0 when isActive is false");
        }
        else {
            NONCONFORMANT_IF(data->changedSinceLastSync && data->lastChangeTime == 0,
                             "lastChangeTime must be non-0 when changedSinceLastSync is true");
        }
    }
    return result;
}

XrResult ConformanceHooks::xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* data)
{
    VALIDATE_STRUCT_CHAIN(getInfo);
    VALIDATE_STRUCT_CHAIN(data);
    const XrResult result = ConformanceHooksBase::xrGetActionStateFloat(session, getInfo, data);
    if (XR_SUCCEEDED(result)) {
        CustomActionState* const actionData = GetCustomActionState(getInfo->action);
        NONCONFORMANT_IF(actionData->type != XR_ACTION_TYPE_FLOAT_INPUT, "Expected failure due to action type mismatch");
        VALIDATE_XRBOOL32(data->isActive);
        VALIDATE_XRBOOL32(data->changedSinceLastSync);
        VALIDATE_FLOAT(data->currentState, -1.0, +1.0);  // TODO: This could be more strict depending on suggested bindings being used (0.0
                                                         // to 1.0). Not sure if this is possible though.

        if (!data->isActive) {
            NONCONFORMANT_IF(data->currentState != 0.0f, "currentState must be 0 when isActive is false");
            NONCONFORMANT_IF(data->changedSinceLastSync != XR_FALSE, "changedSinceLastSync must be false when isActive is false");
            NONCONFORMANT_IF(data->lastChangeTime != 0, "lastChangeTime must be 0 when isActive is false");
        }
        else {
            NONCONFORMANT_IF(data->changedSinceLastSync && data->lastChangeTime == 0,
                             "lastChangeTime must be non-0 when changedSinceLastSync is true");
        }
    }
    return result;
}

XrResult ConformanceHooks::xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* data)
{
    VALIDATE_STRUCT_CHAIN(getInfo);
    VALIDATE_STRUCT_CHAIN(data);
    const XrResult result = ConformanceHooksBase::xrGetActionStateVector2f(session, getInfo, data);
    if (XR_SUCCEEDED(result)) {
        CustomActionState* const actionData = GetCustomActionState(getInfo->action);
        NONCONFORMANT_IF(actionData->type != XR_ACTION_TYPE_VECTOR2F_INPUT, "Expected failure due to action type mismatch");
        VALIDATE_XRBOOL32(data->isActive);
        VALIDATE_XRBOOL32(data->changedSinceLastSync);
        VALIDATE_XRTIME(data->lastChangeTime);
        VALIDATE_FLOAT(data->currentState.x, -1.0, +1.0);
        VALIDATE_FLOAT(data->currentState.y, -1.0, +1.0);

        if (!data->isActive) {
            NONCONFORMANT_IF(data->currentState.x != 0, "currentState.x must be 0 when isActive is false");
            NONCONFORMANT_IF(data->currentState.y != 0, "currentState.y must be 0 when isActive is false");
            NONCONFORMANT_IF(data->changedSinceLastSync != XR_FALSE, "changedSinceLastSync must be false when isActive is false");
            NONCONFORMANT_IF(data->lastChangeTime != 0, "lastChangeTime must be 0 when isActive is false");
        }
        else {
            NONCONFORMANT_IF(data->changedSinceLastSync && data->lastChangeTime == 0,
                             "lastChangeTime must be non-0 when changedSinceLastSync is true");
        }
    }
    return result;
}

XrResult ConformanceHooks::xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* data)
{
    VALIDATE_STRUCT_CHAIN(getInfo);
    VALIDATE_STRUCT_CHAIN(data);
    const XrResult result = ConformanceHooksBase::xrGetActionStatePose(session, getInfo, data);
    if (XR_SUCCEEDED(result)) {
        CustomActionState* const actionData = GetCustomActionState(getInfo->action);
        NONCONFORMANT_IF(actionData->type != XR_ACTION_TYPE_POSE_INPUT, "Unexpected success with action handle type %s",
                         (int)actionData->type);
        VALIDATE_XRBOOL32(data->isActive);
    }
    return result;
}
