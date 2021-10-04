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

#include <assert.h>
#include "ConformanceHooks.h"
#include "CustomHandleState.h"
#include "RuntimeFailure.h"

namespace
{
    bool IsValidStateTransition(XrSessionState oldState, XrSessionState newState)
    {
        // A pair representing a valid state transition from old state to new state.
        static const std::set<std::pair<XrSessionState, XrSessionState>> s_validStateTransitions = {
            {XR_SESSION_STATE_UNKNOWN, XR_SESSION_STATE_IDLE},         {XR_SESSION_STATE_IDLE, XR_SESSION_STATE_READY},
            {XR_SESSION_STATE_READY, XR_SESSION_STATE_SYNCHRONIZED},   {XR_SESSION_STATE_READY, XR_SESSION_STATE_IDLE},
            {XR_SESSION_STATE_SYNCHRONIZED, XR_SESSION_STATE_IDLE},    {XR_SESSION_STATE_SYNCHRONIZED, XR_SESSION_STATE_VISIBLE},
            {XR_SESSION_STATE_VISIBLE, XR_SESSION_STATE_FOCUSED},      {XR_SESSION_STATE_FOCUSED, XR_SESSION_STATE_VISIBLE},
            {XR_SESSION_STATE_VISIBLE, XR_SESSION_STATE_SYNCHRONIZED}, {XR_SESSION_STATE_SYNCHRONIZED, XR_SESSION_STATE_STOPPING},
            {XR_SESSION_STATE_STOPPING, XR_SESSION_STATE_IDLE},        {XR_SESSION_STATE_IDLE, XR_SESSION_STATE_EXITING},
        };

        if (newState == XR_SESSION_STATE_LOSS_PENDING) {
            return true;  // any state can transit to loss_pending.
        }

        return s_validStateTransitions.find(std::make_pair(oldState, newState)) != s_validStateTransitions.end();
    };
}  // namespace

namespace session
{
    HandleState* GetSessionState(XrSession handle)
    {
        return GetHandleState({(IntHandle)handle, XR_OBJECT_TYPE_SESSION});
    }

    CustomSessionState* GetCustomSessionState(XrSession handle)
    {
        return dynamic_cast<CustomSessionState*>(GetSessionState(handle)->customState.get());
    }

    void SessionStateChanged(ConformanceHooksBase* conformanceHooks, const XrEventDataSessionStateChanged* sessionStateChanged)
    {
        // Check under the lock to guarantee xrEndFrame completes if it's being called on another thread.
        CustomSessionState* const customSessionState = GetCustomSessionState(sessionStateChanged->session);
        std::unique_lock<std::mutex> lock(customSessionState->lock);

        if (!IsValidStateTransition(customSessionState->sessionState, sessionStateChanged->state)) {
            conformanceHooks->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "XrEventDataSessionStateChanged",
                                                 "Invalid session state transition from %s to %s",
                                                 to_string(customSessionState->sessionState), to_string(sessionStateChanged->state));
        }

        if (sessionStateChanged->state == XR_SESSION_STATE_SYNCHRONIZED && !customSessionState->sessionBegun) {
            conformanceHooks->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "XrEventDataSessionStateChanged",
                                                 "Illegal session state transition to %s when session has not been begun.",
                                                 to_string(sessionStateChanged->state));
        }

        // Transition from READY to SYNCHRONIZED should only happen after frames have been synchronized (1 or more frames submitted).
        if (sessionStateChanged->state == XR_SESSION_STATE_SYNCHRONIZED && customSessionState->frameCount == 0) {
            // There are three exceptions:
            // 1. The app has requested the session to exit while in the RUNNING state.
            // 2. The session is headless.
            // 3. Rare cases where the runtime wants to end the session before becoming synchornized.
            //    For this reason it this is a warning rather than an error.
            if (!customSessionState->sessionExitRequested && !customSessionState->headless) {
                conformanceHooks->ConformanceFailure(
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "XrEventDataSessionStateChanged",
                    "Suspicious session state transition to %s when no frame(s) have been submitted and session has not requested an exit.",
                    to_string(sessionStateChanged->state));
            }
        }

        if (sessionStateChanged->state == XR_SESSION_STATE_IDLE && customSessionState->sessionBegun) {
            conformanceHooks->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "XrEventDataSessionStateChanged",
                                                 "Illegal session state transition to %s when session has not been ended.",
                                                 to_string(sessionStateChanged->state));
        }

        customSessionState->sessionState = sessionStateChanged->state;
    }

    void VisibilityMaskChanged(ConformanceHooksBase* conformanceHooks, const XrEventDataVisibilityMaskChangedKHR* visibilityMaskChanged)
    {
        // Look up parent handle required to validate view configuration metadata.
        XrInstance instance;
        {
            HandleState* const handleState = GetSessionState(visibilityMaskChanged->session);
            instance = (XrInstance)handleState->parent->handle;
            assert(handleState->parent->type == XR_OBJECT_TYPE_INSTANCE);
        }

        CustomSessionState* const customSessionState = GetCustomSessionState(visibilityMaskChanged->session);

        // Verify the viewIndex against the size of the view configuration (as reported by the runtime).
        uint32_t viewCount;
        XrResult enumRes = conformanceHooks->xrEnumerateViewConfigurationViews(
            instance, customSessionState->systemId, visibilityMaskChanged->viewConfigurationType, 0, &viewCount, nullptr);
        if (!XR_SUCCEEDED(enumRes)) {
            conformanceHooks->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrPollEvent",
                                                 "xrEnumerateViewConfigurationViews failed due to error %s", to_string(enumRes));
        }
        else {
            if (visibilityMaskChanged->viewIndex >= viewCount) {
                conformanceHooks->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrPollEvent",
                                                     "XrEventDataVisibilityMaskChangedKHR::viewIndex out of bounds with value %d >= %d",
                                                     visibilityMaskChanged->viewIndex, viewCount);
            }
        }
    }
}  // namespace session

/////////////////
// ABI
/////////////////

using namespace session;

XrResult ConformanceHooks::xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
{
    const XrResult result = ConformanceHooksBase::xrCreateSession(instance, createInfo, session);
    if (XR_SUCCEEDED(result)) {
        std::unique_ptr<CustomSessionState> customSessionState = std::unique_ptr<CustomSessionState>(new CustomSessionState());
        customSessionState->systemId = createInfo->systemId;

        ForEachExtension(createInfo->next,
                         [&](const XrBaseInStructure* ext) { customSessionState->creationExtensionTypes.push_back(ext->type); });

        static_assert(XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR == XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR, "vulkan binding mismatch");
        std::initializer_list<XrStructureType> graphicsBindingStructures{
            XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
            XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR,
            XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR,
            XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR,
            XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
            XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
            XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,
            XR_TYPE_GRAPHICS_BINDING_D3D12_KHR,
        };
        auto it = std::find_first_of(customSessionState->creationExtensionTypes.begin(), customSessionState->creationExtensionTypes.end(),
                                     graphicsBindingStructures.begin(), graphicsBindingStructures.end());

        if (this->enabledExtensions.mnd_headless) {
            if (it == customSessionState->creationExtensionTypes.end()) {
                customSessionState->headless = true;
            }
        }
        else {
            NONCONFORMANT_IF(it == customSessionState->creationExtensionTypes.end(), "Graphics Binding not found");
            if (it != customSessionState->creationExtensionTypes.end()) {
                customSessionState->graphicsBinding = *it;
            }
        }

        // Tag on the custom session state to the generated handle state.
        GetSessionState(*session)->customState = std::move(customSessionState);
    }

    return result;
}

XrResult ConformanceHooks::xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
{
    const XrResult result = ConformanceHooksBase::xrSyncActions(session, syncInfo);

    CustomSessionState* const customSessionState = GetCustomSessionState(session);
    std::unique_lock<std::mutex> lock(customSessionState->lock);

    if (result == XR_SESSION_NOT_FOCUSED && customSessionState->sessionState == XR_SESSION_STATE_FOCUSED) {
        // Suspicious but possibly legal if there is a queued-but-unobserved state change.
        POSSIBLE_NONCONFORMANT("XR_SESSION_NOT_FOCUSED returned when session state is XR_SESSION_STATE_FOCUSED");
    }
    else if (result == XR_SUCCESS && customSessionState->sessionState != XR_SESSION_STATE_FOCUSED) {
        // Suspicious but possibly legal if there is a queued-but-unobserved state change.
        POSSIBLE_NONCONFORMANT("XR_SUCCESS returned when session state is %s", to_string(customSessionState->sessionState));
    }

    // Notify each action set individually.
    for (uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        actionset::OnSyncActionData(result, &syncInfo->activeActionSets[i]);
    }
    return result;
}

XrResult ConformanceHooks::xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState,
                                         uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
{
    std::vector<XrBaseStructChainValidator> viewChainValidations;
    for (uint32_t i = 0; i < viewCapacityInput; i++) {
        viewChainValidations.emplace_back(CREATE_STRUCT_CHAIN_VALIDATOR(&views[i]));
    }

    const XrResult result =
        ConformanceHooksBase::xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);

    if (XR_SUCCEEDED(result)) {
        CustomSessionState* const customSessionState = GetCustomSessionState(session);
        std::unique_lock<std::mutex> lock(customSessionState->lock);

        NONCONFORMANT_IF(!customSessionState->sessionBegun, "Session must be begun");

        // TODO: What is status of viewState if called two-idiom style to look up capacity?
        // For now, only check ViewState if viewCountOutput > 0.
        if (*viewCountOutput > 0) {
            if ((viewState->viewStateFlags & XR_VIEW_STATE_ORIENTATION_TRACKED_BIT) != 0 &&
                (viewState->viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
                NONCONFORMANT("View state orientation cannot be tracked but invalid");
            }

            if ((viewState->viewStateFlags & XR_VIEW_STATE_POSITION_TRACKED_BIT) != 0 &&
                (viewState->viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0) {
                NONCONFORMANT("View state position cannot be tracked but invalid");
            }

            for (uint32_t i = 0; i < *viewCountOutput; i++) {
                const XrView& currentView = views[i];
                if ((viewState->viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0) {
                    VALIDATE_QUATERNION(currentView.pose.orientation);
                }
                if ((viewState->viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0) {
                    VALIDATE_VECTOR3F(currentView.pose.position);
                }

                // TODO: Validate FOV.
            }
        }
    }
    return result;
}

XrResult ConformanceHooks::xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo)
{
    const XrResult result = ConformanceHooksBase::xrBeginSession(session, beginInfo);
    CustomSessionState* const customSessionState = GetCustomSessionState(session);
    if (XR_SUCCEEDED(result)) {
        std::unique_lock<std::mutex> lock(customSessionState->lock);
        NONCONFORMANT_IF(customSessionState->sessionBegun, "Session cannot be begun when already begun");
        customSessionState->sessionBegun = true;
        customSessionState->frameCount = 0;
    }
    else if (result == XR_ERROR_SESSION_RUNNING) {
        std::unique_lock<std::mutex> lock(customSessionState->lock);
        NONCONFORMANT_IF(!customSessionState->sessionBegun, "Session claims to be running when not begun");
    }
    return result;
}

XrResult ConformanceHooks::xrEndSession(XrSession session)
{
    const XrResult result = ConformanceHooksBase::xrEndSession(session);

    CustomSessionState* const customSessionState = GetCustomSessionState(session);
    std::unique_lock<std::mutex> lock(customSessionState->lock);

    if (XR_SUCCEEDED(result)) {
        NONCONFORMANT_IF(!customSessionState->sessionBegun, "Expected XR_ERROR_SESSION_NOT_RUNNING but got %s", to_string(result));
        POSSIBLE_NONCONFORMANT_IF(customSessionState->sessionState != XR_SESSION_STATE_STOPPING,
                                  "Expected XR_ERROR_SESSION_NOT_STOPPING when last known session state was %s", to_string(result),
                                  to_string(customSessionState->sessionState));

        customSessionState->sessionBegun = false;
        customSessionState->sessionExitRequested = false;
    }
    else if (result == XR_ERROR_SESSION_NOT_RUNNING) {
        NONCONFORMANT_IF(customSessionState->sessionBegun, "Unexpected XR_ERROR_SESSION_NOT_RUNNING failure for running session");
    }
    else if (result == XR_ERROR_SESSION_NOT_STOPPING) {
        POSSIBLE_NONCONFORMANT_IF(
            customSessionState->sessionState == XR_SESSION_STATE_STOPPING,
            "Unexpected XR_ERROR_SESSION_NOT_STOPPING failure when last observed session state was XR_SESSION_STATE_STOPPING");
    }

    return result;
}

XrResult ConformanceHooks::xrRequestExitSession(XrSession session)
{
    const XrResult result = ConformanceHooksBase::xrRequestExitSession(session);

    CustomSessionState* const customSessionState = GetCustomSessionState(session);
    std::unique_lock<std::mutex> lock(customSessionState->lock);
    if (XR_SUCCEEDED(result)) {
        NONCONFORMANT_IF(!customSessionState->sessionBegun, "Expected XR_ERROR_SESSION_NOT_RUNNING but got %s", to_string(result));
        customSessionState->sessionExitRequested = true;
    }
    else if (result == XR_ERROR_SESSION_NOT_RUNNING) {
        NONCONFORMANT_IF(customSessionState->sessionBegun, "Unexpected XR_ERROR_SESSION_NOT_RUNNING failure for running session");
    }

    return result;
}

XrResult ConformanceHooks::xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState)
{
    VALIDATE_STRUCT_CHAIN(frameState);

    const XrResult result = ConformanceHooksBase::xrWaitFrame(session, frameWaitInfo, frameState);

    if (XR_SUCCEEDED(result)) {
        CustomSessionState* const customSessionState = GetCustomSessionState(session);
        std::unique_lock<std::mutex> lock(customSessionState->lock);

        // SPEC: If a frame submitted to xrEndFrame is consumed by the compositor before its target display time, a subsequent call
        // to xrWaitFrame must block the caller until the start of the next rendering interval after the frame's target display time
        // as determined by the runtime.
        NONCONFORMANT_IF(frameState->predictedDisplayTime <= customSessionState->lastPredictedDisplayTime,
                         "New predicted display time %lld is less or equal to the previous predicted display time %lld",
                         frameState->predictedDisplayTime, customSessionState->lastPredictedDisplayTime);

        customSessionState->lastPredictedDisplayTime = frameState->predictedDisplayTime;
        customSessionState->lastPredictedDisplayPeriod = frameState->predictedDisplayPeriod;
    }
    return result;
}

XrResult ConformanceHooks::xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)
{
    const XrResult result = ConformanceHooksBase::xrBeginFrame(session, frameBeginInfo);
    if (XR_SUCCEEDED(result)) {
        CustomSessionState* const customSessionState = GetCustomSessionState(session);
        std::unique_lock<std::mutex> lock(customSessionState->lock);
        NONCONFORMANT_IF(customSessionState->frameBegun && result == XR_SUCCESS, "XR_FRAME_DISCARDED expected but XR_SUCCESS returned");
        NONCONFORMANT_IF(!customSessionState->frameBegun && result == XR_FRAME_DISCARDED,
                         "XR_SUCCESS expected but XR_FRAME_DISCARDED returned");
        customSessionState->frameBegun = true;
    }
    return result;
}

XrResult ConformanceHooks::xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
{
    // Call xrEndFrame under the lock because it might generate XR_SESSION_STATE_SYNCHRONIZED
    // at any time during the call and frameCount needs to increment in unison.
    CustomSessionState* const customSessionState = GetCustomSessionState(session);
    std::unique_lock<std::mutex> lock(customSessionState->lock);

    const XrResult result = ConformanceHooksBase::xrEndFrame(session, frameEndInfo);

    if (XR_SUCCEEDED(result)) {
        NONCONFORMANT_IF(!customSessionState->frameBegun,
                         "Unexpected success. XR_ERROR_CALL_ORDER_INVALID expected because xrBeginFrame was not called");
        customSessionState->frameBegun = false;
        customSessionState->frameCount++;
    }
    else if (result == XR_ERROR_CALL_ORDER_INVALID) {
        // This error can also happen due to not having a released swapchain image available.
        // std::unique_lock<std::mutex> lock(customSessionState->lock);
        // NONCONFORMANT_IF(customSessionState->frameBegun, "XR_ERROR_CALL_ORDER_INVALID returned but frame has been begun");
    }
    return result;
}

XrResult ConformanceHooks::xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput,
                                                      XrReferenceSpaceType* spaces)
{
    const XrResult result = ConformanceHooksBase::xrEnumerateReferenceSpaces(session, spaceCapacityInput, spaceCountOutput, spaces);
    if (XR_SUCCEEDED(result)) {
        if (spaceCountOutput != nullptr && spaces != nullptr) {
            std::vector<XrReferenceSpaceType> referenceSpaceCopy(spaces, spaces + *spaceCountOutput);
            VectorInspection<XrReferenceSpaceType> referenceSpaceInspect(referenceSpaceCopy);

            NONCONFORMANT_IF(referenceSpaceInspect.ContainsDuplicates(), "Duplicate reference spaces found");

            NONCONFORMANT_IF(!referenceSpaceInspect.ContainsValue(XR_REFERENCE_SPACE_TYPE_LOCAL),
                             "Local space must be a supported reference space");
            NONCONFORMANT_IF(!referenceSpaceInspect.ContainsValue(XR_REFERENCE_SPACE_TYPE_VIEW),
                             "View space must be a supported reference space");

            /* TODO: Needs to be generated since this is already stale with MSFT UNBOUNDED reference space.
            NONCONFORMANT_IF(referenceSpaceInspect.ContainsAnyNotIn(
                                 {XR_REFERENCE_SPACE_TYPE_LOCAL, XR_REFERENCE_SPACE_TYPE_STAGE, XR_REFERENCE_SPACE_TYPE_VIEW}),
                             "References spaces include an unknown value.");
            */

            CustomSessionState* const customSessionState = GetCustomSessionState(session);
            std::unique_lock<std::mutex> lock(customSessionState->lock);

            // If reference spaces are already cached, then make sure the enumeration function is returning the same results.
            if (customSessionState->referenceSpaces.size() > 0) {
                NONCONFORMANT_IF(!referenceSpaceInspect.SameElementsAs(customSessionState->referenceSpaces),
                                 "References spaces differs from original enumeration of reference spaces.");
            }
            else {
                // This is the first time the enumeration has been returned, so cache it.
                customSessionState->referenceSpaces = std::move(referenceSpaceCopy);
            }
        }
    }
    return result;
}

XrResult ConformanceHooks::xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput,
                                                       int64_t* formats)
{
    const XrResult result = ConformanceHooksBase::xrEnumerateSwapchainFormats(session, formatCapacityInput, formatCountOutput, formats);
    if (!XR_SUCCEEDED(result)) {
        return result;
    }
    if (formatCountOutput != nullptr && formats != nullptr) {
        CustomSessionState* const customSessionState = GetCustomSessionState(session);

        std::unique_lock<std::mutex> lock(customSessionState->lock);
        if (customSessionState->headless) {
            NONCONFORMANT_IF(*formatCountOutput != 0, "Headless session must enumerate zero swapchain formats");
            return result;
        }
        if (*formatCountOutput == 0) {
            // TODO: There is no actual rule for this.
            NONCONFORMANT("Session must enumerate one or more swapchain formats");
        }

        std::vector<int64_t> formatsCopy(formats, formats + *formatCountOutput);
        VectorInspection<int64_t> formatsInspect(formatsCopy);
        // TODO: Technically the spec doesn't disallow this explicitly like it does for reference spaces.
        NONCONFORMANT_IF(formatsInspect.ContainsDuplicates(), "Duplicate swapchain formats found");

        // If swapchain formats are already cached, then make sure the enumeration function is returning the same results.
        if (customSessionState->swapchainFormats.size() > 0) {
            NONCONFORMANT_IF(!formatsInspect.SameElementsAs(customSessionState->swapchainFormats),
                             "Swapchain formats differs from original enumeration of swapchain formats");

            // TODO: Depending on the graphics API, Validate all swapchain formats are known good types.
            // can use formatsInspect.ContainsAnyNotIn({valid values here})
        }
        else {
            // This is the first time the enumeration has been returned, so cache it.
            customSessionState->swapchainFormats = std::move(formatsCopy);
        }
    }

    return result;
}
