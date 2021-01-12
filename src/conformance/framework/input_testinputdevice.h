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

#include "composition_utils.h"

namespace Conformance
{
    struct InputSourcePathData
    {
        std::string Path;
        XrActionType Type;
    };

    using InteractionProfileWhitelistData = std::vector<InputSourcePathData>;

    // clang-format off

    const InteractionProfileWhitelistData cSimpleControllerIPData{
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

    const InteractionProfileWhitelistData cGoogleDaydreamControllerIPData{
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

    const InteractionProfileWhitelistData cViveControllerIPData{
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

    const InteractionProfileWhitelistData cViveProIPData{
        {"/user/head/input/volume_up/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/head/input/volume_down/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/head/input/mute_mic/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
    };

    const InteractionProfileWhitelistData cWMRControllerIPData{
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

    const InteractionProfileWhitelistData cGamepadIPData{
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

    const InteractionProfileWhitelistData cOculusGoIPData{
        {"/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/back/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT},
        {"/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT},
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

    const InteractionProfileWhitelistData cOculusTouchIPData{
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
        // The system ("Oculus") button is reserved for system applications
        // {"/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT},
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

    const InteractionProfileWhitelistData cValveIndexIPData{
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
        InteractionProfileWhitelistData WhitelistData;
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

        virtual void SetDeviceActive(bool state, bool skipInteraction = false) = 0;
        virtual void SetButtonStateBool(XrPath button, bool state, bool skipInteraction = false, XrActionSet extraActionSet = XR_NULL_HANDLE ) = 0;
        virtual void SetButtonStateFloat(XrPath button, float state, float epsilon = 0, bool skipInteraction = false, XrActionSet extraActionSet = XR_NULL_HANDLE ) = 0;
        virtual void SetButtonStateVector2(XrPath button, XrVector2f state, float epsilon = 0, bool skipInteraction = false, XrActionSet extraActionSet = XR_NULL_HANDLE ) = 0;
    };

    struct ITestMessageDisplay
    {
        virtual ~ITestMessageDisplay() = default;

        virtual void DisplayMessage(const std::string& message) = 0;

        virtual void IterateFrame() = 0;
    };

    std::unique_ptr<IInputTestDevice> CreateTestDevice(ITestMessageDisplay* const messageDisplay,
                                                       InteractionManager* const interactionManager, XrInstance instance, XrSession session,
                                                       XrPath interactionProfile, XrPath topLevelPath,
                                                       InteractionProfileWhitelistData interactionProfilePaths);
}  // namespace Conformance
