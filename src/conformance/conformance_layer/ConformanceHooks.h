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

#pragma once

#include "gen_dispatch.h"

// Implementation of methods are distributed across multiple files, based on the primary handle type.
// IConformanceHooks provides empty default implementations of all OpenXR functions. Only provide an override
// if custom validation logic needs to be written.
// This interface exists purely to cause build breaks when xr.xml changes and the manually written conformance layer
// code hasn't been updated to match it. If the compiler sees a method with the "override" keyword and it is not virtual
// in the base interface, the compiler will yell.
struct ConformanceHooks : ConformanceHooksBase
{
    ConformanceHooks(XrInstance instance, const XrGeneratedDispatchTable dispatchTable, const EnabledExtensions enabledExtensions)
        : ConformanceHooksBase(instance, dispatchTable, enabledExtensions)
    {
    }

    void ConformanceFailure(XrDebugUtilsMessageSeverityFlagsEXT severity, const char* functionName, const char* fmtMessage, ...) override;

    //
    // Defined in Instance.cpp
    //
    // xrCreateInstance is handled by CreateApiLayerInstance()
    //XrResult xrDestroyInstance(XrInstance instance) override;
    XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) override;

    // Defined in Session.cpp
    XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session) override;
    //XrResult xrDestroySession(XrSession session) override;
    XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) override;
    XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput,
                           uint32_t* viewCountOutput, XrView* views) override;
    XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo) override;
    XrResult xrEndSession(XrSession session) override;
    XrResult xrRequestExitSession(XrSession session) override;
    XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState) override;
    XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo) override;
    XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override;
    XrResult xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput,
                                        XrReferenceSpaceType* spaces) override;
    XrResult xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput,
                                         int64_t* formats) override;

    //
    // Defined in Action.cpp
    //
    XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action) override;
    //XrResult xrDestroyAction(XrAction action) override;
    XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* data) override;
    XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* data) override;
    XrResult xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* data) override;
    XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* data) override;

    //
    // Defined in ActionSet.cpp
    //
    XrResult xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet) override;
    //XrResult xrDestroyActionSet(XrActionSet actionSet) override;

    //
    // Defined in Space.cpp
    //
    // TODO/FIXME: Generated code assumes the first handle (action) is the parent, but for an action space, the parent is actually the session.
    //             This should resolve itself when XrAction/XrActionSet becomes parented from XrInstance because the first parameter will be
    //             XrSession instead. If this is not resolved, then the auto-generated code needs a hack so that destroying an XrAction does not
    //             remove the action space from the lookup table.
    //XrResult xrCreateActionSpace(XrAction action, const XrActionSpaceCreateInfo* createInfo,
    //                             XrSpace* space) override;
    //XrResult xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo,
    //                                XrSpace* space) override;
    //XrResult xrDestroySpace(XrSpace space) override;
    XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) override;

    //
    // Defined in Swapchain.cpp
    //
    XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain) override;
    //XrResult xrDestroySwapchain(XrSwapchain swapchain) override;
    XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput,
                                        XrSwapchainImageBaseHeader* images) override;
    XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index) override;
    XrResult xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo) override;
    XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo) override;

#if 0
    // TODO (Warning this will become stale!)
    XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) override;
    XrResult xrDestroyInstance(XrInstance instance) override;
    XrResult xrGetInstanceProperties(XrInstance instance, XrInstanceProperties* instanceProperties) override;
    XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) override;
    XrResult xrResultToString(XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE]) override;
    XrResult xrStructureTypeToString(XrInstance instance, XrStructureType value, char buffer[XR_MAX_STRUCTURE_NAME_SIZE]) override;
    XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override;
    XrResult xrGetSystemProperties(XrInstance instance, XrSystemId systemId, XrSystemProperties* properties) override;
    XrResult xrEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t environmentBlendModeCapacityInput, uint32_t* environmentBlendModeCountOutput, XrEnvironmentBlendMode* environmentBlendModes) override;
    XrResult xrGetReferenceSpaceBoundsRect(XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds) override;
    XrResult xrEnumerateViewConfigurations(XrInstance instance, XrSystemId systemId, uint32_t viewConfigurationTypeCapacityInput, uint32_t* viewConfigurationTypeCountOutput, XrViewConfigurationType* viewConfigurationTypes) override;
    XrResult xrGetViewConfigurationProperties(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, XrViewConfigurationProperties* configurationProperties) override;
    XrResult xrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views) override;
    XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* path) override;
    XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer) override;
    XrResult xrCreateActionSet(XrSession session, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet) override;
    XrResult xrDestroyActionSet(XrActionSet actionSet) override;
    XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action) override;
    XrResult xrDestroyAction(XrAction action) override;
    XrResult xrSuggestInteractionProfileBindings(const HandleState& handleState, XrSession session, const XrInteractionProfileSuggestedBinding* suggestedBindings) override;
    XrResult xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile) override;
    XrResult xrEnumerateBoundSourcesForAction(XrAction action, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources) override;
    XrResult xrGetInputSourceLocalizedName(XrSession session, XrPath source, XrInputSourceLocalizedNameFlags whichComponents, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer) override;
    XrResult xrApplyHapticFeedback(XrAction hapticAction, uint32_t countSubactionPaths, const XrPath* subactionPaths, const XrHapticBaseHeader* hapticEvent) override;
    XrResult xrStopHapticFeedback(XrAction hapticAction, uint32_t countSubactionPaths, const XrPath* subactionPaths) override;
#endif

private:
};
