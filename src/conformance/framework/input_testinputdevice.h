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

#include "interaction_info.h"

#include <openxr/openxr.h>

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Conformance
{
    struct InteractionManager;

    struct IInputTestDevice
    {
        virtual ~IInputTestDevice() = default;
        virtual XrPath TopLevelPath() const = 0;

        /// Set device active or inactive
        ///
        /// This will run xrSyncActions with an internally-defined action set to detect when the device is on/off!
        /// (unless skipInteraction)
        ///
        /// @param state activation or deactivate device
        /// @param skipInteraction Skip human interaction (i.e. this is a hint for the conformance extension)
        /// @param detectionBoolAction Boolean action used to determine if device became active
        /// @param detectionActionSet Action Set associated with detectionBoolAction
        virtual void SetDeviceActive(bool state, bool skipInteraction = false, XrAction detectionBoolAction = XR_NULL_HANDLE,
                                     XrActionSet detectionActionSet = XR_NULL_HANDLE) = 0;

        /// Call xrLocateSpace until XR_SPACE_LOCATION_ORIENTATION_VALID matches the desired state
        struct WaitUntilLosesOrGainsOrientationValidity
        {
            XrSpace actionSpace;
            XrSpace baseSpace;
            XrTime initialLocateTime;
        };

        /// When passed to IInputTestDevice::SetDeviceActive, will call xrSyncActions until a bool action
        /// on the same controller reports isActive equal to the desired state.
        struct WaitUntilBoolActionIsActiveUpdated
        {
            XrAction detectionBoolAction = XR_NULL_HANDLE;
            XrActionSet detectionActionSet = XR_NULL_HANDLE;
        };

        /// Set device active or inactive (displaying message), but do not wait.
        ///
        /// Will use conformance automation extension if available.
        ///
        /// @param state activation or deactivate device
        /// @param extraMessage text to append to the end of the message, if any.
        virtual void SetDeviceActiveWithoutWaiting(bool state, const char* extraMessage = nullptr) const = 0;

        /// Loop while running xrSyncActions, until an action reports its active state matching @p state.
        ///
        /// @param state whether to await activation or deactivation of device
        /// @param waitCondition Tag struct with optional parameters
        virtual void Wait(bool state, const WaitUntilBoolActionIsActiveUpdated& waitCondition) const = 0;

        /// Loop while running xrLocateSpace, until the presence or absence of XR_SPACE_LOCATION_ORIENTATION_VALID
        /// matches the @p state.
        ///
        /// @param state whether to await activation or deactivation of device
        /// @param waitCondition Tag struct with required parameters
        virtual XrTime Wait(bool state, const WaitUntilLosesOrGainsOrientationValidity& waitCondition) const = 0;

        /// This will run xrSyncActions with an internally-defined action set to wait until the state occurs!
        /// (unless skipInteraction is true)
        virtual void SetButtonStateBool(XrPath button, bool state, bool skipInteraction = false,
                                        XrActionSet extraActionSet = XR_NULL_HANDLE) = 0;
        /// This will run xrSyncActions with an internally-defined action set to wait until the state occurs!
        /// (unless skipInteraction is true)
        virtual void SetButtonStateFloat(XrPath button, float state, float epsilon = 0, bool skipInteraction = false,
                                         XrActionSet extraActionSet = XR_NULL_HANDLE) = 0;
        /// This will run xrSyncActions with an internally-defined action set to wait until the state occurs!
        /// (unless skipInteraction is true)
        virtual void SetButtonStateVector2(XrPath button, XrVector2f state, float epsilon = 0, bool skipInteraction = false,
                                           XrActionSet extraActionSet = XR_NULL_HANDLE) = 0;
    };

    struct ITestMessageDisplay
    {
        virtual ~ITestMessageDisplay() = default;

        virtual void DisplayMessage(const std::string& message) = 0;

        virtual void IterateFrame() = 0;

        // virtual XrTime GetLastPredictedDisplayTime() const = 0;
    };

    std::unique_ptr<IInputTestDevice> CreateTestDevice(ITestMessageDisplay* const messageDisplay,
                                                       InteractionManager* const interactionManager, XrInstance instance, XrSession session,
                                                       XrPath interactionProfile, XrPath topLevelPath,
                                                       const InputSourcePathAvailCollection& interactionProfilePaths);

    std::unique_ptr<IInputTestDevice> CreateTestDevice(ITestMessageDisplay* const messageDisplay, XrInstance instance, XrSession session,
                                                       XrPath interactionProfile, XrPath topLevelPath, XrActionSet actionSet,
                                                       XrAction firstBooleanAction, std::map<XrPath, XrAction>& actionMap);
}  // namespace Conformance
