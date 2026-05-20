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

#include "action_utils.h"
#include "composition_utils.h"
#include "conformance_framework.h"

#include <catch2/catch_message.hpp>
#include <openxr/openxr.h>
#include <algorithm>
#include <thread>

namespace Conformance
{
    namespace
    {
        XrActionSet CreateHapticActionSet(XrInstance instance)
        {
            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.actionSetName, "haptics");
            strcpy(actionSetCreateInfo.localizedActionSetName, "haptics");

            XrActionSet actionSet = XR_NULL_HANDLE;
            REQUIRE(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet) == XR_SUCCESS);
            return actionSet;
        }

        XrAction CreateAction(XrActionSet actionSet, const std::array<XrPath, 2> subactionPaths, XrActionType actionType,
                              const char* actionName)
        {
            XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionCreateInfo.subactionPaths = subactionPaths.data();
            actionCreateInfo.countSubactionPaths = static_cast<uint32_t>(subactionPaths.size());
            actionCreateInfo.actionType = actionType;
            strcpy(actionCreateInfo.localizedActionName, actionName);
            strcpy(actionCreateInfo.actionName, actionName);

            XrAction action = XR_NULL_HANDLE;
            REQUIRE(xrCreateAction(actionSet, &actionCreateInfo, &action) == XR_SUCCESS);
            return action;
        }

        XrSpace CreateActionSpace(XrSession session, XrAction action, const std::array<XrPath, 2> subactionPaths)
        {
            XrActionSpaceCreateInfo actionSpaceCreateInfo = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
            actionSpaceCreateInfo.action = action;
            actionSpaceCreateInfo.poseInActionSpace.orientation.w = 1.0f;
            actionSpaceCreateInfo.subactionPath = subactionPaths[1];

            XrSpace space = XR_NULL_HANDLE;
            REQUIRE(xrCreateActionSpace(session, &actionSpaceCreateInfo, &space) == XR_SUCCESS);
            return space;
        }

        XrAction CreatePoseAction(XrActionSet actionSet, std::array<XrPath, 2> subactionPaths)
        {
            return CreateAction(actionSet, subactionPaths, XR_ACTION_TYPE_POSE_INPUT, "pose");
        }

        XrAction CreateHapticAction(XrActionSet actionSet, std::array<XrPath, 2> subactionPaths)
        {
            return CreateAction(actionSet, subactionPaths, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptics");
        }

        XrHapticActionInfo CreateHapticActionInfo(XrAction hapticAction, XrPath subactionPath)
        {
            XrHapticActionInfo hapticActionInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
            hapticActionInfo.action = hapticAction;
            hapticActionInfo.subactionPath = subactionPath;
            return hapticActionInfo;
        }

        void BindAction(XrInstance instance, InteractionManager& interactionManager, XrAction action, const char* leftPath,
                        const char* rightPath)
        {
            const XrPath interactionProfilePath = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
            const XrPath pathLeft = StringToPath(instance, leftPath);
            const XrPath pathRight = StringToPath(instance, rightPath);
            const std::vector<XrActionSuggestedBinding> bindings{{action, pathLeft}, {action, pathRight}};
            interactionManager.AddActionBindings(interactionProfilePath, bindings);
        }

        void BindHapticAction(XrInstance instance, InteractionManager& interactionManager, XrAction hapticAction)
        {
            BindAction(instance, interactionManager, hapticAction, "/user/hand/left/output/haptic", "/user/hand/right/output/haptic");
        }

        void BindGripPoseAction(XrInstance instance, InteractionManager& interactionManager, XrAction gripPoseAction)
        {
            BindAction(instance, interactionManager, gripPoseAction, "/user/hand/left/input/grip/pose", "/user/hand/right/input/grip/pose");
        }

        XrResult ApplyHapticFeedback(XrSession session, const XrHapticActionInfo& hapticActionInfo,
                                     const std::vector<XrHapticParametricPointEXT>& amplitudePoints,
                                     const std::vector<XrHapticParametricPointEXT>& frequencyPoints,
                                     const std::vector<XrHapticParametricTransientEXT>& transients, float minFrequencyHz,
                                     float maxFrequencyHz, XrHapticParametricStreamFrameTypeEXT streamFrameType)
        {
            XrHapticParametricVibrationEXT vibration{XR_TYPE_HAPTIC_PARAMETRIC_VIBRATION_EXT};
            vibration.amplitudePointCount = static_cast<uint32_t>(amplitudePoints.size());
            vibration.amplitudePoints = amplitudePoints.data();
            vibration.frequencyPointCount = static_cast<uint32_t>(frequencyPoints.size());
            vibration.frequencyPoints = frequencyPoints.data();
            vibration.transientCount = static_cast<uint32_t>(transients.size());
            vibration.transients = transients.data();
            vibration.minFrequencyHz = minFrequencyHz;
            vibration.maxFrequencyHz = maxFrequencyHz;
            vibration.streamFrameType = streamFrameType;
            return xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
        }

        XrHapticParametricPropertiesEXT GetProperties(XrInstance instance, XrSession session, const XrHapticActionInfo& hapticActionInfo)
        {
            const auto xrHapticParametricGetPropertiesEXT =
                GetInstanceExtensionFunction<PFN_xrHapticParametricGetPropertiesEXT>(instance, "xrHapticParametricGetPropertiesEXT");
            XrHapticParametricPropertiesEXT properties = {XR_TYPE_HAPTIC_PARAMETRIC_PROPERTIES_EXT};
            REQUIRE(xrHapticParametricGetPropertiesEXT(session, &hapticActionInfo, &properties) == XR_SUCCESS);
            return properties;
        }

        struct TimingProperties
        {
            XrDuration firstFrameDuration;
            XrDuration frameSubmissionRate;
        };
        TimingProperties GetTimingProperties(XrHapticParametricPropertiesEXT properties)
        {
            // Values to use when the runtime doesn't have a preference and returns 0
            constexpr XrDuration fallbackFirstFrameDuration = 50'000'000;   // 50ms
            constexpr XrDuration fallbackFrameSubmissionRate = 13'888'000;  // 72 FPS

            const XrDuration firstFrameDuration =
                properties.minimumFirstFrameDuration == 0 ? fallbackFirstFrameDuration : properties.minimumFirstFrameDuration;
            const XrDuration frameSubmissionRate =
                properties.idealFrameSubmissionRate == 0 ? fallbackFrameSubmissionRate : properties.idealFrameSubmissionRate;
            return {firstFrameDuration, frameSubmissionRate};
        }

        void StopHapticFeedback(XrSession session, const XrHapticActionInfo& hapticActionInfo)
        {
            REQUIRE(xrStopHapticFeedback(session, &hapticActionInfo) == XR_SUCCESS);
        }

        bool CheckExtensionSupport()
        {
            const bool extensionSupported = GetGlobalData().IsInstanceExtensionSupported(XR_EXT_HAPTIC_PARAMETRIC_EXTENSION_NAME);
            if (!extensionSupported) {
                AutoBasicInstance instance;
                ValidateInstanceExtensionFunctionNotSupported(instance, "xrHapticParametricGetPropertiesEXT");
            }
            return extensionSupported;
        }

        bool CheckSystemSupport(XrInstance instance, XrSession session, XrSystemId systemId, const XrHapticActionInfo& hapticActionInfo)
        {
            XrSystemHapticParametricPropertiesEXT systemHapticParametricProperties{XR_TYPE_SYSTEM_HAPTIC_PARAMETRIC_PROPERTIES_EXT};
            XrSystemProperties systemProperties = {XR_TYPE_SYSTEM_PROPERTIES, &systemHapticParametricProperties};
            REQUIRE(xrGetSystemProperties(instance, systemId, &systemProperties) == XR_SUCCESS);

            // If the system doesn't support parametric haptics, verify that xrHapticParametricGetPropertiesEXT and xrApplyHapticFeedback return
            // XR_ERROR_FEATURE_UNSUPPORTED.
            if (systemHapticParametricProperties.supportsParametricHaptics == XR_FALSE) {
                const auto xrHapticParametricGetPropertiesEXT =
                    GetInstanceExtensionFunction<PFN_xrHapticParametricGetPropertiesEXT>(instance, "xrHapticParametricGetPropertiesEXT");
                XrHapticParametricPropertiesEXT properties = {XR_TYPE_HAPTIC_PARAMETRIC_PROPERTIES_EXT};
                REQUIRE(xrHapticParametricGetPropertiesEXT(session, &hapticActionInfo, &properties) == XR_ERROR_FEATURE_UNSUPPORTED);

                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, {}, {}, {}, XR_FREQUENCY_UNSPECIFIED, XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT) == XR_ERROR_FEATURE_UNSUPPORTED);
            }

            return systemHapticParametricProperties.supportsParametricHaptics == XR_TRUE;
        }

        // Make sure session is focused, as haptic functions like xrApplyHapticFeedback() require that.
        void RunToFocusedState(CompositionHelper& compositionHelper)
        {
            compositionHelper.BeginSession();
            ActionLayerManager actionLayerManager(compositionHelper);
            actionLayerManager.WaitForSessionFocusWithMessage();
        }

        // Make sure the runtime has set up the interaction profile bindings, by calling xrSyncActions() until the
        // XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event has been received. For some runtimes, haptic functions
        // like xrApplyHapticFeedback() require that to be able to resolve the suggested haptic action bindings.
        void RunUntilInteractionProfileChanged(XrInstance instance, FrameIterator& frameIterator, InteractionManager& interactionManager)
        {
            const auto timeout = (Options::Get().debugMode ? 3600s : 10s);
            CountdownTimer countdownTimer(timeout);
            while (!countdownTimer.IsTimeUp()) {
                REQUIRE(frameIterator.SubmitFrame() == FrameIterator::RunResult::Success);
                interactionManager.SyncActions(XR_NULL_PATH);

                XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
                const XrResult result = xrPollEvent(instance, &eventData);
                REQUIRE((result == XR_SUCCESS || result == XR_EVENT_UNAVAILABLE));
                if (result == XR_SUCCESS && eventData.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
                    break;
                }
            }
            REQUIRE(!countdownTimer.IsTimeUp());
        }
    }  // namespace

    // This test automatically evaluates its results, but because it require controllers, it is
    // marked as "interactive".
    TEST_CASE("XR_EXT_haptic_parametric-non_scenario", "[interactive][actions]")
    {
        if (!CheckExtensionSupport()) {
            SKIP(XR_EXT_HAPTIC_PARAMETRIC_EXTENSION_NAME " not supported");
        }

        // Create instance and session
        AutoBasicInstance instance({XR_EXT_HAPTIC_PARAMETRIC_EXTENSION_NAME});
        REQUIRE(instance != XR_NULL_HANDLE_CPP);
        AutoBasicSession session(AutoBasicSession::beginSession | AutoBasicSession::createActions | AutoBasicSession::createSpaces |
                                     AutoBasicSession::createSwapchains,
                                 instance);
        REQUIRE(session != XR_NULL_HANDLE_CPP);

        // Setup actions
        InteractionManager interactionManager(instance, session);
        const XrAction hapticAction = CreateHapticAction(session.actionSet, session.handSubactionArray);
        const XrHapticActionInfo hapticActionInfo = CreateHapticActionInfo(hapticAction, session.handSubactionArray[1]);
        BindHapticAction(instance, interactionManager, hapticAction);
        interactionManager.AddActionSet(session.actionSet);
        interactionManager.AttachActionSets();
        FrameIterator frameIterator(&session);
        frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);
        RunUntilInteractionProfileChanged(instance, frameIterator, interactionManager);

        if (!CheckSystemSupport(instance, session, session.GetSystemId(), hapticActionInfo)) {
            SKIP("System does not support parametric haptics");
        }

        const auto properties = GetProperties(instance, session, hapticActionInfo);
        const auto firstFrameDuration = GetTimingProperties(properties).firstFrameDuration;

        SECTION("Get Properties")
        {
            // The runtime may: populate both with dlink:XR_FREQUENCY_UNSPECIFIED if it
            // does not have the information.
            // Otherwise, pname:maxFrequencyHz must: be pname:minFrequencyHz or higher, and
            // both values must: be between dlink:XR_HAPTIC_PARAMETRIC_FREQUENCY_MIN_HZ_EXT
            // and dlink:XR_HAPTIC_PARAMETRIC_FREQUENCY_MAX_HZ_EXT (inclusive).

            REQUIRE((properties.minFrequencyHz == XR_FREQUENCY_UNSPECIFIED ||
                     (properties.minFrequencyHz >= XR_HAPTIC_PARAMETRIC_FREQUENCY_MIN_HZ_EXT &&
                      properties.minFrequencyHz <= XR_HAPTIC_PARAMETRIC_FREQUENCY_MAX_HZ_EXT)));
            REQUIRE((properties.maxFrequencyHz == XR_FREQUENCY_UNSPECIFIED ||
                     (properties.maxFrequencyHz >= XR_HAPTIC_PARAMETRIC_FREQUENCY_MIN_HZ_EXT &&
                      properties.maxFrequencyHz <= XR_HAPTIC_PARAMETRIC_FREQUENCY_MAX_HZ_EXT)));
            REQUIRE((properties.minFrequencyHz == XR_FREQUENCY_UNSPECIFIED || properties.maxFrequencyHz == XR_FREQUENCY_UNSPECIFIED ||
                     properties.maxFrequencyHz >= properties.minFrequencyHz));
            REQUIRE(properties.idealFrameSubmissionRate >= 0);
            REQUIRE(properties.minimumFirstFrameDuration >= 0);
        }

        SECTION("Validation Failures")
        {
            const auto testValidationFailure = [&session, hapticActionInfo](
                                                   const std::vector<XrHapticParametricPointEXT>& amplitudePoints,
                                                   const std::vector<XrHapticParametricPointEXT>& frequencyPoints = {},
                                                   const std::vector<XrHapticParametricTransientEXT>& transients = {},
                                                   XrHapticParametricStreamFrameTypeEXT streamFrameType =
                                                       XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT) -> bool {
                return ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, transients,
                                           XR_FREQUENCY_UNSPECIFIED, XR_FREQUENCY_UNSPECIFIED,
                                           streamFrameType) == XR_ERROR_VALIDATION_FAILURE;
            };

            const auto testValidationFailureAbsoluteFrequency = [&session, hapticActionInfo](float minAbsoluteFrequencyHz,
                                                                                             float maxAbsoluteFrequencyHz) -> bool {
                const std::vector<XrHapticParametricPointEXT> amplitudePoints = {{0, 0.0f}, {100000000, 1.0f}};
                return ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, minAbsoluteFrequencyHz,
                                           maxAbsoluteFrequencyHz,
                                           XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT) == XR_ERROR_VALIDATION_FAILURE;
            };

            std::vector<XrHapticParametricPointEXT> amplitudePoints;
            std::vector<XrHapticParametricPointEXT> frequencyPoints;
            std::vector<XrHapticParametricTransientEXT> transients;

            // When pname:streamFrameType is ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT or
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT, the
            // structure must: contain at least two amplitude points.
            {
                INFO("Not enough amplitude points");
                amplitudePoints = {{0, 0.0f}};
                REQUIRE(testValidationFailure(amplitudePoints));
            }

            // For all other values of pname:streamFrameType, the structure must: contain
            // at least one amplitude point.
            {
                INFO("No new amplitude point in frame");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints.clear();
                transients = {{firstFrameDuration * 2, 1.0f, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, transients, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_ERROR_VALIDATION_FAILURE);
            }

            // The structure must: not contain more than
            // dlink:XR_HAPTIC_PARAMETRIC_MAX_POINTS_TRANSIENTS_EXT amplitude points,
            // frequency points or transients, respectively.
            {
                INFO("Too many amplitude points");
                amplitudePoints = {{0, 0.0f}};
                for (uint32_t i = 0; i < XR_HAPTIC_PARAMETRIC_MAX_POINTS_TRANSIENTS_EXT; i++) {
                    amplitudePoints.push_back(XrHapticParametricPointEXT{i * 10000000, 0.0f});
                }
                REQUIRE(testValidationFailure(amplitudePoints, {}, {}, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT));
            }
            {
                INFO("Too many frequency points");
                amplitudePoints = {{0, 0.0f}, {190000000, 1.0f}};
                frequencyPoints = {};
                for (uint32_t i = 0; i < XR_HAPTIC_PARAMETRIC_MAX_POINTS_TRANSIENTS_EXT + 1; i++) {
                    frequencyPoints.push_back(XrHapticParametricPointEXT{i * 10000000, 0.0f});
                }
                REQUIRE(testValidationFailure(amplitudePoints, frequencyPoints, {}, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT));
            }
            {
                INFO("Too many transients");
                amplitudePoints = {{0, 0.0f}, {190000000, 1.0f}};
                transients = {};
                for (uint32_t i = 0; i < XR_HAPTIC_PARAMETRIC_MAX_POINTS_TRANSIENTS_EXT + 1; i++) {
                    transients.push_back(XrHapticParametricTransientEXT{i * 10000000, 1.0f, 1.0f});
                }
                REQUIRE(testValidationFailure(amplitudePoints, {}, transients, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT));
            }

            // When pname:streamFrameType is
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT or
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT, the first
            // amplitude and frequency point must: be at time 0.
            {
                INFO("First amplitude point not at zero");
                amplitudePoints = {{10000000, 0.0f}, {190000000, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, {}, {}, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT));
            }
            {
                INFO("First frequency point not at zero");
                amplitudePoints = {{0, 0.0f}, {190000000, 1.0f}};
                frequencyPoints = {{10000000, 0.0f}, {190000000, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, frequencyPoints, {}, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT));
            }

            // When pname:streamFrameType is
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT, the time of
            // the last amplitude point in the frame must: be greater than or equal to
            // slink:XrHapticParametricPropertiesEXT::pname:minimumFirstFrameDuration.
            if (properties.minimumFirstFrameDuration > 0) {
                INFO("First frame duration too short");
                amplitudePoints = {{0, 0.0f}, {properties.minimumFirstFrameDuration - 1, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, {}, {}, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT));
            }

            // The time of the last frequency point and the last transient of the stream
            // must: be less than or equal to the time of the last amplitude point of the
            // stream.
            {
                INFO("Frequency point after last amplitude point");
                amplitudePoints = {{0, 0.0f}, {190000000, 1.0f}};
                frequencyPoints = {{0, 0.0f}, {200000000, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, frequencyPoints, {}, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT));
            }
            {
                INFO("Transient after last amplitude point");
                amplitudePoints = {{0, 0.0f}, {190000000, 1.0f}};
                transients = {{200000000, 1.0f, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, {}, transients, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT));
            }
            {
                INFO("Frequency point after last amplitude in last frame");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                frequencyPoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints = {{firstFrameDuration + 50000000, 1.0f}};
                frequencyPoints = {{firstFrameDuration + 100000000, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_ERROR_VALIDATION_FAILURE);
            }
            {
                INFO("Transient after last amplitude point in in last frame");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints = {{firstFrameDuration + 50000000, 1.0f}};
                transients = {{firstFrameDuration + 100000000, 1.0f, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, transients, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_ERROR_VALIDATION_FAILURE);
            }

            // When pname:streamFrameType is
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_INTERMEDIATE_FRAME_EXT or
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT, a stream
            // must: already be running on the haptic element.
            {
                INFO("Incorrect frame type");
                amplitudePoints = {{0, 0.0f}, {1000000000, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, {}, {}, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_INTERMEDIATE_FRAME_EXT));
                REQUIRE(testValidationFailure(amplitudePoints, {}, {}, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT));
            }

            // pname:minFrequencyHz and pname:maxFrequencyHz must: either be both
            // dlink:XR_FREQUENCY_UNSPECIFIED, or both specified in the range
            // dlink:XR_HAPTIC_PARAMETRIC_FREQUENCY_MIN_HZ_EXT -
            // dlink:XR_HAPTIC_PARAMETRIC_FREQUENCY_MAX_HZ_EXT (inclusive).
            {
                INFO("Only one absolute frequency specified");
                REQUIRE(testValidationFailureAbsoluteFrequency(180, XR_FREQUENCY_UNSPECIFIED));
                REQUIRE(testValidationFailureAbsoluteFrequency(XR_FREQUENCY_UNSPECIFIED, 180));
            }
            {
                INFO("Absolute frequency range out of valid range");
                REQUIRE(testValidationFailureAbsoluteFrequency(-1, 200));
                REQUIRE(testValidationFailureAbsoluteFrequency(1001, 200));
                REQUIRE(testValidationFailureAbsoluteFrequency(200, -1));
                REQUIRE(testValidationFailureAbsoluteFrequency(200, 1001));
            }

            // pname:maxFrequencyHz must: be equal to or larger than
            // pname:minFrequencyHz, or dlink:XR_FREQUENCY_UNSPECIFIED.
            {
                INFO("Min absolute frequency greater than max");
                REQUIRE(testValidationFailureAbsoluteFrequency(200, 180));
            }

            // When pname:streamFrameType is
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_INTERMEDIATE_FRAME_EXT or
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT,
            // pname:minFrequencyHz and pname:maxFrequencyHz must: both be
            // dlink:XR_FREQUENCY_UNSPECIFIED.
            {
                INFO("Absolute frequency specified in intermediate frame");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, 100, 200,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints = {{firstFrameDuration * 2, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, 110, 190,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_INTERMEDIATE_FRAME_EXT) == XR_ERROR_VALIDATION_FAILURE);
            }

            // Each amplitude point and frequency point must: not have an earlier time
            // than the one preceding it.
            {
                INFO("Amplitude points out of order");
                amplitudePoints = {{0, 0.0f}, {90000000, 1.0f}, {80000000, 0.5f}};
                REQUIRE(testValidationFailure(amplitudePoints));
            }
            {
                INFO("Frequency points out of order");
                amplitudePoints = {{0, 0.0f}, {100000000, 1.0f}};
                frequencyPoints = {{0, 0.0f}, {80000000, 1.0f}, {50000000, 0.5f}};
                REQUIRE(testValidationFailure(amplitudePoints, frequencyPoints));
            }
            {
                INFO("Transients out of order");
                amplitudePoints = {{0, 0.0f}, {100000000, 1.0f}};
                transients = {{50000000, 1.0f, 1.0f}, {25000000, 1.0f, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, {}, transients));
            }
            {
                INFO("Amplitude points out of order across frames");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints = {{firstFrameDuration / 2, 1.0f}, {firstFrameDuration, 1.0f}, {firstFrameDuration * 2, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_ERROR_VALIDATION_FAILURE);
            }
            {
                INFO("Frequency points out of order across frames");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                frequencyPoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints = {{firstFrameDuration, 1.0f}};
                frequencyPoints = {{firstFrameDuration / 2, 1.0f}, {firstFrameDuration, 1.0f}, {firstFrameDuration * 2, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_ERROR_VALIDATION_FAILURE);
            }
            {
                INFO("Transients out of order across frames");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                transients = {{firstFrameDuration, 1.0f, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, transients, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints = {{firstFrameDuration * 2, 1.0f}};
                transients = {{firstFrameDuration / 2, 1.0f, 1.0f}, {firstFrameDuration * 2, 1.0f, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, transients, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_ERROR_VALIDATION_FAILURE);
            }

            // Each transient must: have a later time than the one preceding it.
            {
                INFO("Transients at same time");
                amplitudePoints = {{0, 0.0f}, {100000000, 1.0f}};
                transients = {{50000000, 1.0f, 1.0f}, {50000000, 1.0f, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, {}, transients));
            }
            {
                INFO("Transients at same time across frames");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                transients = {{firstFrameDuration, 1.0f, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, transients, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints = {{firstFrameDuration * 2, 1.0f}};
                transients = {{firstFrameDuration, 1.0f, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, transients, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_ERROR_VALIDATION_FAILURE);
            }

            // The amplitude and frequency of a point or transient must: be between 0.0
            // and 1.0 (inclusive).
            {
                INFO("Amplitude value out of range");
                amplitudePoints = {{0, 1.1f}, {100000000, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints));
                amplitudePoints = {{0, 0.0f}, {50000000, 1.0f}, {100000000, -0.1f}};
                REQUIRE(testValidationFailure(amplitudePoints));
            }
            {
                INFO("Frequency value out of range");
                amplitudePoints = {{0, 0.0f}, {100000000, 1.0f}};
                frequencyPoints = {{0, 1.1f}, {100000000, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, frequencyPoints));
                frequencyPoints = {{0, 0.0f}, {50000000, 1.0f}, {100000000, -0.1f}};
                REQUIRE(testValidationFailure(amplitudePoints, frequencyPoints));
            }
            {
                INFO("Transient value out of range");
                transients = {{50000000, 1.1f, 1.0f}};
                REQUIRE(testValidationFailure(amplitudePoints, {}, transients));
                transients = {{50000000, 1.0f, 1.0f}, {75000000, 1.0f, -0.1f}};
                REQUIRE(testValidationFailure(amplitudePoints, {}, transients));
            }
        }

        // Checks that the runtime accepts valid vibration data.
        SECTION("Validation Success")
        {
            std::vector<XrHapticParametricPointEXT> amplitudePoints;
            std::vector<XrHapticParametricPointEXT> frequencyPoints;
            std::vector<XrHapticParametricTransientEXT> transients;

            // Amplitude points, frequency points, and transients are all at different times.
            // Last frequency point before last amplitude point.
            // Absolute frequencies are used.
            {
                INFO("Points and transients at different times");
                amplitudePoints = {{0, 0.0f}, {80000000, 1.0f}, {190000000, 0.8f}};
                frequencyPoints = {{0, 1.0f}, {90000000, 0.0f}};
                transients = {{85000000, 1.0f, 1.0f}, {170000000, 1.0f, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, transients, 180, 200,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT) == XR_SUCCESS);
            }

            // Only one frequency point
            {
                INFO("Only one frequency point");
                amplitudePoints = {{0, 0.0f}, {90000000, 1.0f}};
                frequencyPoints = {{0, 0.9f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT) == XR_SUCCESS);
            }

            // Frequency point after the last amplitude point in the first frame
            // This is ok as long as there is no frequency point after the last amplitude point in the last frame.
            {
                INFO("Frequency point after last amplitude point in first frame");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                frequencyPoints = {{0, 0.0f}, {firstFrameDuration, 0.5f}, {firstFrameDuration * 2, 1.0f}};
                transients = {{firstFrameDuration * 2, 1.0f, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, transients,
                                            XR_FREQUENCY_UNSPECIFIED, XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                amplitudePoints = {{firstFrameDuration * 3, 0.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_SUCCESS);
            }

            // Amplitude and frequency points at the same time
            {
                INFO("Amplitude and frequency points at the same time");
                amplitudePoints = {{0, 0.0f}, {10000000, 1.0f}, {10000000, 0.5f}, {20000000, 1.0f}};
                frequencyPoints = {{0, 0.0f}, {10000000, 1.0f}, {10000000, 0.5f}, {20000000, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT) == XR_SUCCESS);
            }

            // If the application does not submit a new haptic frame within dlink:XR_HAPTIC_PARAMETRIC_VIBRATION_EXTEND_DURATION_EXT
            // nanoseconds after the previously submitted haptic frame finished, [..] the runtime must:
            // still accept new haptic frames of type ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_INTERMEDIATE_FRAME_EXT or
            // ename:XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT.
            {
                INFO("Intermediate frame after extend duration");
                amplitudePoints = {{0, 0.0f}, {firstFrameDuration, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED,
                                            XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);

                // Wait until at least XR_HAPTIC_PARAMETRIC_VIBRATION_EXTEND_DURATION_EXT nanoseconds have
                // passed since playback of the first frame finished
                std::this_thread::sleep_for(
                    std::chrono::nanoseconds(firstFrameDuration + XR_HAPTIC_PARAMETRIC_VIBRATION_EXTEND_DURATION_EXT * 2));

                amplitudePoints = {{firstFrameDuration * 2, 1.0f}};
                REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT) == XR_SUCCESS);
            }
        }
    }

    TEST_CASE("XR_EXT_haptic_parametric-scenario", "[scenario][interactive][actions][no_auto]")
    {
        if (!CheckExtensionSupport()) {
            SKIP(XR_EXT_HAPTIC_PARAMETRIC_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper("XR_EXT_haptic_parametric", {XR_EXT_HAPTIC_PARAMETRIC_EXTENSION_NAME});
        InteractiveLayerManager interactiveLayerManager(compositionHelper, nullptr, "XR_EXT_haptic_parametric");
        const XrInstance instance = compositionHelper.GetInstance();
        const XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();

        // Setup actions
        const std::array<XrPath, 2> subactionPaths{{
            StringToPath(instance, "/user/hand/left"),
            StringToPath(instance, "/user/hand/right"),
        }};
        const XrActionSet actionSet = CreateHapticActionSet(instance);
        interactionManager.AddActionSet(actionSet);
        const XrAction hapticAction = CreateHapticAction(actionSet, subactionPaths);
        const XrAction gripPoseAction = CreatePoseAction(actionSet, subactionPaths);
        const XrHapticActionInfo hapticActionInfo = CreateHapticActionInfo(hapticAction, subactionPaths[1]);
        BindHapticAction(instance, interactionManager, hapticAction);
        BindGripPoseAction(instance, interactionManager, gripPoseAction);
        interactionManager.AttachActionSets();
        RunToFocusedState(compositionHelper);
        interactionManager.SyncActions(XR_NULL_PATH);

        if (!CheckSystemSupport(instance, session, compositionHelper.GetSystemId(), hapticActionInfo)) {
            SKIP("System does not support parametric haptics");
        }

        const auto properties = GetProperties(instance, session, hapticActionInfo);
        const auto timingProperties = GetTimingProperties(properties);

        SECTION("Simple Haptic Effect Playback")
        {
            interactiveLayerManager.Configure(
                "ext_haptic_parametric_effect.png",
                "Simple Haptic Effect Playback\n\n"
                "A 10s haptic effect should be played every 12s.\n"
                "In the first 4s, the vibration intensity ramps up from 0 to 1.\n"
                "At 5s, a transient (a short 'clicky' burst) is played.\n"
                "From 6s to 10s, the vibration frequency ramps down from 1 to 0 (if supported by the device).");

            CountdownTimer countdownTimer(1s);
            RenderLoop(session, [&](const XrFrameState& frameState) {
                if (countdownTimer.IsTimeUp()) {
                    countdownTimer.Restart(12s);
                    const std::vector<XrHapticParametricPointEXT> amplitudePoints{{{0, 0.0f}, {4000000000, 1.0f}, {10000000000, 1.0f}}};
                    const std::vector<XrHapticParametricPointEXT> frequencyPoints{{{0, 1.0f}, {6000000000, 1.0f}, {10000000000, 0.0f}}};
                    const std::vector<XrHapticParametricTransientEXT> transients{{{5000000000, 1.0f, 1.0f}}};

                    ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, transients, XR_FREQUENCY_UNSPECIFIED,
                                        XR_FREQUENCY_UNSPECIFIED, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_NONE_EXT);
                }
                return interactiveLayerManager.EndFrame(frameState);
            }).Loop();
            StopHapticFeedback(session, hapticActionInfo);
        }

        SECTION("Streaming Playback")
        {
            interactiveLayerManager.Configure(
                nullptr,
                "Streaming Playback\n\n"
                "An haptic effect should be played indefinitely.\n"
                "Moving the controller to the left decreases the vibration intensity, moving it to the right increases it.\n"
                "Moving the controller up increases the vibration frequency, moving it down decreases it (if supported by the device).\n");

            const XrSpace gripPoseSpace = CreateActionSpace(session, gripPoseAction, subactionPaths);
            const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

            bool hasValidPose = false;
            XrPosef firstPose;
            XrPosef lastPose;
            uint32_t hapticFrameNum = 0;
            XrDuration endOfLastHapticFrame = 0;
            std::chrono::steady_clock::time_point timeAtStartOfStream;
            CountdownTimer countdownTimer(1s);
            RenderLoop(session, [&](const XrFrameState& frameState) {
                // Calculate amplitude and frequency based on how far the controller has moved since the start
                float amplitude = 0.5f;
                float frequency = 0.5f;
                XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
                REQUIRE(xrLocateSpace(gripPoseSpace, localSpace, frameState.predictedDisplayTime, &spaceLocation) == XR_SUCCESS);
                if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                    if (!hasValidPose) {
                        firstPose = spaceLocation.pose;
                        hasValidPose = true;
                    }
                    lastPose = spaceLocation.pose;

                    float poseDeltaX = lastPose.position.x - firstPose.position.x;
                    float poseDeltaY = lastPose.position.y - firstPose.position.y;
                    const float maxPoseDelta = 0.2f;
                    poseDeltaX = std::min(std::max(poseDeltaX, -maxPoseDelta), maxPoseDelta);
                    poseDeltaY = std::min(std::max(poseDeltaY, -maxPoseDelta), maxPoseDelta);
                    amplitude = (poseDeltaX + maxPoseDelta) / (maxPoseDelta * 2);
                    frequency = (poseDeltaY + maxPoseDelta) / (maxPoseDelta * 2);
                }

                // Submit a new haptic frame when it is time to do so
                if (countdownTimer.IsTimeUp()) {
                    if (hapticFrameNum == 0) {
                        endOfLastHapticFrame = timingProperties.firstFrameDuration;
                        timeAtStartOfStream = std::chrono::steady_clock::now();

                        const std::vector<XrHapticParametricPointEXT> amplitudePoints{{{0, amplitude}, {endOfLastHapticFrame, frequency}}};
                        const std::vector<XrHapticParametricPointEXT> frequencyPoints{{{0, amplitude}, {endOfLastHapticFrame, frequency}}};
                        REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, {},
                                                    XR_FREQUENCY_UNSPECIFIED, XR_FREQUENCY_UNSPECIFIED,
                                                    XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT) == XR_SUCCESS);
                    }
                    else {
                        endOfLastHapticFrame += timingProperties.frameSubmissionRate;
                        const std::vector<XrHapticParametricPointEXT> amplitudePoints{{{endOfLastHapticFrame, amplitude}}};
                        const std::vector<XrHapticParametricPointEXT> frequencyPoints{{{endOfLastHapticFrame, frequency}}};
                        REQUIRE(ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, frequencyPoints, {},
                                                    XR_FREQUENCY_UNSPECIFIED, XR_FREQUENCY_UNSPECIFIED,
                                                    XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_INTERMEDIATE_FRAME_EXT) == XR_SUCCESS);
                    }

                    hapticFrameNum++;
                    const auto timeAtNextHapticFrame =
                        timeAtStartOfStream + std::chrono::nanoseconds{timingProperties.frameSubmissionRate} * hapticFrameNum;
                    const std::chrono::nanoseconds durationToNextHapticFrame = timeAtNextHapticFrame - std::chrono::steady_clock::now();
                    countdownTimer.Restart(durationToNextHapticFrame);
                }

                return interactiveLayerManager.EndFrame(frameState);
            }).Loop();
            StopHapticFeedback(session, hapticActionInfo);
        }

        SECTION("Late frame handling")
        {
            const XrDuration firstFrameVibrationDuration =
                timingProperties.firstFrameDuration + XR_HAPTIC_PARAMETRIC_VIBRATION_EXTEND_DURATION_EXT;
            const XrDuration lastFrameDuration = 100'000'000;     // 100ms
            constexpr XrDuration pauseDuration = 500'000'000;     // 500ms
            constexpr XrDuration repeatDuration = 2'000'000'000;  // 2s

            const std::string message =
                "Late frame handling\n\n"
                "The following vibration pattern should be played:\n"
                "1. A short (~" +
                std::to_string(firstFrameVibrationDuration / 1'000'000) +
                "ms) vibration\n"
                "2. A 500ms pause\n"
                "3. A 100ms vibration\n"
                "The pattern repeats after another 2s.\n";
            interactiveLayerManager.Configure(nullptr, message.c_str());

            enum class State
            {
                FirstFrame,
                LastFrame
            };
            State state = State::FirstFrame;

            CountdownTimer countdownTimer(0s);
            RenderLoop(session, [&](const XrFrameState& frameState) {
                if (countdownTimer.IsTimeUp()) {
                    switch (state) {
                    // Submit first frame
                    case State::FirstFrame: {
                        const std::vector<XrHapticParametricPointEXT> amplitudePoints{
                            {{0, 1.0f}, {timingProperties.firstFrameDuration, 1.0f}}};
                        ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_FIRST_FRAME_EXT);

                        // Switch to State::LastFrame 500ms after the vibration has finished
                        countdownTimer.Restart(std::chrono::nanoseconds{firstFrameVibrationDuration + pauseDuration});
                        state = State::LastFrame;
                        break;
                    }

                    // Submit the last frame
                    case State::LastFrame: {
                        const std::vector<XrHapticParametricPointEXT> amplitudePoints{
                            {{firstFrameVibrationDuration + pauseDuration + lastFrameDuration, 1.0f}}};
                        ApplyHapticFeedback(session, hapticActionInfo, amplitudePoints, {}, {}, XR_FREQUENCY_UNSPECIFIED,
                                            XR_FREQUENCY_UNSPECIFIED, XR_HAPTIC_PARAMETRIC_STREAM_FRAME_TYPE_LAST_FRAME_EXT);

                        // Switch to State::FirstFrame 2s after the last frame has finished
                        countdownTimer.Restart(std::chrono::nanoseconds{lastFrameDuration + repeatDuration});
                        state = State::FirstFrame;
                        break;
                    }
                    }
                }
                return interactiveLayerManager.EndFrame(frameState);
            }).Loop();
            StopHapticFeedback(session, hapticActionInfo);
        }
    }

}  // namespace Conformance
