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

#pragma once

#include <initializer_list>
#include "composition_utils.h"

namespace Conformance
{
    struct InputSourcePathData
    {
        const char* Path;
        XrActionType Type;
        bool systemOnly;
    };

    using InputSourcePathCollection = std::initializer_list<InputSourcePathData>;

    // clang-format off

    const InputSourcePathCollection cSimpleControllerIPData{
        {"/user/hand/left/input/select/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"/user/hand/right/input/select/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
    };

    const InputSourcePathCollection cGoogleDaydreamControllerIPData{
        {"/user/hand/left/input/select/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/select/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
    };

    const InputSourcePathCollection cViveControllerIPData{
        {"/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT},
        // TODO should we include this? value should coerce to click on conformant systems
        {"/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT},
        // TODO should we include this? value should coerce to click on conformant systems
        {"/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
    };

    const InputSourcePathCollection cViveProIPData{
        {"/user/head/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/head/input/volume_up/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/head/input/volume_down/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/head/input/mute_mic/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
    };

    const InputSourcePathCollection cWMRControllerIPData{
        {"/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        // TODO should we include these or just infer them from /x and /y?
        {"/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
    };

    const InputSourcePathCollection cGamepadIPData{
        {"/user/gamepad/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/view/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/x/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/y/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/dpad_down/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/dpad_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/dpad_up/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/dpad_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/shoulder_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/shoulder_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/thumbstick_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/thumbstick_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/gamepad/input/trigger_left/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/gamepad/input/trigger_right/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/gamepad/input/thumbstick_left/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/gamepad/input/thumbstick_left/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/gamepad/input/thumbstick_left", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/gamepad/input/thumbstick_right/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/gamepad/input/thumbstick_right/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/gamepad/input/thumbstick_right", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/gamepad/output/haptic_left", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"/user/gamepad/output/haptic_right", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"/user/gamepad/output/haptic_left_trigger", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"/user/gamepad/output/haptic_right_trigger", XR_ACTION_TYPE_VIBRATION_OUTPUT},
    };

    const InputSourcePathCollection cOculusGoIPData{
        {"/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/back/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/back/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
    };

    const InputSourcePathCollection cOculusTouchIPData{
        {"/user/hand/left/input/x/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/x/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/y/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/y/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trigger/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/thumbstick/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT},
        // Rift S and Quest controllers lack thumbrests
        // {"/user/hand/left/input/thumbrest/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"/user/hand/right/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/a/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/b/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/right/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trigger/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/thumbstick/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT},
        // Rift S and Quest controllers lack thumbrests
        // {"/user/hand/right/input/thumbrest/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
    };

    const InputSourcePathCollection cValveIndexIPData{
        {"/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/left/input/system/touch", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/left/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/a/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/b/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/squeeze/force", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trigger/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/thumbstick/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/force", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/right/input/system/touch", XR_ACTION_TYPE_BOOLEAN_INPUT, true},
        {"/user/hand/right/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/a/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/b/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/squeeze/force", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trigger/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/thumbstick/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/force", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
    };

    // clang-format on

    struct InteractionProfileMetadata
    {
        std::string InteractionProfilePathString;
        std::string InteractionProfileShortname;
        std::vector<std::string> TopLevelPaths;
        InputSourcePathCollection InputSourcePaths;
    };

    // Define this separately as it is useful to reference directly
    const InteractionProfileMetadata cSimpleKHRInteractionProfileDefinition{
        "/interaction_profiles/khr/simple_controller",
        // The prefix has been removed from these as Catch2 cannot handle args that begin with a forward slash and it cannot be escaped
        "khr/simple_controller",
        {"/user/hand/left", "/user/hand/right"},
        cSimpleControllerIPData};

    const InteractionProfileMetadata cInteractionProfileDefinitions[] = {
        cSimpleKHRInteractionProfileDefinition,
        {"/interaction_profiles/google/daydream_controller",
         "google/daydream_controller",
         {"/user/hand/left", "/user/hand/right"},
         cGoogleDaydreamControllerIPData},
        {"/interaction_profiles/htc/vive_controller",
         "htc/vive_controller",
         {"/user/hand/left", "/user/hand/right"},
         cViveControllerIPData},
        {"/interaction_profiles/htc/vive_pro", "htc/vive_pro", {"/user/head"}, cViveProIPData},
        {"/interaction_profiles/microsoft/motion_controller",
         "microsoft/motion_controller",
         {"/user/hand/left", "/user/hand/right"},
         cWMRControllerIPData},
        {"/interaction_profiles/microsoft/xbox_controller", "microsoft/xbox_controller", {"/user/gamepad"}, cGamepadIPData},
        {"/interaction_profiles/oculus/go_controller", "oculus/go_controller", {"/user/hand/left", "/user/hand/right"}, cOculusGoIPData},
        {"/interaction_profiles/oculus/touch_controller",
         "oculus/touch_controller",
         {"/user/hand/left", "/user/hand/right"},
         cOculusTouchIPData},
        {"/interaction_profiles/valve/index_controller",
         "valve/index_controller",
         {"/user/hand/left", "/user/hand/right"},
         cValveIndexIPData}};

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
                                                       const InputSourcePathCollection& interactionProfilePaths);

    std::unique_ptr<IInputTestDevice> CreateTestDevice(ITestMessageDisplay* const messageDisplay, XrInstance instance, XrSession session,
                                                       XrPath interactionProfile, XrPath topLevelPath, XrActionSet actionSet,
                                                       XrAction firstBooleanAction, std::map<XrPath, XrAction>& actionMap);
}  // namespace Conformance
