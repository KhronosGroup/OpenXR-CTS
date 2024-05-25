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

#include "action_utils.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "input_testinputdevice.h"
#include "interaction_info.h"
#include "report.h"
#include "two_call.h"
#include "utilities/feature_availability.h"
#include "utilities/bitmask_to_string.h"
#include "utilities/event_reader.h"
#include "utilities/types_and_constants.h"
#include "utilities/string_utils.h"

#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <ostream>
#include <ratio>
#include <regex>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using namespace Conformance;

// Stores the top level path in slot 2 and the identifier path in slot 5 or 6 based on whether or not the component was included.
// If the component was included, 6 and 7 will be matched with the parent and component, otherwise 5 will be matched.
const std::regex cInteractionSourcePathRegex("^((.+)/(input|output))/(([^/]+)|([^/]+)/([^/]+))$");

#define OPTIONAL_ACTIVE_ACTION_SET_PRIORITY_SECTION                                                       \
    if (GetGlobalData().IsInstanceExtensionSupported(XR_EXT_ACTIVE_ACTION_SET_PRIORITY_EXTENSION_NAME) && \
        GetGlobalData().leftHandUnderTest && GetGlobalData().rightHandUnderTest)                          \
    SECTION("XR_EXT_active_action_set_priority")

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
                XrPath subactionPaths[2] = {StringToPath(instance, "/user/head"), StringToPath(instance, "/user/head")};
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

    struct InteractionAvailabilityEval
    {
        template <typename F>
        explicit InteractionAvailabilityEval(F&& getFeatures)
        {
            const FeatureSet features = getFeatures();
            for (uint32_t i = 0; i < satisfied.size(); ++i) {
                satisfied[i] = kInteractionAvailabilities[i].IsSatisfiedBy(features);
            }
        }
        // cannot copy, cannot move
        InteractionAvailabilityEval(InteractionAvailabilityEval&&) = delete;
        InteractionAvailabilityEval(const InteractionAvailabilityEval&) = delete;

        std::array<bool, kInteractionAvailabilities.size()> satisfied{};
    };

    static bool SatisfiedByDefault(InteractionProfileAvailability a)
    {
        static InteractionAvailabilityEval eval([] {
            FeatureSet features;
            GetGlobalData().PopulateVersionAndEnabledExtensions(features);
            return features;
        });
        return eval.satisfied[(size_t)a];
    }

    // static bool PossibleToSatisfy(InteractionProfileAvailability a)
    // {
    //     static InteractionAvailabilityEval eval([] {
    //         FeatureSet features;
    //         GetGlobalData().PopulateVersionAndAvailableExtensions(features);
    //         return features;
    //     });
    //     return eval.satisfied[(size_t)a];
    // }

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
        SECTION("Path formats and known profiles")
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
                for (const auto& invalidIP : invalidInteractionProfiles) {
                    bindings.interactionProfile = StringToPath(instance, invalidIP);
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_PATH_UNSUPPORTED);
                }

                bindings.interactionProfile = StringToPath(instance, "/interaction_profiles/khr/another_controller");
                REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_PATH_UNSUPPORTED);
            }
            SECTION("Unknown binding path")
            {
                for (const auto& invalidBindingPath : invalidBindingPaths) {
                    XrActionSuggestedBinding invalidBindingPathBinding = {action, StringToPath(instance, invalidBindingPath)};
                    bindings.suggestedBindings = &invalidBindingPathBinding;
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_PATH_UNSUPPORTED);
                }
            }
            SECTION("Supports all specified interaction profiles")
            {
                for (const auto& ipMetadata : GetAllInteractionProfiles()) {
                    XrAction boolAction;
                    XrAction floatAction;
                    XrAction vectorAction;
                    XrAction poseAction;
                    XrAction hapticAction;

                    if (!SatisfiedByDefault(ipMetadata.Availability)) {
                        continue;
                    }

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

                    CAPTURE(ipMetadata.InteractionProfilePathString);
                    bindings.interactionProfile = StringToPath(instance, ipMetadata.InteractionProfilePathString);
                    bindings.countSuggestedBindings = 1;
                    for (const auto& inputSourcePathData : ipMetadata.InputSourcePaths) {
                        CAPTURE(inputSourcePathData.Path);
                        CAPTURE(inputSourcePathData.Type);

                        if (!SatisfiedByDefault(inputSourcePathData.Availability)) {
                            continue;
                        }

                        XrAction selectedAction;
                        switch (inputSourcePathData.Type) {
                        case XR_ACTION_TYPE_BOOLEAN_INPUT:
                            selectedAction = boolAction;
                            break;
                        case XR_ACTION_TYPE_FLOAT_INPUT:
                            selectedAction = floatAction;
                            break;
                        case XR_ACTION_TYPE_VECTOR2F_INPUT:
                            selectedAction = vectorAction;
                            break;
                        case XR_ACTION_TYPE_VIBRATION_OUTPUT:
                            selectedAction = poseAction;
                            break;
                        default:
                            selectedAction = hapticAction;
                        }

                        XrActionSuggestedBinding suggestedBindings{selectedAction, StringToPath(instance, inputSourcePathData.Path)};
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

    TEST_CASE("xrSuggestInteractionProfileBindings_avail")
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

        XrInteractionProfileSuggestedBinding bindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        bindings.countSuggestedBindings = 1;
        XrActionSuggestedBinding suggestedBindings{};
        XrAction boolAction;
        XrAction floatAction;
        XrAction vectorAction;
        XrAction poseAction;
        XrAction hapticAction;

        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test bool action localized name");
        strcpy(actionCreateInfo.actionName, "test_bool_action_name");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &boolAction), XR_SUCCESS);

        actionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test float action localized name");
        strcpy(actionCreateInfo.actionName, "test_float_action_name");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &floatAction), XR_SUCCESS);

        actionCreateInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test vector action localized name");
        strcpy(actionCreateInfo.actionName, "test_vector_action_name");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &vectorAction), XR_SUCCESS);

        actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test pose action localized name");
        strcpy(actionCreateInfo.actionName, "test_pose_action_name");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &poseAction), XR_SUCCESS);

        actionCreateInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
        strcpy(actionCreateInfo.localizedActionName, "test haptic action localized name");
        strcpy(actionCreateInfo.actionName, "test_haptic_action_name");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &hapticAction), XR_SUCCESS);

        auto setupBinding = [&](const InputSourcePathAvailData& pathData) {
            CAPTURE(pathData.Path);
            CAPTURE(pathData.Type);

            XrAction selectedAction;
            switch (pathData.Type) {
            case XR_ACTION_TYPE_BOOLEAN_INPUT:
                selectedAction = boolAction;
                break;
            case XR_ACTION_TYPE_FLOAT_INPUT:
                selectedAction = floatAction;
                break;
            case XR_ACTION_TYPE_VECTOR2F_INPUT:
                selectedAction = vectorAction;
                break;
            case XR_ACTION_TYPE_VIBRATION_OUTPUT:
                selectedAction = poseAction;
                break;
            default:
                selectedAction = hapticAction;
            }

            suggestedBindings = XrActionSuggestedBinding{selectedAction, StringToPath(instance, pathData.Path)};
            bindings.suggestedBindings = &suggestedBindings;
            CAPTURE(kInteractionAvailabilities[(size_t)pathData.Availability]);
            if (SatisfiedByDefault(pathData.Availability)) {
                CHECK(xrSuggestInteractionProfileBindings(instance, &bindings) == XR_SUCCESS);
            }
            else {
                CHECK(xrSuggestInteractionProfileBindings(instance, &bindings) == XR_ERROR_PATH_UNSUPPORTED);
            }
        };
        FeatureSet features;
        GetGlobalData().PopulateVersionAndEnabledExtensions(features);
        CAPTURE(features);
        for (const InteractionProfileAvailMetadata& ipMetadata : GetAllInteractionProfiles()) {
            CAPTURE(ipMetadata.InteractionProfilePathString);
            bindings.interactionProfile = StringToPath(instance, ipMetadata.InteractionProfilePathString);
            bindings.countSuggestedBindings = 1;
            if (SatisfiedByDefault(ipMetadata.Availability)) {
                DYNAMIC_SECTION(ipMetadata.InteractionProfileShortname << " Expect Available")
                {
                    for (const auto& inputSourcePathData : ipMetadata.InputSourcePaths) {
                        setupBinding(inputSourcePathData);
                    }
                }
            }
            else {
                // Not available by default
                DYNAMIC_SECTION(ipMetadata.InteractionProfileShortname << " Expect Unavailable")
                {
                    for (const auto& inputSourcePathData : ipMetadata.InputSourcePaths) {
                        setupBinding(inputSourcePathData);
                    }
                }
            }
        }
        xrDestroyActionSet(actionSet);
    }

    TEST_CASE("xrSuggestInteractionProfileBindings_interactive", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("xrSuggestInteractionProfileBindings");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();
        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

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

        bool leftUnderTest = GetGlobalData().leftHandUnderTest;
        const char* pathStr = leftUnderTest ? "/user/hand/left" : "/user/hand/right";

        XrPath path{StringToPath(instance, pathStr)};
        std::shared_ptr<IInputTestDevice> inputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), path,
                             GetSimpleInteractionProfile().InputSourcePaths);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);

        std::string selectPathStr = std::string(pathStr) + "/input/select/click";
        XrPath selectPath = StringToPath(instance, selectPathStr);
        XrActionSuggestedBinding testBinding = {selectActionA, selectPath};
        XrInteractionProfileSuggestedBinding bindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        bindings.interactionProfile = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
        bindings.countSuggestedBindings = 1;
        bindings.suggestedBindings = &testBinding;
        REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_SUCCESS);

        // Calling attach on the interaction manager will call xrSuggestInteractionProfileBindings with the bindings provided here, overwriting the previous bindings
        compositionHelper.GetInteractionManager().AddActionBindings(
            StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), {{{selectActionB, selectPath}}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        actionLayerManager.WaitForSessionFocusWithMessage();

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        XrActiveActionSet activeActionSet{actionSet};
        syncInfo.activeActionSets = &activeActionSet;
        syncInfo.countActiveActionSets = 1;

        SECTION("Old bindings discarded")
        {
            inputDevice->SetDeviceActive(true);
            inputDevice->SetButtonStateBool(selectPath, true);

            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

            // selectActionA should have had its bindings discarded and replaced by selectActionB's bindings
            getInfo.action = selectActionA;
            REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanActionState), XR_SUCCESS);
            REQUIRE_FALSE(booleanActionState.isActive);

            getInfo.action = selectActionB;
            REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanActionState), XR_SUCCESS);
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

                // We cannot be in the FOCUSED state here as we are not submitting frames for
                // this session.
                // The spec for `xrApplyHapticFeedback` and `xrStopHapticFeedback` requires that
                // the runtime must: return `XR_SESSION_NOT_FOCUSED` in the case that the session
                // is not focused. However, for the initial version of OpenXR 1.0, this test
                // required that the functions return `XR_SUCCESS` and we only added this change
                // to the spec later (see issue 1270), so it is not really right to enforce this
                // return code; but we can warn runtimes here instead.
                // TODO enforce for 1.1?
                XrResult applyResult =
                    xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket));
                REQUIRE_RESULT_SUCCEEDED(applyResult);
                if (applyResult != XR_SESSION_NOT_FOCUSED) {
                    WARN(
                        "Runtime should prefer XR_SESSION_NOT_FOCUSED over XR_SUCCESS when calling xrApplyHapticFeedback when the session is not focused.");
                }

                XrResult stopResult = xrStopHapticFeedback(session, &hapticActionInfo);
                REQUIRE_RESULT_SUCCEEDED(stopResult);
                if (applyResult != XR_SESSION_NOT_FOCUSED) {
                    WARN(
                        "Runtime should prefer XR_SESSION_NOT_FOCUSED over XR_SUCCESS when calling xrStopHapticFeedback when the session is not focused.");
                }
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

            xrDestroyActionSet(actionSet2);
        }
    }
    TEST_CASE("xrSuggestInteractionProfileBindings_order", "[actions][interactive]")
    {
        auto suggestBindingsAndGetCurrentInteractionProfile = [](bool reverse, bool nullPathExpected,
                                                                 const std::string& topLevelPathString) {
            CompositionHelper compositionHelper("xrSuggestInteractionProfileBindings_order");
            XrInstance instance = compositionHelper.GetInstance();
            XrSession session = compositionHelper.GetSession();
            compositionHelper.BeginSession();

            ActionLayerManager actionLayerManager(compositionHelper);

            XrActionSet actionSet{XR_NULL_HANDLE};
            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
            strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

            XrAction boolAction{XR_NULL_HANDLE};
            XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionCreateInfo.localizedActionName, "test action localized name");
            strcpy(actionCreateInfo.actionName, "test_action_name");
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &boolAction), XR_SUCCESS);

            auto& interactionManager = compositionHelper.GetInteractionManager();
            interactionManager.AddActionSet(actionSet);

            // Keep track of the order this test expects, just used to assert that nothing gets reordered
            std::vector<XrPath> interactionProfileOrder{};
            auto suggestBindings = [&](const InteractionProfileAvailMetadata& interactionProfile) {
                if (!SatisfiedByDefault(interactionProfile.Availability)) {
                    return;
                }
                std::string interactionProfileName = interactionProfile.InteractionProfilePathString;
                XrPath interactionProfilePath = StringToPath(instance, interactionProfileName);

                bool bindingSuggested = false;
                for (auto& bindings : interactionProfile.InputSourcePaths) {
                    // We use the same pattern as the rest of the action conformance suite
                    // and only bind the boolean actions. Note that we bind "boolAction"
                    // to *every* boolean input, not just the first one.
                    if (bindings.Type != XR_ACTION_TYPE_BOOLEAN_INPUT) {
                        continue;
                    }
                    if (!SatisfiedByDefault(bindings.Availability)) {
                        continue;
                    }
                    XrActionSuggestedBinding binding = {boolAction, StringToPath(instance, bindings.Path)};
                    interactionManager.AddActionBindings(interactionProfilePath, {binding});
                    bindingSuggested = true;
                }
                if (bindingSuggested) {
                    // Keep track of the ordering to verify later
                    interactionProfileOrder.push_back(interactionProfilePath);
                }
            };

            auto interactionProfiles = GetAllInteractionProfiles();
            if (reverse) {
                for (auto interactionProfileReverse = std::rbegin(interactionProfiles);
                     interactionProfileReverse != std::rend(interactionProfiles); interactionProfileReverse++) {
                    suggestBindings(*interactionProfileReverse);
                }
            }
            else {
                for (const InteractionProfileAvailMetadata& interactionProfile : interactionProfiles) {
                    suggestBindings(interactionProfile);
                }
            }

            // Hardcoded path valid for simple controller
            XrPath userHandLeftXrPath{StringToPath(instance, topLevelPathString)};
            std::shared_ptr<IInputTestDevice> inputDevice =
                CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                                 StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), userHandLeftXrPath,
                                 GetSimpleInteractionProfile().InputSourcePaths);

            // This function calls xrSuggestInteractionProfileBindings() before attaching the actionsets
            interactionManager.AttachActionSets(&interactionProfileOrder);

            // boolAction is used to detect when the device becomes active
            inputDevice->SetDeviceActive(/*state = */ true, /*skipInteraction = */ false, boolAction, actionSet);
            actionLayerManager.WaitForSessionFocusWithMessage();
            XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
            REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, StringToPath(instance, topLevelPathString), &interactionProfileState),
                           XR_SUCCESS);

            // Are we expecting the topLevelPath to have a active input?
            if (nullPathExpected) {
                if (interactionProfileState.interactionProfile != XR_NULL_PATH) {
                    WARN("Got non-null interactionProfile on path not being tested");
                }

                // Not a valid path, but okay to return since we only compare.
                return std::string("<XR_NULL_PATH>");
            }
            else {
                REQUIRE(interactionProfileState.interactionProfile != XR_NULL_PATH);

                // XrPaths are only valid for the lifetime of the instance so we return a string
                return PathToString(instance, interactionProfileState.interactionProfile);
            }
        };

        GlobalData& globalData = GetGlobalData();
        std::vector<std::tuple<std::string, bool>> list = {
            {"/user/hand/left", !globalData.leftHandUnderTest},
            {"/user/hand/right", !globalData.rightHandUnderTest},
        };

        for (const auto& entry : list) {
            std::string topLevelPath;
            bool nullPathExpected;
            std::tie(topLevelPath, nullPathExpected) = entry;

            CAPTURE(topLevelPath);
            CAPTURE(nullPathExpected);

            auto forwardPath = suggestBindingsAndGetCurrentInteractionProfile(/*reverse = */ false, nullPathExpected, topLevelPath);
            auto reversePath = suggestBindingsAndGetCurrentInteractionProfile(/*reverse =  */ true, nullPathExpected, topLevelPath);
            REQUIRE(forwardPath == reversePath);
        }
    }

    TEST_CASE("xrGetCurrentInteractionProfile", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("xrGetCurrentInteractionProfile");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();
        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile = StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString);

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction selectAction{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test select action");
        strcpy(actionCreateInfo.actionName, "test_select_action");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &selectAction), XR_SUCCESS);

        XrPath leftHandPath{StringToPath(instance, "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), leftHandPath,
                             GetSimpleInteractionProfile().InputSourcePaths);

        XrPath rightHandPath{StringToPath(instance, "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), rightHandPath,
                             GetSimpleInteractionProfile().InputSourcePaths);

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
                StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString),
                {{{selectAction, StringToPath(instance, "/user/hand/left/input/select/click")},
                  {selectAction, StringToPath(instance, "/user/hand/right/input/select/click")}}});
            compositionHelper.GetInteractionManager().AttachActionSets();

            {
                INFO("Parameter validation");

                {
                    INFO("Basic usage");
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, leftHandPath, &interactionProfileState), XR_SUCCESS);
                }
                {
                    INFO("XR_NULL_PATH topLevelPath");
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, XR_NULL_PATH, &interactionProfileState), XR_ERROR_PATH_INVALID);
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
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, invalidTopLevelPath, &interactionProfileState),
                                   XR_ERROR_PATH_INVALID);
                }
                {
                    INFO("Unsupported top level path");
                    XrPath unsupportedTopLevelPath = StringToPath(instance, "/invalid/top/level/path");
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, unsupportedTopLevelPath, &interactionProfileState),
                                   XR_ERROR_PATH_UNSUPPORTED);
                }
                {
                    INFO("Invalid type");
                    interactionProfileState = XrInteractionProfileState{XR_TYPE_ACTION_CREATE_INFO};
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, leftHandPath, &interactionProfileState),
                                   XR_ERROR_VALIDATION_FAILURE);
                    interactionProfileState = XrInteractionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
                }
            }
            {
                INFO("Interaction profile changed event");

                // Ensure controllers are on and synced and by now XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED should have been queued.
                // In fact, it may have been queued earlier when actionsets were attached, but that is okay.
                GlobalData& globalData = GetGlobalData();
                if (globalData.leftHandUnderTest) {
                    leftHandInputDevice->SetDeviceActive(true);
                }
                if (globalData.rightHandUnderTest) {
                    rightHandInputDevice->SetDeviceActive(true);
                }
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
                        actionLayerManager.IterateFrame();
                    }
                    return false;
                };

                REQUIRE(ReadUntilEvent(XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED, 1s));
                if (globalData.leftHandUnderTest) {
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, leftHandPath, &interactionProfileState), XR_SUCCESS);
                    REQUIRE(simpleControllerInteractionProfile == interactionProfileState.interactionProfile);
                }
                if (globalData.rightHandUnderTest) {
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, rightHandPath, &interactionProfileState), XR_SUCCESS);
                    REQUIRE(simpleControllerInteractionProfile == interactionProfileState.interactionProfile);
                }
            }
        }
    }

    TEST_CASE("xrSyncActions", "[actions][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        std::vector<const char*> extensions;
        if (globalData.IsInstanceExtensionSupported(XR_EXT_ACTIVE_ACTION_SET_PRIORITY_EXTENSION_NAME))
            extensions.push_back(XR_EXT_ACTIVE_ACTION_SET_PRIORITY_EXTENSION_NAME);
        CompositionHelper compositionHelper("xrSyncActions", extensions);
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile = StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString);

        std::string leftHandPathString = "/user/hand/left";
        XrPath leftHandPath{StringToPath(instance, "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, leftHandPath, GetSimpleInteractionProfile().InputSourcePaths);

        std::string rightHandPathString = "/user/hand/right";
        XrPath rightHandPath{StringToPath(instance, "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, rightHandPath, GetSimpleInteractionProfile().InputSourcePaths);

        bool leftUnderTest = GetGlobalData().leftHandUnderTest;
        std::string defaultDevicePathStr = leftUnderTest ? leftHandPathString : rightHandPathString;
        XrPath defaultDevicePath = leftUnderTest ? leftHandPath : rightHandPath;
        std::shared_ptr<IInputTestDevice> defaultInputDevice = leftUnderTest ? leftHandInputDevice : rightHandInputDevice;

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

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
            SECTION("No active action sets")
            {
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                syncInfo.activeActionSets = nullptr;
                syncInfo.countActiveActionSets = 0;

                {
                    INFO("No action sets attached");

                    REQUIRE_RESULT_SUCCEEDED(xrSyncActions(session, &syncInfo));
                }
                {
                    INFO("With action sets attached");

                    compositionHelper.GetInteractionManager().AddActionSet(actionSet);
                    compositionHelper.GetInteractionManager().AttachActionSets();

                    REQUIRE_RESULT_SUCCEEDED(xrSyncActions(session, &syncInfo));
                }
            }
            SECTION("Active action sets")
            {
                XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
                REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, defaultDevicePath, &interactionProfileState),
                               XR_ERROR_ACTIONSET_NOT_ATTACHED);

                compositionHelper.GetInteractionManager().AddActionSet(actionSet);
                compositionHelper.GetInteractionManager().AttachActionSets();

                {
                    INFO("Interaction profile selection changes must: only happen when flink:xrSyncActions is called.");
                    REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, defaultDevicePath, &interactionProfileState), XR_SUCCESS);
                    // per spec: "Interaction profile selection changes must: only happen when flink:xrSyncActions is called."
                    REQUIRE(interactionProfileState.interactionProfile == XR_NULL_PATH);
                }

                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                XrActiveActionSet activeActionSet{actionSet};
                syncInfo.activeActionSets = &activeActionSet;
                syncInfo.countActiveActionSets = 1;

                REQUIRE_RESULT(xrSyncActions(session, &syncInfo), XR_SESSION_NOT_FOCUSED);

                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                REQUIRE_FALSE(actionStateBoolean.isActive);
            }
        }
        SECTION("Focus")
        {
            actionLayerManager.WaitForSessionFocusWithMessage();

            SECTION("Parameter validation")
            {
                std::string selectPathStr = defaultDevicePathStr + "/input/select/click";
                XrPath selectPath = StringToPath(instance, selectPathStr);
                compositionHelper.GetInteractionManager().AddActionSet(actionSet);
                compositionHelper.GetInteractionManager().AddActionBindings(simpleControllerInteractionProfile, {{action, selectPath}});
                compositionHelper.GetInteractionManager().AttachActionSets();

                if (globalData.leftHandUnderTest) {
                    leftHandInputDevice->SetDeviceActive(true);
                }
                if (globalData.rightHandUnderTest) {
                    rightHandInputDevice->SetDeviceActive(true);
                }

                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                XrActiveActionSet activeActionSet{actionSet};
                syncInfo.activeActionSets = &activeActionSet;
                syncInfo.countActiveActionSets = 1;

                {
                    INFO("Basic usage");

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                    REQUIRE(actionStateBoolean.isActive);
                    REQUIRE_FALSE(actionStateBoolean.currentState);

                    INFO("Repeated state query calls return the same value");
                    {

                        defaultInputDevice->SetButtonStateBool(selectPath, true);

                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);
                        REQUIRE(actionStateBoolean.currentState);

                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);
                        REQUIRE(actionStateBoolean.currentState);
                    }

                    OPTIONAL_DISCONNECTABLE_DEVICE_INFO
                    {
                        actionLayerManager.DisplayMessage("Turn off " + defaultDevicePathStr + " and wait for 20s");
                        defaultInputDevice->SetDeviceActive(false, true);

                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                                REQUIRE(actionStateBoolean.isActive);
                                REQUIRE(actionStateBoolean.currentState);
                                return false;
                            },
                            20s, kActionWaitDelay);

                        actionLayerManager.DisplayMessage("Wait for 5s");

                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                                REQUIRE_FALSE(actionStateBoolean.isActive);
                                REQUIRE_FALSE(actionStateBoolean.currentState);
                                return false;
                            },
                            5s, kActionWaitDelay);
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
                REQUIRE_RESULT(xrCreateActionSet(instance, &setCreateInfo, &highPriorityActionSet), XR_SUCCESS);

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
                REQUIRE_RESULT(xrCreateActionSet(instance, &setCreateInfo, &lowPriorityActionSet), XR_SUCCESS);

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
                        {highPrioritySelectAction, StringToPath(instance, "/user/hand/left/input/select/click")},
                        {highPrioritySelectAction, StringToPath(instance, "/user/hand/right/input/select/click")},
                        {highPrioritySelectAction2, StringToPath(instance, "/user/hand/left/input/select/click")},
                        {highPrioritySelectAction2, StringToPath(instance, "/user/hand/right/input/select/click")},
                        {lowPrioritySelectAction, StringToPath(instance, "/user/hand/left/input/select/click")},
                        {lowPrioritySelectAction, StringToPath(instance, "/user/hand/right/input/select/click")},
                        {lowPriorityMenuAction, StringToPath(instance, "/user/hand/left/input/menu/click")},
                        {lowPriorityMenuAction, StringToPath(instance, "/user/hand/right/input/menu/click")},
                        {lowPrioritySelectAndMenuAction, StringToPath(instance, "/user/hand/left/input/select/click")},
                        {lowPrioritySelectAndMenuAction, StringToPath(instance, "/user/hand/left/input/menu/click")},
                        {lowPrioritySelectAndMenuAction, StringToPath(instance, "/user/hand/right/input/select/click")},
                        {lowPrioritySelectAndMenuAction, StringToPath(instance, "/user/hand/right/input/menu/click")},
                    });

                compositionHelper.GetInteractionManager().AddActionSet(highPriorityActionSet);
                compositionHelper.GetInteractionManager().AddActionSet(lowPriorityActionSet);
                compositionHelper.GetInteractionManager().AttachActionSets();

                if (globalData.leftHandUnderTest) {
                    leftHandInputDevice->SetDeviceActive(true);
                }
                if (globalData.rightHandUnderTest) {
                    rightHandInputDevice->SetDeviceActive(true);
                }

                XrActiveActionSet highPriorityRightHandActiveActionSet{highPriorityActionSet, rightHandPath};
                XrActiveActionSet lowPriorityRightHandActiveActionSet{lowPriorityActionSet, rightHandPath};
                XrActiveActionSet highPriorityLeftHandActiveActionSet{highPriorityActionSet, leftHandPath};
                XrActiveActionSet lowPriorityLeftHandActiveActionSet{lowPriorityActionSet, leftHandPath};

                auto getActionActiveState = [&](XrAction action, XrPath subactionPath) {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = action;
                    getInfo.subactionPath = subactionPath;
                    XrActionStateBoolean booleanData{XR_TYPE_ACTION_STATE_BOOLEAN};
                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanData), XR_SUCCESS);
                    return static_cast<bool>(booleanData.isActive);
                };

                std::vector<XrActiveActionSet> activeSets;
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};

                if (globalData.leftHandUnderTest && globalData.rightHandUnderTest) {
                    // Both sets with null subaction path
                    activeSets = {lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet,
                                  highPriorityLeftHandActiveActionSet, highPriorityRightHandActiveActionSet};
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
                }

                if (globalData.rightHandUnderTest) {
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
                }

                if (globalData.leftHandUnderTest) {
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
                }

                if (globalData.leftHandUnderTest && globalData.rightHandUnderTest) {
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
                    REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) ==
                            true);  // Menu blocked but squeeze active

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
                    REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) ==
                            true);  // Menu blocked but squeeze active
                }

                // Optional active action set priority tests
                OPTIONAL_ACTIVE_ACTION_SET_PRIORITY_SECTION
                {
                    std::vector<XrActiveActionSetPriorityEXT> actionSetPriorities;
                    XrActiveActionSetPrioritiesEXT activeActionSetPriorities{XR_TYPE_ACTIVE_ACTION_SET_PRIORITIES_EXT};

                    // Both sets with priorities swapped
                    activeSets = {lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet,
                                  highPriorityLeftHandActiveActionSet, highPriorityRightHandActiveActionSet};
                    actionSetPriorities = {{highPriorityActionSet, 2}, {lowPriorityActionSet, 3}};
                    syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                    syncInfo.activeActionSets = activeSets.data();
                    activeActionSetPriorities.actionSetPriorityCount = static_cast<uint32_t>(actionSetPriorities.size());
                    activeActionSetPriorities.actionSetPriorities = actionSetPriorities.data();
                    syncInfo.next = &activeActionSetPriorities;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    INFO("high priority + low priority with active priorities swapped");
                    REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == false);    // Blocked by high priority
                    REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == false);    // Blocked by high priority
                    REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == false);   // Blocked by high priority
                    REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == false);   // Blocked by high priority
                    REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == false);   // Blocked by high priority
                    REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == false);  // Blocked by high priority

                    REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == true);
                    REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                    REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == true);
                    REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == true);

                    // Both sets with equal priorities
                    activeSets = {lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet,
                                  highPriorityLeftHandActiveActionSet, highPriorityRightHandActiveActionSet};
                    actionSetPriorities = {{highPriorityActionSet, 2}, {lowPriorityActionSet, 2}};
                    syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                    syncInfo.activeActionSets = activeSets.data();
                    activeActionSetPriorities.actionSetPriorityCount = static_cast<uint32_t>(actionSetPriorities.size());
                    activeActionSetPriorities.actionSetPriorities = actionSetPriorities.data();
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    INFO("active priorities set to be equal");
                    REQUIRE(getActionActiveState(highPrioritySelectAction, XR_NULL_PATH) == true);
                    REQUIRE(getActionActiveState(highPrioritySelectAction, leftHandPath) == true);
                    REQUIRE(getActionActiveState(highPrioritySelectAction, rightHandPath) == true);
                    REQUIRE(getActionActiveState(highPrioritySelectAction2, XR_NULL_PATH) == true);
                    REQUIRE(getActionActiveState(highPrioritySelectAction2, leftHandPath) == true);
                    REQUIRE(getActionActiveState(highPrioritySelectAction2, rightHandPath) == true);

                    REQUIRE(getActionActiveState(lowPrioritySelectAction, XR_NULL_PATH) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAction, leftHandPath) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAction, rightHandPath) == true);
                    REQUIRE(getActionActiveState(lowPriorityMenuAction, XR_NULL_PATH) == true);
                    REQUIRE(getActionActiveState(lowPriorityMenuAction, leftHandPath) == true);
                    REQUIRE(getActionActiveState(lowPriorityMenuAction, rightHandPath) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, XR_NULL_PATH) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, leftHandPath) == true);
                    REQUIRE(getActionActiveState(lowPrioritySelectAndMenuAction, rightHandPath) == true);
                }
            }
            SECTION("subaction path rules")
            {
                XrActionSet subactionPathFreeActionSet{XR_NULL_HANDLE};
                strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 2");
                strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_2");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &subactionPathFreeActionSet), XR_SUCCESS);

                XrActionSet unboundActionActionSet{XR_NULL_HANDLE};
                strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 5");
                strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_5");
                REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &unboundActionActionSet), XR_SUCCESS);

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

                XrAction unboundAction{XR_NULL_HANDLE};
                strcpy(actionCreateInfo.localizedActionName, "test select action 4");
                strcpy(actionCreateInfo.actionName, "test_select_action_4");
                actionCreateInfo.subactionPaths = &defaultDevicePath;
                REQUIRE_RESULT(xrCreateAction(unboundActionActionSet, &actionCreateInfo, &unboundAction), XR_SUCCESS);

                compositionHelper.GetInteractionManager().AddActionBindings(
                    simpleControllerInteractionProfile, {{leftHandAction, StringToPath(instance, "/user/hand/left/input/select/click")},
                                                         {rightHandAction, StringToPath(instance, "/user/hand/right/input/select/click")}});
                compositionHelper.GetInteractionManager().AddActionSet(actionSet);
                compositionHelper.GetInteractionManager().AddActionSet(subactionPathFreeActionSet);
                compositionHelper.GetInteractionManager().AddActionSet(unboundActionActionSet);
                compositionHelper.GetInteractionManager().AttachActionSets();

                if (globalData.leftHandUnderTest) {
                    leftHandInputDevice->SetDeviceActive(true);
                }
                if (globalData.rightHandUnderTest) {
                    rightHandInputDevice->SetDeviceActive(true);
                }

                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                XrActiveActionSet activeActionSet{actionSet};
                XrActiveActionSet subactionPathFreeActiveActionSet{subactionPathFreeActionSet};
                XrActiveActionSet unboundActionActiveActionSet{unboundActionActionSet};
                syncInfo.activeActionSets = &activeActionSet;
                syncInfo.countActiveActionSets = 1;

                {

                    INFO("Basic usage");

                    if (globalData.leftHandUnderTest) {
                        INFO("Left hand");
                        activeActionSet.subactionPath = leftHandPath;
                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        getInfo.action = leftHandAction;
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);

                        getInfo.action = rightHandAction;
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE_FALSE(actionStateBoolean.isActive);
                        {
                            INFO("Values match those specified for isActive == XR_FALSE");
                            // Set these to the wrong thing if not active, to make sure runtime overwrites the values
                            PoisonStructContents(actionStateBoolean);
                            REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                            REQUIRE_FALSE(actionStateBoolean.isActive);
                            // The conformance layer will verify that the other fields have been cleared appropriately.
                        }
                    }

                    if (globalData.rightHandUnderTest) {
                        INFO("Right hand");
                        activeActionSet.subactionPath = rightHandPath;
                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        getInfo.action = leftHandAction;
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE_FALSE(actionStateBoolean.isActive);

                        getInfo.action = rightHandAction;
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);
#
                        {
                            INFO("Values match those specified for isActive == XR_FALSE");
                            // Set these to the wrong thing if not active, to make sure runtime overwrites the values
                            PoisonStructContents(actionStateBoolean);
                            REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                            REQUIRE(actionStateBoolean.isActive);
                            // The conformance layer will verify that the other fields have been cleared appropriately.
                        }
                    }

                    if (globalData.leftHandUnderTest && globalData.rightHandUnderTest) {
                        INFO("both synchronized");
                        XrActiveActionSet bothHands[2] = {{actionSet}, {actionSet}};
                        bothHands[0].subactionPath = leftHandPath;
                        bothHands[1].subactionPath = rightHandPath;
                        syncInfo.countActiveActionSets = 2;
                        syncInfo.activeActionSets = bothHands;
                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        getInfo.action = leftHandAction;
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);

                        getInfo.action = rightHandAction;
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);
                    }

                    INFO("No subaction path");
                    activeActionSet.subactionPath = XR_NULL_PATH;
                    syncInfo.countActiveActionSets = 1;
                    syncInfo.activeActionSets = &activeActionSet;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    syncInfo.activeActionSets = &subactionPathFreeActiveActionSet;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    INFO("Subaction path used but not declared");
                    subactionPathFreeActiveActionSet.subactionPath = defaultDevicePath;
                    REQUIRE_RESULT(xrSyncActions(session, &syncInfo), XR_ERROR_PATH_UNSUPPORTED);

                    XrActionSet unattachedActionSet{XR_NULL_HANDLE};
                    strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 3");
                    strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_3");
                    REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &unattachedActionSet), XR_SUCCESS);

                    INFO("Unbound action");
                    syncInfo.activeActionSets = &unboundActionActiveActionSet;
                    unboundActionActiveActionSet.subactionPath = defaultDevicePath;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    INFO("unattached action set");
                    XrActiveActionSet activeActionSet2 = {unattachedActionSet};
                    syncInfo.countActiveActionSets = 1;
                    syncInfo.activeActionSets = &activeActionSet2;
                    REQUIRE_RESULT(xrSyncActions(session, &syncInfo), XR_ERROR_ACTIONSET_NOT_ATTACHED);

                    XrActiveActionSet bothSets[2] = {{actionSet}, {unattachedActionSet}};
                    syncInfo.countActiveActionSets = 2;
                    syncInfo.activeActionSets = bothSets;
                    REQUIRE_RESULT(xrSyncActions(session, &syncInfo), XR_ERROR_ACTIONSET_NOT_ATTACHED);
                }
                {
                    INFO("Invalid subaction path");
                    syncInfo.countActiveActionSets = 1;
                    syncInfo.activeActionSets = &activeActionSet;
                    activeActionSet.subactionPath = (XrPath)0x1234;
                    REQUIRE_RESULT(xrSyncActions(session, &syncInfo), XR_ERROR_PATH_INVALID);
                }
            }
        }
    }

    TEST_CASE("StateQueryFunctionsInteractive", "[actions][interactive][gamepad]")
    {
        struct ActionInfo
        {
            InputSourcePathAvailData Data;
            XrAction Action{XR_NULL_HANDLE};
            XrAction XAction{XR_NULL_HANDLE};  // Set if type is vector2f
            XrAction YAction{XR_NULL_HANDLE};  // Set if type is vector2f
            std::set<int32_t> UnseenValues;
        };

        constexpr float cStepSize = 0.5f;
        const int32_t cStepSizeOffset = -int32_t(std::roundf(-1.f / cStepSize));
        constexpr float cEpsilon = 0.1f;
        constexpr float cLargeEpsilon = 0.15f;

        auto TestInteractionProfile = [&](const InteractionProfileAvailMetadata& ipMetadata, const std::string& topLevelPathString) {
            CompositionHelper compositionHelper("Input device state query");
            XrInstance instance = compositionHelper.GetInstance();
            XrSession session = compositionHelper.GetSession();
            compositionHelper.BeginSession();
            ActionLayerManager actionLayerManager(compositionHelper);

            actionLayerManager.WaitForSessionFocusWithMessage();

            XrPath interactionProfile = StringToPath(instance, ipMetadata.InteractionProfilePathString);
            XrPath inputDevicePath{StringToPath(instance, topLevelPathString.data())};
            std::shared_ptr<IInputTestDevice> inputDevice =
                CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session, interactionProfile,
                                 inputDevicePath, ipMetadata.InputSourcePaths);

            XrActionSet actionSet{XR_NULL_HANDLE};

            std::string actionSetName = "state_query_test_action_set_" + std::to_string(inputDevicePath);
            std::string localizedActionSetName = "State Query Test Action Set " + std::to_string(inputDevicePath);

            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.localizedActionSetName, localizedActionSetName.c_str());
            strcpy(actionSetCreateInfo.actionSetName, actionSetName.c_str());
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

            uint32_t uniqueActionNameCounter = 0;
            auto GetActionNames = [&uniqueActionNameCounter]() mutable -> std::tuple<std::string, std::string> {
                uniqueActionNameCounter++;
                return std::tuple<std::string, std::string>{"state_query_test_action_" + std::to_string(uniqueActionNameCounter),
                                                            "state query test action " + std::to_string(uniqueActionNameCounter)};
            };

            auto shouldExercisePath = [&ipMetadata](const InputSourcePathAvailData& inputSourceData) -> bool {
                if (inputSourceData.systemOnly) {
                    return false;
                }
                if (strcmp(ipMetadata.InteractionProfileShortname, "oculus/touch_controller") == 0 &&
                    ends_with(inputSourceData.Path, "/input/thumbrest/touch")) {
                    // Rift S and Quest 1 controllers lack thumbrests.
                    return false;
                }
                if (!SatisfiedByDefault(inputSourceData.Availability)) {
                    return false;
                }
                return true;
            };

            auto InputSourceDataForTopLevelPath = [&]() {
                std::vector<InputSourcePathAvailData> ret;
                for (const InputSourcePathAvailData& inputSourceData : ipMetadata.InputSourcePaths) {
                    if (!starts_with(inputSourceData.Path, topLevelPathString)) {
                        continue;
                    }
                    ret.push_back(inputSourceData);
                }
                return ret;
            };
            auto ActionsForTopLevelPath = [&](XrActionType type) -> std::vector<ActionInfo> {
                auto inputSourceDataList = InputSourceDataForTopLevelPath();
                std::vector<ActionInfo> actions;
                for (const InputSourcePathAvailData& inputSourceData : inputSourceDataList) {
                    if (type != inputSourceData.Type) {
                        continue;
                    }
                    if (!shouldExercisePath(inputSourceData)) {
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

                    XrPath bindingPath = StringToPath(instance, inputSourceData.Path);
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
                    case XR_ACTION_TYPE_VECTOR2F_INPUT: {
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

                        std::string xSubBindingPath = std::string(inputSourceData.Path) + "/x";
                        bindingPath = StringToPath(instance, xSubBindingPath);
                        compositionHelper.GetInteractionManager().AddActionBindings(interactionProfile, {{xAction, bindingPath}});

                        actionNames = GetActionNames();
                        strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                        strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &yAction), XR_SUCCESS);

                        std::string ySubBindingPath = std::string(inputSourceData.Path) + "/y";
                        bindingPath = StringToPath(instance, ySubBindingPath);
                        compositionHelper.GetInteractionManager().AddActionBindings(interactionProfile, {{yAction, bindingPath}});
                        break;
                    }
                    case XR_ACTION_TYPE_POSE_INPUT:
                    case XR_ACTION_TYPE_VIBRATION_OUTPUT:
                        break;
                    case XR_ACTION_TYPE_MAX_ENUM:
                    default:
                        WARN("Unexpected action type " << inputSourceData.Type);
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
                    for (const InputSourcePathAvailData& inputSourceData : inputSourceDataList) {
                        if (inputSourceData.Type != type) {
                            continue;
                        }
                        if (!shouldExercisePath(inputSourceData)) {
                            continue;
                        }
                        auto prefixedByParentPath = starts_with(inputSourceData.Path, parentPath);
                        if (prefixedByParentPath) {
                            return true;
                        }
                    }
                    return false;
                };

                std::vector<ActionInfo> actions;
                for (const InputSourcePathAvailData& inputSourceData : inputSourceDataList) {
                    if (type != inputSourceData.Type) {
                        continue;
                    }
                    if (!shouldExercisePath(inputSourceData)) {
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
                        if (coercionType == XR_ACTION_TYPE_FLOAT_INPUT &&
                            HasSubpathOfType(inputSourceData.Path, XR_ACTION_TYPE_FLOAT_INPUT)) {
                            continue;
                        }
                        if (coercionType == XR_ACTION_TYPE_POSE_INPUT &&
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

                    XrPath bindingPath = StringToPath(instance, inputSourceData.Path);
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

                for (const InputSourcePathAvailData& inputSourceData : inputSourceDataList) {
                    if (type != inputSourceData.Type) {
                        continue;
                    }
                    if (!shouldExercisePath(inputSourceData)) {
                        continue;
                    }

                    XrPath bindingPath = StringToPath(instance, inputSourceData.Path);
                    compositionHelper.GetInteractionManager().AddActionBindings(interactionProfile, {{action, bindingPath}});
                }

                ActionInfo info{};
                info.Action = action;

                return info;
            };
            auto concat = [](std::vector<ActionInfo> a, const std::vector<std::vector<ActionInfo>>& tail) {
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
            REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, inputDevicePath, &interactionProfileState), XR_SUCCESS);
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
                actionLayerManager.Sleep_For(1s);

                XrActionStateBoolean combinedBoolState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XrActionStateFloat combinedFloatState{XR_TYPE_ACTION_STATE_FLOAT};
                XrActionStateVector2f combinedVectorState{XR_TYPE_ACTION_STATE_VECTOR2F};
                XrActionStateBoolean previousBoolState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XrActionStateFloat previousFloatState{XR_TYPE_ACTION_STATE_FLOAT};
                XrActionStateVector2f previousVectorState{XR_TYPE_ACTION_STATE_VECTOR2F};

                getInfo.action = allBooleanAction.Action;
                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &previousBoolState), XR_SUCCESS);
                getInfo.action = allFloatAction.Action;
                REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &previousFloatState), XR_SUCCESS);
                getInfo.action = allVectorAction.Action;
                REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &previousVectorState), XR_SUCCESS);

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
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &combinedBoolState), XR_SUCCESS);
                        getInfo.action = allFloatAction.Action;
                        REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &combinedFloatState), XR_SUCCESS);
                        getInfo.action = allVectorAction.Action;
                        REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &combinedVectorState), XR_SUCCESS);

                        REQUIRE((bool)combinedBoolState.isActive == (booleanActions.size() > 0));
                        REQUIRE((bool)combinedFloatState.isActive == (floatActions.size() > 0));
                        REQUIRE((bool)combinedVectorState.isActive == (vectorActions.size() > 0));

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
                                ReportF("%s saw %d", actionInfo.Data.Path, key);
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
                            nextActionPrompt = "\n" + std::string(actionInfo.Data.Path) + ":\n";
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
                                    nextActionPrompt += fmt_float(static_cast<float>(remainingKeys) * cStepSize) + " ";
                                    break;
                                case XR_ACTION_TYPE_VECTOR2F_INPUT: {
                                    float x = static_cast<float>((remainingKeys % 10) - 2) * cStepSize;
                                    float y = ((static_cast<float>(remainingKeys) / 10) - 2) * cStepSize;
                                    nextActionPrompt += "(" + fmt_float(x) + "," + fmt_float(y) + ") ";
                                    break;
                                }
                                case XR_ACTION_TYPE_POSE_INPUT:
                                case XR_ACTION_TYPE_VIBRATION_OUTPUT:
                                    break;
                                case XR_ACTION_TYPE_MAX_ENUM:
                                default:
                                    WARN("Unexpected action type " << actionInfo.Data.Type);
                                    break;
                                }
                            }
                        };

                        for (auto& actionInfo : booleanActions) {
                            getInfo.action = actionInfo.Action;
                            REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
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
                            REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
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
                            REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_SUCCESS);
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
                                REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                                REQUIRE(floatState.isActive);
                                REQUIRE(floatState.currentState == vectorState.currentState.x);
                                ++combinedFloatCount;
                                XrTime xTime = 0;
                                if (!largestFloatState.isActive ||
                                    (std::fabs(largestFloatState.currentState) < std::fabs(floatState.currentState))) {
                                    largestFloatState = floatState;
                                }
                                if (floatState.changedSinceLastSync) {
                                    REQUIRE(floatState.changedSinceLastSync == vectorState.changedSinceLastSync);
                                    xTime = floatState.lastChangeTime;
                                    ++xyChanges;
                                }
                                if (!vectorState.changedSinceLastSync) {
                                    REQUIRE(floatState.changedSinceLastSync == XR_FALSE);
                                }

                                // Verify the y action matches the parent vector.
                                getInfo.action = actionInfo.YAction;
                                REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                                REQUIRE(floatState.isActive);
                                REQUIRE(floatState.currentState == vectorState.currentState.y);
                                ++combinedFloatCount;
                                XrTime yTime = 0;
                                if (!largestFloatState.isActive ||
                                    (std::fabs(largestFloatState.currentState) < std::fabs(floatState.currentState))) {
                                    largestFloatState = floatState;
                                }
                                if (floatState.changedSinceLastSync) {
                                    REQUIRE(floatState.changedSinceLastSync == vectorState.changedSinceLastSync);
                                    yTime = floatState.lastChangeTime;
                                    ++xyChanges;
                                }
                                if (!vectorState.changedSinceLastSync) {
                                    REQUIRE(floatState.changedSinceLastSync == XR_FALSE);
                                }

                                if (xTime && yTime) {
                                    XrTime expectedTime = xTime > yTime ? xTime : yTime;
                                    REQUIRE(expectedTime == vectorState.lastChangeTime);
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
                            for (const auto& actionInfo : booleanActions) {
                                inputDevice->SetButtonStateBool(StringToPath(instance, actionInfo.Data.Path), synthesizedX > 0.5f, true);
                            }

                            for (const auto& actionInfo : floatActions) {
                                inputDevice->SetButtonStateFloat(StringToPath(instance, actionInfo.Data.Path), synthesizedX, 0, true);
                            }

                            for (const auto& actionInfo : vectorActions) {
                                float x = (synthesizedX - 0.5f) * 2.f;
                                float y = (synthesizedY - 0.5f) * 2.f;
                                inputDevice->SetButtonStateVector2(StringToPath(instance, actionInfo.Data.Path), {x, y}, 0, true);
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
                    600s, kActionWaitDelay);

                REQUIRE(seenActions.size() == actionCount);
                REQUIRE_FALSE(waitForCombinedBools);
                REQUIRE_FALSE(waitForCombinedFloats);
                REQUIRE_FALSE(waitForCombinedVectors);

                actionLayerManager.DisplayMessage("Release all inputs");
                actionLayerManager.Sleep_For(2s);
            }

            OPTIONAL_DISCONNECTABLE_DEVICE_INFO
            {
                INFO("Pose state query");

                for (const auto& poseActionData : poseActions) {
                    CAPTURE(poseActionData.Data.Path);

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = poseActionData.Action;

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_SUCCESS);
                    REQUIRE(poseState.isActive);

                    inputDevice->SetDeviceActive(false);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_SUCCESS);
                    REQUIRE_FALSE(poseState.isActive);

                    inputDevice->SetDeviceActive(true);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_SUCCESS);
                    REQUIRE(poseState.isActive);
                }
            }

            INFO("Haptics state query");
            {
                // Need at least one boolean action to confirm haptics
                if (booleanActions.size() > 0) {
                    for (const auto& hapticActionData : hapticActions) {
                        CAPTURE(hapticActionData.Data.Path);

                        XrPath inputSourcePath = StringToPath(instance, booleanActions[0].Data.Path);

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
                                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                                if (booleanState.changedSinceLastSync && booleanState.currentState) {
                                    currentBooleanAction = booleanActionData.Action;
                                    return true;
                                }
                            }
                            return false;
                        };

                        actionLayerManager.DisplayMessage("Press any button when you feel the 3 second haptic vibration");
                        actionLayerManager.IterateFrame();
                        actionLayerManager.Sleep_For(3s);

                        hapticPacket.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(3s).count();
                        REQUIRE_RESULT(
                            xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
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
                            15s, kActionWaitDelay);
                        REQUIRE_FALSE(currentBooleanAction == XR_NULL_HANDLE);

                        {
                            // For automation only
                            inputDevice->SetButtonStateBool(inputSourcePath, false, true);
                        }

                        REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_SUCCESS);

                        actionLayerManager.DisplayMessage("Press any button when you feel the short haptic pulse");
                        actionLayerManager.IterateFrame();
                        actionLayerManager.Sleep_For(3s);

                        hapticPacket.duration = XR_MIN_HAPTIC_DURATION;
                        REQUIRE_RESULT(
                            xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
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
                            15s, kActionWaitDelay);
                        REQUIRE_FALSE(currentBooleanAction == XR_NULL_HANDLE);

                        {
                            // For automation only
                            inputDevice->SetButtonStateBool(inputSourcePath, false, true);
                        }
                    }

                    actionLayerManager.DisplayMessage("Release all inputs");
                    actionLayerManager.Sleep_For(2s);
                }
            }

            INFO("Action value coercion");
            {
                INFO("Boolean->Float");
                for (const auto& booleanToFloatActionData : floatActionsCoercedToBoolean) {
                    CAPTURE(booleanToFloatActionData.Data.Path);
                    XrPath inputSourcePath = StringToPath(instance, booleanToFloatActionData.Data.Path);

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = booleanToFloatActionData.Action;

                    inputDevice->SetButtonStateFloat(inputSourcePath, 0.0f, cEpsilon, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE_FALSE(booleanState.currentState);

                    inputDevice->SetButtonStateFloat(inputSourcePath, 1.0f, cEpsilon, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE(booleanState.currentState);
                    REQUIRE(booleanState.lastChangeTime > 0);

                    inputDevice->SetButtonStateFloat(inputSourcePath, 0.0f, cEpsilon, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE_FALSE(booleanState.currentState);
                    REQUIRE(booleanState.lastChangeTime > 0);
                }

                INFO("Float->Boolean");
                for (const auto& floatToBooleanActionData : booleanActionsCoercedToFloat) {
                    CAPTURE(floatToBooleanActionData.Data.Path);
                    XrPath inputSourcePath = StringToPath(instance, floatToBooleanActionData.Data.Path);

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = floatToBooleanActionData.Action;

                    inputDevice->SetButtonStateBool(inputSourcePath, false, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                    REQUIRE(floatState.isActive);
                    REQUIRE(floatState.currentState == Catch::Approx(0.0f).margin(cLargeEpsilon));

                    inputDevice->SetButtonStateBool(inputSourcePath, true, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                    REQUIRE(floatState.isActive);
                    REQUIRE(floatState.currentState == Catch::Approx(1.0f).margin(cLargeEpsilon));
                    REQUIRE(floatState.lastChangeTime > 0);

                    inputDevice->SetButtonStateBool(inputSourcePath, false, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                    REQUIRE(floatState.isActive);
                    REQUIRE(floatState.currentState == Catch::Approx(0.0f).margin(cLargeEpsilon));
                    REQUIRE(floatState.lastChangeTime > 0);
                }

                INFO("All other coercions");
                for (const auto& actionData : allOtherCoercions) {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = actionData.Action;

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    if (actionData.Data.Type == XR_ACTION_TYPE_BOOLEAN_INPUT) {
                        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &state), XR_SUCCESS);
                        REQUIRE_FALSE(state.isActive);
                    }
                    else if (actionData.Data.Type == XR_ACTION_TYPE_FLOAT_INPUT) {
                        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
                        REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &state), XR_SUCCESS);
                        REQUIRE_FALSE(state.isActive);
                    }
                    else if (actionData.Data.Type == XR_ACTION_TYPE_VECTOR2F_INPUT) {
                        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
                        REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &state), XR_SUCCESS);
                        REQUIRE_FALSE(state.isActive);
                    }
                    else if (actionData.Data.Type == XR_ACTION_TYPE_POSE_INPUT) {
                        XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};
                        REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &state), XR_SUCCESS);
                        REQUIRE_FALSE(state.isActive);
                    }
                }
            }
        };
        const std::string leftHandString{"/user/hand/left"};
        const std::string rightHandString{"/user/hand/right"};
        for (const InteractionProfileAvailMetadata& ipMetadata : GetAllInteractionProfiles()) {
            if (IsInteractionProfileEnabled(ipMetadata.InteractionProfileShortname)) {
                // If the profile has additional required extensions, they should have been enabled automatically.
                REQUIRE(SatisfiedByDefault(ipMetadata.Availability));
                for (const char* const topLevelPathString : ipMetadata.TopLevelPaths) {
                    GlobalData& globalData = GetGlobalData();
                    if ((topLevelPathString == leftHandString && !globalData.leftHandUnderTest) ||
                        (topLevelPathString == rightHandString && !globalData.rightHandUnderTest)) {
                        continue;
                    }
                    ReportF("Testing interaction profile %s for %s", ipMetadata.InteractionProfileShortname, topLevelPathString);
                    TestInteractionProfile(ipMetadata, topLevelPathString);
                }
            }
        }
    }

    TEST_CASE("StateQueryFunctionsAndHaptics", "[actions]")
    {
        CompositionHelper compositionHelper("Input device state query");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrPath leftHandPath = StringToPath(instance, "/user/hand/left");
        XrPath rightHandPath = StringToPath(instance, "/user/hand/right");
        XrPath gamepadPath = StringToPath(instance, "/user/gamepad");
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

        XrPath simpleControllerInteractionProfile = StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString);

        XrPath leftHandSelectClickPath = StringToPath(instance, "/user/hand/left/input/select/click");
        XrPath rightHandSelectClickPath = StringToPath(instance, "/user/hand/right/input/select/click");
        XrPath leftHandMenuClickPath = StringToPath(instance, "/user/hand/left/input/menu/click");
        XrPath rightHandMenuClickPath = StringToPath(instance, "/user/hand/right/input/menu/click");

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
                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);

                    getInfo.action = floatAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);

                    getInfo.action = vectorAction;
                    REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_SUCCESS);

                    getInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_SUCCESS);
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

                        REQUIRE_RESULT(
                            xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                            XR_SUCCESS);
                        REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_SUCCESS);
                    }
                    SECTION("Invalid action")
                    {
                        getInfo.action = (XrAction)0x1234;
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_ERROR_HANDLE_INVALID);
                        REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_HANDLE_INVALID);
                        REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_ERROR_HANDLE_INVALID);
                        REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_ERROR_HANDLE_INVALID);

                        hapticActionInfo.action = getInfo.action;
                        REQUIRE_RESULT(
                            xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                            XR_ERROR_HANDLE_INVALID);
                        REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_ERROR_HANDLE_INVALID);
                    }
                }
                SECTION("Invalid subaction path")
                {
                    getInfo.subactionPath = (XrPath)0x1234;
                    getInfo.action = booleanAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_ERROR_PATH_INVALID);

                    getInfo.action = floatAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_PATH_INVALID);

                    getInfo.action = vectorAction;
                    REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_ERROR_PATH_INVALID);

                    getInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_ERROR_PATH_INVALID);

                    hapticActionInfo.subactionPath = getInfo.subactionPath;
                    REQUIRE_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_PATH_INVALID);
                    REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_ERROR_PATH_INVALID);
                }
                SECTION("Unspecified subaction path")
                {
                    getInfo.subactionPath = gamepadPath;
                    getInfo.action = booleanAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_ERROR_PATH_UNSUPPORTED);

                    getInfo.action = floatAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_PATH_UNSUPPORTED);

                    getInfo.action = vectorAction;
                    REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_ERROR_PATH_UNSUPPORTED);

                    getInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_ERROR_PATH_UNSUPPORTED);

                    hapticActionInfo.subactionPath = getInfo.subactionPath;
                    REQUIRE_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_PATH_UNSUPPORTED);
                    REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_ERROR_PATH_UNSUPPORTED);

                    {
                        INFO("Action created with no subaction paths, cannot be queried with any");
                        getInfo.action = confirmAction;
                        getInfo.subactionPath = leftHandPath;
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_ERROR_PATH_UNSUPPORTED);
                    }
                }
                SECTION("Type mismatch")
                {
                    getInfo.action = booleanAction;
                    hapticActionInfo.action = booleanAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_ERROR_ACTION_TYPE_MISMATCH);

                    getInfo.action = floatAction;
                    hapticActionInfo.action = floatAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_ERROR_ACTION_TYPE_MISMATCH);

                    getInfo.action = vectorAction;
                    hapticActionInfo.action = vectorAction;
                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_ERROR_ACTION_TYPE_MISMATCH);

                    getInfo.action = poseAction;
                    hapticActionInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                                   XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_ERROR_ACTION_TYPE_MISMATCH);

                    getInfo.action = hapticAction;
                    hapticActionInfo.action = hapticAction;
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_ERROR_ACTION_TYPE_MISMATCH);
                    REQUIRE_RESULT(xrGetActionStateVector2f(session, &getInfo, &vectorState), XR_ERROR_ACTION_TYPE_MISMATCH);
                }
            }
        }
    }
    TEST_CASE("action_space_creation_pre_suggest", "[actions][interactive]")
    {
        // Creates two ActionSpaces
        // - one is created before xrSuggestInteractionProfileBindings and
        // - the other is created after.
        // These two action spaces should both return (the same) valid data.
        CompositionHelper compositionHelper("action_space_creation_pre_suggest");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();
        compositionHelper.BeginSession();
        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile = StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString);

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction poseAction{XR_NULL_HANDLE};
        XrActionCreateInfo createInfo{XR_TYPE_ACTION_CREATE_INFO};
        createInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        strcpy(createInfo.actionName, "test_action_name");
        strcpy(createInfo.localizedActionName, "test localized name");
        createInfo.countSubactionPaths = 0;
        createInfo.subactionPaths = nullptr;
        REQUIRE_RESULT(xrCreateAction(actionSet, &createInfo, &poseAction), XR_SUCCESS);

        // Create an ActionSpace before xrSuggestInteractionProfileBindings
        XrActionSpaceCreateInfo earlySpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        earlySpaceCreateInfo.poseInActionSpace = XrPosefCPP();
        earlySpaceCreateInfo.action = poseAction;
        XrSpace earlyActionSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(session, &earlySpaceCreateInfo, &earlyActionSpace), XR_SUCCESS);

        std::shared_ptr<IInputTestDevice> leftHandInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session, simpleControllerInteractionProfile,
            StringToPath(instance, "/user/hand/left"), GetSimpleInteractionProfile().InputSourcePaths);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(
            simpleControllerInteractionProfile, {{poseAction, StringToPath(instance, "/user/hand/left/input/grip/pose")}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        // Create an ActionSpace after xrSuggestInteractionProfileBindings
        XrActionSpaceCreateInfo lateSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        lateSpaceCreateInfo.poseInActionSpace = XrPosefCPP();
        lateSpaceCreateInfo.action = poseAction;
        XrSpace lateActionSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(session, &lateSpaceCreateInfo, &lateActionSpace), XR_SUCCESS);

        actionLayerManager.WaitForSessionFocusWithMessage();

        XrSpace localSpace{XR_NULL_HANDLE};
        XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        createSpaceInfo.poseInReferenceSpace = XrPosefCPP();
        REQUIRE_RESULT(xrCreateReferenceSpace(session, &createSpaceInfo, &localSpace), XR_SUCCESS);

        leftHandInputDevice->SetDeviceActive(true);

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        XrActiveActionSet activeActionSet{actionSet};
        syncInfo.activeActionSets = &activeActionSet;
        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

        XrSpaceLocation earlyLocation{XR_TYPE_SPACE_LOCATION, nullptr};
        XrSpaceLocation lateLocation{XR_TYPE_SPACE_LOCATION, nullptr};
        REQUIRE(actionLayerManager.WaitForLocatability("left", lateActionSpace, localSpace, &lateLocation, true));

        XrTime locateTime =
            actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime();  // Ensure using the same time for the pose checks.

        REQUIRE_RESULT(xrLocateSpace(lateActionSpace, localSpace, locateTime, &lateLocation), XR_SUCCESS);
        REQUIRE_RESULT(xrLocateSpace(earlyActionSpace, localSpace, locateTime, &earlyLocation), XR_SUCCESS);

        REQUIRE_MSG(lateLocation.locationFlags != 0,
                    "xrLocateSpace on action space created after binding suggestion should return valid pose");
        REQUIRE_MSG(earlyLocation.locationFlags != 0,
                    "xrLocateSpace on action space created before binding suggestion should return valid pose");
        REQUIRE_MSG(earlyLocation.locationFlags == lateLocation.locationFlags,
                    "xrLocateSpace on action space created before and after binding suggestion should return the same locationFlags");
    }

    TEST_CASE("ActionSpaces", "[actions][interactive]")
    {
        GlobalData& globalData = GetGlobalData();

        CompositionHelper compositionHelper("Action Spaces");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();
        compositionHelper.BeginSession();
        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile = StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString);
        XrPath leftHandPath{StringToPath(instance, "/user/hand/left")};
        XrPath rightHandPath{StringToPath(instance, "/user/hand/right")};
        const XrPath bothHands[2] = {leftHandPath, rightHandPath};

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction poseAction{XR_NULL_HANDLE};
        XrActionCreateInfo createInfo{XR_TYPE_ACTION_CREATE_INFO};
        createInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        strcpy(createInfo.actionName, "test_action_name");
        strcpy(createInfo.localizedActionName, "test localized name");
        createInfo.countSubactionPaths = 2;
        createInfo.subactionPaths = bothHands;
        REQUIRE_RESULT(xrCreateAction(actionSet, &createInfo, &poseAction), XR_SUCCESS);

        std::shared_ptr<IInputTestDevice> leftHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, leftHandPath, GetSimpleInteractionProfile().InputSourcePaths);

        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, rightHandPath, GetSimpleInteractionProfile().InputSourcePaths);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(
            simpleControllerInteractionProfile, {{poseAction, StringToPath(instance, "/user/hand/left/input/grip/pose")},
                                                 {poseAction, StringToPath(instance, "/user/hand/right/input/grip/pose")}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        actionLayerManager.WaitForSessionFocusWithMessage();

        XrActiveActionSet leftHandActiveSet{actionSet, leftHandPath};
        XrActiveActionSet rightHandActiveSet{actionSet, rightHandPath};
        XrActiveActionSet bothSets[] = {leftHandActiveSet, rightHandActiveSet};

        XrSpace localSpace{XR_NULL_HANDLE};
        XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        createSpaceInfo.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};
        REQUIRE_RESULT(xrCreateReferenceSpace(session, &createSpaceInfo, &localSpace), XR_SUCCESS);

        XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        spaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, 0}};
        spaceCreateInfo.action = poseAction;

        // Can track left or right, but may: only switch at xrSyncActions.
        XrSpace actionSpaceWithoutSubactionPath{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(session, &spaceCreateInfo, &actionSpaceWithoutSubactionPath), XR_SUCCESS);

        // Only tracks left
        spaceCreateInfo.subactionPath = leftHandPath;
        XrSpace leftSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(session, &spaceCreateInfo, &leftSpace), XR_SUCCESS);

        // Only tracks right
        spaceCreateInfo.subactionPath = rightHandPath;
        XrSpace rightSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(session, &spaceCreateInfo, &rightSpace), XR_SUCCESS);

        if (globalData.leftHandUnderTest) {
            leftHandInputDevice->SetDeviceActive(true);
        }
        if (globalData.rightHandUnderTest) {
            rightHandInputDevice->SetDeviceActive(true);
        }

        OPTIONAL_DISCONNECTABLE_DEVICE_INFO
        {
            XrSpaceVelocity leftVelocity{XR_TYPE_SPACE_VELOCITY};
            XrSpaceVelocity rightVelocity{XR_TYPE_SPACE_VELOCITY};
            XrSpaceLocation leftRelation{XR_TYPE_SPACE_LOCATION, &leftVelocity};
            XrSpaceLocation rightRelation{XR_TYPE_SPACE_LOCATION, &rightVelocity};

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.countActiveActionSets = 2;
            syncInfo.activeActionSets = bothSets;

            auto PosesAreEqual = [](XrPosef a, XrPosef b) -> bool {
                constexpr float e = 0.001f;  // 1mm
                return (a.position.x == Catch::Approx(b.position.x).epsilon(e)) &&
                       (a.position.y == Catch::Approx(b.position.y).epsilon(e)) &&
                       (a.position.z == Catch::Approx(b.position.z).epsilon(e)) &&
                       (a.orientation.x == Catch::Approx(b.orientation.x).epsilon(e)) &&
                       (a.orientation.y == Catch::Approx(b.orientation.y).epsilon(e)) &&
                       (a.orientation.z == Catch::Approx(b.orientation.z).epsilon(e)) &&
                       (a.orientation.w == Catch::Approx(b.orientation.w).epsilon(e));
            };

            if (globalData.leftHandUnderTest && globalData.rightHandUnderTest) {
                // two-handed tests
                // If we tell them to place the controllers somewhere they don't move,
                // we can compare poses to verify identity.

                leftHandInputDevice->SetDeviceActive(false);
                actionLayerManager.DisplayMessage("Place left controller somewhere static but trackable");
                actionLayerManager.Sleep_For(5s);
                // wait to lose left
                REQUIRE(actionLayerManager.WaitForLocatability("left", leftSpace, localSpace, &leftRelation, false));
                leftHandInputDevice->SetDeviceActive(true);
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                // wait to gain left
                REQUIRE(actionLayerManager.WaitForLocatability("left", leftSpace, localSpace, &leftRelation, true));

                rightHandInputDevice->SetDeviceActive(false);
                actionLayerManager.DisplayMessage(
                    "Place right controller somewhere static but trackable. Keep left controller on and trackable.");
                actionLayerManager.Sleep_For(5s);
                // wait to lose right
                REQUIRE(actionLayerManager.WaitForLocatability("right", rightSpace, localSpace, &rightRelation, false));
                rightHandInputDevice->SetDeviceActive(true);
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                // wait to gain right
                REQUIRE(actionLayerManager.WaitForLocatability("right", rightSpace, localSpace, &rightRelation, true));

                // turn right back off again
                rightHandInputDevice->SetDeviceActive(false);

                // wait until we lose right again
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                REQUIRE(actionLayerManager.WaitForLocatability("left", leftSpace, localSpace, &leftRelation, true));
                REQUIRE(actionLayerManager.WaitForLocatability("right", rightSpace, localSpace, &rightRelation, false));

                XrSpaceVelocity currentVelocity{XR_TYPE_SPACE_VELOCITY};
                XrSpaceLocation currentRelation{XR_TYPE_SPACE_LOCATION, &currentVelocity};
                XrTime locateTime =
                    actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime();  // Ensure using the same time for the pose checks.
                REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace, locateTime, &currentRelation), XR_SUCCESS);
                REQUIRE_RESULT(xrLocateSpace(leftSpace, localSpace, locateTime, &leftRelation), XR_SUCCESS);
                REQUIRE_RESULT(xrLocateSpace(rightSpace, localSpace, locateTime, &rightRelation), XR_SUCCESS);
                REQUIRE(currentRelation.locationFlags != 0);
                REQUIRE(leftRelation.locationFlags != 0);
                REQUIRE(PosesAreEqual(currentRelation.pose, leftRelation.pose));
                REQUIRE_FALSE(PosesAreEqual(leftRelation.pose, rightRelation.pose));

                // Try making sure action spaces don't un-stick from actions without an xrSyncActions
                // Making right active to tempt the runtime
                rightHandInputDevice->SetDeviceActive(true);
                leftHandInputDevice->SetDeviceActive(false);

                INFO("Left is off but we're still tracking it");
                // wait for it to go away
                REQUIRE(actionLayerManager.WaitForLocatability("left", leftSpace, localSpace, &leftRelation, false));
                REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                             actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 == currentRelation.locationFlags);

                // TODO illegal assumption
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                // It should still be unlocatable
                INFO("We are still tracking left as action spaces pick one device and stick with it");
                REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                             actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 == currentRelation.locationFlags);

                leftHandInputDevice->SetDeviceActive(false);
                rightHandInputDevice->SetDeviceActive(false);

                INFO("We are still tracking left, but it's off");
                REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                             actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 == currentRelation.locationFlags);

                // TODO illegal assumption
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                INFO("We are still tracking left, but they're both off");
                REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                             actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 == currentRelation.locationFlags);

                leftHandInputDevice->SetDeviceActive(true);
                rightHandInputDevice->SetDeviceActive(true);

                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                REQUIRE(actionLayerManager.WaitForLocatability("left", leftSpace, localSpace, &leftRelation, true));
                REQUIRE(actionLayerManager.WaitForLocatability("right", rightSpace, localSpace, &rightRelation, true));

                REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                             actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 != currentRelation.locationFlags);

                INFO("The action space should remain locatable despite destruction of the action");
                REQUIRE_RESULT(xrDestroyAction(poseAction), XR_SUCCESS);

                REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                             actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 != currentRelation.locationFlags);
                REQUIRE_RESULT(xrLocateSpace(leftSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(),
                                             &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 != currentRelation.locationFlags);
                REQUIRE_RESULT(xrLocateSpace(rightSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(),
                                             &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 != currentRelation.locationFlags);

                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                XrActionStatePose poseActionState{XR_TYPE_ACTION_STATE_POSE};
                OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
                {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = poseAction;
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseActionState), XR_ERROR_HANDLE_INVALID);
                }

                REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                             actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 != currentRelation.locationFlags);
                REQUIRE_RESULT(xrLocateSpace(leftSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(),
                                             &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 != currentRelation.locationFlags);
                REQUIRE_RESULT(xrLocateSpace(rightSpace, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(),
                                             &currentRelation),
                               XR_SUCCESS);
                REQUIRE(0 != currentRelation.locationFlags);
            }
            else {
                // One handed
                std::shared_ptr<IInputTestDevice> availableInputDevice =
                    globalData.leftHandUnderTest ? leftHandInputDevice : rightHandInputDevice;
                std::shared_ptr<IInputTestDevice> unavailableInputDevice =
                    globalData.leftHandUnderTest ? rightHandInputDevice : leftHandInputDevice;

                XrPath controllerSubactionPath = globalData.leftHandUnderTest ? leftHandPath : rightHandPath;

                XrSpace actionSpaceWithSubactionPath = globalData.leftHandUnderTest ? leftSpace : rightSpace;
                XrSpaceLocation relationWithSubactionPath{XR_TYPE_SPACE_LOCATION};

                // This will wait until it is inactive, which implies not locatable
                {
                    INFO(
                        "Repeatedly syncing actions, waiting for boolean action associated with controller to be reported as not active after turning it off");
                    availableInputDevice->SetDeviceActive(false);
                }
                actionLayerManager.DisplayMessage("Place controller somewhere static but trackable");
                actionLayerManager.Sleep_For(5s);

                // Tries to locate the controller space, and returns the location flags.
                auto checkTrackingFlags = [&]() -> XrSpaceLocationFlags {
                    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr, 0, XrPosefCPP{}};
                    REQUIRE_RESULT(xrLocateSpace(actionSpaceWithSubactionPath, localSpace,
                                                 actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &location),
                                   XR_SUCCESS);
                    return location.locationFlags;
                };

                // Gets the action state for the pose action, and returns whether isActive is XR_TRUE.
                auto getActionStatePoseActive = [&] {
                    const auto getInfo = XrActionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, poseAction, controllerSubactionPath};
                    XrActionStatePose statePose{XR_TYPE_ACTION_STATE_POSE};
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &statePose), XR_SUCCESS);
                    return statePose.isActive == XR_TRUE;
                };

                // Does not call xrSyncActions nor does it wait, so the pose action state should remain inactive until we sync
                {
                    INFO("Off controller should not have active pose state");
                    REQUIRE(getActionStatePoseActive() == false);
                }
                availableInputDevice->SetDeviceActiveWithoutWaiting(true, " - Will wait 20s whether or not the controller is found");
                {
                    INFO("Pose state should not become active again without a call to xrSyncActions");
                    REQUIRE(getActionStatePoseActive() == false);
                }

                // Just spin the loop for 20s making sure we don't get orientation valid: we didn't call xrSyncActions so
                // we shouldn't be able to start tracking. (last xrGetPoseActionState returned isActive = false)
                {
                    INFO("Make sure that a re-connected device that was inactive doesn't become active or locatable without xrSyncActions");
                    WaitUntilPredicateWithTimeout(
                        [&] {
                            CAPTURE(XrSpaceLocationFlagsCPP(checkTrackingFlags()));
                            REQUIRE((checkTrackingFlags() & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) == 0);

                            REQUIRE(getActionStatePoseActive() == false);
                            actionLayerManager.IterateFrame();
                            return false;
                        },
                        20s, 3ms);
                }

                // OK, now we can call xrSyncActions - should be active immediately once we do so.
                {
                    INFO("Confirming the pose state still inactive");
                    CAPTURE(getActionStatePoseActive());
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    INFO("Have now done xrSyncActions, would expect trackable, on controller to be active in this case");
                    CAPTURE(getActionStatePoseActive());
                    INFO("Should be instantaneous after one suitable xrSyncActions for a trackable controller to become active");
                    const auto initialTime = std::chrono::steady_clock::now();
                    availableInputDevice->Wait(true, IInputTestDevice::WaitUntilBoolActionIsActiveUpdated{XR_NULL_HANDLE, actionSet});
                    CAPTURE(getActionStatePoseActive());
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    REQUIRE(getActionStatePoseActive());
                    REQUIRE(std::chrono::steady_clock::now() - initialTime < 500ms);
                }
                // wait to gain
                {
                    INFO("Controller should be on and trackable");
                    REQUIRE(actionLayerManager.WaitForLocatability("only", actionSpaceWithSubactionPath, localSpace,
                                                                   &relationWithSubactionPath, true));
                }

                // Try locating - verify that our action space that is not filtered by subaction path
                // has picked up the active controller
                XrSpaceVelocity currentVelocity{XR_TYPE_SPACE_VELOCITY};
                XrSpaceLocation relationWithoutSubactionPath{XR_TYPE_SPACE_LOCATION, &currentVelocity};
                XrTime locateTime =
                    actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime();  // Ensure using the same time for the pose checks.
                {
                    INFO("Making sure our space with no subaction path filter has found our active controller");
                    REQUIRE_RESULT(xrLocateSpace(actionSpaceWithSubactionPath, localSpace, locateTime, &relationWithSubactionPath),
                                   XR_SUCCESS);
                    REQUIRE(relationWithSubactionPath.locationFlags != 0);
                    CAPTURE(relationWithSubactionPath.pose);

                    REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace, locateTime, &relationWithoutSubactionPath),
                                   XR_SUCCESS);
                    REQUIRE(relationWithoutSubactionPath.locationFlags != 0);
                    CAPTURE(relationWithoutSubactionPath.pose);

                    REQUIRE(PosesAreEqual(relationWithoutSubactionPath.pose, relationWithSubactionPath.pose));
                }

                {

                    INFO("General goal: Make sure both bindings - with and without subaction path - go away when we make it inactive");

                    // Now turn it off, and make sure it goes away from both bindings
                    INFO(
                        "Turn off controller and wait until orientation is no longer valid, without calling xrSyncActions so runtime cannot change bindings");
                    availableInputDevice->SetDeviceActiveWithoutWaiting(false);
                    auto lastUsed = availableInputDevice->Wait(
                        false,
                        IInputTestDevice::WaitUntilLosesOrGainsOrientationValidity{
                            actionSpaceWithSubactionPath, localSpace, actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime()});
                    {
                        INFO("Pose action state must still say 'active' because we didn't yet xrSyncActions");
                        REQUIRE(getActionStatePoseActive());
                    }

                    {
                        INFO(
                            "Locating action space created with subaction path (for 'off' controller) must have neither orientation nor position tracked.");
                        REQUIRE_RESULT(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace, lastUsed, &relationWithoutSubactionPath),
                                       XR_SUCCESS);
                        CAPTURE(XrSpaceLocationFlagsCPP(relationWithoutSubactionPath.locationFlags));
                        REQUIRE((relationWithoutSubactionPath.locationFlags &
                                 (XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)) == 0);
                    }

                    {
                        INFO(
                            "Locating action space created without subaction path, but known to be bound to the same "
                            "(off) controller, must also have neither orientation nor position tracked: "
                            "Runtime not allowed to change binding without xrSyncActions call");
                        REQUIRE_RESULT(xrLocateSpace(actionSpaceWithSubactionPath, localSpace, lastUsed, &relationWithSubactionPath),
                                       XR_SUCCESS);
                        CAPTURE(XrSpaceLocationFlagsCPP(relationWithSubactionPath.locationFlags));
                        REQUIRE((relationWithSubactionPath.locationFlags &
                                 (XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)) == 0);
                    }

                    // now run xrSyncActions
                    availableInputDevice->Wait(false, IInputTestDevice::WaitUntilBoolActionIsActiveUpdated{});

                    INFO("After xrSyncActions, both spaces must still be not locatable, and the action must now report being inactive.");
                    REQUIRE_FALSE(getActionStatePoseActive());
                    {
                        INFO("xrGetActionStatePose with subactionPath populated: must be inactive");

                        const auto getInfo =
                            XrActionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, poseAction, controllerSubactionPath};
                        XrActionStatePose statePose{XR_TYPE_ACTION_STATE_POSE};
                        REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &statePose), XR_SUCCESS);
                        REQUIRE(statePose.isActive == XR_FALSE);
                    }
                    {
                        INFO(
                            "xrGetActionStatePose with subactionPath empty: must be inactive"
                            " (previously bound controller now off, and no other controller to bind to)");

                        const auto getInfo = XrActionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, poseAction, XR_NULL_PATH};
                        XrActionStatePose statePose{XR_TYPE_ACTION_STATE_POSE};
                        REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &statePose), XR_SUCCESS);
                        REQUIRE(statePose.isActive == XR_FALSE);
                    }
                }

                // turn it back on, then test that destroying the action doesn't break the tracking
                // This will wait for isActive = true, which does not necessarily imply locatable, so we must wait further.
                {
                    INFO(
                        "Turning controller back on, syncing actions until focused, and waiting until locatable - "
                        "should still be in a trackable location");
                    availableInputDevice->SetDeviceActive(true);
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                    REQUIRE(actionLayerManager.WaitForLocatability("only", actionSpaceWithSubactionPath, localSpace,
                                                                   &relationWithSubactionPath, true));

                    INFO("Verify that the space without a subaction path has also gotten this device");
                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                                                     actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(),
                                                                     &relationWithoutSubactionPath));
                    REQUIRE(0 != relationWithoutSubactionPath.locationFlags);
                }

                {
                    INFO("The action space must remain locatable despite destruction of the action");
                    REQUIRE_RESULT(xrDestroyAction(poseAction), XR_SUCCESS);

                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrLocateSpace(actionSpaceWithSubactionPath, localSpace,
                                                                     actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(),
                                                                     &relationWithSubactionPath));
                    REQUIRE(0 != relationWithSubactionPath.locationFlags);

                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrLocateSpace(actionSpaceWithoutSubactionPath, localSpace,
                                                                     actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(),
                                                                     &relationWithoutSubactionPath));
                    REQUIRE(0 != relationWithoutSubactionPath.locationFlags);

                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    // Try using our destroyed handle, if we're checking for handle validation
                    OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
                    {
                        XrActionStatePose poseActionState{XR_TYPE_ACTION_STATE_POSE};
                        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                        getInfo.action = poseAction;
                        REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseActionState), XR_ERROR_HANDLE_INVALID);
                    }
                }
            }
        }
    }

    TEST_CASE("xrEnumerateBoundSourcesForAction_and_xrGetInputSourceLocalizedName", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("BoundSources and LocalizedName");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
        strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
        REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

        XrAction action{XR_NULL_HANDLE};
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy(actionCreateInfo.localizedActionName, "test action localized name bool");
        strcpy(actionCreateInfo.actionName, "test_action_name_bool");
        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

        compositionHelper.BeginSession();

        ActionLayerManager actionLayerManager(compositionHelper);
        actionLayerManager.WaitForSessionFocusWithMessage();

        bool leftUnderTest = GetGlobalData().leftHandUnderTest;
        const char* pathStr = leftUnderTest ? "/user/hand/left" : "/user/hand/right";

        XrPath path{StringToPath(instance, pathStr)};
        std::shared_ptr<IInputTestDevice> inputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), path,
                             GetSimpleInteractionProfile().InputSourcePaths);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(
            StringToPath(instance, "/interaction_profiles/khr/simple_controller"),
            {{action, StringToPath(instance, "/user/hand/left/input/select/click")},
             {action, StringToPath(instance, "/user/hand/right/input/select/click")}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        inputDevice->SetDeviceActive(true);

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
                std::vector<XrPath> enumerateResult = REQUIRE_TWO_CALL(XrPath, {}, xrEnumerateBoundSourcesForAction, session, &info);

                // Note that runtimes may return bound sources even when not focused, though they don't have to

                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                enumerateResult = REQUIRE_TWO_CALL(XrPath, {}, xrEnumerateBoundSourcesForAction, session, &info);

                REQUIRE(enumerateResult.size() > 0);

                XrInputSourceLocalizedNameGetInfo getInfo{XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
                getInfo.sourcePath = enumerateResult[0];
                SECTION("xrGetInputSourceLocalizedName")
                {
                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT;
                    std::string localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT;
                    localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents =
                        XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT;
                    localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents =
                        XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                              XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                              XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo).data();
                    REQUIRE_FALSE(localizedStringResult.empty());

                    uint32_t sourceCountOutput;
                    char buffer;
                    SECTION("Invalid components")
                    {
                        getInfo.whichComponents = 0;
                        REQUIRE_RESULT(xrGetInputSourceLocalizedName(session, &getInfo, 0, &sourceCountOutput, &buffer),
                                       XR_ERROR_VALIDATION_FAILURE);
                    }
                    SECTION("Invalid path")
                    {
                        getInfo.sourcePath = XR_NULL_PATH;
                        REQUIRE_RESULT(xrGetInputSourceLocalizedName(session, &getInfo, 0, &sourceCountOutput, &buffer),
                                       XR_ERROR_PATH_INVALID);
                        getInfo.sourcePath = (XrPath)0x1234;
                        REQUIRE_RESULT(xrGetInputSourceLocalizedName(session, &getInfo, 0, &sourceCountOutput, &buffer),
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
                    REQUIRE_RESULT(xrEnumerateBoundSourcesForAction(session, &info, 0, &sourceCountOutput, &buffer),
                                   XR_ERROR_HANDLE_INVALID);
                }
            }
        }
    }
}  // namespace Conformance
