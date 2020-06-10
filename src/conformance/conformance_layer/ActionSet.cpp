// Copyright (c) 2019-2020 The Khronos Group Inc.
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

namespace actionset
{
    HandleState* GetActionSetState(XrActionSet handle)
    {
        return GetHandleState({HandleToInt(handle), XR_OBJECT_TYPE_ACTION_SET});
    }

    CustomActionSetState* GetCustomActionSetState(XrActionSet handle)
    {
        return reinterpret_cast<CustomActionSetState*>(GetActionSetState(handle)->customState.get());
    }

    void OnSyncActionData(XrResult syncResult, const XrActiveActionSet* activeActionSet)
    {
        CustomActionSetState* const actionSet = GetCustomActionSetState(activeActionSet->actionSet);

        std::unique_lock<std::mutex> lock(actionSet->mutex);
        if (syncResult == XR_SESSION_NOT_FOCUSED) {
            actionSet->lastSyncResult = SyncResult::NotFocused;
        }
        else if (syncResult == XR_SUCCESS) {
            actionSet->lastSyncResult = SyncResult::Synced;
        }
        else if (XR_SUCCEEDED(syncResult)) {
            // e.g. XR_SESSION_LOSS_PENDING
            // TODO: Is loss pending success or not focused? For now, treat as no-op.
        }
        else {
            // In case of failure, assume xrSyncActionData was no-op.
        }
    }
}  // namespace actionset

/////////////////
// ABI
/////////////////

using namespace actionset;

XrResult ConformanceHooks::xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet)
{
    const XrResult result = ConformanceHooksBase::xrCreateActionSet(instance, createInfo, actionSet);
    if (XR_SUCCEEDED(result)) {
        // Tag on the custom action set state to the generated handle state.
        GetActionSetState(*actionSet)->customState = std::unique_ptr<CustomActionSetState>(new CustomActionSetState(createInfo));
    }
    return result;
}
