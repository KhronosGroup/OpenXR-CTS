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

#include "utils.h"
#include "report.h"
#include "two_call.h"
#include "report.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "composition_utils.h"
#include "input_testinputdevice.h"
#include <openxr/openxr.h>
#include <catch2/catch.hpp>
#include <chrono>
#include <atomic>
#include <set>
#include <regex>

using namespace std::chrono_literals;
using namespace Conformance;

// Stores the top level path in slot 2 and the identifier path in slot 5 or 6 based on whether or not the component was included.
// If the component was included, 6 and 7 will be matched with the parent and component, otherwise 5 will be matched.
const std::regex cInteractionSourcePathRegex("^((.+)/(input|output))/(([^/]+)|([^/]+)/([^/]+))$");

namespace
{
    // Manages showing a quad with help text.
    struct ActionLayerManager : public ITestMessageDisplay
    {
        ActionLayerManager(CompositionHelper& compositionHelper)
            : m_compositionHelper(compositionHelper)
            , m_viewSpace(compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW))
            , m_eventReader(m_compositionHelper.GetEventQueue())
            , m_renderLoop(m_compositionHelper.GetSession(), [&](const XrFrameState& frameState) { return EndFrame(frameState); })
        {
        }

        void WaitForSessionFocusWithMessage()
        {
            XrSession session = m_compositionHelper.GetSession();

            DisplayMessage("Waiting for session focus...");

            bool focused = WaitUntilPredicateWithTimeout(
                [&]() {
                    m_renderLoop.IterateFrame();
                    XrEventDataBuffer eventData;
                    while (m_eventReader.TryReadNext(eventData)) {
                        if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                            auto sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                            if (sessionStateChanged->session == session && sessionStateChanged->state == XR_SESSION_STATE_FOCUSED) {
                                return true;
                            }
                        }
                    }
                    return false;
                },
                20s, 5ms);

            REQUIRE_MSG(focused, "Time out waiting for session focus");
            DisplayMessage("");
        }

        EventReader& GetEventReader()
        {
            return m_eventReader;
        }

        RenderLoop& GetRenderLoop()
        {
            return m_renderLoop;
        }

        // Sync until focus is available, in case focus was lost at some point.
        void SyncActionsUntilFocusWithMessage(const XrActionsSyncInfo& syncInfo)
        {
            XrResult res;

            bool messageShown = false;
            const auto startTime = std::chrono::system_clock::now();
            while (std::chrono::system_clock::now() - startTime < 30s) {
                {
                    res = xrSyncActions(m_compositionHelper.GetSession(), &syncInfo);
                    if (XR_UNQUALIFIED_SUCCESS(res)) {
                        if (messageShown) {
                            DisplayMessage("");
                        }

                        return;
                    }

                    REQUIRE_RESULT_SUCCEEDED(res);
                    if (res == XR_SESSION_NOT_FOCUSED && !messageShown) {
                        DisplayMessage("Waiting for session focus...");
                        messageShown = true;
                    }
                }
                m_renderLoop.IterateFrame();

                std::this_thread::sleep_for(5ms);
            }

            FAIL("Time out waiting for session focus on xrSyncActions");
        }

        bool EndFrame(const XrFrameState& frameState)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_displayMessageImage) {
                m_messageQuad = std::make_unique<MessageQuad>(m_compositionHelper, std::move(m_displayMessageImage), m_viewSpace);
            }

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (m_messageQuad != nullptr) {
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(m_messageQuad.get()));
            }
            m_compositionHelper.EndFrame(frameState.predictedDisplayTime, std::move(layers));
            return m_compositionHelper.PollEvents();
        }

        void IterateFrame() override
        {
            m_renderLoop.IterateFrame();
        }

        void DisplayMessage(const std::string& message) override
        {
            if (message == m_lastMessage) {
                return;  // No need to regenerate the swapchain.
            }

            if (!message.empty()) {
                ReportStr(("Interaction message: " + message).c_str());
            }

            constexpr int TitleFontHeightPixels = 40;
            constexpr int TitleFontPaddingPixels = 2;
            constexpr int TitleBorderPixels = 2;
            constexpr int InsetPixels = TitleBorderPixels + TitleFontPaddingPixels;

            std::lock_guard<std::mutex> lock(m_mutex);

            auto image = std::make_unique<RGBAImage>(768, (TitleFontHeightPixels + InsetPixels * 2) * 5);
            if (!message.empty()) {
                image->DrawRect(0, 0, image->width, image->height, {0.25f, 0.25f, 0.25f, 0.25f});
                image->DrawRectBorder(0, 0, image->width, image->height, TitleBorderPixels, {0.5f, 0.5f, 0.5f, 1});
                image->PutText(XrRect2Di{{InsetPixels, InsetPixels}, {image->width - InsetPixels * 2, image->height - InsetPixels * 2}},
                               message.c_str(), TitleFontHeightPixels, {1, 1, 1, 1});
            }

            m_displayMessageImage = std::move(image);
            m_lastMessage = message;
        }

    private:
        std::mutex m_mutex;

        CompositionHelper& m_compositionHelper;
        const XrSpace m_viewSpace;
        EventReader m_eventReader;
        RenderLoop m_renderLoop;

        std::string m_lastMessage;
        std::unique_ptr<RGBAImage> m_displayMessageImage;

        struct MessageQuad : public XrCompositionLayerQuad
        {
            MessageQuad(CompositionHelper& compositionHelper, std::unique_ptr<RGBAImage> image, XrSpace compositionSpace)
                : m_compositionHelper(compositionHelper)
            {
                const XrSwapchain messageSwapchain = m_compositionHelper.CreateStaticSwapchainImage(*image);

                *static_cast<XrCompositionLayerQuad*>(this) = {XR_TYPE_COMPOSITION_LAYER_QUAD};
                size.width = 1;
                size.height = size.width * image->height / image->width;
                pose = XrPosef{{0, 0, 0, 1}, {0, 0, -1.5f}};
                subImage = m_compositionHelper.MakeDefaultSubImage(messageSwapchain);
                space = compositionSpace;
            }
            ~MessageQuad()
            {
                if (subImage.swapchain) {
                    m_compositionHelper.DestroySwapchain(subImage.swapchain);
                }
            }

        private:
            CompositionHelper& m_compositionHelper;
        };

        std::unique_ptr<MessageQuad> m_messageQuad;
    };
}  // namespace

namespace Conformance
{
    TEST_CASE("xrCreateActionSet", "[actions]")
    {
        AutoBasicInstance instance(AutoBasicInstance::createSystemId);
        REQUIRE_MSG(instance != XR_NULL_HANDLE_CPP,
                    "If this (XrInstance creation) fails, ensure the runtime location is set and the runtime is started, if applicable.");
        REQUIRE_MSG(instance.systemId != XR_NULL_SYSTEM_ID,
                    "XrInstance SystemId creation failed. Does the runtime have hardware available?");

        XrInstance invalidInstance = (XrInstance)0x1234;

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");

        SECTION("Basic action creation")
        {
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);
        }
        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            REQUIRE_RESULT(xrCreateActionSet(invalidInstance, &actionSetCreateInfo, &actionSet), XR_ERROR_HANDLE_INVALID);
        }
        SECTION("Naming rules")
        {
            SECTION("Empty names")
            {
                strcpy(actionSetCreateInfo.actionSetName, "");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_ERROR_NAME_INVALID);

                strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
                strcpy(actionSetCreateInfo.localizedActionSetName, "");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_ERROR_LOCALIZED_NAME_INVALID);
            }
            SECTION("Invalid names")
            {
                strcpy(actionSetCreateInfo.actionSetName, "INVALID PATH COMPONENT");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_ERROR_PATH_FORMAT_INVALID);
            }
            SECTION("Name duplication")
            {
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);
                strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 2");
                XrActionSet actionSet2{XR_NULL_HANDLE};
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet2), XR_ERROR_NAME_DUPLICATED);

                // If we delete and re-add the action set, the name will be available to be used
                REQUIRE_RESULT(xrDestroyActionSet(actionSet), XR_SUCCESS);

                strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 3");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);
                strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 4");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_ERROR_NAME_DUPLICATED);
            }
            SECTION("Localized name duplication")
            {
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);
                strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_2");
                XrActionSet actionSet2{XR_NULL_HANDLE};
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet2), XR_ERROR_LOCALIZED_NAME_DUPLICATED);

                // If we delete and re-add the action set, the name will be available to be used
                REQUIRE_RESULT(xrDestroyActionSet(actionSet), XR_SUCCESS);

                strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_3");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);
                strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_4");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_ERROR_LOCALIZED_NAME_DUPLICATED);
            }
        }
    }

    TEST_CASE("xrDestroyActionSet", "[actions]")
    {
        AutoBasicInstance instance(AutoBasicInstance::createSystemId);
        REQUIRE_MSG(instance != XR_NULL_HANDLE_CPP,
                    "If this (XrInstance creation) fails, ensure the runtime location is set and the runtime is started, if applicable.");
        REQUIRE_MSG(instance.systemId != XR_NULL_SYSTEM_ID,
                    "XrInstance SystemId creation failed. Does the runtime have hardware available?");

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSet invalidActionSet = (XrActionSet)0x1234;
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            REQUIRE_RESULT(xrDestroyActionSet(actionSet), XR_SUCCESS);
            REQUIRE_RESULT(xrDestroyActionSet(actionSet), XR_ERROR_HANDLE_INVALID);
            REQUIRE_RESULT(xrDestroyActionSet(invalidActionSet), XR_ERROR_HANDLE_INVALID);
        }
        SECTION("Child handle destruction")
        {
            XrAction action{XR_NULL_HANDLE};
            XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionCreateInfo.localizedActionName, "test action localized name");
            strcpy(actionCreateInfo.actionName, "test_action_name");
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

            REQUIRE_RESULT(xrDestroyActionSet(actionSet), XR_SUCCESS);

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // Destruction of action sets destroys its actions
                REQUIRE_RESULT(xrDestroyAction(action), XR_ERROR_HANDLE_INVALID);
            }
        }
    }

    TEST_CASE("xrCreateAction", "[actions]")
    {
        AutoBasicInstance instance(AutoBasicInstance::createSystemId);
        REQUIRE_MSG(instance != XR_NULL_HANDLE_CPP,
                    "If this (XrInstance creation) fails, ensure the runtime location is set and the runtime is started, if applicable.");
        REQUIRE_MSG(instance.systemId != XR_NULL_SYSTEM_ID,
                    "XrInstance SystemId creation failed. Does the runtime have hardware available?");

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSet invalidActionSet = (XrActionSet)0x1234;
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction action{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name");
        strcpy(actionCreateInfo.actionName, "test_action_name");

        SECTION("Basic action creation")
        {
            actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
            REQUIRE_RESULT(xrDestroyAction(action), XR_SUCCESS);

            actionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
            REQUIRE_RESULT(xrDestroyAction(action), XR_SUCCESS);

            actionCreateInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
            REQUIRE_RESULT(xrDestroyAction(action), XR_SUCCESS);

            actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
            REQUIRE_RESULT(xrDestroyAction(action), XR_SUCCESS);

            actionCreateInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
            REQUIRE_RESULT(xrDestroyAction(action), XR_SUCCESS);
        }
        SECTION("Parameter validation")
        {
            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                REQUIRE_RESULT(xrCreateAction(invalidActionSet, &actionCreateInfo, &action), XR_ERROR_HANDLE_INVALID);
            }

            SECTION("Duplicate subaction paths")
            {
                XrPath subactionPaths[2] = {StringToPath(instance, "/user"), StringToPath(instance, "/user")};
                actionCreateInfo.countSubactionPaths = 2;
                actionCreateInfo.subactionPaths = subactionPaths;
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_ERROR_PATH_UNSUPPORTED);
            }

            SECTION("Invalid subaction paths")
            {
                XrPath subactionPath{StringToPath(instance, "/user/invalid")};
                actionCreateInfo.countSubactionPaths = 1;
                actionCreateInfo.subactionPaths = &subactionPath;
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_ERROR_PATH_UNSUPPORTED);
            }
        }
        SECTION("Naming rules")
        {
            SECTION("Empty names")
            {
                strcpy(actionCreateInfo.actionName, "");
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_ERROR_NAME_INVALID);

                strcpy(actionCreateInfo.actionName, "test_action_name");
                strcpy(actionCreateInfo.localizedActionName, "");
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_ERROR_LOCALIZED_NAME_INVALID);
            }
            SECTION("Invalid names")
            {
                strcpy(actionCreateInfo.actionName, "INVALID PATH COMPONENT");
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_ERROR_PATH_FORMAT_INVALID);
            }
            SECTION("Name duplication")
            {
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
                strcpy(actionCreateInfo.localizedActionName, "test action localized name 2");
                XrAction action2{XR_NULL_HANDLE};
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action2), XR_ERROR_NAME_DUPLICATED);

                // If we delete and re-add the action, the name will be available to be used
                REQUIRE_RESULT(xrDestroyAction(action), XR_SUCCESS);

                strcpy(actionCreateInfo.localizedActionName, "test action set localized name 3");
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
                strcpy(actionCreateInfo.localizedActionName, "test action set localized name 4");
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_ERROR_NAME_DUPLICATED);
            }
            SECTION("Localized name duplication")
            {
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
                strcpy(actionCreateInfo.actionName, "test_action_set_name_2");
                XrAction action2{XR_NULL_HANDLE};
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action2), XR_ERROR_LOCALIZED_NAME_DUPLICATED);

                // If we delete and re-add the action, the name will be available to be used
                REQUIRE_RESULT(xrDestroyAction(action), XR_SUCCESS);

                strcpy(actionCreateInfo.actionName, "test_action_set_name_3");
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);
                strcpy(actionCreateInfo.actionName, "test_action_set_name_4");
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_ERROR_LOCALIZED_NAME_DUPLICATED);
            }
        }
    }

    TEST_CASE("xrDestroyAction", "[actions]")
    {
        AutoBasicInstance instance(AutoBasicInstance::createSystemId);
        REQUIRE_MSG(instance != XR_NULL_HANDLE_CPP,
                    "If this (XrInstance creation) fails, ensure the runtime location is set and the runtime is started, if applicable.");
        REQUIRE_MSG(instance.systemId != XR_NULL_SYSTEM_ID,
                    "XrInstance SystemId creation failed. Does the runtime have hardware available?");

        SECTION("Parameter validation")
        {
            XrActionSet actionSet{XR_NULL_HANDLE};
            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
            strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

            XrAction action{XR_NULL_HANDLE};
            XrAction invalidAction = (XrAction)0x1234;
            XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionCreateInfo.localizedActionName, "test action localized name");
            strcpy(actionCreateInfo.actionName, "test_action_name");
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

            REQUIRE_RESULT(xrDestroyAction(action), XR_SUCCESS);
            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                REQUIRE_RESULT(xrDestroyAction(action), XR_ERROR_HANDLE_INVALID);
                REQUIRE_RESULT(xrDestroyAction(invalidAction), XR_ERROR_HANDLE_INVALID);
            }

            REQUIRE_RESULT(xrDestroyActionSet(actionSet), XR_SUCCESS);
        }
    }

    TEST_CASE("xrSuggestInteractionProfileBindings", "[actions]")
    {
        AutoBasicInstance instance(AutoBasicInstance::createSystemId);
        REQUIRE_MSG(instance != XR_NULL_HANDLE_CPP,
                    "If this (XrInstance creation) fails, ensure the runtime location is set and the runtime is started, if applicable.");
        REQUIRE_MSG(instance.systemId != XR_NULL_SYSTEM_ID,
                    "XrInstance SystemId creation failed. Does the runtime have hardware available?");

        XrInstance invalidInstance = (XrInstance)0x1234;

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction action{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name");
        strcpy(actionCreateInfo.actionName, "test_action_name");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

        XrActionSuggestedBinding testBinding = {action, StringToPath(instance, "/user/hand/left/input/select/click")};
        XrInteractionProfileSuggestedBinding bindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        bindings.interactionProfile = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
        bindings.countSuggestedBindings = 1;
        bindings.suggestedBindings = &testBinding;

        SECTION("Parameter validation")
        {
            SECTION("Basic usage")
            {
                REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_SUCCESS);
            }

            SECTION("Called twice")
            {
                REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_SUCCESS);
                REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_SUCCESS);
            }

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                SECTION("Invalid instance")
                {
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(invalidInstance, &bindings), XR_ERROR_HANDLE_INVALID);
                }
                SECTION("Invalid action")
                {
                    XrAction invalidAction = (XrAction)0x1234;
                    XrActionSuggestedBinding invalidSuggestedBinding{invalidAction,
                                                                     StringToPath(instance, "/user/hand/left/input/select/click")};
                    bindings.countSuggestedBindings = 1;
                    bindings.suggestedBindings = &invalidSuggestedBinding;
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_HANDLE_INVALID);
                }
            }

            SECTION("countSuggestedBindings must be > 0")
            {
                bindings.countSuggestedBindings = 0;
                REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_VALIDATION_FAILURE);
            }

            SECTION("Invalid type")
            {
                bindings = XrInteractionProfileSuggestedBinding{XR_TYPE_ACTIONS_SYNC_INFO};
                bindings.countSuggestedBindings = 1;
                bindings.suggestedBindings = &testBinding;
                REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_VALIDATION_FAILURE);
            }
        }
        SECTION("Path formats and whitelisting")
        {
            std::vector<std::string> invalidInteractionProfiles{"/invalid", "/interaction_profiles", "/interaction_profiles/invalid",
                                                                "/interaction_profiles/khr/simple_controller/invalid"};

            std::vector<std::string> invalidBindingPaths{"/invalid",
                                                         "/user/invalid",
                                                         "/user/hand/invalid",
                                                         "/user/hand/right",
                                                         "/user/hand/right/invalid",
                                                         "/user/hand/right/input",
                                                         "/user/hand/invalid/input",
                                                         "/user/invalid/right/input",
                                                         "/invalid/hand/right/input",
                                                         "/user/hand/left/input_bad/menu/click",
                                                         "/user/hand/right/input/select/click/invalid"};

            SECTION("Unknown interaction profile")
            {
                for (auto invalidIP : invalidInteractionProfiles) {
                    bindings.interactionProfile = StringToPath(instance, invalidIP.c_str());
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_PATH_UNSUPPORTED);
                }

                bindings.interactionProfile = StringToPath(instance, "/interaction_profiles/khr/another_controller");
                REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_PATH_UNSUPPORTED);
            }
            SECTION("Unknown binding path")
            {
                for (auto invalidBindingPath : invalidBindingPaths) {
                    XrActionSuggestedBinding invalidBindingPathBinding = {action, StringToPath(instance, invalidBindingPath.c_str())};
                    bindings.suggestedBindings = &invalidBindingPathBinding;
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_PATH_UNSUPPORTED);
                }
            }
            SECTION("Supports all specified interaction profiles")
            {
                for (const auto& ipMetadata : cInteractionProfileDefinitions) {
                    XrAction boolAction;
                    XrAction floatAction;
                    XrAction vectorAction;
                    XrAction poseAction;
                    XrAction hapticAction;

                    std::string actionNamePrefix = ipMetadata.InteractionProfileShortname;
                    std::replace(actionNamePrefix.begin(), actionNamePrefix.end(), '/', '_');
                    XrActionCreateInfo allIPActionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
                    allIPActionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                    strcpy(allIPActionCreateInfo.localizedActionName, (actionNamePrefix + "test bool action localized name").c_str());
                    strcpy(allIPActionCreateInfo.actionName, (actionNamePrefix + "test_bool_action_name").c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &allIPActionCreateInfo, &boolAction), XR_SUCCESS);

                    allIPActionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                    strcpy(allIPActionCreateInfo.localizedActionName, (actionNamePrefix + "test float action localized name").c_str());
                    strcpy(allIPActionCreateInfo.actionName, (actionNamePrefix + "test_float_action_name").c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &allIPActionCreateInfo, &floatAction), XR_SUCCESS);

                    allIPActionCreateInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
                    strcpy(allIPActionCreateInfo.localizedActionName, (actionNamePrefix + "test vector action localized name").c_str());
                    strcpy(allIPActionCreateInfo.actionName, (actionNamePrefix + "test_vector_action_name").c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &allIPActionCreateInfo, &vectorAction), XR_SUCCESS);

                    allIPActionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                    strcpy(allIPActionCreateInfo.localizedActionName, (actionNamePrefix + "test pose action localized name").c_str());
                    strcpy(allIPActionCreateInfo.actionName, (actionNamePrefix + "test_pose_action_name").c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &allIPActionCreateInfo, &poseAction), XR_SUCCESS);

                    allIPActionCreateInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
                    strcpy(allIPActionCreateInfo.localizedActionName, (actionNamePrefix + "test haptic action localized name").c_str());
                    strcpy(allIPActionCreateInfo.actionName, (actionNamePrefix + "test_haptic_action_name").c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &allIPActionCreateInfo, &hapticAction), XR_SUCCESS);

                    bindings.interactionProfile = StringToPath(instance, ipMetadata.InteractionProfilePathString.c_str());
                    bindings.countSuggestedBindings = 1;
                    for (const auto& inputSourcePathData : ipMetadata.WhitelistData) {
                        const std::string& bindingPath = inputSourcePathData.Path;
                        const XrActionType& actionType = inputSourcePathData.Type;

                        XrAction* actionRef;
                        if (actionType == XR_ACTION_TYPE_BOOLEAN_INPUT) {
                            actionRef = &boolAction;
                        }
                        else if (actionType == XR_ACTION_TYPE_FLOAT_INPUT) {
                            actionRef = &floatAction;
                        }
                        else if (actionType == XR_ACTION_TYPE_VECTOR2F_INPUT) {
                            actionRef = &vectorAction;
                        }
                        else if (actionType == XR_ACTION_TYPE_VIBRATION_OUTPUT) {
                            actionRef = &poseAction;
                        }
                        else {
                            actionRef = &hapticAction;
                        }

                        XrActionSuggestedBinding suggestedBindings{*actionRef, StringToPath(instance, bindingPath.c_str())};
                        bindings.suggestedBindings = &suggestedBindings;
                        REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_SUCCESS);
                    }
                }
            }
        }
        SECTION("Duplicate bindings")
        {
            // Duplicate bindings are not prevented. Runtimes should union these.
            std::vector<XrActionSuggestedBinding> leftHandActionSuggestedBindings{
                {action, StringToPath(instance, "/user/hand/left/input/select")},
                {action, StringToPath(instance, "/user/hand/left/input/select/click")},
                {action, StringToPath(instance, "/user/hand/left/input/select")},
                {action, StringToPath(instance, "/user/hand/left/input/select/click")},
                {action, StringToPath(instance, "/user/hand/left/input/select")},
                {action, StringToPath(instance, "/user/hand/left/input/select/click")},
                {action, StringToPath(instance, "/user/hand/left/input/select")},
                {action, StringToPath(instance, "/user/hand/left/input/select/click")},
                {action, StringToPath(instance, "/user/hand/left/input/select")},
                {action, StringToPath(instance, "/user/hand/left/input/select/click")},
                {action, StringToPath(instance, "/user/hand/left/input/select")},
                {action, StringToPath(instance, "/user/hand/left/input/select/click")},
                {action, StringToPath(instance, "/user/hand/left/input/select")},
                {action, StringToPath(instance, "/user/hand/left/input/select/click")},
                {action, StringToPath(instance, "/user/hand/left/input/select")},
                {action, StringToPath(instance, "/user/hand/left/input/select/click")},
            };

            bindings.countSuggestedBindings = (uint32_t)(leftHandActionSuggestedBindings.size());
            bindings.suggestedBindings = leftHandActionSuggestedBindings.data();
            REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_SUCCESS);
        }
        SECTION("Attachment rules")
        {
            REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_SUCCESS);

            AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);
            REQUIRE(session != XR_NULL_HANDLE_CPP);

            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.countActionSets = 1;
            attachInfo.actionSets = &actionSet;
            REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

            REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_ACTIONSETS_ALREADY_ATTACHED);
        }
    }

    TEST_CASE("xrSuggestInteractionProfileBindings_interactive", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("xrSuggestInteractionProfileBindings");
        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction selectActionA{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test select action A");
        strcpy(actionCreateInfo.actionName, "test_select_action_a");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &selectActionA), XR_SUCCESS);

        XrAction selectActionB{XR_NULL_HANDLE};
        strcpy(actionCreateInfo.localizedActionName, "test select action B");
        strcpy(actionCreateInfo.actionName, "test_select_action_b");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &selectActionB), XR_SUCCESS);

        XrActionStateBoolean booleanActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};

        XrPath leftHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
            compositionHelper.GetSession(),
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str()),
            leftHandPath, cSimpleKHRInteractionProfileDefinition.WhitelistData);

        XrPath rightHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
            compositionHelper.GetSession(),
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str()),
            rightHandPath, cSimpleKHRInteractionProfileDefinition.WhitelistData);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);

        XrPath selectPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click");
        XrActionSuggestedBinding testBinding = {selectActionA, selectPath};
        XrInteractionProfileSuggestedBinding bindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        bindings.interactionProfile = StringToPath(compositionHelper.GetInstance(), "/interaction_profiles/khr/simple_controller");
        bindings.countSuggestedBindings = 1;
        bindings.suggestedBindings = &testBinding;
        REQUIRE_RESULT(xrSuggestInteractionProfileBindings(compositionHelper.GetInstance(), &bindings), XR_SUCCESS);

        // Calling attach on the interaction manager will call xrSuggestInteractionProfileBindings with the bindings provided here, overwriting the previous bindings
        compositionHelper.GetInteractionManager().AddActionBindings(
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str()),
            {{{selectActionB, selectPath}}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        actionLayerManager.WaitForSessionFocusWithMessage();

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        XrActiveActionSet activeActionSet{actionSet};
        syncInfo.activeActionSets = &activeActionSet;
        syncInfo.countActiveActionSets = 1;

        SECTION("Old bindings discarded")
        {
            leftHandInputDevice->SetDeviceActive(true);
            leftHandInputDevice->SetButtonStateBool(selectPath, true);

            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

            // selectActionA should have had its bindings discarded and replaced by selectActionB's bindings
            getInfo.action = selectActionA;
            REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanActionState), XR_SUCCESS);
            REQUIRE_FALSE(booleanActionState.isActive);

            getInfo.action = selectActionB;
            REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanActionState), XR_SUCCESS);
            REQUIRE(booleanActionState.isActive);
        }
    }

    TEST_CASE("xrAttachSessionActionSets", "[actions]")
    {
        AutoBasicInstance instance(AutoBasicInstance::createSystemId);
        REQUIRE_MSG(instance != XR_NULL_HANDLE_CPP,
                    "If this (XrInstance creation) fails, ensure the runtime location is set and the runtime is started, if applicable.");
        REQUIRE_MSG(instance.systemId != XR_NULL_SYSTEM_ID,
                    "XrInstance SystemId creation failed. Does the runtime have hardware available?");

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);
        REQUIRE(session != XR_NULL_HANDLE_CPP);
        XrSession invalidSession = (XrSession)0x1234;

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &actionSet;

        XrAction selectAction{XR_NULL_HANDLE};
        XrActionCreateInfo selectActionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        selectActionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(selectActionCreateInfo.localizedActionName, "test select action");
        strcpy(selectActionCreateInfo.actionName, "test_select_action");

        XrAction floatAction{XR_NULL_HANDLE};
        XrActionCreateInfo floatActionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        floatActionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
        strcpy(floatActionCreateInfo.localizedActionName, "test float action");
        strcpy(floatActionCreateInfo.actionName, "test_float_action");

        XrAction vectorAction{XR_NULL_HANDLE};
        XrActionCreateInfo vectorActionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        vectorActionCreateInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
        strcpy(vectorActionCreateInfo.localizedActionName, "test vector action");
        strcpy(vectorActionCreateInfo.actionName, "test_vector_action");

        XrAction poseAction{XR_NULL_HANDLE};
        XrActionCreateInfo poseActionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        poseActionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        strcpy(poseActionCreateInfo.localizedActionName, "test pose action");
        strcpy(poseActionCreateInfo.actionName, "test_pose_action");

        XrAction hapticAction{XR_NULL_HANDLE};
        XrActionCreateInfo hapticActionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        hapticActionCreateInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
        strcpy(hapticActionCreateInfo.localizedActionName, "test haptic action");
        strcpy(hapticActionCreateInfo.actionName, "test_haptic_action");

        SECTION("Parameter validation")
        {
            SECTION("Basic usage")
            {
                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);
            }
            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                SECTION("Invalid session")
                {
                    REQUIRE_RESULT(xrAttachSessionActionSets(invalidSession, &attachInfo), XR_ERROR_HANDLE_INVALID);
                }
                SECTION("Invalid action set")
                {
                    XrActionSet invalidActionSet = (XrActionSet)0x1234;
                    attachInfo.actionSets = &invalidActionSet;
                    REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_ERROR_HANDLE_INVALID);
                }
            }
            SECTION("countActionSets must be > 0")
            {
                attachInfo.countActionSets = 0;
                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_ERROR_VALIDATION_FAILURE);
            }
            SECTION("Can attach to multiple sessions")
            {
                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

                // Shut down the old session since runtimes are only required to support one.
                session.Shutdown();
                session.Init(AutoBasicSession::OptionFlags::createSession, instance);
                REQUIRE(session != XR_NULL_HANDLE_CPP);

                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);
            }
        }
        SECTION("Action sets and actions immutability")
        {
            REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);
            REQUIRE_RESULT(xrCreateAction(actionSet, &selectActionCreateInfo, &selectAction), XR_ERROR_ACTIONSETS_ALREADY_ATTACHED);
        }
        SECTION("Dependent functions")
        {
            SECTION("xrAttachSessionActionSets")
            {
                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);
                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_ERROR_ACTIONSETS_ALREADY_ATTACHED);
            }
            SECTION("xrSyncActions")
            {
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                XrActiveActionSet activeActionSet{actionSet};
                syncInfo.activeActionSets = &activeActionSet;
                syncInfo.countActiveActionSets = 1;

                REQUIRE_RESULT(xrSyncActions(session, &syncInfo), XR_ERROR_ACTIONSET_NOT_ATTACHED);

                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

                REQUIRE_RESULT_SUCCEEDED(xrSyncActions(session, &syncInfo));
            }
            SECTION("Action state querying")
            {
                REQUIRE_RESULT(xrCreateAction(actionSet, &selectActionCreateInfo, &selectAction), XR_SUCCESS);
                REQUIRE_RESULT(xrCreateAction(actionSet, &floatActionCreateInfo, &floatAction), XR_SUCCESS);
                REQUIRE_RESULT(xrCreateAction(actionSet, &vectorActionCreateInfo, &vectorAction), XR_SUCCESS);
                REQUIRE_RESULT(xrCreateAction(actionSet, &poseActionCreateInfo, &poseAction), XR_SUCCESS);
                REQUIRE_RESULT(xrCreateAction(actionSet, &hapticActionCreateInfo, &hapticAction), XR_SUCCESS);

                XrActionStateBoolean booleanActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XrActionStateFloat floatState{XR_TYPE_ACTION_STATE_FLOAT};
                XrActionStateVector2f vectorState{XR_TYPE_ACTION_STATE_VECTOR2F};
                XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};

                XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                hapticActionInfo.action = hapticAction;

                XrHapticVibration hapticPacket{XR_TYPE_HAPTIC_VIBRATION};
                hapticPacket.amplitude = 1;
                hapticPacket.frequency = XR_FREQUENCY_UNSPECIFIED;
                hapticPacket.duration = XR_MIN_HAPTIC_DURATION;

                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};

                getInfo.action = selectAction;
                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanActionState), XR_ERROR_ACTIONSET_NOT_ATTACHED);

                getInfo.action = floatAction;
                REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_ACTIONSET_NOT_ATTACHED);

                getInfo.action = vectorAction;
                REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_ERROR_ACTIONSET_NOT_ATTACHED);

                getInfo.action = poseAction;
                REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_ERROR_ACTIONSET_NOT_ATTACHED);

                REQUIRE_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                               XR_ERROR_ACTIONSET_NOT_ATTACHED);
                REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_ERROR_ACTIONSET_NOT_ATTACHED);

                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

                getInfo.action = selectAction;
                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanActionState), XR_SUCCESS);

                getInfo.action = floatAction;
                REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);

                getInfo.action = vectorAction;
                REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_SUCCESS);

                getInfo.action = poseAction;
                REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_SUCCESS);

                REQUIRE_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                               XR_SUCCESS);
                REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_SUCCESS);
            }
            SECTION("Current interaction profile")
            {
                XrPath leftHandPath = StringToPath(instance, "/user/hand/left");
                XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
                REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, leftHandPath, &interactionProfileState),
                               XR_ERROR_ACTIONSET_NOT_ATTACHED);

                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

                REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, leftHandPath, &interactionProfileState), XR_SUCCESS);
            }
            SECTION("Enumerate sources")
            {
                REQUIRE_RESULT(xrCreateAction(actionSet, &selectActionCreateInfo, &selectAction), XR_SUCCESS);
                XrBoundSourcesForActionEnumerateInfo info{XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
                info.action = selectAction;
                uint32_t sourceCountOutput;
                XrPath buffer;
                REQUIRE_RESULT(xrEnumerateBoundSourcesForAction(session, &info, 0, &sourceCountOutput, &buffer),
                               XR_ERROR_ACTIONSET_NOT_ATTACHED);
            }
            SECTION("Get localized source name")
            {
                XrInputSourceLocalizedNameGetInfo getInfo{XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
                getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT;
                getInfo.sourcePath = StringToPath(instance, "/user/hand/left/input/select/click");
                uint32_t sourceCountOutput;
                char buffer;
                REQUIRE_RESULT(xrGetInputSourceLocalizedName(session, &getInfo, 0, &sourceCountOutput, &buffer),
                               XR_ERROR_ACTIONSET_NOT_ATTACHED);
            }
        }
        SECTION("Unattached action sets")
        {
            REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

            XrActionSet actionSet2{XR_NULL_HANDLE};
            strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 2");
            strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_2");
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet2), XR_SUCCESS);

            XrAction selectAction2{XR_NULL_HANDLE};
            XrActionCreateInfo select2ActionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
            select2ActionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(select2ActionCreateInfo.localizedActionName, "test select action 2");
            strcpy(select2ActionCreateInfo.actionName, "test_select_action_2");
            REQUIRE_RESULT(xrCreateAction(actionSet2, &select2ActionCreateInfo, &selectAction2), XR_SUCCESS);

            attachInfo.actionSets = &actionSet2;
            REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_ERROR_ACTIONSETS_ALREADY_ATTACHED);
        }
    }

    TEST_CASE("xrGetCurrentInteractionProfile", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("xrGetCurrentInteractionProfile");
        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile =
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str());

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction selectAction{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test select action");
        strcpy(actionCreateInfo.actionName, "test_select_action");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &selectAction), XR_SUCCESS);

        XrPath leftHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
            compositionHelper.GetSession(),
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str()),
            leftHandPath, cSimpleKHRInteractionProfileDefinition.WhitelistData);

        XrPath rightHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
            compositionHelper.GetSession(),
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str()),
            rightHandPath, cSimpleKHRInteractionProfileDefinition.WhitelistData);

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        XrActiveActionSet activeActionSet{actionSet};
        syncInfo.activeActionSets = &activeActionSet;
        syncInfo.countActiveActionSets = 1;

        XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};

        SECTION("Bindings provided")
        {
            actionLayerManager.WaitForSessionFocusWithMessage();

            compositionHelper.GetInteractionManager().AddActionSet(actionSet);
            compositionHelper.GetInteractionManager().AddActionBindings(
                StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str()),
                {{{selectAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
                  {selectAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")}}});
            compositionHelper.GetInteractionManager().AttachActionSets();

            {
                INFO("Parameter validation");

                {
                    INFO("Basic usage");
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(compositionHelper.GetSession(), leftHandPath, &interactionProfileState),
                                   XR_SUCCESS);
                }
                {
                    INFO("XR_NULL_PATH topLevelPath");
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(compositionHelper.GetSession(), XR_NULL_PATH, &interactionProfileState),
                                   XR_ERROR_PATH_INVALID);
                }
                OPTIONAL_INVALID_HANDLE_VALIDATION_INFO
                {
                    XrSession invalidSession = (XrSession)0x1234;
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(invalidSession, leftHandPath, &interactionProfileState),
                                   XR_ERROR_HANDLE_INVALID);
                }
                {
                    INFO("Invalid top level path");
                    XrPath invalidTopLevelPath = (XrPath)0x1234;
                    REQUIRE_RESULT(
                        xrGetCurrentInteractionProfile(compositionHelper.GetSession(), invalidTopLevelPath, &interactionProfileState),
                        XR_ERROR_PATH_INVALID);
                }
                {
                    INFO("Unsupported top level path");
                    XrPath unsupportedTopLevelPath = StringToPath(compositionHelper.GetInstance(), "/invalid/top/level/path");
                    REQUIRE_RESULT(
                        xrGetCurrentInteractionProfile(compositionHelper.GetSession(), unsupportedTopLevelPath, &interactionProfileState),
                        XR_ERROR_PATH_UNSUPPORTED);
                }
                {
                    INFO("Invalid type");
                    interactionProfileState = XrInteractionProfileState{XR_TYPE_ACTION_CREATE_INFO};
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(compositionHelper.GetSession(), leftHandPath, &interactionProfileState),
                                   XR_ERROR_VALIDATION_FAILURE);
                    interactionProfileState = XrInteractionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
                }
            }
            {
                INFO("Interaction profile changed event");

                // Ensure controllers are on and synced and by now XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED should have been queued.
                // In fact, it may have been queued earlier when actionsets were attached, but that is okay.
                leftHandInputDevice->SetDeviceActive(true);
                rightHandInputDevice->SetDeviceActive(true);
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                XrEventDataBuffer latestEventData{XR_TYPE_EVENT_DATA_BUFFER};
                auto ReadUntilEvent = [&](XrStructureType expectedType, std::chrono::duration<float> timeout) {
                    auto startTime = std::chrono::system_clock::now();
                    while (std::chrono::system_clock::now() - startTime < timeout) {
                        XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
                        if (actionLayerManager.GetEventReader().TryReadNext(eventData) && eventData.type == expectedType) {
                            latestEventData = eventData;
                            return true;
                        }

                        std::this_thread::sleep_for(10ms);
                    }
                    return false;
                };

                REQUIRE(ReadUntilEvent(XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED, 1s));

                REQUIRE_RESULT(xrGetCurrentInteractionProfile(compositionHelper.GetSession(), leftHandPath, &interactionProfileState),
                               XR_SUCCESS);
                REQUIRE(simpleControllerInteractionProfile == interactionProfileState.interactionProfile);
                REQUIRE_RESULT(xrGetCurrentInteractionProfile(compositionHelper.GetSession(), rightHandPath, &interactionProfileState),
                               XR_SUCCESS);
                REQUIRE(simpleControllerInteractionProfile == interactionProfileState.interactionProfile);
            }
        }
    }

    TEST_CASE("xrSyncActions", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("xrSyncActions");

        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile =
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str());

        std::string leftHandPathString = "/user/hand/left";
        XrPath leftHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
                             compositionHelper.GetSession(), simpleControllerInteractionProfile, leftHandPath,
                             cSimpleKHRInteractionProfileDefinition.WhitelistData);

        XrPath rightHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
                             compositionHelper.GetSession(), simpleControllerInteractionProfile, rightHandPath,
                             cSimpleKHRInteractionProfileDefinition.WhitelistData);

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction action{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action");
        strcpy(actionCreateInfo.actionName, "test_action");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

        XrActionStateBoolean actionStateBoolean{XR_TYPE_ACTION_STATE_BOOLEAN};
        PoisonStructContents(actionStateBoolean);
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;

        compositionHelper.BeginSession();
        SECTION("No Focus")
        {
            compositionHelper.GetInteractionManager().AddActionSet(actionSet);
            compositionHelper.GetInteractionManager().AttachActionSets();

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            XrActiveActionSet activeActionSet{actionSet};
            syncInfo.activeActionSets = &activeActionSet;
            syncInfo.countActiveActionSets = 1;

            REQUIRE_RESULT(xrSyncActions(compositionHelper.GetSession(), &syncInfo), XR_SESSION_NOT_FOCUSED);

            REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
            REQUIRE_FALSE(actionStateBoolean.isActive);
        }
        SECTION("Focus")
        {
            actionLayerManager.WaitForSessionFocusWithMessage();

            SECTION("Parameter validation")
            {
                XrPath leftHandSelectPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click");
                compositionHelper.GetInteractionManager().AddActionSet(actionSet);
                compositionHelper.GetInteractionManager().AddActionBindings(simpleControllerInteractionProfile,
                                                                            {{action, leftHandSelectPath}});
                compositionHelper.GetInteractionManager().AttachActionSets();

                leftHandInputDevice->SetDeviceActive(true);
                rightHandInputDevice->SetDeviceActive(true);

                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                XrActiveActionSet activeActionSet{actionSet};
                syncInfo.activeActionSets = &activeActionSet;
                syncInfo.countActiveActionSets = 1;

                {
                    INFO("Basic usage");

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                    REQUIRE(actionStateBoolean.isActive);
                    REQUIRE_FALSE(actionStateBoolean.currentState);

                    {
                        INFO("Repeated state query calls return the same value");

                        leftHandInputDevice->SetButtonStateBool(leftHandSelectPath, true);

                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);
                        REQUIRE(actionStateBoolean.currentState);

                        REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);
                        REQUIRE(actionStateBoolean.currentState);

                        actionLayerManager.DisplayMessage("Turn off " + leftHandPathString + " and wait for 20s");
                        leftHandInputDevice->SetDeviceActive(false, true);

                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean),
                                               XR_SUCCESS);
                                REQUIRE(actionStateBoolean.isActive);
                                REQUIRE(actionStateBoolean.currentState);
                                return false;
                            },
                            20s, 100ms);

                        actionLayerManager.DisplayMessage("Wait for 5s");

                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean),
                                               XR_SUCCESS);
                                REQUIRE_FALSE(actionStateBoolean.isActive);
                                REQUIRE_FALSE(actionStateBoolean.currentState);
                                return false;
                            },
                            5s, 100ms);
                    }
                }
                OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
                {
                    XrSession invalidSession = (XrSession)0x1234;
                    REQUIRE_RESULT(xrSyncActions(invalidSession, &syncInfo), XR_ERROR_HANDLE_INVALID);
                }
            }
            SECTION("Priority rules")
            {
                const XrPath bothPaths[2] = {leftHandPath, rightHandPath};

                XrActionSet highPriorityActionSet{XR_NULL_HANDLE};
                XrActionSetCreateInfo setCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                strcpy(setCreateInfo.actionSetName, "high_priority_action_set");
                strcpy(setCreateInfo.localizedActionSetName, "high priority action set");
                setCreateInfo.priority = 3;
                REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &setCreateInfo, &highPriorityActionSet), XR_SUCCESS);

                XrAction highPrioritySelectAction{XR_NULL_HANDLE};
                XrAction highPrioritySelectAction2{XR_NULL_HANDLE};
                XrActionCreateInfo createInfo{XR_TYPE_ACTION_CREATE_INFO};
                strcpy(createInfo.actionName, std::string("test_click_a").c_str());
                createInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy(createInfo.localizedActionName, "test click action a");
                createInfo.countSubactionPaths = 2;
                createInfo.subactionPaths = bothPaths;
                REQUIRE_RESULT(xrCreateAction(highPriorityActionSet, &createInfo, &highPrioritySelectAction), XR_SUCCESS);

                strcpy(createInfo.actionName, std::string("test_click_a_2").c_str());
                strcpy(createInfo.localizedActionName, "test click action a 2");
                REQUIRE_RESULT(xrCreateAction(highPriorityActionSet, &createInfo, &highPrioritySelectAction2), XR_SUCCESS);

                XrActionSet lowPriorityActionSet{XR_NULL_HANDLE};
                strcpy(setCreateInfo.actionSetName, "low_priority_action_set");
                strcpy(setCreateInfo.localizedActionSetName, "low priority action set");
                setCreateInfo.priority = 2;
                REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &setCreateInfo, &lowPriorityActionSet), XR_SUCCESS);

                XrAction lowPrioritySelectAction{XR_NULL_HANDLE};
                XrAction lowPriorityMenuAction{XR_NULL_HANDLE};
                XrAction lowPrioritySelectAndMenuAction{XR_NULL_HANDLE};
                strcpy(createInfo.actionName, std::string("test_click_b").c_str());
                createInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy(createInfo.localizedActionName, "test click action b");
                REQUIRE_RESULT(xrCreateAction(lowPriorityActionSet, &createInfo, &lowPrioritySelectAction), XR_SUCCESS);

                strcpy(createInfo.actionName, std::string("test_click_b_2").c_str());
                strcpy(createInfo.localizedActionName, "test click action b 2");
                REQUIRE_RESULT(xrCreateAction(lowPriorityActionSet, &createInfo, &lowPriorityMenuAction), XR_SUCCESS);

                strcpy(createInfo.actionName, std::string("test_click_b_3").c_str());
                strcpy(createInfo.localizedActionName, "test click action b 3");
                REQUIRE_RESULT(xrCreateAction(lowPriorityActionSet, &createInfo, &lowPrioritySelectAndMenuAction), XR_SUCCESS);

                compositionHelper.GetInteractionManager().AddActionBindings(
                    simpleControllerInteractionProfile,
                    {
                        {highPrioritySelectAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
                        {highPrioritySelectAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
                        {highPrioritySelectAction2, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
                        {highPrioritySelectAction2, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
                        {lowPrioritySelectAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
                        {lowPrioritySelectAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
                        {lowPriorityMenuAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click")},
                        {lowPriorityMenuAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click")},
                        {lowPrioritySelectAndMenuAction,
                         StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
                        {lowPrioritySelectAndMenuAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click")},
                        {lowPrioritySelectAndMenuAction,
                         StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
                        {lowPrioritySelectAndMenuAction,
                         StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click")},
                    });

                compositionHelper.GetInteractionManager().AddActionSet(highPriorityActionSet);
                compositionHelper.GetInteractionManager().AddActionSet(lowPriorityActionSet);
                compositionHelper.GetInteractionManager().AttachActionSets();

                leftHandInputDevice->SetDeviceActive(true);
                rightHandInputDevice->SetDeviceActive(true);

                XrActiveActionSet highPriorityRightHandActiveActionSet{highPriorityActionSet, rightHandPath};
                XrActiveActionSet lowPriorityRightHandActiveActionSet{lowPriorityActionSet, rightHandPath};
                XrActiveActionSet highPriorityLeftHandActiveActionSet{highPriorityActionSet, leftHandPath};
                XrActiveActionSet lowPriorityLeftHandActiveActionSet{lowPriorityActionSet, leftHandPath};

                auto getActionActiveState = [&](XrAction action, XrPath subactionPath) {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = action;
                    getInfo.subactionPath = subactionPath;
                    XrActionStateBoolean booleanData{XR_TYPE_ACTION_STATE_BOOLEAN};
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanData), XR_SUCCESS);
                    return static_cast<bool>(booleanData.isActive);
                };

                std::vector<XrActiveActionSet> activeSets;
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};

                // Both sets with null subaction path
                activeSets = {lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet, highPriorityLeftHandActiveActionSet,
                              highPriorityRightHandActiveActionSet};
                syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                syncInfo.activeActionSets = activeSets.data();
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                INFO("high priority + low priority");
                REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == true);

                REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == false);   // Blocked by high priority
                REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == false);   // Blocked by high priority
                REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == false);  // Blocked by high priority
                REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == true);

                // Both sets with right hand subaction path
                activeSets = {highPriorityRightHandActiveActionSet, lowPriorityRightHandActiveActionSet};
                syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                syncInfo.activeActionSets = activeSets.data();
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                INFO("right handed high priority + right handed low priority");
                REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == true);

                REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == false);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == true);

                // Both sets with left hand subaction path
                activeSets = {highPriorityLeftHandActiveActionSet, lowPriorityLeftHandActiveActionSet};
                syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                syncInfo.activeActionSets = activeSets.data();
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                INFO("left handed high priority + left handed low priority");
                REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == false);

                REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == false);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == false);

                // Both sets with differing subaction path
                activeSets = {highPriorityRightHandActiveActionSet, lowPriorityLeftHandActiveActionSet};
                syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                syncInfo.activeActionSets = activeSets.data();
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                INFO("right handed high priority + left handed low priority");
                REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == true);

                REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == false);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == false);

                // Both sets with differing subaction path
                activeSets = {highPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet};
                syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                syncInfo.activeActionSets = activeSets.data();
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                INFO("left handed high priority + right handed low priority");
                REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == false);

                REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == true);

                // Both sets with differing subaction path
                activeSets = {highPriorityRightHandActiveActionSet, lowPriorityLeftHandActiveActionSet,
                              lowPriorityRightHandActiveActionSet};
                syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                syncInfo.activeActionSets = activeSets.data();
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                INFO("right handed high priority + low priority");
                REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == true);

                REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == false);  // Blocked by high priority
                REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == true);  // Menu blocked but squeeze active

                // Both sets with differing subaction path
                activeSets = {highPriorityRightHandActiveActionSet, lowPriorityLeftHandActiveActionSet,
                              lowPriorityRightHandActiveActionSet};
                syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                syncInfo.activeActionSets = activeSets.data();
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                INFO("right handed high priority + left handed low priority + right handed low priority");
                REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == false);
                REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == true);

                REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == false);  // Blocked by high priority
                REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == true);
                REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == true);  // Menu blocked but squeeze active
            }
            SECTION("subaction path rules")
            {
                XrActionSet subactionPathFreeActionSet{XR_NULL_HANDLE};
                strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 2");
                strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_2");
                REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &subactionPathFreeActionSet),
                               XR_SUCCESS);

                XrAction leftHandAction{XR_NULL_HANDLE};
                actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy(actionCreateInfo.localizedActionName, "test select action");
                strcpy(actionCreateInfo.actionName, "test_select_action");
                actionCreateInfo.countSubactionPaths = 1;
                actionCreateInfo.subactionPaths = &leftHandPath;
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &leftHandAction), XR_SUCCESS);

                XrAction rightHandAction{XR_NULL_HANDLE};
                strcpy(actionCreateInfo.localizedActionName, "test select action 2");
                strcpy(actionCreateInfo.actionName, "test_select_action_2");
                actionCreateInfo.subactionPaths = &rightHandPath;
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &rightHandAction), XR_SUCCESS);

                compositionHelper.GetInteractionManager().AddActionBindings(
                    simpleControllerInteractionProfile,
                    {{leftHandAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
                     {rightHandAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")}});
                compositionHelper.GetInteractionManager().AddActionSet(actionSet);
                compositionHelper.GetInteractionManager().AddActionSet(subactionPathFreeActionSet);
                compositionHelper.GetInteractionManager().AttachActionSets();

                leftHandInputDevice->SetDeviceActive(true);
                rightHandInputDevice->SetDeviceActive(true);

                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                XrActiveActionSet activeActionSet{actionSet};
                XrActiveActionSet subactionPathFreeActiveActionSet{subactionPathFreeActionSet};
                syncInfo.activeActionSets = &activeActionSet;
                syncInfo.countActiveActionSets = 1;

                {
                    INFO("Basic usage");

                    INFO("Left hand");
                    activeActionSet.subactionPath = leftHandPath;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    getInfo.action = leftHandAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                    REQUIRE(actionStateBoolean.isActive);

                    getInfo.action = rightHandAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                    REQUIRE_FALSE(actionStateBoolean.isActive);
                    {
                        INFO("Values match those specified for isActive == XR_FALSE");
                        // Set these to the wrong thing if not active, to make sure runtime overwrites the values
                        PoisonStructContents(actionStateBoolean);
                        REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE_FALSE(actionStateBoolean.isActive);
                        // The conformance layer will verify that the other fields have been cleared appropriately.
                    }

                    INFO("Right hand");
                    activeActionSet.subactionPath = rightHandPath;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    getInfo.action = leftHandAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                    REQUIRE_FALSE(actionStateBoolean.isActive);

                    getInfo.action = rightHandAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                    REQUIRE(actionStateBoolean.isActive);

                    INFO("both synchronized");
                    XrActiveActionSet bothHands[2] = {{actionSet}, {actionSet}};
                    bothHands[0].subactionPath = leftHandPath;
                    bothHands[1].subactionPath = rightHandPath;
                    syncInfo.countActiveActionSets = 2;
                    syncInfo.activeActionSets = bothHands;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    getInfo.action = leftHandAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                    REQUIRE(actionStateBoolean.isActive);

                    getInfo.action = rightHandAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &actionStateBoolean), XR_SUCCESS);
                    REQUIRE(actionStateBoolean.isActive);

                    INFO("No subaction path");
                    activeActionSet.subactionPath = XR_NULL_PATH;
                    syncInfo.countActiveActionSets = 1;
                    syncInfo.activeActionSets = &activeActionSet;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    syncInfo.activeActionSets = &subactionPathFreeActiveActionSet;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    INFO("Subaction path used but not declared");
                    subactionPathFreeActiveActionSet.subactionPath = leftHandPath;
                    REQUIRE_RESULT(xrSyncActions(compositionHelper.GetSession(), &syncInfo), XR_ERROR_PATH_UNSUPPORTED);

                    XrActionSet unattachedActionSet{XR_NULL_HANDLE};
                    strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 3");
                    strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_3");
                    REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &unattachedActionSet),
                                   XR_SUCCESS);

                    INFO("unattached action set");
                    XrActiveActionSet activeActionSet2 = {unattachedActionSet};
                    syncInfo.countActiveActionSets = 1;
                    syncInfo.activeActionSets = &activeActionSet2;
                    REQUIRE_RESULT(xrSyncActions(compositionHelper.GetSession(), &syncInfo), XR_ERROR_ACTIONSET_NOT_ATTACHED);

                    XrActiveActionSet bothSets[2] = {{actionSet}, {unattachedActionSet}};
                    syncInfo.countActiveActionSets = 2;
                    syncInfo.activeActionSets = bothSets;
                    REQUIRE_RESULT(xrSyncActions(compositionHelper.GetSession(), &syncInfo), XR_ERROR_ACTIONSET_NOT_ATTACHED);
                }
                {
                    INFO("Invalid subaction path");
                    syncInfo.countActiveActionSets = 1;
                    syncInfo.activeActionSets = &activeActionSet;
                    activeActionSet.subactionPath = (XrPath)0x1234;
                    REQUIRE_RESULT(xrSyncActions(compositionHelper.GetSession(), &syncInfo), XR_ERROR_PATH_INVALID);
                }
            }
        }
    }

    TEST_CASE("State query functions interactive", "[actions][interactive][gamepad]")
    {
        struct ActionInfo
        {
            InputSourcePathData Data;
            XrAction Action{XR_NULL_HANDLE};
            XrAction XAction{XR_NULL_HANDLE};  // Set if type is vector2f
            XrAction YAction{XR_NULL_HANDLE};  // Set if type is vector2f
            std::set<int32_t> UnseenValues;
        };

        constexpr float cStepSize = 0.5f;
        const int32_t cStepSizeOffset = -int32_t(std::roundf(-1.f / cStepSize));
        constexpr float cEpsilon = 0.1f;
        constexpr float cLargeEpsilon = 0.15f;

        auto TestInteractionProfile = [&](const InteractionProfileMetadata& ipMetadata, const std::string& topLevelPathString) {
            CompositionHelper compositionHelper("Input device state query");
            compositionHelper.BeginSession();
            ActionLayerManager actionLayerManager(compositionHelper);

            actionLayerManager.WaitForSessionFocusWithMessage();

            XrPath interactionProfile = StringToPath(compositionHelper.GetInstance(), ipMetadata.InteractionProfilePathString.c_str());
            XrPath inputDevicePath{StringToPath(compositionHelper.GetInstance(), topLevelPathString.data())};
            std::shared_ptr<IInputTestDevice> inputDevice =
                CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
                                 compositionHelper.GetSession(), interactionProfile, inputDevicePath, ipMetadata.WhitelistData);

            XrActionSet actionSet{XR_NULL_HANDLE};

            std::string actionSetName = "state_query_test_action_set_" + std::to_string(inputDevicePath);
            std::string localizedActionSetName = "State Query Test Action Set " + std::to_string(inputDevicePath);

            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.localizedActionSetName, localizedActionSetName.c_str());
            strcpy(actionSetCreateInfo.actionSetName, actionSetName.c_str());
            REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &actionSet), XR_SUCCESS);

            uint32_t uniqueActionNameCounter = 0;
            auto GetActionNames = [&uniqueActionNameCounter]() mutable -> std::tuple<std::string, std::string> {
                uniqueActionNameCounter++;
                return std::tuple<std::string, std::string>{"state_query_test_action_" + std::to_string(uniqueActionNameCounter),
                                                            "state query test action " + std::to_string(uniqueActionNameCounter)};
            };

            auto PrefixedByTopLevelPath = [&topLevelPathString](std::string binding) {
                return (binding.length() > topLevelPathString.size()) &&
                       (std::mismatch(topLevelPathString.begin(), topLevelPathString.end(), binding.begin()).first ==
                        topLevelPathString.end());
            };

            auto InputSourceDataForTopLevelPath = [&]() {
                std::vector<InputSourcePathData> ret;
                for (InputSourcePathData inputSourceData : ipMetadata.WhitelistData) {
                    if (!PrefixedByTopLevelPath(inputSourceData.Path)) {
                        continue;
                    }
                    ret.push_back(inputSourceData);
                }
                return ret;
            };
            auto ActionsForTopLevelPath = [&](XrActionType type) -> std::vector<ActionInfo> {
                auto inputSourceDataList = InputSourceDataForTopLevelPath();
                std::vector<ActionInfo> actions;
                for (InputSourcePathData inputSourceData : inputSourceDataList) {
                    if (type != inputSourceData.Type) {
                        continue;
                    }

                    // Skip /x or /y components since we handle those with the parent Vector2f.
                    const std::string pathString = inputSourceData.Path;
                    std::cmatch bindingPathRegexMatch;
                    REQUIRE_MSG(std::regex_match(pathString.data(), bindingPathRegexMatch, cInteractionSourcePathRegex),
                                "input source path does not match require format");
                    if (bindingPathRegexMatch[7].matched) {
                        if (bindingPathRegexMatch[7] == "x" || bindingPathRegexMatch[7] == "y") {
#if !defined(NDEBUG)
                            ReportF("Skipping %s", pathString.c_str());
#endif
                            continue;
                        }
                    }

                    XrAction action{XR_NULL_HANDLE};
                    XrAction xAction{XR_NULL_HANDLE};
                    XrAction yAction{XR_NULL_HANDLE};

                    XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
                    actionCreateInfo.actionType = inputSourceData.Type;
                    auto actionNames = GetActionNames();
                    strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                    strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

                    XrPath bindingPath = StringToPath(compositionHelper.GetInstance(), inputSourceData.Path.c_str());
                    compositionHelper.GetInteractionManager().AddActionBindings(interactionProfile, {{action, bindingPath}});

                    ActionInfo info{};

                    switch (inputSourceData.Type) {
                    case XR_ACTION_TYPE_BOOLEAN_INPUT:
                        // Need to see 0 1
                        info.UnseenValues = {false, true};
                        break;
                    case XR_ACTION_TYPE_FLOAT_INPUT:
                        // Need to see normalized [0..2] 0 1 2
                        for (float f = 0.f; f <= 1.f; f += cStepSize) {
                            info.UnseenValues.insert(int32_t(std::roundf(f / cStepSize)));
                        }
                        break;
                    case XR_ACTION_TYPE_VECTOR2F_INPUT:
                        // Need to see normalized [0..4] x + y * 10:
                        //    01 02 03
                        // 10 11 12 13 14
                        // 20 21 22 23 24
                        // 30 31 32 33 34
                        //    41 42 43

                        // Avoid corner values that a circular thumbstick can't generate (both > cos(45_deg)):
                        const float limit = std::cos(std::acos(-1.f) / 4.f);

                        for (float x = -1.f; x <= 1.f; x += cStepSize) {
                            int32_t i = int32_t(std::roundf(x / cStepSize)) + cStepSizeOffset;
                            for (float y = -1.f; y <= 1.f; y += cStepSize) {
                                if ((std::fabs(x) > limit) && (std::fabs(y) > limit))
                                    continue;
                                int32_t j = int32_t(std::roundf(y / cStepSize)) + cStepSizeOffset;
                                info.UnseenValues.insert(i + j * 10);
                            }
                        }

                        // If we have a vector action, we must have /x and /y float actions.
                        actionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                        actionNames = GetActionNames();
                        strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                        strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &xAction), XR_SUCCESS);

                        std::string xSubBindingPath = std::string(inputSourceData.Path.c_str()) + "/x";
                        bindingPath = StringToPath(compositionHelper.GetInstance(), xSubBindingPath.c_str());
                        compositionHelper.GetInteractionManager().AddActionBindings(interactionProfile, {{xAction, bindingPath}});

                        actionNames = GetActionNames();
                        strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                        strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &yAction), XR_SUCCESS);

                        std::string ySubBindingPath = std::string(inputSourceData.Path.c_str()) + "/y";
                        bindingPath = StringToPath(compositionHelper.GetInstance(), ySubBindingPath.c_str());
                        compositionHelper.GetInteractionManager().AddActionBindings(interactionProfile, {{yAction, bindingPath}});
                        break;
                    }

#if !defined(NDEBUG)
                    // Debug UnseenValues
                    std::string unseen = inputSourceData.Path;
                    for (auto key : info.UnseenValues) {
                        unseen += " " + std::to_string(key);
                    }
                    ReportF("Keys for %s", unseen.c_str());
#endif

                    info.Data = inputSourceData;
                    info.Action = action;
                    info.XAction = xAction;
                    info.YAction = yAction;

                    actions.push_back(info);
                }

                return actions;
            };
            auto ActionsForTopLevelPathCoerced = [&](XrActionType type, XrActionType coercionType) -> std::vector<ActionInfo> {
                auto inputSourceDataList = InputSourceDataForTopLevelPath();

                auto HasSubpathOfType = [&](std::string parentPath, XrActionType type) {
                    for (const InputSourcePathData& inputSourceData : inputSourceDataList) {
                        if (inputSourceData.Type != type) {
                            continue;
                        }
                        auto prefixedByParentPath =
                            (inputSourceData.Path.length() > parentPath.size()) &&
                            (std::mismatch(parentPath.begin(), parentPath.end(), inputSourceData.Path.begin()).first == parentPath.end());
                        if (prefixedByParentPath) {
                            return true;
                        }
                    }
                    return false;
                };

                std::vector<ActionInfo> actions;
                for (const InputSourcePathData& inputSourceData : inputSourceDataList) {
                    if (type != inputSourceData.Type) {
                        continue;
                    }

                    // If we are using the parent path, the runtime should map it if there is a subpath
                    // e.g. .../thumbstick may get bound to .../thumbstick/click which is valid
                    const std::string pathString = inputSourceData.Path;
                    std::cmatch bindingPathRegexMatch;
                    REQUIRE_MSG(std::regex_match(pathString.data(), bindingPathRegexMatch, cInteractionSourcePathRegex),
                                "input source path does not match require format");
                    if (bindingPathRegexMatch[5].matched) {
                        if (coercionType == XR_ACTION_TYPE_BOOLEAN_INPUT &&
                            HasSubpathOfType(inputSourceData.Path, XR_ACTION_TYPE_BOOLEAN_INPUT)) {
                            continue;
                        }
                        else if (coercionType == XR_ACTION_TYPE_FLOAT_INPUT &&
                                 HasSubpathOfType(inputSourceData.Path, XR_ACTION_TYPE_FLOAT_INPUT)) {
                            continue;
                        }
                        else if (coercionType == XR_ACTION_TYPE_POSE_INPUT &&
                                 HasSubpathOfType(inputSourceData.Path, XR_ACTION_TYPE_POSE_INPUT)) {
                            continue;
                        }
                    }

                    XrAction action{XR_NULL_HANDLE};
                    XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
                    actionCreateInfo.actionType = coercionType;
                    auto actionNames = GetActionNames();
                    strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                    strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

                    XrPath bindingPath = StringToPath(compositionHelper.GetInstance(), inputSourceData.Path.c_str());
                    compositionHelper.GetInteractionManager().AddActionBindings(interactionProfile, {{action, bindingPath}});

                    ActionInfo info{};
                    info.Data = inputSourceData;
                    info.Data.Type = coercionType;
                    info.Action = action;
                    actions.push_back(info);
                }

                return actions;
            };
            auto ActionOfTypeForTopLevelPath = [&](XrActionType type) -> ActionInfo {
                auto inputSourceDataList = InputSourceDataForTopLevelPath();

                XrAction action{XR_NULL_HANDLE};
                XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
                actionCreateInfo.actionType = type;
                auto actionNames = GetActionNames();
                strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

                for (InputSourcePathData inputSourceData : inputSourceDataList) {
                    if (type != inputSourceData.Type) {
                        continue;
                    }

                    XrPath bindingPath = StringToPath(compositionHelper.GetInstance(), inputSourceData.Path.c_str());
                    compositionHelper.GetInteractionManager().AddActionBindings(interactionProfile, {{action, bindingPath}});
                }

                ActionInfo info{};
                info.Action = action;

                return info;
            };
            auto concat = [](std::vector<ActionInfo> a, std::vector<std::vector<ActionInfo>> tail) {
                for (const auto& b : tail) {
                    a.insert(a.end(), b.begin(), b.end());
                }
                return a;
            };

            // Actions for each of source of a type
            auto booleanActions = ActionsForTopLevelPath(XR_ACTION_TYPE_BOOLEAN_INPUT);
            auto floatActions = ActionsForTopLevelPath(XR_ACTION_TYPE_FLOAT_INPUT);
            auto vectorActions = ActionsForTopLevelPath(XR_ACTION_TYPE_VECTOR2F_INPUT);
            auto poseActions = ActionsForTopLevelPath(XR_ACTION_TYPE_POSE_INPUT);
            auto hapticActions = ActionsForTopLevelPath(XR_ACTION_TYPE_VIBRATION_OUTPUT);

            // Single actions bound to all of a type
            auto allBooleanAction = ActionOfTypeForTopLevelPath(XR_ACTION_TYPE_BOOLEAN_INPUT);
            auto allFloatAction = ActionOfTypeForTopLevelPath(XR_ACTION_TYPE_FLOAT_INPUT);
            auto allVectorAction = ActionOfTypeForTopLevelPath(XR_ACTION_TYPE_VECTOR2F_INPUT);

            // Actions for each source of a type coerced to a different type
            auto booleanActionsCoercedToFloat = ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_BOOLEAN_INPUT, XR_ACTION_TYPE_FLOAT_INPUT);
            auto floatActionsCoercedToBoolean = ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_FLOAT_INPUT, XR_ACTION_TYPE_BOOLEAN_INPUT);
            auto allOtherCoercions = concat({}, {ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_BOOLEAN_INPUT, XR_ACTION_TYPE_VECTOR2F_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_BOOLEAN_INPUT, XR_ACTION_TYPE_POSE_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_FLOAT_INPUT, XR_ACTION_TYPE_VECTOR2F_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_FLOAT_INPUT, XR_ACTION_TYPE_POSE_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_VECTOR2F_INPUT, XR_ACTION_TYPE_BOOLEAN_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_VECTOR2F_INPUT, XR_ACTION_TYPE_FLOAT_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_VECTOR2F_INPUT, XR_ACTION_TYPE_POSE_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_POSE_INPUT, XR_ACTION_TYPE_BOOLEAN_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_POSE_INPUT, XR_ACTION_TYPE_FLOAT_INPUT),
                                                 ActionsForTopLevelPathCoerced(XR_ACTION_TYPE_POSE_INPUT, XR_ACTION_TYPE_VECTOR2F_INPUT)});

            compositionHelper.GetInteractionManager().AddActionSet(actionSet);
            compositionHelper.GetInteractionManager().AttachActionSets();

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            XrActiveActionSet activeActionSet{actionSet};
            syncInfo.activeActionSets = &activeActionSet;
            syncInfo.countActiveActionSets = 1;

            inputDevice->SetDeviceActive(true);

            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

            XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
            REQUIRE_RESULT(xrGetCurrentInteractionProfile(compositionHelper.GetSession(), inputDevicePath, &interactionProfileState),
                           XR_SUCCESS);
            REQUIRE(interactionProfile == interactionProfileState.interactionProfile);

            XrActionStateBoolean booleanState{XR_TYPE_ACTION_STATE_BOOLEAN};
            PoisonStructContents(booleanState);
            XrActionStateFloat floatState{XR_TYPE_ACTION_STATE_FLOAT};
            PoisonStructContents(floatState);
            XrActionStateVector2f vectorState{XR_TYPE_ACTION_STATE_VECTOR2F};
            PoisonStructContents(vectorState);
            XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};

            INFO("Check controller input values");
            {
                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};

                actionLayerManager.DisplayMessage("Use all controller inputs on\n" + topLevelPathString);
                std::this_thread::sleep_for(1s);

                XrActionStateBoolean combinedBoolState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XrActionStateFloat combinedFloatState{XR_TYPE_ACTION_STATE_FLOAT};
                XrActionStateVector2f combinedVectorState{XR_TYPE_ACTION_STATE_VECTOR2F};
                XrActionStateBoolean previousBoolState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XrActionStateFloat previousFloatState{XR_TYPE_ACTION_STATE_FLOAT};
                XrActionStateVector2f previousVectorState{XR_TYPE_ACTION_STATE_VECTOR2F};

                getInfo.action = allBooleanAction.Action;
                REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &previousBoolState), XR_SUCCESS);
                getInfo.action = allFloatAction.Action;
                REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &previousFloatState), XR_SUCCESS);
                getInfo.action = allVectorAction.Action;
                REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &previousVectorState), XR_SUCCESS);

                // Synthetic values for automation (these loop around by cStepSize in x then y order).
                float synthesizedX = 0.f;
                float synthesizedY = 0.f;
                // Number of actions that need value observations.
                auto actionCount = booleanActions.size() + floatActions.size() + vectorActions.size();
                // Actions for which all necessary values have been observed.
                std::set<XrAction> seenActions{};
                // If more than one of these input types exist, check that they can be combined.
                bool waitForCombinedBools = booleanActions.size() > 1;
                bool waitForCombinedFloats = floatActions.size() > 1;
                bool waitForCombinedVectors = vectorActions.size() > 1;

                auto mag = [](const XrVector2f& v) -> float { return v.x * v.x + v.y * v.y; };

                WaitUntilPredicateWithTimeout(
                    [&]() {
                        actionLayerManager.GetRenderLoop().IterateFrame();

                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        getInfo.action = allBooleanAction.Action;
                        REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &combinedBoolState), XR_SUCCESS);
                        getInfo.action = allFloatAction.Action;
                        REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &combinedFloatState), XR_SUCCESS);
                        getInfo.action = allVectorAction.Action;
                        REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &combinedVectorState),
                                       XR_SUCCESS);

                        REQUIRE((bool)combinedBoolState.isActive == booleanActions.size() > 0);
                        REQUIRE((bool)combinedFloatState.isActive == floatActions.size() > 0);
                        REQUIRE((bool)combinedVectorState.isActive == vectorActions.size() > 0);

                        bool shouldBeChanged = (combinedBoolState.currentState != previousBoolState.currentState) &&
                                               combinedBoolState.isActive && previousBoolState.isActive;
                        REQUIRE((bool)combinedBoolState.changedSinceLastSync == shouldBeChanged);
                        shouldBeChanged = (combinedFloatState.currentState != previousFloatState.currentState) &&
                                          combinedFloatState.isActive && previousFloatState.isActive;
                        REQUIRE((bool)combinedFloatState.changedSinceLastSync == shouldBeChanged);
                        shouldBeChanged = ((combinedVectorState.currentState.x != previousVectorState.currentState.x) ||
                                           (combinedVectorState.currentState.y != previousVectorState.currentState.y)) &&
                                          combinedVectorState.isActive && previousVectorState.isActive;
                        REQUIRE((bool)combinedVectorState.changedSinceLastSync == shouldBeChanged);

                        previousBoolState = combinedBoolState;
                        previousFloatState = combinedFloatState;
                        previousVectorState = combinedVectorState;

                        int combinedBoolCount = 0;
                        int combinedFloatCount = 0;
                        int combinedVectorCount = 0;
                        XrActionStateBoolean largestBoolState{XR_TYPE_ACTION_STATE_BOOLEAN};
                        XrActionStateFloat largestFloatState{XR_TYPE_ACTION_STATE_FLOAT};
                        XrActionStateVector2f largestVectorState{XR_TYPE_ACTION_STATE_VECTOR2F};

                        // Track a remaining unseen action & values to prompt with.
                        std::string nextActionPrompt;
                        auto update = [&](int32_t key, ActionInfo& actionInfo) {
                            // Don't do anything if all the action's values have been seen.
                            if (seenActions.count(actionInfo.Action) > 0)
                                return;
                            // Remove the key if it's never been seen.
                            if (actionInfo.UnseenValues.count(key) > 0) {
                                actionInfo.UnseenValues.erase(key);
#if !defined(NDEBUG)
                                ReportF("%s saw %d", actionInfo.Data.Path.c_str(), key);
#endif
                            }
                            // If we've seen all the values mark the whole action as seen.
                            if (actionInfo.UnseenValues.empty()) {
                                seenActions.insert(actionInfo.Action);
                                return;
                            }
                            // For now just prompt with the first still-pending action and its values.
                            if (!nextActionPrompt.empty())
                                return;
                            nextActionPrompt = "\n" + actionInfo.Data.Path + ":\n";
                            auto fmt_float = [](float v) -> std::string {
                                auto s = std::to_string(v);
                                if (s.length() > 4)
                                    s.resize(4);
                                return s;
                            };
                            for (auto remainingKeys : actionInfo.UnseenValues) {
                                switch (actionInfo.Data.Type) {
                                case XR_ACTION_TYPE_BOOLEAN_INPUT:
                                    nextActionPrompt += remainingKeys ? "true " : "false ";
                                    break;
                                case XR_ACTION_TYPE_FLOAT_INPUT:
                                    nextActionPrompt += fmt_float(remainingKeys * cStepSize) + " ";
                                    break;
                                case XR_ACTION_TYPE_VECTOR2F_INPUT:
                                    float x = ((remainingKeys % 10) - 2) * cStepSize;
                                    float y = ((remainingKeys / 10) - 2) * cStepSize;
                                    nextActionPrompt += "(" + fmt_float(x) + "," + fmt_float(y) + ") ";
                                    break;
                                }
                            }
                        };

                        for (auto& actionInfo : booleanActions) {
                            getInfo.action = actionInfo.Action;
                            REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState), XR_SUCCESS);
                            if (booleanState.isActive) {
                                auto key = int32_t(booleanState.currentState);
                                update(key, actionInfo);
                                ++combinedBoolCount;
                                if (!largestBoolState.isActive || (largestBoolState.currentState < booleanState.currentState)) {
                                    largestBoolState = booleanState;
                                }
                            }
                        }

                        for (auto& actionInfo : floatActions) {
                            getInfo.action = actionInfo.Action;
                            REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_SUCCESS);
                            if (floatState.isActive) {
                                auto key = int32_t(std::roundf(floatState.currentState / cStepSize));
                                update(key, actionInfo);
                                ++combinedFloatCount;
                                if (!largestFloatState.isActive ||
                                    (std::fabs(largestFloatState.currentState) < std::fabs(floatState.currentState))) {
                                    largestFloatState = floatState;
                                }
                            }
                        }

                        for (auto& actionInfo : vectorActions) {
                            getInfo.action = actionInfo.Action;
                            REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState), XR_SUCCESS);
                            if (vectorState.isActive) {
                                auto i = int32_t(std::roundf(vectorState.currentState.x / cStepSize)) + cStepSizeOffset;
                                auto j = int32_t(std::roundf(vectorState.currentState.y / cStepSize)) + cStepSizeOffset;
                                auto key = i + j * 10;
                                update(key, actionInfo);
                                ++combinedVectorCount;
                                if (!largestVectorState.isActive ||
                                    (mag(largestVectorState.currentState) < mag(vectorState.currentState))) {
                                    largestVectorState = vectorState;
                                }

                                // At least one of x, y has to change if the parent changes.
                                int xyChanges = 0;

                                // Verify the x action matches the parent vector.
                                getInfo.action = actionInfo.XAction;
                                REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_SUCCESS);
                                REQUIRE(floatState.isActive);
                                REQUIRE(floatState.currentState == vectorState.currentState.x);
                                ++combinedFloatCount;
                                if (!largestFloatState.isActive ||
                                    (std::fabs(largestFloatState.currentState) < std::fabs(floatState.currentState))) {
                                    largestFloatState = floatState;
                                }
                                if (floatState.changedSinceLastSync) {
                                    REQUIRE(floatState.changedSinceLastSync == vectorState.changedSinceLastSync);
                                    REQUIRE(floatState.lastChangeTime == vectorState.lastChangeTime);
                                    ++xyChanges;
                                }
                                if (!vectorState.changedSinceLastSync) {
                                    REQUIRE(floatState.changedSinceLastSync == XR_FALSE);
                                }

                                // Verify the y action matches the parent vector.
                                getInfo.action = actionInfo.YAction;
                                REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_SUCCESS);
                                REQUIRE(floatState.isActive);
                                REQUIRE(floatState.currentState == vectorState.currentState.y);
                                ++combinedFloatCount;
                                if (!largestFloatState.isActive ||
                                    (std::fabs(largestFloatState.currentState) < std::fabs(floatState.currentState))) {
                                    largestFloatState = floatState;
                                }
                                if (floatState.changedSinceLastSync) {
                                    REQUIRE(floatState.changedSinceLastSync == vectorState.changedSinceLastSync);
                                    REQUIRE(floatState.lastChangeTime == vectorState.lastChangeTime);
                                    ++xyChanges;
                                }
                                if (!vectorState.changedSinceLastSync) {
                                    REQUIRE(floatState.changedSinceLastSync == XR_FALSE);
                                }

                                if (vectorState.changedSinceLastSync) {
                                    REQUIRE(xyChanges > 0);
                                }
                            }
                        }

                        // Check that combined values followed the rules
                        if (combinedBoolCount >= 1) {
                            REQUIRE(largestBoolState.isActive == combinedBoolState.isActive);
                            REQUIRE(largestBoolState.currentState == combinedBoolState.currentState);
                            // Pass boolean combination if at least two states were combined and one was non-zero.
                            if ((combinedBoolCount >= 2) && (largestBoolState.currentState != 0)) {
                                waitForCombinedBools = false;
                            }
                        }
                        if (combinedFloatCount >= 1) {
                            REQUIRE(largestFloatState.isActive == combinedFloatState.isActive);
                            // Float inputs might be equal in magnitude but differently signed, we don't care which one wins,
                            // just that the magnitudes match.
                            REQUIRE(std::fabs(largestFloatState.currentState) == std::fabs(combinedFloatState.currentState));
                            // Pass float combination if at least two states were combined and one was non-zero.
                            if ((combinedFloatCount >= 2) && (largestFloatState.currentState != 0)) {
                                waitForCombinedFloats = false;
                            }
                        }
                        if (combinedVectorCount > 1) {
                            REQUIRE(largestVectorState.isActive == combinedVectorState.isActive);
                            // Vector2f inputs might be equal in magnitude but differently signed, we don't care which one wins,
                            // just that the magnitudes match.
                            REQUIRE(std::fabs(largestVectorState.currentState.x) == std::fabs(combinedVectorState.currentState.x));
                            REQUIRE(std::fabs(largestVectorState.currentState.y) == std::fabs(combinedVectorState.currentState.y));
                            // Pass vector combination if at least two states were combined and one was non-zero.
                            if ((combinedVectorCount >= 2) && (mag(largestVectorState.currentState) != 0)) {
                                waitForCombinedVectors = false;
                            }
                        }

                        // For automation only, drive inputs through a set of legal values with at most cStepSize intervals.
                        {
                            // Use cStepSize / 2 to generate {-1.0, -0.5, 0.0, 0.5, 1.0} for x, y below.
                            synthesizedX += cStepSize / 2.f;
                            if (synthesizedX > 1.f) {
                                synthesizedX = 0.f;
                                synthesizedY += cStepSize / 2.f;
                                if (synthesizedY > 1.f) {
                                    synthesizedY = 0.f;
                                }
                            }
                            for (auto actionInfo : booleanActions) {
                                inputDevice->SetButtonStateBool(StringToPath(compositionHelper.GetInstance(), actionInfo.Data.Path.c_str()),
                                                                synthesizedX > 0.5f, true);
                            }

                            for (auto actionInfo : floatActions) {
                                inputDevice->SetButtonStateFloat(
                                    StringToPath(compositionHelper.GetInstance(), actionInfo.Data.Path.c_str()), synthesizedX, 0, true);
                            }

                            for (auto actionInfo : vectorActions) {
                                float x = (synthesizedX - 0.5f) * 2.f;
                                float y = (synthesizedY - 0.5f) * 2.f;
                                inputDevice->SetButtonStateVector2(
                                    StringToPath(compositionHelper.GetInstance(), actionInfo.Data.Path.c_str()), {x, y}, 0, true);
                            }
                        }

                        bool waitingForCombinations = waitForCombinedBools || waitForCombinedFloats || waitForCombinedVectors;
                        if ((seenActions.size() == actionCount) && !waitingForCombinations)
                            return true;

                        std::string waitForCombined;
                        std::string waitForCombinedPrefix = "\nCombine: ";
                        if (waitForCombinedBools) {
                            waitForCombined += waitForCombinedPrefix + "bool";
                            waitForCombinedPrefix = ",";
                        }
                        if (waitForCombinedFloats) {
                            waitForCombined += waitForCombinedPrefix + "float";
                            waitForCombinedPrefix = ",";
                        }
                        if (waitForCombinedVectors) {
                            waitForCombined += waitForCombinedPrefix + "vec2f";
                            waitForCombinedPrefix = ",";
                        }
                        std::string prompt = "Used " + std::to_string(seenActions.size()) + "/" + std::to_string(actionCount) +
                                             " inputs on:\n" + topLevelPathString + waitForCombined + nextActionPrompt;
                        actionLayerManager.DisplayMessage(prompt);

                        return false;
                    },
                    600s, 10ms);

                REQUIRE(seenActions.size() == actionCount);
                REQUIRE_FALSE(waitForCombinedBools);
                REQUIRE_FALSE(waitForCombinedFloats);
                REQUIRE_FALSE(waitForCombinedVectors);

                actionLayerManager.DisplayMessage("Release all inputs");
                std::this_thread::sleep_for(2s);
            }

            INFO("Pose state query");
            {
                for (const auto& poseActionData : poseActions) {
                    INFO(poseActionData.Data.Path.c_str());

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = poseActionData.Action;

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState), XR_SUCCESS);
                    REQUIRE(poseState.isActive);

                    inputDevice->SetDeviceActive(false);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState), XR_SUCCESS);
                    REQUIRE_FALSE(poseState.isActive);

                    inputDevice->SetDeviceActive(true);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState), XR_SUCCESS);
                    REQUIRE(poseState.isActive);
                }
            }

            INFO("Haptics state query")
            {
                // Need at least one boolean action to confirm haptics
                if (booleanActions.size() > 0) {
                    for (const auto& hapticActionData : hapticActions) {
                        INFO(hapticActionData.Data.Path.c_str());

                        XrPath inputSourcePath = StringToPath(compositionHelper.GetInstance(), booleanActions[0].Data.Path.c_str());

                        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                        hapticActionInfo.action = hapticActionData.Action;

                        XrHapticVibration hapticPacket{XR_TYPE_HAPTIC_VIBRATION};
                        hapticPacket.amplitude = 1;
                        hapticPacket.frequency = XR_FREQUENCY_UNSPECIFIED;
                        hapticPacket.duration = XR_MIN_HAPTIC_DURATION;

                        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};

                        XrAction currentBooleanAction{XR_NULL_HANDLE};
                        auto GetBooleanButtonState = [&]() -> bool {
                            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                            for (const auto& booleanActionData : booleanActions) {
                                getInfo.action = booleanActionData.Action;
                                REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState),
                                               XR_SUCCESS);
                                if (booleanState.changedSinceLastSync && booleanState.currentState) {
                                    currentBooleanAction = booleanActionData.Action;
                                    return true;
                                }
                            }
                            return false;
                        };

                        actionLayerManager.DisplayMessage("Press any button when you feel the 3 second haptic vibration");
                        actionLayerManager.GetRenderLoop().IterateFrame();
                        std::this_thread::sleep_for(3s);

                        hapticPacket.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(3s).count();
                        REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                             reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                       XR_SUCCESS);

                        {
                            // For automation only
                            inputDevice->SetButtonStateBool(inputSourcePath, false, true);
                            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                            inputDevice->SetButtonStateBool(inputSourcePath, true, true);
                        }
                        currentBooleanAction = XR_NULL_HANDLE;
                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                return GetBooleanButtonState();
                            },
                            15s, 100ms);
                        REQUIRE_FALSE(currentBooleanAction == XR_NULL_HANDLE);

                        {
                            // For automation only
                            inputDevice->SetButtonStateBool(inputSourcePath, false, true);
                        }

                        REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_SUCCESS);

                        actionLayerManager.DisplayMessage("Press any button when you feel the short haptic pulse");
                        actionLayerManager.GetRenderLoop().IterateFrame();
                        std::this_thread::sleep_for(3s);

                        hapticPacket.duration = XR_MIN_HAPTIC_DURATION;
                        REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                             reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                       XR_SUCCESS);

                        {
                            inputDevice->SetButtonStateBool(inputSourcePath, false, true);
                            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                            inputDevice->SetButtonStateBool(inputSourcePath, true, true);
                        }
                        currentBooleanAction = XR_NULL_HANDLE;
                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                return GetBooleanButtonState();
                            },
                            15s, 100ms);
                        REQUIRE_FALSE(currentBooleanAction == XR_NULL_HANDLE);

                        {
                            // For automation only
                            inputDevice->SetButtonStateBool(inputSourcePath, false, true);
                        }
                    }

                    actionLayerManager.DisplayMessage("Release all inputs");
                    std::this_thread::sleep_for(2s);
                }
            }

            INFO("Action value coercion");
            {
                INFO("Boolean->Float");
                for (const auto& booleanToFloatActionData : floatActionsCoercedToBoolean) {
                    XrPath inputSourcePath = StringToPath(compositionHelper.GetInstance(), booleanToFloatActionData.Data.Path.c_str());

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = booleanToFloatActionData.Action;

                    inputDevice->SetButtonStateFloat(inputSourcePath, 0.0f, cEpsilon);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE_FALSE(booleanState.currentState);

                    inputDevice->SetButtonStateFloat(inputSourcePath, 1.0f, cEpsilon);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE(booleanState.currentState);
                    REQUIRE(booleanState.lastChangeTime > 0);

                    inputDevice->SetButtonStateFloat(inputSourcePath, 0.0f, cEpsilon);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE_FALSE(booleanState.currentState);
                    REQUIRE(booleanState.lastChangeTime > 0);
                }

                INFO("Float->Boolean");
                for (const auto& floatToBooleanActionData : booleanActionsCoercedToFloat) {
                    XrPath inputSourcePath = StringToPath(compositionHelper.GetInstance(), floatToBooleanActionData.Data.Path.c_str());

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = floatToBooleanActionData.Action;

                    inputDevice->SetButtonStateBool(inputSourcePath, false);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_SUCCESS);
                    REQUIRE(floatState.isActive);
                    REQUIRE(floatState.currentState == Approx(0.0f).margin(cLargeEpsilon));

                    inputDevice->SetButtonStateBool(inputSourcePath, true);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_SUCCESS);
                    REQUIRE(floatState.isActive);
                    REQUIRE(floatState.currentState == Approx(1.0f).margin(cLargeEpsilon));
                    REQUIRE(floatState.lastChangeTime > 0);

                    inputDevice->SetButtonStateBool(inputSourcePath, false);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_SUCCESS);
                    REQUIRE(floatState.isActive);
                    REQUIRE(floatState.currentState == Approx(0.0f).margin(cLargeEpsilon));
                    REQUIRE(floatState.lastChangeTime > 0);
                }

                INFO("All other coercions");
                for (const auto& actionData : allOtherCoercions) {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = actionData.Action;

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    if (actionData.Data.Type == XR_ACTION_TYPE_BOOLEAN_INPUT) {
                        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
                        REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &state), XR_SUCCESS);
                        REQUIRE_FALSE(state.isActive);
                    }
                    else if (actionData.Data.Type == XR_ACTION_TYPE_FLOAT_INPUT) {
                        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
                        REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &state), XR_SUCCESS);
                        REQUIRE_FALSE(state.isActive);
                    }
                    else if (actionData.Data.Type == XR_ACTION_TYPE_VECTOR2F_INPUT) {
                        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
                        REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &state), XR_SUCCESS);
                        REQUIRE_FALSE(state.isActive);
                    }
                    else if (actionData.Data.Type == XR_ACTION_TYPE_POSE_INPUT) {
                        XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};
                        REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &state), XR_SUCCESS);
                        REQUIRE_FALSE(state.isActive);
                    }
                }
            }
        };

        for (const InteractionProfileMetadata& ipMetadata : cInteractionProfileDefinitions) {
            if (IsInteractionProfileEnabled(ipMetadata.InteractionProfileShortname.c_str())) {
                for (const std::string& topLevelPathString : ipMetadata.TopLevelPaths) {
                    ReportF("Testing interaction profile %s for %s", ipMetadata.InteractionProfileShortname.c_str(),
                            topLevelPathString.c_str());
                    TestInteractionProfile(ipMetadata, topLevelPathString);
                }
            }
        }
    }

    TEST_CASE("State query functions and haptics", "[actions]")
    {
        CompositionHelper compositionHelper("Input device state query");

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrPath leftHandPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left");
        XrPath rightHandPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/right");
        XrPath gamepadPath = StringToPath(compositionHelper.GetInstance(), "/user/gamepad");
        XrPath bothHands[] = {leftHandPath, rightHandPath};

        XrAction booleanAction{XR_NULL_HANDLE};
        XrAction floatAction{XR_NULL_HANDLE};
        XrAction vectorAction{XR_NULL_HANDLE};
        XrAction poseAction{XR_NULL_HANDLE};
        XrAction hapticAction{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name bool");
        strcpy(actionCreateInfo.actionName, "test_action_name_bool");
        actionCreateInfo.countSubactionPaths = 2;
        actionCreateInfo.subactionPaths = bothHands;
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &booleanAction), XR_SUCCESS);

        actionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name float");
        strcpy(actionCreateInfo.actionName, "test_action_name_float");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &floatAction), XR_SUCCESS);

        actionCreateInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name vector");
        strcpy(actionCreateInfo.actionName, "test_action_name_vector");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &vectorAction), XR_SUCCESS);

        actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name pose");
        strcpy(actionCreateInfo.actionName, "test_action_name_pose");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &poseAction), XR_SUCCESS);

        actionCreateInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name haptic");
        strcpy(actionCreateInfo.actionName, "test_action_name_haptic");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &hapticAction), XR_SUCCESS);

        XrAction confirmAction{XR_NULL_HANDLE};
        XrAction denyAction{XR_NULL_HANDLE};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name confirm");
        strcpy(actionCreateInfo.actionName, "test_action_name_confirm");
        actionCreateInfo.countSubactionPaths = 0;
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &confirmAction), XR_SUCCESS);

        strcpy(actionCreateInfo.localizedActionName, "test action localized name deny");
        strcpy(actionCreateInfo.actionName, "test_action_name_deny");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &denyAction), XR_SUCCESS);

        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);
        actionLayerManager.WaitForSessionFocusWithMessage();

        XrPath simpleControllerInteractionProfile =
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str());

        XrPath leftHandSelectClickPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click");
        XrPath rightHandSelectClickPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click");
        XrPath leftHandMenuClickPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click");
        XrPath rightHandMenuClickPath = StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click");

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(simpleControllerInteractionProfile,
                                                                    {{confirmAction, leftHandSelectClickPath},
                                                                     {confirmAction, rightHandSelectClickPath},
                                                                     {denyAction, leftHandMenuClickPath},
                                                                     {denyAction, rightHandMenuClickPath}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        XrActionStateBoolean booleanState{XR_TYPE_ACTION_STATE_BOOLEAN};
        XrActionStateFloat floatState{XR_TYPE_ACTION_STATE_FLOAT};
        XrActionStateVector2f vectorState{XR_TYPE_ACTION_STATE_VECTOR2F};
        XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};

        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
        hapticActionInfo.action = hapticAction;

        XrHapticVibration hapticPacket{XR_TYPE_HAPTIC_VIBRATION};
        hapticPacket.amplitude = 1;
        hapticPacket.frequency = XR_FREQUENCY_UNSPECIFIED;
        hapticPacket.duration = XR_MIN_HAPTIC_DURATION;

        SECTION("State query functions")
        {
            SECTION("Parameter validation")
            {
                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                SECTION("Basic usage")
                {
                    getInfo.action = booleanAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState), XR_SUCCESS);

                    getInfo.action = floatAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_SUCCESS);

                    getInfo.action = vectorAction;
                    REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState), XR_SUCCESS);

                    getInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState), XR_SUCCESS);
                }
                OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
                {
                    SECTION("Invalid session")
                    {
                        XrSession invalidSession = (XrSession)0x1234;

                        getInfo.action = booleanAction;
                        REQUIRE_RESULT(xrGetActionStateBoolean(invalidSession, &getInfo, &booleanState), XR_ERROR_HANDLE_INVALID);

                        getInfo.action = floatAction;
                        REQUIRE_RESULT(xrGetActionStateFloat(invalidSession, &getInfo, &floatState), XR_ERROR_HANDLE_INVALID);

                        getInfo.action = vectorAction;
                        REQUIRE_RESULT(xrGetActionStateVector2f(invalidSession, &getInfo, &vectorState), XR_ERROR_HANDLE_INVALID);

                        getInfo.action = poseAction;
                        REQUIRE_RESULT(xrGetActionStatePose(invalidSession, &getInfo, &poseState), XR_ERROR_HANDLE_INVALID);

                        REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                             reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                       XR_SUCCESS);
                        REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_SUCCESS);
                    }
                    SECTION("Invalid action")
                    {
                        getInfo.action = (XrAction)0x1234;
                        REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState),
                                       XR_ERROR_HANDLE_INVALID);
                        REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState),
                                       XR_ERROR_HANDLE_INVALID);
                        REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState),
                                       XR_ERROR_HANDLE_INVALID);
                        REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState), XR_ERROR_HANDLE_INVALID);

                        hapticActionInfo.action = getInfo.action;
                        REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                             reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                       XR_ERROR_HANDLE_INVALID);
                        REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_ERROR_HANDLE_INVALID);
                    }
                }
                SECTION("Invalid subaction path")
                {
                    getInfo.subactionPath = (XrPath)0x1234;
                    getInfo.action = booleanAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState), XR_ERROR_PATH_INVALID);

                    getInfo.action = floatAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_ERROR_PATH_INVALID);

                    getInfo.action = vectorAction;
                    REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState), XR_ERROR_PATH_INVALID);

                    getInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState), XR_ERROR_PATH_INVALID);

                    hapticActionInfo.subactionPath = getInfo.subactionPath;
                    REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                         reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_PATH_INVALID);
                    REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_ERROR_PATH_INVALID);
                }
                SECTION("Unspecified subaction path")
                {
                    getInfo.subactionPath = gamepadPath;
                    getInfo.action = booleanAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState),
                                   XR_ERROR_PATH_UNSUPPORTED);

                    getInfo.action = floatAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState), XR_ERROR_PATH_UNSUPPORTED);

                    getInfo.action = vectorAction;
                    REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState),
                                   XR_ERROR_PATH_UNSUPPORTED);

                    getInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState), XR_ERROR_PATH_UNSUPPORTED);

                    hapticActionInfo.subactionPath = getInfo.subactionPath;
                    REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                         reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_PATH_UNSUPPORTED);
                    REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_ERROR_PATH_UNSUPPORTED);
                }
                SECTION("Type mismatch")
                {
                    getInfo.action = booleanAction;
                    hapticActionInfo.action = booleanAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                         reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_ERROR_ACTION_TYPE_MISMATCH);

                    getInfo.action = floatAction;
                    hapticActionInfo.action = floatAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                         reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_ERROR_ACTION_TYPE_MISMATCH);

                    getInfo.action = vectorAction;
                    hapticActionInfo.action = vectorAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(compositionHelper.GetSession(), &getInfo, &booleanState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                         reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_ERROR_ACTION_TYPE_MISMATCH);

                    getInfo.action = poseAction;
                    hapticActionInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrApplyHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo,
                                                         reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrStopHapticFeedback(compositionHelper.GetSession(), &hapticActionInfo), XR_ERROR_ACTION_TYPE_MISMATCH);

                    getInfo.action = hapticAction;
                    hapticActionInfo.action = hapticAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateFloat(compositionHelper.GetSession(), &getInfo, &floatState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateVector2f(compositionHelper.GetSession(), &getInfo, &vectorState),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                }
            }
        }
    }

    TEST_CASE("Action spaces", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("Action Spaces");
        compositionHelper.BeginSession();
        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile =
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str());
        XrPath leftHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/left")};
        XrPath rightHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};
        const XrPath bothHands[2] = {leftHandPath, rightHandPath};

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction poseAction{XR_NULL_HANDLE};
        XrActionCreateInfo createInfo{XR_TYPE_ACTION_CREATE_INFO};
        createInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        strcpy(createInfo.actionName, "test_action_name");
        strcpy(createInfo.localizedActionName, "test localized name");
        createInfo.countSubactionPaths = 2;
        createInfo.subactionPaths = bothHands;
        REQUIRE_RESULT(xrCreateAction(actionSet, &createInfo, &poseAction), XR_SUCCESS);

        std::shared_ptr<IInputTestDevice> leftHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
                             compositionHelper.GetSession(), simpleControllerInteractionProfile, leftHandPath,
                             cSimpleKHRInteractionProfileDefinition.WhitelistData);

        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
                             compositionHelper.GetSession(), simpleControllerInteractionProfile, rightHandPath,
                             cSimpleKHRInteractionProfileDefinition.WhitelistData);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(
            simpleControllerInteractionProfile,
            {{poseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/grip/pose")},
             {poseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/grip/pose")}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        actionLayerManager.WaitForSessionFocusWithMessage();

        XrActiveActionSet leftHandActiveSet{actionSet, leftHandPath};
        XrActiveActionSet rightHandActiveSet{actionSet, rightHandPath};
        XrActiveActionSet bothSets[] = {leftHandActiveSet, rightHandActiveSet};

        XrSpace localSpace{XR_NULL_HANDLE};
        XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        createSpaceInfo.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};
        REQUIRE_RESULT(xrCreateReferenceSpace(compositionHelper.GetSession(), &createSpaceInfo, &localSpace), XR_SUCCESS);

        XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        spaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, 0}};
        spaceCreateInfo.action = poseAction;

        XrSpace actionSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &actionSpace), XR_SUCCESS);

        spaceCreateInfo.subactionPath = leftHandPath;
        XrSpace leftSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &leftSpace), XR_SUCCESS);

        spaceCreateInfo.subactionPath = rightHandPath;
        XrSpace rightSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &rightSpace), XR_SUCCESS);

        leftHandInputDevice->SetDeviceActive(true);
        rightHandInputDevice->SetDeviceActive(true);

        auto WaitForLocatability = [&](const std::string& hand, XrSpace space, XrSpaceLocation* location, bool expectLocatability) {
            bool messageShown = false;
            bool success = WaitUntilPredicateWithTimeout(
                [&]() {
                    actionLayerManager.GetRenderLoop().IterateFrame();
                    REQUIRE_RESULT(
                        xrLocateSpace(space, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), location),
                        XR_SUCCESS);

                    constexpr XrSpaceLocationFlags LocatableFlags =
                        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
                    const bool isLocatable = (location->locationFlags & LocatableFlags) == LocatableFlags;
                    const bool isExpected = expectLocatability == isLocatable;
                    if (!isExpected) {
                        actionLayerManager.DisplayMessage("Waiting for " + hand + " controller to " +
                                                          (expectLocatability ? "gain" : "lose") + " tracking...");
                        messageShown = true;
                    }
                    return isExpected;
                },
                15s, 50ms);

            if (messageShown) {
                actionLayerManager.DisplayMessage("");
            }

            return success;
        };

        {
            XrSpaceVelocity leftVelocity{XR_TYPE_SPACE_VELOCITY};
            XrSpaceVelocity rightVelocity{XR_TYPE_SPACE_VELOCITY};
            XrSpaceLocation leftRelation{XR_TYPE_SPACE_LOCATION, &leftVelocity};
            XrSpaceLocation rightRelation{XR_TYPE_SPACE_LOCATION, &rightVelocity};

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.countActiveActionSets = 2;
            syncInfo.activeActionSets = bothSets;

            leftHandInputDevice->SetDeviceActive(false);
            actionLayerManager.DisplayMessage("Place left controller somewhere static but trackable");
            std::this_thread::sleep_for(5s);
            leftHandInputDevice->SetDeviceActive(true);
            REQUIRE(WaitForLocatability("left", leftSpace, &leftRelation, false));
            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
            REQUIRE(WaitForLocatability("left", leftSpace, &leftRelation, true));

            rightHandInputDevice->SetDeviceActive(false);
            actionLayerManager.DisplayMessage(
                "Place right controller somewhere static but trackable. Keep left controller on and trackable.");
            std::this_thread::sleep_for(5s);
            rightHandInputDevice->SetDeviceActive(true);
            REQUIRE(WaitForLocatability("right", rightSpace, &rightRelation, false));
            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
            REQUIRE(WaitForLocatability("right", rightSpace, &rightRelation, true));

            rightHandInputDevice->SetDeviceActive(false);

            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
            REQUIRE(WaitForLocatability("left", leftSpace, &leftRelation, true));
            REQUIRE(WaitForLocatability("right", rightSpace, &rightRelation, false));

            auto PosesAreEqual = [](XrPosef a, XrPosef b) -> bool {
                constexpr float e = 0.001f;  // 1mm
                return (a.position.x == Approx(b.position.x).epsilon(e)) && (a.position.y == Approx(b.position.y).epsilon(e)) &&
                       (a.position.z == Approx(b.position.z).epsilon(e)) && (a.orientation.x == Approx(b.orientation.x).epsilon(e)) &&
                       (a.orientation.y == Approx(b.orientation.y).epsilon(e)) && (a.orientation.z == Approx(b.orientation.z).epsilon(e)) &&
                       (a.orientation.w == Approx(b.orientation.w).epsilon(e));
            };

            XrSpaceVelocity currentVelocity{XR_TYPE_SPACE_VELOCITY};
            XrSpaceLocation currentRelation{XR_TYPE_SPACE_LOCATION, &currentVelocity};
            XrTime locateTime =
                actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime();  // Ensure using the same time for the pose checks.
            REQUIRE_RESULT(xrLocateSpace(actionSpace, localSpace, locateTime, &currentRelation), XR_SUCCESS);
            REQUIRE_RESULT(xrLocateSpace(leftSpace, localSpace, locateTime, &leftRelation), XR_SUCCESS);
            REQUIRE_RESULT(xrLocateSpace(rightSpace, localSpace, locateTime, &rightRelation), XR_SUCCESS);
            REQUIRE(currentRelation.locationFlags != 0);
            REQUIRE(leftRelation.locationFlags != 0);
            REQUIRE(PosesAreEqual(currentRelation.pose, leftRelation.pose));
            REQUIRE_FALSE(PosesAreEqual(leftRelation.pose, rightRelation.pose));
            REQUIRE(0 != currentRelation.locationFlags);

            rightHandInputDevice->SetDeviceActive(true);
            leftHandInputDevice->SetDeviceActive(false);

            INFO("Left is off but we're still tracking it");
            REQUIRE(WaitForLocatability("left", leftSpace, &leftRelation, false));
            REQUIRE_RESULT(
                xrLocateSpace(actionSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 == currentRelation.locationFlags);

            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

            INFO("We are still tracking left as action spaces pick one device and stick with it");
            REQUIRE_RESULT(
                xrLocateSpace(actionSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 == currentRelation.locationFlags);

            leftHandInputDevice->SetDeviceActive(false);
            rightHandInputDevice->SetDeviceActive(false);

            INFO("We are still tracking left, but it's off");
            REQUIRE_RESULT(
                xrLocateSpace(actionSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 == currentRelation.locationFlags);

            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

            INFO("We are still tracking left, but they're both off");
            REQUIRE_RESULT(
                xrLocateSpace(actionSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 == currentRelation.locationFlags);

            leftHandInputDevice->SetDeviceActive(true);
            rightHandInputDevice->SetDeviceActive(true);

            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

            REQUIRE(WaitForLocatability("left", leftSpace, &leftRelation, true));
            REQUIRE(WaitForLocatability("right", rightSpace, &rightRelation, true));

            REQUIRE_RESULT(
                xrLocateSpace(actionSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 != currentRelation.locationFlags);

            INFO("The action space should remain locatable despite destruction of the action");
            REQUIRE_RESULT(xrDestroyAction(poseAction), XR_SUCCESS);

            REQUIRE_RESULT(
                xrLocateSpace(actionSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 != currentRelation.locationFlags);
            REQUIRE_RESULT(
                xrLocateSpace(leftSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 != currentRelation.locationFlags);
            REQUIRE_RESULT(
                xrLocateSpace(rightSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 != currentRelation.locationFlags);

            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

            XrActionStatePose poseActionState{XR_TYPE_ACTION_STATE_POSE};
            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                getInfo.action = poseAction;
                REQUIRE_RESULT(xrGetActionStatePose(compositionHelper.GetSession(), &getInfo, &poseActionState), XR_ERROR_HANDLE_INVALID);
            }

            REQUIRE_RESULT(
                xrLocateSpace(actionSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 != currentRelation.locationFlags);
            REQUIRE_RESULT(
                xrLocateSpace(leftSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 != currentRelation.locationFlags);
            REQUIRE_RESULT(
                xrLocateSpace(rightSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                XR_SUCCESS);
            REQUIRE(0 != currentRelation.locationFlags);
        }
    }

    TEST_CASE("xrEnumerateBoundSourcesForAction and xrGetInputSourceLocalizedName", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("BoundSources and LocalizedName");

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction action{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name bool");
        strcpy(actionCreateInfo.actionName, "test_action_name_bool");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);
        actionLayerManager.WaitForSessionFocusWithMessage();

        XrPath leftHandPath{StringToPath(compositionHelper.GetInstance(), "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), compositionHelper.GetInstance(),
            compositionHelper.GetSession(),
            StringToPath(compositionHelper.GetInstance(), cSimpleKHRInteractionProfileDefinition.InteractionProfilePathString.c_str()),
            leftHandPath, cSimpleKHRInteractionProfileDefinition.WhitelistData);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(
            StringToPath(compositionHelper.GetInstance(), "/interaction_profiles/khr/simple_controller"),
            {{action, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
             {action, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        leftHandInputDevice->SetDeviceActive(true);

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        XrActiveActionSet activeActionSet{actionSet};
        syncInfo.activeActionSets = &activeActionSet;
        syncInfo.countActiveActionSets = 1;

        SECTION("Parameter validation")
        {
            XrBoundSourcesForActionEnumerateInfo info{XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
            info.action = action;
            SECTION("Basic usage")
            {
                std::vector<XrPath> enumerateResult =
                    REQUIRE_TWO_CALL(XrPath, {}, xrEnumerateBoundSourcesForAction, compositionHelper.GetSession(), &info);

                // Note that runtimes may return bound sources even when not focused, though they don't have to

                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                enumerateResult = REQUIRE_TWO_CALL(XrPath, {}, xrEnumerateBoundSourcesForAction, compositionHelper.GetSession(), &info);

                REQUIRE(enumerateResult.size() > 0);

                XrInputSourceLocalizedNameGetInfo getInfo{XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
                getInfo.sourcePath = enumerateResult[0];
                SECTION("xrGetInputSourceLocalizedName")
                {
                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT;
                    std::string localizedStringResult =
                        REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, compositionHelper.GetSession(), &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT;
                    localizedStringResult =
                        REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, compositionHelper.GetSession(), &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    localizedStringResult =
                        REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, compositionHelper.GetSession(), &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents =
                        XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT;
                    localizedStringResult =
                        REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, compositionHelper.GetSession(), &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    localizedStringResult =
                        REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, compositionHelper.GetSession(), &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents =
                        XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    localizedStringResult =
                        REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, compositionHelper.GetSession(), &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                              XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                              XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    localizedStringResult =
                        REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, compositionHelper.GetSession(), &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    uint32_t sourceCountOutput;
                    char buffer;
                    SECTION("Invalid components")
                    {
                        getInfo.whichComponents = 0;
                        REQUIRE_RESULT(
                            xrGetInputSourceLocalizedName(compositionHelper.GetSession(), &getInfo, 0, &sourceCountOutput, &buffer),
                            XR_ERROR_VALIDATION_FAILURE);
                    }
                    SECTION("Invalid path")
                    {
                        getInfo.sourcePath = XR_NULL_PATH;
                        REQUIRE_RESULT(
                            xrGetInputSourceLocalizedName(compositionHelper.GetSession(), &getInfo, 0, &sourceCountOutput, &buffer),
                            XR_ERROR_PATH_INVALID);
                        getInfo.sourcePath = (XrPath)0x1234;
                        REQUIRE_RESULT(
                            xrGetInputSourceLocalizedName(compositionHelper.GetSession(), &getInfo, 0, &sourceCountOutput, &buffer),
                            XR_ERROR_PATH_INVALID);
                    }
                }
            }
            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                SECTION("Invalid session")
                {
                    XrSession invalidSession = (XrSession)0x1234;
                    uint32_t sourceCountOutput;
                    XrPath buffer;
                    REQUIRE_RESULT(xrEnumerateBoundSourcesForAction(invalidSession, &info, 0, &sourceCountOutput, &buffer),
                                   XR_ERROR_HANDLE_INVALID);
                }
                SECTION("Invalid action")
                {
                    info.action = (XrAction)0x1234;
                    uint32_t sourceCountOutput;
                    XrPath buffer;
                    REQUIRE_RESULT(xrEnumerateBoundSourcesForAction(compositionHelper.GetSession(), &info, 0, &sourceCountOutput, &buffer),
                                   XR_ERROR_HANDLE_INVALID);
                }
            }
        }
    }
}  // namespace Conformance
