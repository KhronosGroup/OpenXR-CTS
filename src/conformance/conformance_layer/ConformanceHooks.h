// Copyright (c) 2019-2026 The Khronos Group Inc.
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
#include <cstdint>

#ifdef __GNUC__
#define LAYER_PRINT_ATTRIBUTE(a, b) __attribute__((format(printf, a, b)))
#else
#define LAYER_PRINT_ATTRIBUTE(a, b)
#endif

// Implementation of methods are distributed across multiple files, based on the primary handle type.
// IConformanceHooks provides empty default implementations of all OpenXR functions. Only provide an override
// if custom validation logic needs to be written.
// This interface exists purely to cause build breaks when xr.xml changes and the manually written conformance layer
// code hasn't been updated to match it. If the compiler sees a method with the "override" keyword and it is not virtual
// in the base interface, the compiler will yell.
struct ConformanceHooks : ConformanceHooksBase
{
    using ConformanceHooksBase::ConformanceHooksBase;

    void LAYER_PRINT_ATTRIBUTE(4, 5)
        ConformanceFailure(XrDebugUtilsMessageSeverityFlagsEXT severity, const char* functionName, const char* fmtMessage, ...) override;

    //
    // Defined in Instance.cpp
    //
    // xrCreateInstance is handled by CreateApiLayerInstance()
    //XrResult xrDestroyInstance(XrInstance instance) override;
    XrResult xrEnumerateViewConfigurations(HandleState* const handleState, XrInstance instance, XrSystemId systemId,
                                           uint32_t viewConfigurationTypeCapacityInput, uint32_t* viewConfigurationTypeCountOutput,
                                           XrViewConfigurationType* viewConfigurationTypes) override;
    XrResult xrEnumerateEnvironmentBlendModes(HandleState* const handleState, XrInstance instance, XrSystemId systemId,
                                              XrViewConfigurationType viewConfigurationType, uint32_t environmentBlendModeCapacityInput,
                                              uint32_t* environmentBlendModeCountOutput,
                                              XrEnvironmentBlendMode* environmentBlendModes) override;
    XrResult xrPollEvent(HandleState* const handleState, XrInstance instance, XrEventDataBuffer* eventData) override;

    XrResult xrGetSystemProperties(HandleState* const handleState, XrInstance instance, XrSystemId systemId,
                                   XrSystemProperties* properties) override;

    // Defined in Session.cpp
    XrResult xrCreateSession(HandleState* const handleState, XrInstance instance, const XrSessionCreateInfo* createInfo,
                             XrSession* session) override;
    //XrResult xrDestroySession(HandleState* const handleState, XrSession session) override;
    XrResult xrSyncActions(HandleState* const handleState, XrSession session, const XrActionsSyncInfo* syncInfo) override;
    XrResult xrLocateViews(HandleState* const handleState, XrSession session, const XrViewLocateInfo* viewLocateInfo,
                           XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views) override;
    XrResult xrBeginSession(HandleState* const handleState, XrSession session, const XrSessionBeginInfo* beginInfo) override;
    XrResult xrEndSession(HandleState* const handleState, XrSession session) override;
    XrResult xrRequestExitSession(HandleState* const handleState, XrSession session) override;
    XrResult xrWaitFrame(HandleState* const handleState, XrSession session, const XrFrameWaitInfo* frameWaitInfo,
                         XrFrameState* frameState) override;
    XrResult xrBeginFrame(HandleState* const handleState, XrSession session, const XrFrameBeginInfo* frameBeginInfo) override;
    XrResult xrEndFrame(HandleState* const handleState, XrSession session, const XrFrameEndInfo* frameEndInfo) override;
    XrResult xrEnumerateReferenceSpaces(HandleState* const handleState, XrSession session, uint32_t spaceCapacityInput,
                                        uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces) override;
    XrResult xrEnumerateSwapchainFormats(HandleState* const handleState, XrSession session, uint32_t formatCapacityInput,
                                         uint32_t* formatCountOutput, int64_t* formats) override;

    //
    // Defined in Action.cpp
    //
    XrResult xrCreateAction(HandleState* const handleState, XrActionSet actionSet, const XrActionCreateInfo* createInfo,
                            XrAction* action) override;
    //XrResult xrDestroyAction(HandleState* const handleState, XrAction action) override;
    XrResult xrGetActionStateBoolean(HandleState* const handleState, XrSession session, const XrActionStateGetInfo* getInfo,
                                     XrActionStateBoolean* data) override;
    XrResult xrGetActionStateFloat(HandleState* const handleState, XrSession session, const XrActionStateGetInfo* getInfo,
                                   XrActionStateFloat* data) override;
    XrResult xrGetActionStateVector2f(HandleState* const handleState, XrSession session, const XrActionStateGetInfo* getInfo,
                                      XrActionStateVector2f* data) override;
    XrResult xrGetActionStatePose(HandleState* const handleState, XrSession session, const XrActionStateGetInfo* getInfo,
                                  XrActionStatePose* data) override;

    //
    // Defined in ActionSet.cpp
    //
    XrResult xrCreateActionSet(HandleState* const handleState, XrInstance instance, const XrActionSetCreateInfo* createInfo,
                               XrActionSet* actionSet) override;
    //XrResult xrDestroyActionSet(HandleState* const handleState, XrActionSet actionSet) override;

    //
    // Defined in Space.cpp
    //
    // TODO/FIXME: Generated code assumes the first handle (action) is the parent, but for an action space, the parent is actually the session.
    //             This should resolve itself when XrAction/XrActionSet becomes parented from XrInstance because the first parameter will be
    //             XrSession instead. If this is not resolved, then the auto-generated code needs a hack so that destroying an XrAction does not
    //             remove the action space from the lookup table.
    //XrResult xrCreateActionSpace(HandleState* const handleState, XrAction action, const XrActionSpaceCreateInfo* createInfo,
    //                             XrSpace* space) override;
    //XrResult xrCreateReferenceSpace(HandleState* const handleState, XrSession session, const XrReferenceSpaceCreateInfo* createInfo,
    //                                XrSpace* space) override;
    //XrResult xrDestroySpace(HandleState* const handleState, XrSpace space) override;
    XrResult xrLocateSpace(HandleState* const handleState, XrSpace space, XrSpace baseSpace, XrTime time,
                           XrSpaceLocation* location) override;

    //
    // Defined in Swapchain.cpp
    //
    XrResult xrCreateSwapchain(HandleState* const handleState, XrSession session, const XrSwapchainCreateInfo* createInfo,
                               XrSwapchain* swapchain) override;
    XrResult xrDestroySwapchain(HandleState* const handleState, XrSwapchain swapchain) override;
    XrResult xrEnumerateSwapchainImages(HandleState* const handleState, XrSwapchain swapchain, uint32_t imageCapacityInput,
                                        uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images) override;
    XrResult xrAcquireSwapchainImage(HandleState* const handleState, XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo,
                                     uint32_t* index) override;
    XrResult xrWaitSwapchainImage(HandleState* const handleState, XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo) override;
    XrResult xrReleaseSwapchainImage(HandleState* const handleState, XrSwapchain swapchain,
                                     const XrSwapchainImageReleaseInfo* releaseInfo) override;

#if defined(XR_USE_PLATFORM_ANDROID)
    XrResult xrCreateSwapchainAndroidSurfaceKHR(HandleState* const handleState, XrSession session, const XrSwapchainCreateInfo* info,
                                                XrSwapchain* swapchain, jobject* surface) override;
#endif  // defined(XR_USE_PLATFORM_ANDROID)

#if defined(XR_USE_GRAPHICS_API_VULKAN)
    //
    // Defined in ApiVulkan.cpp
    //
    XrResult xrCreateVulkanDeviceKHR(HandleState* const handleState, XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo,
                                     VkDevice* vulkanDevice, VkResult* vulkanResult) override;
#endif  // defined(XR_USE_GRAPHICS_API_VULKAN)

#if 0
    // TODO (Warning this will become stale!)
    XrResult xrGetInstanceProcAddr(HandleState* const handleState, XrInstance instance, const char* name, PFN_xrVoidFunction* function) override;
    XrResult xrDestroyInstance(HandleState* const handleState, XrInstance instance) override;
    XrResult xrGetInstanceProperties(HandleState* const handleState, XrInstance instance, XrInstanceProperties* instanceProperties) override;
    XrResult xrPollEvent(HandleState* const handleState, XrInstance instance, XrEventDataBuffer* eventData) override;
    XrResult xrResultToString(HandleState* const handleState, XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE]) override;
    XrResult xrStructureTypeToString(HandleState* const handleState, XrInstance instance, XrStructureType value, char buffer[XR_MAX_STRUCTURE_NAME_SIZE]) override;
    XrResult xrGetSystem(HandleState* const handleState, XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override;
    XrResult xrGetReferenceSpaceBoundsRect(HandleState* const handleState, XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds) override;
    XrResult xrGetViewConfigurationProperties(HandleState* const handleState, XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, XrViewConfigurationProperties* configurationProperties) override;
    XrResult xrEnumerateViewConfigurationViews(HandleState* const handleState, XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views) override;
    XrResult xrStringToPath(HandleState* const handleState, XrInstance instance, const char* pathString, XrPath* path) override;
    XrResult xrPathToString(HandleState* const handleState, XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer) override;
    XrResult xrCreateActionSet(HandleState* const handleState, XrSession session, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet) override;
    XrResult xrDestroyActionSet(HandleState* const handleState, XrActionSet actionSet) override;
    XrResult xrCreateAction(HandleState* const handleState, XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action) override;
    XrResult xrDestroyAction(HandleState* const handleState, XrAction action) override;
    XrResult xrSuggestInteractionProfileBindings(HandleState* const handleState, const HandleState& handleState, XrSession session, const XrInteractionProfileSuggestedBinding* suggestedBindings) override;
    XrResult xrGetCurrentInteractionProfile(HandleState* const handleState, XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile) override;
    XrResult xrEnumerateBoundSourcesForAction(HandleState* const handleState, XrAction action, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources) override;
    XrResult xrGetInputSourceLocalizedName(HandleState* const handleState, XrSession session, XrPath source, XrInputSourceLocalizedNameFlags whichComponents, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer) override;
    XrResult xrApplyHapticFeedback(HandleState* const handleState, XrAction hapticAction, uint32_t countSubactionPaths, const XrPath* subactionPaths, const XrHapticBaseHeader* hapticEvent) override;
    XrResult xrStopHapticFeedback(HandleState* const handleState, XrAction hapticAction, uint32_t countSubactionPaths, const XrPath* subactionPaths) override;
#endif

private:
    /// returns true if there are array outputs to validate.
    bool checkTwoCallIdiomFunc(const char* function, XrResult result, uint32_t capacityInput, const uint32_t* countOutput, void* array);

    /// fallback for event types for which we have no further verification
    void checkEventPayload(const void*)
    {
    }
    void checkEventPayload(const XrEventDataEventsLost* data);
    void checkEventPayload(const XrEventDataInstanceLossPending* data);
    void checkEventPayload(const XrEventDataSessionStateChanged* data);
    void checkEventPayload(const XrEventDataReferenceSpaceChangePending* data);
    void checkEventPayload(const XrEventDataInteractionProfileChanged* data);
    void checkEventPayload(const XrEventDataVisibilityMaskChangedKHR* data);
    void checkEventPayload(const XrEventDataPerfSettingsEXT* data);
    void checkEventPayload(const XrEventDataSpatialAnchorCreateCompleteFB* data);
    void checkEventPayload(const XrEventDataUserPresenceChangedEXT* data);
};
