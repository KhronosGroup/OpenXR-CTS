// Copyright (c) 2019-2024, The Khronos Group Inc.
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

#pragma once

#include "HandleState.h"

#include <openxr/openxr.h>

#include <atomic>
#include <mutex>
#include <queue>
#include <stdint.h>
#include <vector>

struct HandleState;

//
// XrInstance
//
namespace instance
{
    HandleState* GetInstanceState(XrInstance handle);
}

//
// XrSession
//
namespace session
{
    enum class SyncActionsState : uint32_t
    {
        NOT_CALLED_SINCE_QUEUE_EXHAUST,
        CALLED_SINCE_QUEUE_EXHAUST,
        ONGOING,
    };

    struct CustomSessionState : ICustomHandleState
    {
        std::mutex lock;
        XrSystemId systemId{XR_NULL_SYSTEM_ID};
        XrSessionState sessionState{XR_SESSION_STATE_UNKNOWN};
        bool sessionBegun{false};
        bool sessionExitRequested{false};
        bool frameBegun{false};
        bool headless{false};  //< true if a headless extension is enabled *and* in use
        std::atomic<SyncActionsState> syncActionsState{SyncActionsState::NOT_CALLED_SINCE_QUEUE_EXHAUST};
        XrStructureType graphicsBinding{XR_TYPE_UNKNOWN};
        XrTime lastPredictedDisplayTime{0};
        XrDuration lastPredictedDisplayPeriod{0};
        uint32_t frameCount{0};
        std::vector<XrReferenceSpaceType> referenceSpaces;
        std::vector<int64_t> swapchainFormats;
        std::vector<XrStructureType> creationExtensionTypes;
    };

    HandleState* GetSessionState(XrSession handle);
    CustomSessionState* GetCustomSessionState(XrSession handle);

    void SessionStateChanged(ConformanceHooksBase* conformanceHooks, const XrEventDataSessionStateChanged* sessionStateChanged);
    void VisibilityMaskChanged(ConformanceHooksBase* conformanceHooks, const XrEventDataVisibilityMaskChangedKHR* visibilityMaskChanged);
    void InteractionProfileChanged(ConformanceHooksBase* conformanceHooks,
                                   const XrEventDataInteractionProfileChanged* interactionProfileChanged);
}  // namespace session

//
// XrSpace
//
namespace space
{
    // TODO: Currently there is no custom state to maintain for spaces.

    HandleState* GetSpaceState(XrAction handle);
}  // namespace space

//
// XrSwapchain
//
namespace swapchain
{
    enum class ImageState
    {
        Created,
        Acquired,
        Waited,
        Released,
    };

    struct CustomSwapchainState : ICustomHandleState
    {
        CustomSwapchainState(const XrSwapchainCreateInfo* createInfo, const XrStructureType graphicsBinding)
            : isStatic((createInfo->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) != 0)
            , graphicsBinding(graphicsBinding)
            , createInfo(*createInfo)
        {
        }

        std::recursive_mutex mutex;
        bool isStatic;
        std::vector<ImageState> imageStates;
        std::queue<int> acquiredSwapchains;
        XrStructureType graphicsBinding;
        XrSwapchainCreateInfo createInfo;
    };

    HandleState* GetSwapchainState(XrSwapchain handle);
    CustomSwapchainState* GetCustomSwapchainState(XrSwapchain handle);
}  // namespace swapchain

//
// XrActionSet
//
namespace actionset
{
    enum class SyncResult
    {
        NotSynced,
        Synced,
        NotFocused
    };

    struct CustomActionSetState : ICustomHandleState
    {
        CustomActionSetState(const XrActionSetCreateInfo* /*createInfo*/)
        {
        }

        std::mutex mutex;
        SyncResult lastSyncResult{SyncResult::NotSynced};
    };

    HandleState* GetActionSetState(XrActionSet handle);
    CustomActionSetState* GetCustomActionSetState(XrActionSet handle);

    void OnSyncActionData(XrResult syncResult, const XrActiveActionSet* activeActionSet);
}  // namespace actionset

//
// XrAction
//
namespace action
{
    struct CustomActionState : ICustomHandleState
    {
        CustomActionState(const XrActionCreateInfo* actionCreateInfo) : type(actionCreateInfo->actionType)
        {
        }

        const XrActionType type;

        std::mutex mutex;
        // TODO: Useful state.
    };

    HandleState* GetActionState(XrAction handle);
    CustomActionState* GetCustomActionState(XrAction handle);
}  // namespace action
