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

#include "ConformanceHooks.h"
#include "CustomHandleState.h"
#include "HandleState.h"
#include "RuntimeFailure.h"

using namespace swapchain;

namespace
{
    const char* ToStr(ImageState state)
    {
        switch (state) {
        case ImageState::Created:
            return "Created";
        case ImageState::Acquired:
            return "Acquired";
        case ImageState::Waited:
            return "Waited";
        case ImageState::Released:
            return "Released";
        default:
            return "unknown";
        }
    }
}  // namespace

namespace swapchain
{
    HandleState* GetSwapchainState(XrSwapchain handle)
    {
        return GetHandleState({HandleToInt(handle), XR_OBJECT_TYPE_SWAPCHAIN});
    }

    CustomSwapchainState* GetCustomSwapchainState(XrSwapchain handle)
    {
        return dynamic_cast<CustomSwapchainState*>(GetSwapchainState(handle)->GetCustomState());
    }

    CustomSwapchainState* GetCustomSwapchainState(HandleState* handleState)
    {
        return dynamic_cast<CustomSwapchainState*>(handleState->GetCustomState());
    }

}  // namespace swapchain

/////////////////
// ABI
/////////////////

XrResult ConformanceHooks::xrCreateSwapchain(HandleState* const handleState, XrSession session, const XrSwapchainCreateInfo* createInfo,
                                             XrSwapchain* swapchain)
{
    const XrResult result = ConformanceHooksBase::xrCreateSwapchain(handleState, session, createInfo, swapchain);
    if (XR_SUCCEEDED(result)) {
        // Tag on the custom swapchain state to the generated handle state.
        session::CustomSessionState* customSessionState = session::GetCustomSessionState(handleState);
        GetSwapchainState(*swapchain)->SetCustomState(std::make_unique<CustomSwapchainState>(createInfo, customSessionState));
    }
    return result;
}

XrResult ConformanceHooks::xrDestroySwapchain(HandleState* const handleState, XrSwapchain swapchain)
{
    CustomSwapchainState* const swapchainData = GetCustomSwapchainState(handleState);
    // There is no CustomSwapchainState for XrSwapchain handles created via
    // xrCreateSwapchainAndroidSurfaceKHR(), so make sure to check for null before using it.
    auto validator = swapchainData ? swapchainData->sessionState->graphicsValidator : nullptr;

    if (validator) {
        validator->AllowVkQueueAccess(false);
    }

    const XrResult result = ConformanceHooksBase::xrDestroySwapchain(handleState, swapchain);

    if (validator) {
        NONCONFORMANT_IF(!validator->CheckState(), "Invalid graphics state");
    }
    return result;
}

XrResult ConformanceHooks::xrEnumerateSwapchainImages(HandleState* const handleState, XrSwapchain swapchain, uint32_t imageCapacityInput,
                                                      uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images)
{
    CustomSwapchainState* const customSwapchainState = GetCustomSwapchainState(handleState);
    auto validator = customSwapchainState->sessionState->graphicsValidator;

    if (validator) {
        validator->AllowVkQueueAccess(false);
    }

    const XrResult result =
        ConformanceHooksBase::xrEnumerateSwapchainImages(handleState, swapchain, imageCapacityInput, imageCountOutput, images);

    if (validator) {
        NONCONFORMANT_IF(!validator->CheckState(), "Invalid graphics state");
    }

    if (XR_SUCCEEDED(result)) {
        if (imageCountOutput != nullptr) {
            std::unique_lock<std::recursive_mutex> lock(customSwapchainState->mutex);

            NONCONFORMANT_IF(*imageCountOutput == 0, "Invalid empty image count.");

            NONCONFORMANT_IF(*imageCountOutput != 1 && customSwapchainState->isStatic, "Invalid image count %d for static swapchain.",
                             *imageCountOutput);

            if (customSwapchainState->imageStates.empty()) {
                // Set up initial image states once the capacity is known.
                customSwapchainState->imageStates.resize(*imageCountOutput, ImageState::Created);
            }

            NONCONFORMANT_IF(customSwapchainState->imageStates.size() != *imageCountOutput,
                             "Image count %d differs from previous count %d.", *imageCountOutput,
                             (uint32_t)customSwapchainState->imageStates.size());

            if (images != nullptr) {
                if (validator) {
                    validator->ValidateSwapchainImageStructs(this, customSwapchainState->createInfo.format, *imageCountOutput, images);
                    validator->ValidateUsageFlags(this, customSwapchainState->createInfo.usageFlags, *imageCountOutput, images);
                }
            }
        }
    }
    return result;
}

XrResult ConformanceHooks::xrAcquireSwapchainImage(HandleState* const handleState, XrSwapchain swapchain,
                                                   const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index)
{
    CustomSwapchainState* const swapchainData = GetCustomSwapchainState(handleState);
    auto validator = swapchainData->sessionState->graphicsValidator;

    if (validator) {
        validator->AllowVkQueueAccess(true);
    }

    const XrResult result = ConformanceHooksBase::xrAcquireSwapchainImage(handleState, swapchain, acquireInfo, index);

    if (validator) {
        NONCONFORMANT_IF(!validator->CheckState(), "Invalid graphics state");
    }

    if (XR_SUCCEEDED(result)) {
        std::unique_lock<std::recursive_mutex> lock(swapchainData->mutex);

        if (swapchainData->imageStates.empty()) {
            // Must enumerate the swapchain images to set up the imageStates vector to the correct size.
            // This is an unusual situation because it means the app is calling xrAcquireSwapchainImage without first enumerating the swapchain images.
            uint32_t imageCountOutput;
            const XrResult enumRes = ConformanceHooks::xrEnumerateSwapchainImages(handleState, swapchain, 0, &imageCountOutput, nullptr);
            NONCONFORMANT_IF(!XR_SUCCEEDED(enumRes), "Unable to enumerate swapchain images due to error %s", to_string(enumRes));
        }
        else {
            NONCONFORMANT_IF(*index >= swapchainData->imageStates.size(), "Out-of-bounds image index.");
        }

        ImageState& imageState = swapchainData->imageStates[*index];

        NONCONFORMANT_IF(imageState == ImageState::Waited, "Acquired image in Waited state.");
        NONCONFORMANT_IF(imageState == ImageState::Acquired, "Acquired image already in Acquired state.");
        NONCONFORMANT_IF(imageState == ImageState::Released && swapchainData->isStatic, "Static image cannot be acquired again.");

        imageState = ImageState::Acquired;
        swapchainData->acquiredSwapchains.push(*index);
    }
    return result;
}

XrResult ConformanceHooks::xrWaitSwapchainImage(HandleState* const handleState, XrSwapchain swapchain,
                                                const XrSwapchainImageWaitInfo* waitInfo)
{
    auto waitStart = std::chrono::high_resolution_clock::now();

    CustomSwapchainState* const swapchainData = GetCustomSwapchainState(handleState);
    auto validator = swapchainData->sessionState->graphicsValidator;

    if (validator) {
        validator->AllowVkQueueAccess(false);
    }

    const XrResult result = ConformanceHooksBase::xrWaitSwapchainImage(handleState, swapchain, waitInfo);
    if (validator) {
        NONCONFORMANT_IF(!validator->CheckState(), "Invalid graphics state");
    }

    if (result == XR_TIMEOUT_EXPIRED) {
        XrDuration waitDuration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - waitStart).count();
        NONCONFORMANT_IF(waitDuration < waitInfo->timeout, "Wait returned before timeout.");
    }
    else if (result == XR_SUCCESS) {
        std::unique_lock<std::recursive_mutex> lock(swapchainData->mutex);

        if (!swapchainData->acquiredSwapchains.empty()) {
            const uint32_t waitIndex = swapchainData->acquiredSwapchains.front();
            ImageState& imageState = swapchainData->imageStates[waitIndex];
            NONCONFORMANT_IF(imageState != ImageState::Acquired, "Wait succeeded for image in wrong state %s", ToStr(imageState));

            imageState = ImageState::Waited;
        }
        else {
            NONCONFORMANT("Wait succeeded with no acquired image.");
        }
    }
    else if (result == XR_TIMEOUT_EXPIRED) {
        // No change in state.
    }
    return result;
}

XrResult ConformanceHooks::xrReleaseSwapchainImage(HandleState* const handleState, XrSwapchain swapchain,
                                                   const XrSwapchainImageReleaseInfo* releaseInfo)
{
    CustomSwapchainState* const swapchainData = GetCustomSwapchainState(handleState);
    auto validator = swapchainData->sessionState->graphicsValidator;

    if (validator) {
        validator->AllowVkQueueAccess(true);
    }

    const XrResult result = ConformanceHooksBase::xrReleaseSwapchainImage(handleState, swapchain, releaseInfo);

    if (validator) {
        NONCONFORMANT_IF(!validator->CheckState(), "Invalid graphics state");
    }

    if (XR_SUCCEEDED(result)) {
        std::unique_lock<std::recursive_mutex> lock(swapchainData->mutex);

        if (!swapchainData->acquiredSwapchains.empty()) {
            const uint32_t waitIndex = swapchainData->acquiredSwapchains.front();
            ImageState& imageState = swapchainData->imageStates[waitIndex];
            NONCONFORMANT_IF(imageState != ImageState::Waited, "Release succeeded for image in wrong state %s", ToStr(imageState));

            imageState = ImageState::Released;
            swapchainData->acquiredSwapchains.pop();
        }
        else {
            NONCONFORMANT("Release succeeded with no acquired image.");
        }
    }
    return result;
}

#if defined(XR_USE_PLATFORM_ANDROID)
XrResult ConformanceHooks::xrCreateSwapchainAndroidSurfaceKHR(HandleState* const handleState, XrSession session,
                                                              const XrSwapchainCreateInfo* info, XrSwapchain* swapchain, jobject* surface)
{
    const XrResult result = ConformanceHooksBase::xrCreateSwapchainAndroidSurfaceKHR(handleState, session, info, swapchain, surface);
    if (XR_SUCCEEDED(result)) {
        // Tag on the custom swapchain state to the generated handle state.
        session::CustomSessionState* customSessionState = session::GetCustomSessionState(handleState);
        GetSwapchainState(*swapchain)->SetCustomState(std::make_unique<CustomSwapchainState>(info, customSessionState));
    }
    return result;
}
#endif  // defined(XR_USE_PLATFORM_ANDROID)
