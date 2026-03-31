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
#include "availability_helper.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "input_testinputdevice.h"
#include "interaction_info.h"
#include "matchers.h"
#include "report.h"
#include "two_call.h"
#include "utilities/feature_availability.h"
#include "utilities/bitmask_to_string.h"
#include "utilities/event_reader.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"
#include "utilities/string_utils.h"
#include "xr_math_approx.h"

#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace std::chrono_literals;
using namespace Conformance;

// Stores the top level path in slot 2 and the identifier path in slot 5 or 6 based on whether or not the component was included.
// If the component was included, 6 and 7 will be matched with the parent and component, otherwise 5 will be matched.
const std::regex cInteractionSourcePathRegex("^((.+)/(input|output))/(([^/]+)|([^/]+)/([^/]+))$");

namespace Conformance
{
    using namespace openxr::math_operators;

    struct ExpectedResult
    {
        const char* pathStr;
        XrResult expectedResult;
    };
    std::array<ExpectedResult, 18> expectedSingleLevelPathNameResult = {{
        {"INVALID_PATH_COMPONENT", XR_ERROR_PATH_FORMAT_INVALID},     //
        {"invalid path component", XR_ERROR_PATH_FORMAT_INVALID},     //
        {"invalid_path_component_!", XR_ERROR_PATH_FORMAT_INVALID},   //
        {"invalid_path_component_\"", XR_ERROR_PATH_FORMAT_INVALID},  //
        {"invalid_path_component_@", XR_ERROR_PATH_FORMAT_INVALID},   //
        {"invalid_path_component_€", XR_ERROR_PATH_FORMAT_INVALID},   //
        {"invalid_path_component_ß", XR_ERROR_PATH_FORMAT_INVALID},   //
        {"invalid_path_component_,", XR_ERROR_PATH_FORMAT_INVALID},   //
        {"invalid_path_component_ä", XR_ERROR_PATH_FORMAT_INVALID},   //
        {"invalid_path_component_~", XR_ERROR_PATH_FORMAT_INVALID},   //
        {"invalid/path_component", XR_ERROR_PATH_FORMAT_INVALID},     //
        {"valid_path_component", XR_SUCCESS},                         //
        {"valid_path_component_0", XR_SUCCESS},                       //
        {"valid-path-component", XR_SUCCESS},                         //
        {"valid.path.component", XR_SUCCESS},                         //
        {".", XR_SUCCESS},                                            //
        {"..", XR_SUCCESS},                                           //
        {"...", XR_SUCCESS},                                          //
    }};

    TEST_CASE("xrCreateActionSet", "[actions]")
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

        OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
        {
            XrActionSetCreateInfo actionSetCreateInfoWithoutType = actionSetCreateInfo;
            actionSetCreateInfoWithoutType.type = (XrStructureType)0;
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfoWithoutType, &actionSet), XR_ERROR_VALIDATION_FAILURE);
        }

        OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
        {
            XrActionSetCreateInfo actionSetCreateInfoWithInvalidType = actionSetCreateInfo;
            actionSetCreateInfoWithInvalidType.type = XR_TYPE_ACTIONS_SYNC_INFO;
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfoWithInvalidType, &actionSet), XR_ERROR_VALIDATION_FAILURE);
        }

        SECTION("Basic action creation")
        {
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);
        }
        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            REQUIRE_RESULT(xrCreateActionSet(GetGlobalData().invalidInstance, &actionSetCreateInfo, &actionSet), XR_ERROR_HANDLE_INVALID);
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
                for (auto& value : expectedSingleLevelPathNameResult) {
                    CAPTURE(value.pathStr);
                    strcpy(actionSetCreateInfo.actionSetName, value.pathStr);
                    strcpy(actionSetCreateInfo.localizedActionSetName, value.pathStr);  // easy way to avoid duplication

                    XrResult result = XR_SUCCESS;
                    CHECK((result = xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet)) == value.expectedResult);
                    if (result == XR_SUCCESS) {
                        CHECK(xrDestroyActionSet(actionSet) == XR_SUCCESS);
                    }
                }
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

        OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
        {
            XrActionCreateInfo actionCreateInfoWithoutType = actionCreateInfo;
            actionCreateInfoWithoutType.type = (XrStructureType)0;
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfoWithoutType, &action), XR_ERROR_VALIDATION_FAILURE);
        }

        OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
        {
            XrActionCreateInfo actionCreateInfoWithInvalidType = actionCreateInfo;
            actionCreateInfoWithInvalidType.type = XR_TYPE_ACTIONS_SYNC_INFO;
            REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfoWithInvalidType, &action), XR_ERROR_VALIDATION_FAILURE);
        }

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
                const std::array<XrPath, 2> subactionPaths = {{
                    StringToPath(instance, "/user/head"),
                    StringToPath(instance, "/user/head"),
                }};
                actionCreateInfo.countSubactionPaths = 2;
                actionCreateInfo.subactionPaths = subactionPaths.data();
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
                for (auto& value : expectedSingleLevelPathNameResult) {
                    CAPTURE(value.pathStr);
                    strcpy(actionCreateInfo.actionName, value.pathStr);
                    strcpy(actionCreateInfo.localizedActionName, value.pathStr);  // easy way to avoid duplication

                    XrResult result = XR_SUCCESS;
                    CHECK((result = xrCreateAction(actionSet, &actionCreateInfo, &action)) == value.expectedResult);
                    if (result == XR_SUCCESS) {
                        CHECK(xrDestroyAction(action) == XR_SUCCESS);
                    }
                }
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

    static inline void TestXrSuggestInteractionProfileBindings(const FeatureSet& features)
    {
        Conformance::GlobalData& globalData = Conformance::GetGlobalData();

        FeatureSet globalFeatures;
        globalData.PopulateMinVersionAndEnabledExtensions(globalFeatures);
        SkipIfNotSatisfiable("xrSuggestInteractionProfileBindings", globalData, features);
        AutoBasicInstance instance(features, AutoBasicInstance::createSystemId);
        REQUIRE_MSG(instance != XR_NULL_HANDLE_CPP,
                    "If this (XrInstance creation) fails, ensure the runtime location is set and the runtime is started, if applicable.");
        REQUIRE_MSG(instance.systemId != XR_NULL_SYSTEM_ID,
                    "XrInstance SystemId creation failed. Does the runtime have hardware available?");

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
                    REQUIRE_RESULT(xrSuggestInteractionProfileBindings(GetGlobalData().invalidInstance, &bindings),
                                   XR_ERROR_HANDLE_INVALID);
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

            OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
            {
                bindings = XrInteractionProfileSuggestedBinding{};
                bindings.countSuggestedBindings = 1;
                bindings.suggestedBindings = &testBinding;
                REQUIRE_RESULT(xrSuggestInteractionProfileBindings(instance, &bindings), XR_ERROR_VALIDATION_FAILURE);
            }

            OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
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

                    if (!GetInteractionProfileAvailability(ipMetadata.Availability).IsSatisfiedBy(globalFeatures + features)) {
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
                    for (const auto& bindingPathData : ipMetadata.BindingPaths) {
                        CAPTURE(bindingPathData.Path);
                        CAPTURE(bindingPathData.Type);

                        if (!GetInteractionProfileAvailability(bindingPathData.Availability).IsSatisfiedBy(features + globalFeatures)) {
                            continue;
                        }

                        XrAction selectedAction;
                        switch (bindingPathData.Type) {
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

                        XrActionSuggestedBinding suggestedBindings{selectedAction, StringToPath(instance, bindingPathData.Path)};
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

    TEST_CASE("xrSuggestInteractionProfileBindings", "[actions]")
    {
        TestXrSuggestInteractionProfileBindings(FeatureSet(XR_API_VERSION_1_0));
    }
    TEST_CASE("xrSuggestInteractionProfileBindings_maintenance1", "[actions][XR_KHR_maintenance1]")
    {
        TestXrSuggestInteractionProfileBindings(
            FeatureSet({FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_KHR_maintenance1}));
    }
    TEST_CASE("xrSuggestInteractionProfileBindings_1_1", "[actions][XR_VERSION_1_1]")
    {
        TestXrSuggestInteractionProfileBindings(FeatureSet(XR_API_VERSION_1_1));
    }

    namespace
    {
        void TestBindingsAvailabilityUnderFeatureSet(const InteractionProfileAvailMetadata& ipMetadata, const FeatureSet& features)
        {
            INFO("Creating instance with these features:");
            CAPTURE(features);
            AutoBasicInstance instance(features, AutoBasicInstance::createSystemId);
            REQUIRE_MSG(
                instance != XR_NULL_HANDLE_CPP,
                "If this (XrInstance creation) fails, ensure the runtime location is set and the runtime is started, if applicable.");
            REQUIRE_MSG(instance.systemId != XR_NULL_SYSTEM_ID,
                        "XrInstance SystemId creation failed. Does the runtime have hardware available?");

            XrActionSet actionSet{XR_NULL_HANDLE};
            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
            strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);
            ActionSetScoped actionSetOwned{actionSet};

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

            for (const auto& bindingPathData : ipMetadata.BindingPaths) {
                CAPTURE(bindingPathData.Path);
                CAPTURE(bindingPathData.Type);

                XrAction selectedAction;
                switch (bindingPathData.Type) {
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

                XrActionSuggestedBinding suggestedBindings =
                    XrActionSuggestedBinding{selectedAction, StringToPath(instance, bindingPathData.Path)};
                XrInteractionProfileSuggestedBinding bindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                bindings.interactionProfile = StringToPath(instance, ipMetadata.InteractionProfilePathString);
                bindings.suggestedBindings = &suggestedBindings;
                bindings.countSuggestedBindings = 1;

                const Availability& bindingPathDataAvailability = GetInteractionProfileAvailability(bindingPathData.Availability);
                INFO("Testing binding path that requires the following availability to be satisfied:");
                CAPTURE(bindingPathDataAvailability);
                // note: for debugging, it may be useful to put a dynamic section here and sections in the caller
                // and to use -c to specify a series of sections. Note that this extends the test run duration a lot because
                // it means the test is re-run from the start for every path, but it makes breakpoints more useful.
                if (bindingPathDataAvailability.IsSatisfiedBy(features)) {
                    INFO("Instance features satisfy availability, expect success in suggesting binding.");
                    CHECK(xrSuggestInteractionProfileBindings(instance, &bindings) == XR_SUCCESS);
                }
                else {
                    INFO("Instance features do not satisfy availability, expect failure in suggesting binding.");
                    CHECK(xrSuggestInteractionProfileBindings(instance, &bindings) == XR_ERROR_PATH_UNSUPPORTED);
                }
            }
        }
    }  // namespace

    static inline void TestXrSuggestInteractionProfileBindings_avail(const FeatureSet& features)
    {
        GlobalData& globalData = GetGlobalData();
        FeatureSet globalFeatures;
        globalData.PopulateMinVersionAndEnabledExtensions(globalFeatures);

        SkipIfNotSatisfiable("xrSuggestInteractionProfileBindings_avail", globalData, features);

        FeatureSet baseFeatures = globalFeatures + features;

        SECTION("No specific extensions")
        {
            for (const InteractionProfileAvailMetadata& ipMetadata : GetAllInteractionProfiles()) {
                CAPTURE(ipMetadata.InteractionProfilePathString);
                bool profileAvailable = GetInteractionProfileAvailability(ipMetadata.Availability).IsSatisfiedBy(baseFeatures);
                DYNAMIC_SECTION(ipMetadata.InteractionProfileShortname << (profileAvailable ? " Expect Available" : " Expect Unavailable"))
                {
                    TestBindingsAvailabilityUnderFeatureSet(ipMetadata, baseFeatures);
                }
            }
        }
        SECTION("With non-default extensions")
        {
            FeatureSet available;
            globalData.PopulateMaxSupportedVersionAndAvailableExtensions(available);
            for (const InteractionProfileAvailMetadata& ipMetadata : GetAllInteractionProfiles()) {
                CAPTURE(ipMetadata.InteractionProfilePathString);
                FeatureSet requiredForProfile;
                if (!FindFeasibleFeatureSetFromAvailability(ipMetadata.Availability, available, baseFeatures, false, requiredForProfile)) {
                    continue;
                }

                bool havePathsWithExtraRequirements = false;
                FeatureSet requiredForBindings = requiredForProfile;
                for (const auto& bindingPathData : ipMetadata.BindingPaths) {
                    FeatureSet requiredForBinding;
                    if (GetInteractionProfileAvailability(bindingPathData.Availability).IsSatisfiedBy(requiredForProfile)) {
                        // no new extensions needed, skip here to avoid setting flag
                        continue;
                    }
                    if (FindFeasibleFeatureSetFromAvailability(bindingPathData.Availability, available, baseFeatures + requiredForBindings,
                                                               false, requiredForBinding)) {
                        requiredForBindings += requiredForBinding;
                        havePathsWithExtraRequirements = true;
                    }
                }
                DYNAMIC_SECTION(ipMetadata.InteractionProfileShortname)
                {
                    if (!GetInteractionProfileAvailability(ipMetadata.Availability).IsSatisfiedBy(baseFeatures)) {
                        // profile isn't available by default, but we found a way to enable it
                        INFO("With additional profile requirements enabled");
                        TestBindingsAvailabilityUnderFeatureSet(ipMetadata, requiredForProfile + baseFeatures);
                    }
                    if (havePathsWithExtraRequirements) {
                        // at least one binding isn't available by default, but we found a way to enable it
                        INFO("With additional binding path requirements enabled");
                        TestBindingsAvailabilityUnderFeatureSet(ipMetadata, requiredForBindings + baseFeatures);
                    }
                }
            }
        }
    }

    TEST_CASE("xrSuggestInteractionProfileBindings_avail", "")
    {
        TestXrSuggestInteractionProfileBindings_avail(FeatureSet(XR_API_VERSION_1_0));
    }
    TEST_CASE("xrSuggestInteractionProfileBindings_avail_maintenance1", "[XR_KHR_maintenance1]")
    {
        TestXrSuggestInteractionProfileBindings_avail(
            FeatureSet({FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_KHR_maintenance1}));
    }
    TEST_CASE("xrSuggestInteractionProfileBindings_avail_1_1", "[XR_VERSION_1_1]")
    {
        TestXrSuggestInteractionProfileBindings_avail(FeatureSet(XR_API_VERSION_1_1));
    }

    TEST_CASE("xrSuggestInteractionProfileBindings_interactive", "[actions][interactive]")
    {
        CompositionHelper compositionHelper("Suggest Bindings Interactive");
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
                             GetSimpleInteractionProfile().BindingPaths);

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
        compositionHelper.GetInteractionManager().AddActionBindings(bindings.interactionProfile, {{{selectActionB, selectPath}}});
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
            OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
            {
                XrSessionActionSetsAttachInfo attachInfoWithoutType = attachInfo;
                attachInfoWithoutType.type = (XrStructureType)0;
                attachInfoWithoutType.countActionSets = 1;
                attachInfoWithoutType.actionSets = &actionSet;
                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfoWithoutType), XR_ERROR_VALIDATION_FAILURE);
            }

            OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
            {
                XrSessionActionSetsAttachInfo attachInfoWithInvalidType = attachInfo;
                attachInfoWithInvalidType.type = XR_TYPE_ACTIONS_SYNC_INFO;
                attachInfoWithInvalidType.countActionSets = 1;
                attachInfoWithInvalidType.actionSets = &actionSet;
                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfoWithInvalidType), XR_ERROR_VALIDATION_FAILURE);
            }

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

                XrResult result = xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket));
                REQUIRE_THAT(result, In<XrResult>({XR_SESSION_NOT_FOCUSED, XR_ERROR_ACTIONSET_NOT_ATTACHED}));
                result = xrStopHapticFeedback(session, &hapticActionInfo);
                REQUIRE_THAT(result, In<XrResult>({XR_SESSION_NOT_FOCUSED, XR_ERROR_ACTIONSET_NOT_ATTACHED}));

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
                XrResult applyResult =
                    xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket));
                REQUIRE_RESULT_SUCCEEDED(applyResult);
                // will usually be 1.0 in combined testing, but if the user asks to test a higher
                // version, the current version will be that version, so we can use a CHECK.
                XrVersion minApiVersion = Options::Get().minApiVersionValue;
                static_assert(XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) == 1, "This code does not handle a major version upgrade");
                bool usingAtLeastVersion11 = XR_VERSION_MAJOR(minApiVersion) == 1 && XR_VERSION_MINOR(minApiVersion) >= 1;
                if (usingAtLeastVersion11) {
                    CHECK(applyResult == XR_SESSION_NOT_FOCUSED);
                }
                else if (applyResult != XR_SESSION_NOT_FOCUSED) {
                    WARN(
                        "Runtime should prefer XR_SESSION_NOT_FOCUSED over XR_SUCCESS when calling xrApplyHapticFeedback when the session is not focused.");
                }

                XrResult stopResult = xrStopHapticFeedback(session, &hapticActionInfo);
                REQUIRE_RESULT_SUCCEEDED(stopResult);
                if (usingAtLeastVersion11) {
                    CHECK(applyResult == XR_SESSION_NOT_FOCUSED);
                }
                else if (applyResult != XR_SESSION_NOT_FOCUSED) {
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
                REQUIRE_RESULT(xrEnumerateBoundSourcesForAction(session, &info, 0, &sourceCountOutput, nullptr),
                               XR_ERROR_ACTIONSET_NOT_ATTACHED);

                REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

                std::vector<XrPath> boundSourcesPaths =
                    REQUIRE_TWO_CALL(XrPath, {}, xrEnumerateBoundSourcesForAction, session.GetSession(), &info);
                sourceCountOutput = static_cast<uint32_t>(boundSourcesPaths.size());

                // should not get a null path, not really much else we can assert here.
                REQUIRE_THAT(boundSourcesPaths, !Catch::Matchers::VectorContains(XrPath{}));
                // Can we assert that we don't enumerate duplicates? Would be weird to return duplicates but may not be strictly forbidden.
                REQUIRE_THAT(boundSourcesPaths, VectorHasOnlyUniqueElements<XrPath>{});
            }
        }
        SECTION("Unattached action sets")
        {
            REQUIRE_RESULT(xrAttachSessionActionSets(session, &attachInfo), XR_SUCCESS);

            XrActionSet actionSet2{XR_NULL_HANDLE};
            strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name 2");
            strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name_2");
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet2), XR_SUCCESS);
            ActionSetScoped actionSetOwned{actionSet};

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

    static inline void TestXrSuggestInteractionProfileBindings_order(const FeatureSet& features)
    {
        GlobalData& globalData = GetGlobalData();

        FeatureSet globalFeatures;
        globalData.PopulateMinVersionAndEnabledExtensions(globalFeatures);

        SkipIfNotSatisfiable("xrSuggestInteractionProfileBindings_order", globalData, features);

        auto suggestBindingsAndGetCurrentInteractionProfile = [features, globalFeatures](bool reverse, bool nullPathExpected,
                                                                                         const std::string& topLevelPathString) {
            CompositionHelper compositionHelper("Suggest Bindings Order", features);
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
                if (!GetInteractionProfileAvailability(interactionProfile.Availability).IsSatisfiedBy(features + globalFeatures)) {
                    return;
                }
                std::string interactionProfileName = interactionProfile.InteractionProfilePathString;
                XrPath interactionProfilePath = StringToPath(instance, interactionProfileName);

                bool bindingSuggested = false;
                for (auto& bindingPathData : interactionProfile.BindingPaths) {
                    // We use the same pattern as the rest of the action conformance suite
                    // and only bind the boolean actions. Note that we bind "boolAction"
                    // to *every* boolean input, not just the first one.
                    if (bindingPathData.Type != XR_ACTION_TYPE_BOOLEAN_INPUT) {
                        continue;
                    }
                    if (!GetInteractionProfileAvailability(bindingPathData.Availability).IsSatisfiedBy(features + globalFeatures)) {
                        continue;
                    }
                    XrActionSuggestedBinding binding = {boolAction, StringToPath(instance, bindingPathData.Path)};
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
                                 GetSimpleInteractionProfile().BindingPaths, &features);

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
    TEST_CASE("xrSuggestInteractionProfileBindings_order", "[actions][interactive]")
    {
        TestXrSuggestInteractionProfileBindings_order(FeatureSet(XR_API_VERSION_1_0));
    }
    TEST_CASE("xrSuggestInteractionProfileBindings_order_maintenance1", "[actions][interactive][XR_KHR_maintenance1]")
    {
        TestXrSuggestInteractionProfileBindings_order(
            FeatureSet({FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_KHR_maintenance1}));
    }
    TEST_CASE("xrSuggestInteractionProfileBindings_order_1_1", "[actions][interactive][XR_VERSION_1_1]")
    {
        TestXrSuggestInteractionProfileBindings_order(FeatureSet(XR_API_VERSION_1_1));
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
                             GetSimpleInteractionProfile().BindingPaths);

        XrPath rightHandPath{StringToPath(instance, "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), rightHandPath,
                             GetSimpleInteractionProfile().BindingPaths);

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
                OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
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

                OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
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

        CompositionHelper compositionHelper("xrSyncActions");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile = StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString);

        std::string leftHandPathString = "/user/hand/left";
        XrPath leftHandPath{StringToPath(instance, "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, leftHandPath, GetSimpleInteractionProfile().BindingPaths);

        std::string rightHandPathString = "/user/hand/right";
        XrPath rightHandPath{StringToPath(instance, "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, rightHandPath, GetSimpleInteractionProfile().BindingPaths);

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

                    {
                        INFO("Repeated state query calls return the same value");

                        defaultInputDevice->SetButtonStateBool(selectPath, true);

                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);
                        REQUIRE(actionStateBoolean.currentState);

                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                        REQUIRE(actionStateBoolean.isActive);
                        REQUIRE(actionStateBoolean.currentState);
                    }

                    OPTIONAL_DISCONNECTABLE_DEVICE_SECTION
                    {
                        defaultInputDevice->SetDeviceActive(false, true, XR_NULL_HANDLE, XR_NULL_HANDLE, " and wait for 20s");

                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &actionStateBoolean), XR_SUCCESS);
                                REQUIRE(actionStateBoolean.isActive);
                                REQUIRE(actionStateBoolean.currentState);
                                return false;
                            },
                            20s, kActionWaitDelay);

                        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                        actionLayerManager.DisplayMessage("Wait for 5s");

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

                    INFO("Unbound and active action");
                    XrActiveActionSet activeAndUnboundActionActiveActionSet[] = {{actionSet}, {unboundActionActionSet}};
                    syncInfo.activeActionSets = activeAndUnboundActionActiveActionSet;
                    syncInfo.countActiveActionSets = 2;
                    unboundActionActiveActionSet.subactionPath = defaultDevicePath;
                    actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                    {
                        INFO("Unbound action state");
                        XrBoundSourcesForActionEnumerateInfo info{XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
                        info.action = unboundAction;
                        uint32_t count = 0;
                        REQUIRE_RESULT(xrEnumerateBoundSourcesForAction(session, &info, 0, &count, nullptr), XR_SUCCESS);
                        if (count == 0) {
                            XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
                            getInfo.action = unboundAction;
                            getInfo.subactionPath = defaultDevicePath;
                            REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &state), XR_SUCCESS);
                            REQUIRE(state.currentState == XR_FALSE);
                            REQUIRE(state.changedSinceLastSync == XR_FALSE);
                            REQUIRE(state.lastChangeTime == 0);
                        }
                        else {
                            // todo: Is it permissible for a runtime to always remap actions and not support unmapped actions at all?
                            WARN(
                                "Unbound action was actually bound! For this test \"test select action 4\" / \"test_select_action_4\" should not be remapped by the runtime");
                        }
                    }

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

    static void xrSyncActions_priorityTest(const FeatureSet& featureSet)
    {
        GlobalData& globalData = GetGlobalData();

        SkipIfNotSatisfiable("xrSyncActions_priorityTest", globalData, featureSet);
        const bool EXT_active_action_set_priority_enabled = featureSet.Get(FeatureBitIndex::BIT_XR_EXT_active_action_set_priority);

        CompositionHelper compositionHelper("xrSyncActions", featureSet);
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        ActionLayerManager actionLayerManager(compositionHelper);

        XrPath simpleControllerInteractionProfile = StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString);

        XrPath leftHandPath{StringToPath(instance, "/user/hand/left")};
        std::shared_ptr<IInputTestDevice> leftHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, leftHandPath, GetSimpleInteractionProfile().BindingPaths, &featureSet);

        XrPath rightHandPath{StringToPath(instance, "/user/hand/right")};
        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, rightHandPath, GetSimpleInteractionProfile().BindingPaths, &featureSet);

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

        compositionHelper.BeginSession();

        actionLayerManager.WaitForSessionFocusWithMessage();

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
            return booleanData.isActive == XR_TRUE;
        };

        if (globalData.leftHandUnderTest && globalData.rightHandUnderTest) {
            {
                // Both sets with null subaction path
                std::array<XrActiveActionSet, 4> activeSets = {{lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet,
                                                                highPriorityLeftHandActiveActionSet, highPriorityRightHandActiveActionSet}};

                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
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

            // Now run the same test again - using both hands using active action set
            if (EXT_active_action_set_priority_enabled) {
                {
                    std::vector<XrActiveActionSetPriorityEXT> actionSetPriorities;
                    XrActiveActionSetPrioritiesEXT activeActionSetPriorities{XR_TYPE_ACTIVE_ACTION_SET_PRIORITIES_EXT};

                    // Both sets with priorities swapped
                    std::array<XrActiveActionSet, 4> activeSets = {{lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet,
                                                                    highPriorityLeftHandActiveActionSet,
                                                                    highPriorityRightHandActiveActionSet}};
                    actionSetPriorities = {{highPriorityActionSet, 2}, {lowPriorityActionSet, 3}};

                    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
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
                }

                {
                    std::vector<XrActiveActionSetPriorityEXT> actionSetPriorities;
                    XrActiveActionSetPrioritiesEXT activeActionSetPriorities{XR_TYPE_ACTIVE_ACTION_SET_PRIORITIES_EXT};

                    // Both sets with equal priorities
                    std::array<XrActiveActionSet, 4> activeSets = {{lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet,
                                                                    highPriorityLeftHandActiveActionSet,
                                                                    highPriorityRightHandActiveActionSet}};
                    actionSetPriorities = {{highPriorityActionSet, 2}, {lowPriorityActionSet, 2}};

                    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                    syncInfo.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
                    syncInfo.activeActionSets = activeSets.data();
                    activeActionSetPriorities.actionSetPriorityCount = static_cast<uint32_t>(actionSetPriorities.size());
                    activeActionSetPriorities.actionSetPriorities = actionSetPriorities.data();
                    syncInfo.next = &activeActionSetPriorities;
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
        }

        if (globalData.rightHandUnderTest) {
            // Both sets with right hand subaction path
            std::array<XrActiveActionSet, 2> activeSets = {{highPriorityRightHandActiveActionSet, lowPriorityRightHandActiveActionSet}};

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
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
            std::array<XrActiveActionSet, 2> activeSets = {{highPriorityLeftHandActiveActionSet, lowPriorityLeftHandActiveActionSet}};

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
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
            {
                // Both sets with differing subaction path
                std::array<XrActiveActionSet, 2> activeSets = {{highPriorityRightHandActiveActionSet, lowPriorityLeftHandActiveActionSet}};

                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
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
            }
            {
                // Both sets with differing subaction path
                std::array<XrActiveActionSet, 2> activeSets = {{highPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet}};
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
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
            }
            {
                // Both sets with differing subaction path
                std::array<XrActiveActionSet, 3> activeSets = {
                    {highPriorityRightHandActiveActionSet, lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet}};
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
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
            }
            {
                // Both sets with differing subaction path
                std::array<XrActiveActionSet, 3> activeSets = {
                    {highPriorityRightHandActiveActionSet, lowPriorityLeftHandActiveActionSet, lowPriorityRightHandActiveActionSet}};
                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
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
        }
    }

    // In theory enabling XR_EXT_active_action_set_priority should not change the
    // runtime behavior unless the extension is enabled. But we are going to run
    // the test with and without the extension to be sure.
    TEST_CASE("xrSyncActions_priority_rules", "[actions][interactive]")
    {
        const auto kCoreRequirements = FeatureSet{};
        xrSyncActions_priorityTest(kCoreRequirements);
    }

    TEST_CASE("xrSyncActions_priority_rules_EXT_active_action_set_priority", "[actions][interactive]")
    {
        const auto kExtensionRequirements = FeatureSet{FeatureBitIndex::BIT_XR_EXT_active_action_set_priority};
        xrSyncActions_priorityTest(kExtensionRequirements);
    }

    namespace
    {
        bool canBeExercised(const InteractionProfileAvailMetadata& ipMetadata, const BindingPathData& bindingPathData)
        {
            if (bindingPathData.systemOnly) {
                return false;
            }
            if (strcmp(ipMetadata.InteractionProfileShortname, "oculus/touch_controller") == 0 &&
                ends_with(bindingPathData.Path, "/input/thumbrest/touch")) {
                // Rift S and Quest 1 controllers lack thumbrests.
                return false;
            }
            if (strcmp(ipMetadata.InteractionProfileShortname, "ext/hand_interaction_ext") == 0 &&
                ends_with(bindingPathData.Path, "/input/aim_activate_ext/value")) {
                // aim_activate_ext/value does not require that the values coming back
                // actually be floats, they can also be boolean so this is hard for
                // us to exercise.
                return false;
            }
            return true;
        }
    }  // namespace

    namespace
    {
        struct ParentPathToTest
        {
            // one or both of click and value will be populated
            const BindingPathData* clickPathData{nullptr};
            XrAction clickAction{XR_NULL_HANDLE};
            const BindingPathData* valuePathData{nullptr};
            XrAction valueAction{XR_NULL_HANDLE};
            // actions on the parent, both of these will be populated
            XrAction booleanAction{XR_NULL_HANDLE};
            XrAction floatAction{XR_NULL_HANDLE};
        };

        bool HasBoolFloatBindingPath(const InteractionProfileAvailMetadata* ipMetadata)
        {
            for (const BindingPathData& bindingPathData : ipMetadata->BindingPaths) {
                if (bindingPathData.Type == XR_ACTION_TYPE_BOOLEAN_INPUT || bindingPathData.Type == XR_ACTION_TYPE_FLOAT_INPUT) {
                    return true;
                }
            }
            return false;
        }

        // Make a map of all the testable /click and /value paths, on a
        // particular profile / user path, keyed by the path with that suffix
        // stripped. In some cases, both may exist, so we store both in one
        // struct so they can be handled as a unit. In addition to identifying
        // these paths, this function also populates actions for /click and/or
        // /value, as well as a boolean and float action on the parent path.
        // (Both are always created, as if one of /click or /value doesn't
        // exist, the other will be used with coercion.)
        //
        // (Uses ordered map because order may be visible, so it's nice to make
        // it consistent.)
        std::map<std::string, ParentPathToTest> SetupParentComponentTest(const InteractionProfileAvailMetadata& ipMetadata,
                                                                         const std::string& topLevelUserPathString,
                                                                         const FeatureSet& profileAndOverallRequirements,
                                                                         XrInstance instance, InteractionManager& interactionManager,
                                                                         XrActionSet actionSet, bool exercisableOnly)
        {
            std::map<std::string, ParentPathToTest> pathsByParent;

            XrPath topLevelUserPath{StringToPath(instance, topLevelUserPathString.data())};
            XrPath interactionProfilePath = StringToPath(instance, ipMetadata.InteractionProfilePathString);

            uint32_t uniqueActionNameCounter = 0;
            auto GetActionNames = [&uniqueActionNameCounter]() mutable -> std::tuple<std::string, std::string> {
                uniqueActionNameCounter++;
                return std::tuple<std::string, std::string>{"parent_component_test_action_" + std::to_string(uniqueActionNameCounter),
                                                            "parent component test action " + std::to_string(uniqueActionNameCounter)};
            };

            // Fill pathsByParent with all /click and /value paths.
            // We will need to know the existence of both for each parent path we test later.
            for (const BindingPathData& bindingPathData : ipMetadata.BindingPaths) {
                INFO("child binding path: " << bindingPathData.Path);

                // First, we filter out non-boolean/float paths, paths out of scope, paths we can't test, and unavailable paths.

                if (bindingPathData.Type != XR_ACTION_TYPE_BOOLEAN_INPUT && bindingPathData.Type != XR_ACTION_TYPE_FLOAT_INPUT) {
                    continue;
                }
                if (!starts_with(bindingPathData.Path, topLevelUserPathString)) {
                    continue;
                }
                if (exercisableOnly && !canBeExercised(ipMetadata, bindingPathData)) {
                    continue;
                }
                auto pathRequirements = GetInteractionProfileAvailability(bindingPathData.Availability);
                if (!pathRequirements.IsSatisfiedBy(profileAndOverallRequirements)) {
                    continue;
                }

                // Then, we check for the suffixes we care about, and put a suffix-stripped copy into parentPath.

                bool isClick = ends_with(bindingPathData.Path, "/click");
                bool isValue = ends_with(bindingPathData.Path, "/value");

                std::string parentPath;
                if (isClick) {
                    parentPath = {bindingPathData.Path, strlen(bindingPathData.Path) - strlen("/click")};
                }
                else if (isValue) {
                    parentPath = {bindingPathData.Path, strlen(bindingPathData.Path) - strlen("/value")};
                }
                else {
                    continue;
                }

                // Now we know we care about this path, so we add it to the map if it isn't.

                auto res = pathsByParent.insert({std::move(parentPath), {}});
                auto it = res.first;
                const std::string& parentPathRef = it->first;
                ParentPathToTest& pathToTest = it->second;
                auto inserted = res.second;

                // Lastly, we create the action for the child path (with e.g. /click),
                // and if it hasn't been done already, both a float and a boolean action
                // for the parent path. These are returned in the struct.

                XrPath childBindingPath = StringToPath(instance, bindingPathData.Path);
                XrPath parentBindingPath = StringToPath(instance, parentPathRef);

                XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
                actionCreateInfo.countSubactionPaths = 1;
                actionCreateInfo.subactionPaths = &topLevelUserPath;

                if (isClick) {
                    pathToTest.clickPathData = &bindingPathData;

                    actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                    auto actionNames = GetActionNames();
                    strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                    strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &pathToTest.clickAction), XR_SUCCESS);
                    interactionManager.AddActionBindings(interactionProfilePath, {{pathToTest.clickAction, childBindingPath}});
                }
                if (isValue) {
                    pathToTest.valuePathData = &bindingPathData;

                    actionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                    auto actionNames = GetActionNames();
                    strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                    strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &pathToTest.valueAction), XR_SUCCESS);
                    interactionManager.AddActionBindings(interactionProfilePath, {{pathToTest.valueAction, childBindingPath}});
                }

                // we set up actions for each parent path once
                if (inserted) {
                    actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                    auto actionNames = GetActionNames();
                    strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                    strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &pathToTest.booleanAction), XR_SUCCESS);

                    actionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                    actionNames = GetActionNames();
                    strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                    strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &pathToTest.floatAction), XR_SUCCESS);

                    interactionManager.AddActionBindings(interactionProfilePath, {{pathToTest.booleanAction, parentBindingPath}});
                    interactionManager.AddActionBindings(interactionProfilePath, {{pathToTest.floatAction, parentBindingPath}});
                }
            }

            return pathsByParent;
        }
    }  // namespace

    namespace UnseenValue
    {
        constexpr float cStepSize = 0.5f;
        const int32_t cStepSizeOffset = -int32_t(std::roundf(-1.f / cStepSize));

        class Tracker
        {
        public:
            Tracker() = default;

            void PopulateBool()
            {
                assert(m_actionType == 0);
                m_actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                // Need to see 0 1
                m_unseenValues = {false, true};
            }

            bool HandleBool(const BindingPathData& data, XrActionStateBoolean booleanState)
            {
                assert(m_actionType == XR_ACTION_TYPE_BOOLEAN_INPUT);
                auto key = int32_t(booleanState.currentState);
                return See(data, key);
            }

            void PopulateFloat()
            {
                assert(m_actionType == 0);
                m_actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                // Need to see normalized [0..2] 0 1 2
                for (float f = 0.f; f <= 1.f; f += cStepSize) {
                    m_unseenValues.insert(int32_t(std::roundf(f / cStepSize)));
                }
            }

            bool HandleFloat(const BindingPathData& data, XrActionStateFloat floatState)
            {
                assert(m_actionType == XR_ACTION_TYPE_FLOAT_INPUT);
                auto key = int32_t(std::roundf(floatState.currentState / cStepSize));
                return See(data, key);
            }

            void PopulateVector2f()
            {
                assert(m_actionType == 0);
                m_actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
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
                        m_unseenValues.insert(i + j * 10);
                    }
                }
            }

            bool HandleVector2f(const BindingPathData& data, XrActionStateVector2f vectorState)
            {
                assert(m_actionType == XR_ACTION_TYPE_VECTOR2F_INPUT);
                auto i = int32_t(std::roundf(vectorState.currentState.x / cStepSize)) + cStepSizeOffset;
                auto j = int32_t(std::roundf(vectorState.currentState.y / cStepSize)) + cStepSizeOffset;
                auto key = i + j * 10;
                return See(data, key);
            }

            std::string GetPrompt(const BindingPathData& data) const
            {
                // If we've seen all the values, leave the prompt empty
                if (m_unseenValues.empty()) {
                    return "";
                }

                // Format the prompt as \n/path/to/component:\n[unseen value 1] [unseen value 2]
                //
                // This can result in messages that aren't intuitive, like:
                // \n/user/hand/right/input/trigger/value:\ntrue
                // but this is because m_actionType may not match data.Type,
                // requiring a coercion by the runtime.
                std::string nextActionPrompt = "\n" + std::string(data.Path) + ":\n";
                auto fmt_float = [](float v) -> std::string {
                    auto s = std::to_string(v);
                    if (s.length() > 4)
                        s.resize(4);
                    return s;
                };
                for (auto remainingKeys : m_unseenValues) {
                    switch (m_actionType) {
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
                        WARN("Unexpected action type " << m_actionType);
                        break;
                    }
                }

                return nextActionPrompt;
            }

            size_t UnseenCount() const
            {
                return m_unseenValues.size();
            }
            size_t SeenCount() const
            {
                return m_seenCount;
            }

#if !defined(NDEBUG)
            const std::set<int32_t>& DebugGetKeys() const
            {
                return m_unseenValues;
            }
#endif

        private:
            bool See(const BindingPathData& data, int32_t key)
            {
                (void)data;  // not always used
                // Remove the key if it's never been seen.
                if (m_unseenValues.count(key) > 0) {
                    m_unseenValues.erase(key);
                    ++m_seenCount;
#if !defined(NDEBUG)
                    ReportF("%s saw %d", data.Path, key);
#endif
                    return true;
                }
                return false;
            }
            XrActionType m_actionType{XrActionType(0)};
            std::set<int32_t> m_unseenValues;
            uint32_t m_seenCount;
        };

        class Synthesizer
        {
        public:
            void Advance()
            {
                // Use cStepSize / 2 to generate {-1.0, -0.5, 0.0, 0.5, 1.0} for x, y in GetVector2f.
                m_synthesizedX += cStepSize / 2.f;
                if (m_synthesizedX > 1.f) {
                    m_synthesizedX = 0.f;
                    m_synthesizedY += cStepSize / 2.f;
                    if (m_synthesizedY > 1.f) {
                        m_synthesizedY = 0.f;
                    }
                }
            }

            bool GetBool()
            {
                return m_synthesizedX > 0.5f;
            }
            float GetFloat()
            {
                return m_synthesizedX;
            }
            XrVector2f GetVector2f()
            {
                float x = (m_synthesizedX - 0.5f) * 2.f;
                float y = (m_synthesizedY - 0.5f) * 2.f;
                return {x, y};
            }

        private:
            // Synthetic values for automation (these loop around by cStepSize in x then y order).
            float m_synthesizedX{0.f};
            float m_synthesizedY{0.f};
        };
    }  // namespace UnseenValue

    // Purpose: Verify that if a parent component and the child component it
    // should be mapped to are both attached, that the isActive of the parent
    // component and that of the component it should be mapped to match. This is
    // tested over five frames in which any path was active to increase the
    // opportunity for failures.
    //
    // See ParentComponentsValues for a more detailed explanation.
    TEST_CASE("ParentComponentsBindState", "[actions][interactive]")
    {
        GlobalData& globalData = GetGlobalData();

        FeatureSet enabled;
        globalData.PopulateMinVersionAndEnabledExtensions(enabled);
        FeatureSet available;
        globalData.PopulateMaxSupportedVersionAndAvailableExtensions(available);

        auto TestParentComponentsOfProfile = [](const InteractionProfileAvailMetadata& ipMetadata,
                                                const std::string& topLevelUserPathString,
                                                const FeatureSet& profileAndOverallRequirements) {
            CompositionHelper compositionHelper("Parent Components Bind State", profileAndOverallRequirements);
            XrInstance instance = compositionHelper.GetInstance();
            XrSession session = compositionHelper.GetSession();
            InteractionManager& interactionManager = compositionHelper.GetInteractionManager();

            compositionHelper.BeginSession();

            ActionLayerManager actionLayerManager(compositionHelper);
            actionLayerManager.WaitForSessionFocusWithMessage();

            XrPath topLevelUserPath{StringToPath(instance, topLevelUserPathString.data())};
            XrPath interactionProfilePath = StringToPath(instance, ipMetadata.InteractionProfilePathString);

            std::shared_ptr<IInputTestDevice> inputDevice =
                CreateTestDevice(&actionLayerManager, &interactionManager, instance, session, interactionProfilePath, topLevelUserPath,
                                 ipMetadata.BindingPaths, &profileAndOverallRequirements);

            XrActionSet actionSet{XR_NULL_HANDLE};
            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
            strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

            std::map<std::string, ParentPathToTest> pathsByParent = SetupParentComponentTest(
                ipMetadata, topLevelUserPathString, profileAndOverallRequirements, instance, interactionManager, actionSet, false);

            if (pathsByParent.empty()) {
                ReportF("Skipping %s as no candidate /click or /value paths were found", ipMetadata.InteractionProfileShortname);
                return;
            }

            interactionManager.AddActionSet(actionSet);
            interactionManager.AttachActionSets();

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            XrActiveActionSet activeActionSet{actionSet};
            syncInfo.activeActionSets = &activeActionSet;
            syncInfo.countActiveActionSets = 1;

            inputDevice->SetDeviceActiveWithoutWaiting(/*state = */ true);

            const std::chrono::seconds bindingWaitTime = 30s;  // matches SetDeviceActive
            const auto timeoutTime = std::chrono::system_clock::now() + bindingWaitTime;

            const int requiredActiveIterations = 5;
            int activeIterations = 0;
            while (activeIterations < requiredActiveIterations && std::chrono::system_clock::now() < timeoutTime) {
                XrBool32 anyPathWasActive = XR_FALSE;
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                for (auto& it : pathsByParent) {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    const ParentPathToTest& pathToTest = it.second;

                    INFO("parent path: " << it.first);

                    getInfo.action = pathToTest.booleanAction;
                    XrActionStateBoolean booleanState{XR_TYPE_ACTION_STATE_BOOLEAN};
                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                    anyPathWasActive |= booleanState.isActive;

                    getInfo.action = pathToTest.floatAction;
                    XrActionStateFloat floatState{XR_TYPE_ACTION_STATE_FLOAT};
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                    anyPathWasActive |= floatState.isActive;

                    if (pathToTest.clickAction != XR_NULL_HANDLE) {
                        getInfo.action = pathToTest.clickAction;
                        XrActionStateBoolean clickState{XR_TYPE_ACTION_STATE_BOOLEAN};
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &clickState), XR_SUCCESS);
                        anyPathWasActive |= clickState.isActive;
                        REQUIRE(booleanState.isActive == clickState.isActive);
                        if (pathToTest.valueAction == XR_NULL_HANDLE) {
                            REQUIRE(floatState.isActive == clickState.isActive);
                            // otherwise, .../value handles float
                        }
                    }
                    if (pathToTest.valueAction != XR_NULL_HANDLE) {
                        getInfo.action = pathToTest.valueAction;
                        XrActionStateFloat valueState{XR_TYPE_ACTION_STATE_FLOAT};
                        REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &valueState), XR_SUCCESS);
                        anyPathWasActive |= valueState.isActive;
                        REQUIRE(floatState.isActive == valueState.isActive);
                        if (pathToTest.clickAction == XR_NULL_HANDLE) {
                            REQUIRE(booleanState.isActive == valueState.isActive);
                            // otherwise, .../click handles boolean
                        }
                    }
                }
                if (anyPathWasActive) {
                    activeIterations++;
                }
            }
            if (activeIterations < requiredActiveIterations) {
                if (activeIterations == 0) {
                    FAIL("Timeout waiting for any binding to become active");
                }
                FAIL("Only had " << activeIterations << " iterations with active bindings before timeout,"
                                 << " require " << requiredActiveIterations << " to pass");
            }
        };

        const std::string leftHandString{"/user/hand/left"};
        const std::string rightHandString{"/user/hand/right"};
        FeatureSet required{enabled};
        std::vector<const InteractionProfileAvailMetadata*> enabledProfiles;
        // Looping over all profiles means we do not have to de-duplicate the command line args
        // We combine all interaction profiles because we may be able to exercise paths
        // that would not be exercised if we only enabled the requirements of one at a time
        for (const InteractionProfileAvailMetadata& ipMetadata : GetAllInteractionProfiles()) {
            if (!IsInteractionProfileEnabled(ipMetadata.InteractionProfileShortname)) {
                continue;
            }
            enabledProfiles.push_back(&ipMetadata);
            FeatureSet requiredForProfile;
            REQUIRE(FindFeasibleFeatureSetFromAvailability(ipMetadata.Availability, available, required, true, requiredForProfile));
            required += requiredForProfile;
        }

        const bool leftHandUnderTest = globalData.leftHandUnderTest;
        const bool rightHandUnderTest = globalData.rightHandUnderTest;
        // `required` now contains all extensions needed for all listed interaction profiles.
        // (But not any extra top level /user paths.)
        for (const InteractionProfileAvailMetadata* ipMetadata : enabledProfiles) {
            for (const auto& topLevelUserPathInfo : ipMetadata->TopLevelPaths) {
                const char* const topLevelUserPathString = topLevelUserPathInfo.first;
                if (!GetInteractionProfileAvailability(topLevelUserPathInfo.second).IsSatisfiedBy(required)) {
                    ReportF("Skipping %s on %s - top level /user path not available", ipMetadata->InteractionProfileShortname,
                            topLevelUserPathString);
                    continue;
                }
                if (!HasBoolFloatBindingPath(ipMetadata)) {
                    ReportF("Skipping %s as no boolean or float paths are supported", ipMetadata->InteractionProfileShortname);
                    continue;
                }
                if ((topLevelUserPathString == leftHandString && !leftHandUnderTest) ||
                    (topLevelUserPathString == rightHandString && !rightHandUnderTest)) {
                    continue;
                }

                INFO(ipMetadata->InteractionProfileShortname);
                TestParentComponentsOfProfile(*ipMetadata, topLevelUserPathString, required);
            }
        }
    }

    // Purpose: Verify that if a parent component and the child component it
    // should be mapped to are both attached, tests that the values reported by
    // the parent and the component it should be mapped to match (including
    // specified coercions).
    //
    // According to the spec, if a path would be valid if you appended /click or
    // /value, then that "parent" path is treated as if it were the path with
    // that appended. If both /click and /value exist, there is preference for
    // selecting /click for boolean actions and /value for float actions.
    //
    // Specifically, we wait for (and ask for) the user to enter, on each parent
    // path, at least two states for each action, i.e. both false and true on
    // the boolean action, and near two of 0.0, 0.5, and 1.0 on the float
    // action. During this time we validate that the returned values match as
    // required by the spec.
    TEST_CASE("ParentComponentsValues", "[actions][interactive]")
    {
        GlobalData& globalData = GetGlobalData();

        FeatureSet enabled;
        globalData.PopulateMinVersionAndEnabledExtensions(enabled);
        FeatureSet available;
        globalData.PopulateMaxSupportedVersionAndAvailableExtensions(available);

        auto TestParentComponentsOfProfile = [](const InteractionProfileAvailMetadata& ipMetadata,
                                                const std::string& topLevelUserPathString,
                                                const FeatureSet& profileAndOverallRequirements) {
            CompositionHelper compositionHelper("Parent Components Values", profileAndOverallRequirements);
            XrInstance instance = compositionHelper.GetInstance();
            XrSession session = compositionHelper.GetSession();
            InteractionManager& interactionManager = compositionHelper.GetInteractionManager();

            compositionHelper.BeginSession();

            ActionLayerManager actionLayerManager(compositionHelper);
            actionLayerManager.WaitForSessionFocusWithMessage();

            XrPath topLevelUserPath{StringToPath(instance, topLevelUserPathString.data())};
            XrPath interactionProfilePath = StringToPath(instance, ipMetadata.InteractionProfilePathString);

            std::shared_ptr<IInputTestDevice> inputDevice =
                CreateTestDevice(&actionLayerManager, &interactionManager, instance, session, interactionProfilePath, topLevelUserPath,
                                 ipMetadata.BindingPaths, &profileAndOverallRequirements);

            XrActionSet actionSet{XR_NULL_HANDLE};
            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetCreateInfo.localizedActionSetName, "test action set localized name");
            strcpy(actionSetCreateInfo.actionSetName, "test_action_set_name");
            REQUIRE_RESULT(xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet), XR_SUCCESS);

            std::map<std::string, ParentPathToTest> rawPathsByParent = SetupParentComponentTest(
                ipMetadata, topLevelUserPathString, profileAndOverallRequirements, instance, interactionManager, actionSet, true);

            if (rawPathsByParent.empty()) {
                ReportF("Skipping %s as no candidate /click or /value paths were found", ipMetadata.InteractionProfileShortname);
                return;
            }

            // To ensure all paths are exercised, and to provide hints to the
            // user of any paths they have not exercised, transform the map
            // above into a vector with tracking of which states we have not
            // seen. Once we have seen two on each, the test has passed.
            struct TrackedParentPath
            {
                std::string parentPath;
                ParentPathToTest pathToTest;
                UnseenValue::Tracker unseenBoolStates;
                UnseenValue::Tracker unseenFloatStates;
            };
            std::vector<TrackedParentPath> trackedPaths;
            for (const auto& pair : rawPathsByParent) {
                TrackedParentPath trackedPath = {pair.first, pair.second};
                trackedPath.unseenBoolStates.PopulateBool();
                trackedPath.unseenFloatStates.PopulateFloat();
                trackedPaths.push_back(trackedPath);
            }

            interactionManager.AddActionSet(actionSet);
            interactionManager.AttachActionSets();

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            XrActiveActionSet activeActionSet{actionSet};
            syncInfo.activeActionSets = &activeActionSet;
            syncInfo.countActiveActionSets = 1;

            inputDevice->SetDeviceActiveWithoutWaiting(/*state = */ true);

            std::chrono::seconds bindingWaitTime = 30s;      // matches SetDeviceActive
            const std::chrono::seconds stallWaitTime = 30s;  // time to wait between novel inputs
            auto timeoutTime = std::chrono::system_clock::now() + bindingWaitTime;

            UnseenValue::Synthesizer valueSynthesizer;

            XrBool32 anyPathHasBeenActive = XR_FALSE;
            std::set<std::string> satisfiedTrackedPaths{};
            while (true) {
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                bool gotNewInput = false;
                std::string nextActionPrompt = "";

                auto updateNextActionPrompt = [&nextActionPrompt](const UnseenValue::Tracker& tracker, const BindingPathData& data) {
                    if (nextActionPrompt.empty() && tracker.SeenCount() < 2) {
                        nextActionPrompt = tracker.GetPrompt(data);
                    }
                };

                for (TrackedParentPath& trackedPath : trackedPaths) {
                    if (satisfiedTrackedPaths.count(trackedPath.parentPath) > 0) {
                        continue;
                    }

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    const ParentPathToTest& pathToTest = trackedPath.pathToTest;

                    INFO("parent path: " << trackedPath.parentPath);

                    getInfo.action = pathToTest.booleanAction;
                    XrActionStateBoolean booleanState{XR_TYPE_ACTION_STATE_BOOLEAN};
                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                    anyPathHasBeenActive |= booleanState.isActive;
                    if (booleanState.isActive) {
                        gotNewInput |= trackedPath.unseenBoolStates.HandleBool(
                            pathToTest.clickAction != XR_NULL_HANDLE ? *pathToTest.clickPathData : *pathToTest.valuePathData, booleanState);
                    }

                    getInfo.action = pathToTest.floatAction;
                    XrActionStateFloat floatState{XR_TYPE_ACTION_STATE_FLOAT};
                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                    anyPathHasBeenActive |= floatState.isActive;
                    if (floatState.isActive) {
                        gotNewInput |= trackedPath.unseenFloatStates.HandleFloat(
                            pathToTest.valueAction != XR_NULL_HANDLE ? *pathToTest.valuePathData : *pathToTest.clickPathData, floatState);
                    }

                    // If there is a child /click action, the boolean action on
                    // the parent must have the same value, and if there is no
                    // /value action, the float action on the parent must be 0.0
                    // or 1.0 if /click is false or true respectively.
                    if (pathToTest.clickAction != XR_NULL_HANDLE) {
                        getInfo.action = pathToTest.clickAction;
                        XrActionStateBoolean clickState{XR_TYPE_ACTION_STATE_BOOLEAN};
                        REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &clickState), XR_SUCCESS);
                        anyPathHasBeenActive |= clickState.isActive;
                        REQUIRE(booleanState.isActive == clickState.isActive);
                        REQUIRE(booleanState.currentState == clickState.currentState);
                        updateNextActionPrompt(trackedPath.unseenBoolStates, *pathToTest.clickPathData);
                        if (pathToTest.valueAction == XR_NULL_HANDLE) {
                            REQUIRE(floatState.isActive == clickState.isActive);
                            REQUIRE(floatState.currentState == (clickState.currentState * 1.0f));
                            updateNextActionPrompt(trackedPath.unseenFloatStates, *pathToTest.clickPathData);
                            // otherwise, .../value handles float
                        }
                    }

                    // If there is a child /value action, the float action on
                    // the parent must have the same value, and if there is no
                    // /click action, the boolean action on the parent must be
                    // based on a thresholding of that float value, however, it
                    // should use hysteresis, so we cannot easily assert
                    // anything here.
                    if (pathToTest.valueAction != XR_NULL_HANDLE) {
                        getInfo.action = pathToTest.valueAction;
                        XrActionStateFloat valueState{XR_TYPE_ACTION_STATE_FLOAT};
                        REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &valueState), XR_SUCCESS);
                        anyPathHasBeenActive |= valueState.isActive;
                        REQUIRE(floatState.isActive == valueState.isActive);
                        REQUIRE(floatState.currentState == valueState.currentState);
                        updateNextActionPrompt(trackedPath.unseenFloatStates, *pathToTest.valuePathData);
                        if (pathToTest.clickAction == XR_NULL_HANDLE) {
                            REQUIRE(booleanState.isActive == (valueState.isActive));
                            updateNextActionPrompt(trackedPath.unseenBoolStates, *pathToTest.valuePathData);
                            // otherwise, .../click handles boolean
                        }
                    }

                    // consider it passed as long as every path has had multiple seen states
                    if (trackedPath.unseenBoolStates.SeenCount() >= 2 && trackedPath.unseenFloatStates.SeenCount() >= 2) {
                        satisfiedTrackedPaths.insert(trackedPath.parentPath);
                    }
                }

                if (satisfiedTrackedPaths.size() == trackedPaths.size()) {
                    break;
                }

                // For automation only, drive inputs through a set of legal values that should cover all cases required by UnseenValueTracker.
                {
                    valueSynthesizer.Advance();

                    for (TrackedParentPath& trackedPath : trackedPaths) {
                        if (trackedPath.pathToTest.clickAction != XR_NULL_HANDLE) {
                            inputDevice->SetButtonStateBool(StringToPath(instance, trackedPath.pathToTest.clickPathData->Path),
                                                            valueSynthesizer.GetBool(), true);
                        }
                        if (trackedPath.pathToTest.valueAction != XR_NULL_HANDLE) {
                            inputDevice->SetButtonStateFloat(StringToPath(instance, trackedPath.pathToTest.valuePathData->Path),
                                                             valueSynthesizer.GetFloat(), 0, true);
                        }
                    }
                }

                if (gotNewInput) {
                    timeoutTime = std::chrono::system_clock::now() + stallWaitTime;
                    continue;
                }
                if (std::chrono::system_clock::now() >= timeoutTime) {
                    if (!anyPathHasBeenActive) {
                        FAIL("Timeout waiting for any binding to become active");
                    }
                    FAIL("Timeout waiting for inputs. All required inputs were satisfied on " << satisfiedTrackedPaths.size() << "/"
                                                                                              << trackedPaths.size() << " paths.");
                }
                if (nextActionPrompt.empty()) {
                    nextActionPrompt = "[unknown next action]";  // possible CTS bug
                }
                std::string prompt = "Used " + std::to_string(satisfiedTrackedPaths.size()) + "/" + std::to_string(trackedPaths.size()) +
                                     " inputs on:\n" + topLevelUserPathString + nextActionPrompt;
                actionLayerManager.DisplayMessage(prompt);
            }
        };

        const std::string leftHandString{"/user/hand/left"};
        const std::string rightHandString{"/user/hand/right"};
        FeatureSet required{enabled};
        std::vector<const InteractionProfileAvailMetadata*> enabledProfiles;
        // Looping over all profiles means we do not have to de-duplicate the command line args
        // We combine all interaction profiles because we may be able to exercise paths
        // that would not be exercised if we only enabled the requirements of one at a time
        for (const InteractionProfileAvailMetadata& ipMetadata : GetAllInteractionProfiles()) {
            if (!IsInteractionProfileEnabled(ipMetadata.InteractionProfileShortname)) {
                continue;
            }
            enabledProfiles.push_back(&ipMetadata);
            FeatureSet requiredForProfile;
            REQUIRE(FindFeasibleFeatureSetFromAvailability(ipMetadata.Availability, available, required, true, requiredForProfile));
            required += requiredForProfile;
        }

        const bool leftHandUnderTest = globalData.leftHandUnderTest;
        const bool rightHandUnderTest = globalData.rightHandUnderTest;
        // `required` now contains all extensions needed for all listed interaction profiles.
        // (But not any extra top level /user paths.)
        for (const InteractionProfileAvailMetadata* ipMetadata : enabledProfiles) {
            for (const auto& topLevelUserPathInfo : ipMetadata->TopLevelPaths) {
                const char* const topLevelUserPathString = topLevelUserPathInfo.first;
                if (!GetInteractionProfileAvailability(topLevelUserPathInfo.second).IsSatisfiedBy(required)) {
                    ReportF("Skipping %s on %s - top level /user path not available", ipMetadata->InteractionProfileShortname,
                            topLevelUserPathString);
                    continue;
                }
                if (!HasBoolFloatBindingPath(ipMetadata)) {
                    ReportF("Skipping %s as no boolean or float paths are supported", ipMetadata->InteractionProfileShortname);
                    continue;
                }
                if ((topLevelUserPathString == leftHandString && !leftHandUnderTest) ||
                    (topLevelUserPathString == rightHandString && !rightHandUnderTest)) {
                    continue;
                }

                INFO(ipMetadata->InteractionProfileShortname);
                TestParentComponentsOfProfile(*ipMetadata, topLevelUserPathString, required);
            }
        }
    }

    TEST_CASE("StateQueryFunctionsInteractive", "[actions][interactive][gamepad]")
    {
        struct ActionInfo
        {
            BindingPathData Data;
            XrAction Action{XR_NULL_HANDLE};
            XrAction XAction{XR_NULL_HANDLE};  // Set if type is vector2f
            XrAction YAction{XR_NULL_HANDLE};  // Set if type is vector2f
            UnseenValue::Tracker UnseenValues;
        };

        constexpr float cEpsilon = 0.1f;
        constexpr float cLargeEpsilon = 0.15f;
        GlobalData& globalData = GetGlobalData();
        FeatureSet enabled;
        globalData.PopulateMinVersionAndEnabledExtensions(enabled);
        FeatureSet available;
        globalData.PopulateMaxSupportedVersionAndAvailableExtensions(available);

        auto TestInteractionProfile = [&](const InteractionProfileAvailMetadata& ipMetadata, const std::string& topLevelUserPathString,
                                          const FeatureSet& profileAndOverallRequirements) {
            // If the profile has additional required extensions, they should have been enabled in `required`.
            auto availability = GetInteractionProfileAvailability(ipMetadata.Availability);
            REQUIRE(availability.IsSatisfiedBy(profileAndOverallRequirements));
            XrInstance instance{};
            REQUIRE_RESULT_SUCCEEDED(CreateBasicInstance(&instance, profileAndOverallRequirements));
            INFO("Instance created with " + profileAndOverallRequirements.ToString());

            // for ownership and auto-destruction.
            AutoBasicInstance autoInstance(instance, 0);
            CompositionHelper compositionHelper("Input device state query", instance, (XrViewConfigurationType)0, false,
                                                CompositionHelper::EnvironmentBlendModePreference::PreferPassthrough);
            XrSession session = compositionHelper.GetSession();
            compositionHelper.BeginSession();
            ActionLayerManager actionLayerManager(compositionHelper);

            actionLayerManager.WaitForSessionFocusWithMessage();

            XrPath interactionProfilePath = StringToPath(instance, ipMetadata.InteractionProfilePathString);
            XrPath topLevelUserPath{StringToPath(instance, topLevelUserPathString.data())};
            std::shared_ptr<IInputTestDevice> inputDevice =
                CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session, interactionProfilePath,
                                 topLevelUserPath, ipMetadata.BindingPaths, &profileAndOverallRequirements);

            XrActionSet actionSet{XR_NULL_HANDLE};

            std::string actionSetName = "state_query_test_action_set_" + std::to_string(topLevelUserPath);
            std::string localizedActionSetName = "State Query Test Action Set " + std::to_string(topLevelUserPath);

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

            auto shouldExercisePath = [&ipMetadata, &profileAndOverallRequirements](const BindingPathData& bindingPathData) -> bool {
                if (!canBeExercised(ipMetadata, bindingPathData)) {
                    return false;
                }
                auto pathRequirements = GetInteractionProfileAvailability(bindingPathData.Availability);
                if (!pathRequirements.IsSatisfiedBy(profileAndOverallRequirements)) {
                    // TODO should we test a path that would require enabling more stuff?
                    return false;
                }
                return true;
            };

            auto BindingPathDataForTopLevelUserPath = [&]() {
                std::vector<BindingPathData> ret;
                for (const BindingPathData& bindingPathData : ipMetadata.BindingPaths) {
                    if (!starts_with(bindingPathData.Path, topLevelUserPathString)) {
                        continue;
                    }
                    ret.push_back(bindingPathData);
                }
                return ret;
            };
            auto ActionsForTopLevelUserPath = [&](XrActionType type) -> std::vector<ActionInfo> {
                auto bindingPathDataList = BindingPathDataForTopLevelUserPath();
                std::vector<ActionInfo> actions;
                for (const BindingPathData& bindingPathData : bindingPathDataList) {
                    if (type != bindingPathData.Type) {
                        continue;
                    }
                    if (!shouldExercisePath(bindingPathData)) {
                        continue;
                    }

                    // Skip /x or /y components since we handle those with the parent Vector2f.
                    const std::string pathString = bindingPathData.Path;
                    std::cmatch bindingPathRegexMatch;
                    REQUIRE_MSG(std::regex_match(pathString.data(), bindingPathRegexMatch, cInteractionSourcePathRegex),
                                "binding path does not match required format");
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
                    actionCreateInfo.actionType = bindingPathData.Type;
                    auto actionNames = GetActionNames();
                    strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                    strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                    REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

                    XrPath bindingPath = StringToPath(instance, bindingPathData.Path);
                    compositionHelper.GetInteractionManager().AddActionBindings(interactionProfilePath, {{action, bindingPath}});

                    ActionInfo info{};

                    switch (bindingPathData.Type) {
                    case XR_ACTION_TYPE_BOOLEAN_INPUT:
                        info.UnseenValues.PopulateBool();
                        break;
                    case XR_ACTION_TYPE_FLOAT_INPUT:
                        info.UnseenValues.PopulateFloat();
                        break;
                    case XR_ACTION_TYPE_VECTOR2F_INPUT: {
                        info.UnseenValues.PopulateVector2f();

                        // If we have a vector action, we must have /x and /y float actions.
                        actionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                        actionNames = GetActionNames();
                        strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                        strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &xAction), XR_SUCCESS);

                        std::string xSubBindingPath = std::string(bindingPathData.Path) + "/x";
                        bindingPath = StringToPath(instance, xSubBindingPath);
                        compositionHelper.GetInteractionManager().AddActionBindings(interactionProfilePath, {{xAction, bindingPath}});

                        actionNames = GetActionNames();
                        strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                        strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                        REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &yAction), XR_SUCCESS);

                        std::string ySubBindingPath = std::string(bindingPathData.Path) + "/y";
                        bindingPath = StringToPath(instance, ySubBindingPath);
                        compositionHelper.GetInteractionManager().AddActionBindings(interactionProfilePath, {{yAction, bindingPath}});
                        break;
                    }
                    case XR_ACTION_TYPE_POSE_INPUT:
                    case XR_ACTION_TYPE_VIBRATION_OUTPUT:
                        break;
                    case XR_ACTION_TYPE_MAX_ENUM:
                    default:
                        WARN("Unexpected action type " << bindingPathData.Type);
                        break;
                    }

#if !defined(NDEBUG)
                    // Debug UnseenValues
                    std::string unseen = bindingPathData.Path;
                    for (auto key : info.UnseenValues.DebugGetKeys()) {
                        unseen += " " + std::to_string(key);
                    }
                    ReportF("Keys for %s", unseen.c_str());
#endif

                    info.Data = bindingPathData;
                    info.Action = action;
                    info.XAction = xAction;
                    info.YAction = yAction;

                    actions.push_back(info);
                }

                return actions;
            };
            auto ActionsForTopLevelPathCoerced = [&](XrActionType type, XrActionType coercionType) -> std::vector<ActionInfo> {
                auto bindingPathDataList = BindingPathDataForTopLevelUserPath();

                auto HasSubpathOfType = [&](std::string parentPath, XrActionType type) {
                    for (const BindingPathData& bindingPathData : bindingPathDataList) {
                        if (bindingPathData.Type != type) {
                            continue;
                        }
                        if (!shouldExercisePath(bindingPathData)) {
                            continue;
                        }
                        auto prefixedByParentPath = starts_with(bindingPathData.Path, parentPath);
                        if (prefixedByParentPath) {
                            return true;
                        }
                    }
                    return false;
                };

                std::vector<ActionInfo> actions;
                for (const BindingPathData& bindingPathData : bindingPathDataList) {
                    if (type != bindingPathData.Type) {
                        continue;
                    }
                    if (!shouldExercisePath(bindingPathData)) {
                        continue;
                    }

                    // If we are using the parent path, the runtime should map it if there is a subpath
                    // e.g. .../thumbstick may get bound to .../thumbstick/click which is valid
                    const std::string pathString = bindingPathData.Path;
                    std::cmatch bindingPathRegexMatch;
                    REQUIRE_MSG(std::regex_match(pathString.data(), bindingPathRegexMatch, cInteractionSourcePathRegex),
                                "binding path does not match required format");
                    if (bindingPathRegexMatch[5].matched) {
                        if (coercionType == XR_ACTION_TYPE_BOOLEAN_INPUT &&
                            HasSubpathOfType(bindingPathData.Path, XR_ACTION_TYPE_BOOLEAN_INPUT)) {
                            continue;
                        }
                        if (coercionType == XR_ACTION_TYPE_FLOAT_INPUT &&
                            HasSubpathOfType(bindingPathData.Path, XR_ACTION_TYPE_FLOAT_INPUT)) {
                            continue;
                        }
                        if (coercionType == XR_ACTION_TYPE_POSE_INPUT &&
                            HasSubpathOfType(bindingPathData.Path, XR_ACTION_TYPE_POSE_INPUT)) {
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

                    XrPath bindingPath = StringToPath(instance, bindingPathData.Path);
                    compositionHelper.GetInteractionManager().AddActionBindings(interactionProfilePath, {{action, bindingPath}});

                    ActionInfo info{};
                    info.Data = bindingPathData;
                    info.Data.Type = coercionType;
                    info.Action = action;
                    actions.push_back(info);
                }

                return actions;
            };
            auto ActionOfTypeForTopLevelPath = [&](XrActionType type) -> ActionInfo {
                auto bindingPathDataList = BindingPathDataForTopLevelUserPath();

                XrAction action{XR_NULL_HANDLE};
                XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
                actionCreateInfo.actionType = type;
                auto actionNames = GetActionNames();
                strcpy(actionCreateInfo.localizedActionName, std::get<1>(actionNames).c_str());
                strcpy(actionCreateInfo.actionName, std::get<0>(actionNames).c_str());
                REQUIRE_RESULT(xrCreateAction(actionSet, &actionCreateInfo, &action), XR_SUCCESS);

                for (const BindingPathData& bindingPathData : bindingPathDataList) {
                    if (type != bindingPathData.Type) {
                        continue;
                    }
                    if (!shouldExercisePath(bindingPathData)) {
                        continue;
                    }

                    XrPath bindingPath = StringToPath(instance, bindingPathData.Path);
                    compositionHelper.GetInteractionManager().AddActionBindings(interactionProfilePath, {{action, bindingPath}});
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
            auto booleanActions = ActionsForTopLevelUserPath(XR_ACTION_TYPE_BOOLEAN_INPUT);
            auto floatActions = ActionsForTopLevelUserPath(XR_ACTION_TYPE_FLOAT_INPUT);
            auto vectorActions = ActionsForTopLevelUserPath(XR_ACTION_TYPE_VECTOR2F_INPUT);
            auto poseActions = ActionsForTopLevelUserPath(XR_ACTION_TYPE_POSE_INPUT);
            auto hapticActions = ActionsForTopLevelUserPath(XR_ACTION_TYPE_VIBRATION_OUTPUT);

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
            REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, topLevelUserPath, &interactionProfileState), XR_SUCCESS);
            REQUIRE(interactionProfilePath == interactionProfileState.interactionProfile);

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

                actionLayerManager.DisplayMessage("Use all controller inputs on\n" + topLevelUserPathString);

                if (!GetGlobalData().IsUsingConformanceAutomation()) {
                    actionLayerManager.Sleep_For(1s);
                }

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

                // Synthetic values for automation
                UnseenValue::Synthesizer valueSynthesizer;
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
                        auto updateForAction = [&](const ActionInfo& actionInfo) {
                            // by doing this action by action, prompts for the same action will be suggested one after another
                            if (nextActionPrompt.empty()) {
                                nextActionPrompt = actionInfo.UnseenValues.GetPrompt(actionInfo.Data);
                            }
                            if (actionInfo.UnseenValues.UnseenCount() == 0) {
                                seenActions.insert(actionInfo.Action);
                            }
                        };

                        for (auto& actionInfo : booleanActions) {
                            getInfo.action = actionInfo.Action;
                            REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                            if (booleanState.isActive) {
                                actionInfo.UnseenValues.HandleBool(actionInfo.Data, booleanState);
                                updateForAction(actionInfo);

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
                                actionInfo.UnseenValues.HandleFloat(actionInfo.Data, floatState);
                                updateForAction(actionInfo);

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
                                actionInfo.UnseenValues.HandleVector2f(actionInfo.Data, vectorState);
                                updateForAction(actionInfo);

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

                        // For automation only, drive inputs through a set of legal values that should cover all cases required by UnseenValueTracker.
                        {
                            valueSynthesizer.Advance();
                            for (const auto& actionInfo : booleanActions) {
                                inputDevice->SetButtonStateBool(StringToPath(instance, actionInfo.Data.Path), valueSynthesizer.GetBool(),
                                                                true);
                            }

                            for (const auto& actionInfo : floatActions) {
                                inputDevice->SetButtonStateFloat(StringToPath(instance, actionInfo.Data.Path), valueSynthesizer.GetFloat(),
                                                                 0, true);
                            }

                            for (const auto& actionInfo : vectorActions) {
                                inputDevice->SetButtonStateVector2(StringToPath(instance, actionInfo.Data.Path),
                                                                   valueSynthesizer.GetVector2f(), 0, true);
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
                                             " inputs on:\n" + topLevelUserPathString + waitForCombined + nextActionPrompt;
                        actionLayerManager.DisplayMessage(prompt);

                        return false;
                    },
                    600s, kActionWaitDelay);

                REQUIRE(seenActions.size() == actionCount);
                REQUIRE_FALSE(waitForCombinedBools);
                REQUIRE_FALSE(waitForCombinedFloats);
                REQUIRE_FALSE(waitForCombinedVectors);

                actionLayerManager.DisplayMessage("Release all inputs");
                if (!GetGlobalData().IsUsingConformanceAutomation()) {
                    actionLayerManager.Sleep_For(2s);
                }
            }

            OPTIONAL_DISCONNECTABLE_DEVICE_SECTION
            {
                INFO("Pose state query");

                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};

                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                for (const auto& poseActionData : poseActions) {
                    CAPTURE(poseActionData.Data.Path);
                    getInfo.action = poseActionData.Action;
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_SUCCESS);
                    REQUIRE(poseState.isActive);
                }

                inputDevice->SetDeviceActive(false);
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                for (const auto& poseActionData : poseActions) {
                    CAPTURE(poseActionData.Data.Path);
                    getInfo.action = poseActionData.Action;
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &poseState), XR_SUCCESS);
                    REQUIRE_FALSE(poseState.isActive);
                }

                inputDevice->SetDeviceActive(true);
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

                for (const auto& poseActionData : poseActions) {
                    CAPTURE(poseActionData.Data.Path);
                    getInfo.action = poseActionData.Action;
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

                        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                        hapticActionInfo.action = hapticActionData.Action;

                        XrHapticVibration hapticPacket{XR_TYPE_HAPTIC_VIBRATION};
                        hapticPacket.amplitude = 1;
                        hapticPacket.frequency = XR_FREQUENCY_UNSPECIFIED;
                        hapticPacket.duration = XR_MIN_HAPTIC_DURATION;

                        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};

                        std::vector<ActionInfo> selectedBooleanActions;
                        std::copy_if(std::begin(booleanActions), std::end(booleanActions), std::back_inserter(selectedBooleanActions),
                                     [&](const ActionInfo& action) {
                                         const std::string path = std::string(action.Data.Path);
                                         const std::string suffix = "/click";
                                         return path.size() >= suffix.size() &&
                                                path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
                                     });

                        bool foundClickActions = selectedBooleanActions.size() > 0;
                        if (!foundClickActions) {
                            // At time of writing, only hand and eye interaction profiles have no path ending in /click.
                            selectedBooleanActions = booleanActions;
                        }

                        XrPath bindingPath = StringToPath(instance, selectedBooleanActions[0].Data.Path);

                        XrAction currentBooleanAction{XR_NULL_HANDLE};
                        auto GetBooleanButtonState = [&]() -> bool {
                            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                            for (const auto& booleanActionData : selectedBooleanActions) {
                                getInfo.action = booleanActionData.Action;
                                REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                                if (booleanState.changedSinceLastSync && booleanState.currentState) {
                                    currentBooleanAction = booleanActionData.Action;
                                    return true;
                                }
                            }
                            return false;
                        };

                        std::string buttonPromptPrefix =
                            foundClickActions ? "Activate any boolean .../click action " : "Activate any boolean action ";
                        std::string buttonPromptSuffix = "";
                        if (foundClickActions) {
                            std::vector<XrAction> actions;
                            std::transform(selectedBooleanActions.begin(), selectedBooleanActions.end(), std::back_inserter(actions),
                                           [](ActionInfo& action) { return action.Action; });
                            std::string localizedActions = actionLayerManager.ListActionsLocalized(syncInfo, actions, ", ", "; on ", ": ");
                            buttonPromptSuffix = ", e.g.:\non " + localizedActions;
                        }

                        std::string prompt = buttonPromptPrefix + "when you feel the 3 second haptic vibration" + buttonPromptSuffix;
                        actionLayerManager.DisplayMessage(prompt);
                        actionLayerManager.IterateFrame();

                        if (!GetGlobalData().IsUsingConformanceAutomation()) {
                            actionLayerManager.Sleep_For(3s);
                        }

                        hapticPacket.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(3s).count();
                        REQUIRE_RESULT(
                            xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                            XR_SUCCESS);

                        {
                            // For automation only
                            inputDevice->SetButtonStateBool(bindingPath, false, true);
                            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                            inputDevice->SetButtonStateBool(bindingPath, true, true);
                        }
                        currentBooleanAction = XR_NULL_HANDLE;
                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.DisplayMessage(prompt);
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                return GetBooleanButtonState();
                            },
                            15s, kActionWaitDelay);
                        REQUIRE_FALSE(currentBooleanAction == XR_NULL_HANDLE);

                        {
                            // For automation only
                            inputDevice->SetButtonStateBool(bindingPath, false, true);
                        }

                        REQUIRE_RESULT(xrStopHapticFeedback(session, &hapticActionInfo), XR_SUCCESS);

                        actionLayerManager.IterateFrame();
                        if (!GetGlobalData().IsUsingConformanceAutomation()) {
                            actionLayerManager.Sleep_For(0.25s);
                        }

                        prompt = buttonPromptPrefix + "when you feel the short haptic pulse" + buttonPromptSuffix;
                        actionLayerManager.DisplayMessage(prompt);
                        actionLayerManager.IterateFrame();
                        if (!GetGlobalData().IsUsingConformanceAutomation()) {
                            actionLayerManager.Sleep_For(3s);
                        }

                        hapticPacket.duration = XR_MIN_HAPTIC_DURATION;
                        REQUIRE_RESULT(
                            xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket)),
                            XR_SUCCESS);

                        {
                            inputDevice->SetButtonStateBool(bindingPath, false, true);
                            actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                            inputDevice->SetButtonStateBool(bindingPath, true, true);
                        }
                        currentBooleanAction = XR_NULL_HANDLE;
                        WaitUntilPredicateWithTimeout(
                            [&]() {
                                actionLayerManager.DisplayMessage(prompt);
                                actionLayerManager.GetRenderLoop().IterateFrame();
                                return GetBooleanButtonState();
                            },
                            15s, kActionWaitDelay);
                        REQUIRE_FALSE(currentBooleanAction == XR_NULL_HANDLE);

                        {
                            // For automation only
                            inputDevice->SetButtonStateBool(bindingPath, false, true);
                        }
                    }

                    actionLayerManager.DisplayMessage("Release all inputs");
                    if (!GetGlobalData().IsUsingConformanceAutomation()) {
                        actionLayerManager.Sleep_For(2s);
                    }
                }
            }

            INFO("Action value coercion");
            {
                INFO("Boolean->Float");
                for (const auto& booleanToFloatActionData : floatActionsCoercedToBoolean) {
                    CAPTURE(booleanToFloatActionData.Data.Path);
                    XrPath bindingPath = StringToPath(instance, booleanToFloatActionData.Data.Path);

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = booleanToFloatActionData.Action;

                    inputDevice->SetButtonStateFloat(bindingPath, 0.0f, cEpsilon, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE_FALSE(booleanState.currentState);

                    inputDevice->SetButtonStateFloat(bindingPath, 1.0f, cEpsilon, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE(booleanState.currentState);
                    REQUIRE(booleanState.lastChangeTime > 0);

                    inputDevice->SetButtonStateFloat(bindingPath, 0.0f, cEpsilon, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateBoolean(session, &getInfo, &booleanState), XR_SUCCESS);
                    REQUIRE(booleanState.isActive);
                    REQUIRE_FALSE(booleanState.currentState);
                    REQUIRE(booleanState.lastChangeTime > 0);
                }

                INFO("Float->Boolean");
                for (const auto& floatToBooleanActionData : booleanActionsCoercedToFloat) {
                    CAPTURE(floatToBooleanActionData.Data.Path);
                    XrPath bindingPath = StringToPath(instance, floatToBooleanActionData.Data.Path);

                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = floatToBooleanActionData.Action;

                    inputDevice->SetButtonStateBool(bindingPath, false, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                    REQUIRE(floatState.isActive);
                    REQUIRE(floatState.currentState == Catch::Approx(0.0f).margin(cLargeEpsilon));

                    inputDevice->SetButtonStateBool(bindingPath, true, false, actionSet);

                    REQUIRE_RESULT(xrGetActionStateFloat(session, &getInfo, &floatState), XR_SUCCESS);
                    REQUIRE(floatState.isActive);
                    REQUIRE(floatState.currentState == Catch::Approx(1.0f).margin(cLargeEpsilon));
                    REQUIRE(floatState.lastChangeTime > 0);

                    inputDevice->SetButtonStateBool(bindingPath, false, false, actionSet);

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
        FeatureSet required{enabled};
        std::vector<const InteractionProfileAvailMetadata*> enabledProfiles;
        // Looping over all profiles means we do not have to de-duplicate the command line args
        // We combine all interaction profiles because we may be able to exercise paths
        // that would not be exercised if we only enabled the requirements of one at a time
        for (const InteractionProfileAvailMetadata& ipMetadata : GetAllInteractionProfiles()) {
            if (!IsInteractionProfileEnabled(ipMetadata.InteractionProfileShortname)) {
                continue;
            }
            enabledProfiles.push_back(&ipMetadata);
            FeatureSet requiredForProfile;
            REQUIRE(FindFeasibleFeatureSetFromAvailability(ipMetadata.Availability, available, required, true, requiredForProfile));
            required += requiredForProfile;
        }

        const bool leftHandUnderTest = globalData.leftHandUnderTest;
        const bool rightHandUnderTest = globalData.rightHandUnderTest;
        // `required` now contains all extensions needed for all listed interaction profiles.
        // (But not any extra top level /user paths.)
        for (const InteractionProfileAvailMetadata* ipMetadata : enabledProfiles) {
            for (const auto& topLevelUserPathInfo : ipMetadata->TopLevelPaths) {
                const char* const topLevelUserPathString = topLevelUserPathInfo.first;
                if (!GetInteractionProfileAvailability(topLevelUserPathInfo.second).IsSatisfiedBy(required)) {
                    ReportF("Skipping %s on %s - top level /user path not available", ipMetadata->InteractionProfileShortname,
                            topLevelUserPathString);
                    continue;
                }
                if ((topLevelUserPathString == leftHandString && !leftHandUnderTest) ||
                    (topLevelUserPathString == rightHandString && !rightHandUnderTest)) {
                    continue;
                }
                ReportF("Testing interaction profile %s for %s", ipMetadata->InteractionProfileShortname, topLevelUserPathString);
                TestInteractionProfile(*ipMetadata, topLevelUserPathString, required);
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
                    // This XrPath is potentially invalid and potentially unsupported.
                    getInfo.subactionPath = (XrPath)0x1234;
                    getInfo.action = booleanAction;
                    XrResult result = xrGetActionStateBoolean(session, &getInfo, &booleanState);
                    REQUIRE_THAT(result, In<XrResult>({XR_ERROR_PATH_INVALID, XR_ERROR_PATH_UNSUPPORTED}));

                    getInfo.action = floatAction;
                    result = xrGetActionStateFloat(session, &getInfo, &floatState);
                    REQUIRE_THAT(result, In<XrResult>({XR_ERROR_PATH_INVALID, XR_ERROR_PATH_UNSUPPORTED}));

                    getInfo.action = vectorAction;
                    result = xrGetActionStateVector2f(session, &getInfo, &vectorState);
                    REQUIRE_THAT(result, In<XrResult>({XR_ERROR_PATH_INVALID, XR_ERROR_PATH_UNSUPPORTED}));

                    getInfo.action = poseAction;
                    result = xrGetActionStatePose(session, &getInfo, &poseState);
                    REQUIRE_THAT(result, In<XrResult>({XR_ERROR_PATH_INVALID, XR_ERROR_PATH_UNSUPPORTED}));

                    hapticActionInfo.subactionPath = getInfo.subactionPath;
                    result = xrApplyHapticFeedback(session, &hapticActionInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticPacket));
                    REQUIRE_THAT(result, In<XrResult>({XR_ERROR_PATH_INVALID, XR_ERROR_PATH_UNSUPPORTED}));
                    result = xrStopHapticFeedback(session, &hapticActionInfo);
                    REQUIRE_THAT(result, In<XrResult>({XR_ERROR_PATH_INVALID, XR_ERROR_PATH_UNSUPPORTED}));
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

    TEST_CASE("action_space_creation-noninteractive", "[actions]")
    {
        CompositionHelper compositionHelper("action_space_creation-noni");
        compositionHelper.BeginSession();
        XrSession session = compositionHelper.GetSession();

        ActionLayerManager actionLayerManager(compositionHelper);

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
        createInfo.countSubactionPaths = 0;
        createInfo.subactionPaths = nullptr;
        REQUIRE_RESULT(xrCreateAction(actionSet, &createInfo, &poseAction), XR_SUCCESS);

        XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        spaceCreateInfo.poseInActionSpace = Pose::Identity;
        spaceCreateInfo.action = poseAction;
        XrSpace space{XR_NULL_HANDLE};

        OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
        {
            XrActionSpaceCreateInfo spaceCreateInfoWithoutType = spaceCreateInfo;
            spaceCreateInfoWithoutType.type = (XrStructureType)0;

            REQUIRE_RESULT(xrCreateActionSpace(session, &spaceCreateInfoWithoutType, &space), XR_ERROR_VALIDATION_FAILURE);
        }

        OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
        {
            XrActionSpaceCreateInfo spaceCreateInfoWithInvalidType = spaceCreateInfo;
            spaceCreateInfoWithInvalidType.type = XR_TYPE_ACTIONS_SYNC_INFO;

            REQUIRE_RESULT(xrCreateActionSpace(session, &spaceCreateInfoWithInvalidType, &space), XR_ERROR_VALIDATION_FAILURE);
        }
    }

    TEST_CASE("action_space_creation_pre_suggest", "[actions][interactive]")
    {
        bool useLeftHand = GetGlobalData().leftHandUnderTest;

        // Creates two ActionSpaces
        // - one is created before xrSuggestInteractionProfileBindings and
        // - the other is created after.
        // These two action spaces should both return (the same) valid data.
        CompositionHelper compositionHelper("action_space_create presuggest");
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
        earlySpaceCreateInfo.poseInActionSpace = Pose::Identity;
        earlySpaceCreateInfo.action = poseAction;
        XrSpace earlyActionSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(session, &earlySpaceCreateInfo, &earlyActionSpace), XR_SUCCESS);

        std::shared_ptr<IInputTestDevice> handInputDevice = CreateTestDevice(
            &actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session, simpleControllerInteractionProfile,
            StringToPath(instance, useLeftHand ? "/user/hand/left" : "/user/hand/right"), GetSimpleInteractionProfile().BindingPaths);

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        compositionHelper.GetInteractionManager().AddActionBindings(
            simpleControllerInteractionProfile,
            {{poseAction, StringToPath(instance, useLeftHand ? "/user/hand/left/input/grip/pose" : "/user/hand/right/input/grip/pose")}});
        compositionHelper.GetInteractionManager().AttachActionSets();

        // Create an ActionSpace after xrSuggestInteractionProfileBindings
        XrActionSpaceCreateInfo lateSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        lateSpaceCreateInfo.poseInActionSpace = Pose::Identity;
        lateSpaceCreateInfo.action = poseAction;
        XrSpace lateActionSpace{XR_NULL_HANDLE};
        REQUIRE_RESULT(xrCreateActionSpace(session, &lateSpaceCreateInfo, &lateActionSpace), XR_SUCCESS);

        actionLayerManager.WaitForSessionFocusWithMessage();

        XrSpace localSpace{XR_NULL_HANDLE};
        XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        createSpaceInfo.poseInReferenceSpace = Pose::Identity;
        REQUIRE_RESULT(xrCreateReferenceSpace(session, &createSpaceInfo, &localSpace), XR_SUCCESS);

        handInputDevice->SetDeviceActive(true);

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        XrActiveActionSet activeActionSet{actionSet};
        syncInfo.activeActionSets = &activeActionSet;
        actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);

        XrSpaceLocation earlyLocation{XR_TYPE_SPACE_LOCATION};
        XrSpaceLocation lateLocation{XR_TYPE_SPACE_LOCATION};
        REQUIRE(actionLayerManager.WaitForLocatability(useLeftHand ? "left" : "right", lateActionSpace, localSpace, &lateLocation, true));

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
                             simpleControllerInteractionProfile, leftHandPath, GetSimpleInteractionProfile().BindingPaths);

        std::shared_ptr<IInputTestDevice> rightHandInputDevice =
            CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                             simpleControllerInteractionProfile, rightHandPath, GetSimpleInteractionProfile().BindingPaths);

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

        OPTIONAL_DISCONNECTABLE_DEVICE_SECTION
        {
            XrSpaceVelocity leftVelocity{XR_TYPE_SPACE_VELOCITY};
            XrSpaceVelocity rightVelocity{XR_TYPE_SPACE_VELOCITY};
            XrSpaceLocation leftRelation{XR_TYPE_SPACE_LOCATION, &leftVelocity};
            XrSpaceLocation rightRelation{XR_TYPE_SPACE_LOCATION, &rightVelocity};

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.countActiveActionSets = 2;
            syncInfo.activeActionSets = bothSets;

            if (globalData.leftHandUnderTest && globalData.rightHandUnderTest) {
                // two-handed tests
                // If we tell them to place the controllers somewhere they don't move,
                // we can compare poses to verify identity.

                leftHandInputDevice->SetDeviceActive(false);
                actionLayerManager.DisplayMessage("Place left controller somewhere static but trackable");
                if (!GetGlobalData().IsUsingConformanceAutomation()) {
                    actionLayerManager.Sleep_For(5s);
                }
                // wait to lose left
                REQUIRE(actionLayerManager.WaitForLocatability("left", leftSpace, localSpace, &leftRelation, false));
                leftHandInputDevice->SetDeviceActive(true);
                actionLayerManager.SyncActionsUntilFocusWithMessage(syncInfo);
                // wait to gain left
                REQUIRE(actionLayerManager.WaitForLocatability("left", leftSpace, localSpace, &leftRelation, true));

                rightHandInputDevice->SetDeviceActive(false);
                actionLayerManager.DisplayMessage(
                    "Place right controller somewhere static but trackable. Keep left controller on and trackable.");
                if (!GetGlobalData().IsUsingConformanceAutomation()) {
                    actionLayerManager.Sleep_For(5s);
                }
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
                REQUIRE(currentRelation.pose == Pose::Approx(leftRelation.pose));
                REQUIRE_FALSE(Pose::Approx(leftRelation.pose) == rightRelation.pose);

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
                if (!GetGlobalData().IsUsingConformanceAutomation()) {
                    actionLayerManager.Sleep_For(5s);
                }

                // Tries to locate the controller space, and returns the location flags.
                auto checkTrackingFlags = [&]() -> XrSpaceLocationFlags {
                    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
                    location.locationFlags = 0;
                    location.pose = Pose::Identity;

                    REQUIRE_RESULT(xrLocateSpace(actionSpaceWithSubactionPath, localSpace,
                                                 actionLayerManager.GetRenderLoop().GetLastPredictedDisplayTime(), &location),
                                   XR_SUCCESS);
                    return location.locationFlags;
                };

                // Gets the action state for the pose action, and returns whether isActive is XR_TRUE.
                auto getActionStatePoseActive = [&] {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = poseAction;
                    getInfo.subactionPath = controllerSubactionPath;

                    XrActionStatePose statePose{XR_TYPE_ACTION_STATE_POSE};
                    REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &statePose), XR_SUCCESS);
                    return statePose.isActive == XR_TRUE;
                };

                // Does not call xrSyncActions nor does it wait, so the pose action state should remain inactive until we sync
                {
                    INFO("Off controller should not have active pose state");
                    REQUIRE(getActionStatePoseActive() == false);
                }
                availableInputDevice->SetDeviceActiveWithoutWaiting(
                    true, "\nSlowly move the controller for 20 seconds to prevent it from going idle and becoming inactive.");
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

                    REQUIRE(Pose::Approx(relationWithoutSubactionPath.pose) == relationWithSubactionPath.pose);
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

                        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                        getInfo.action = poseAction;
                        getInfo.subactionPath = controllerSubactionPath;

                        XrActionStatePose statePose{XR_TYPE_ACTION_STATE_POSE};
                        REQUIRE_RESULT(xrGetActionStatePose(session, &getInfo, &statePose), XR_SUCCESS);
                        REQUIRE(statePose.isActive == XR_FALSE);
                    }
                    {
                        INFO(
                            "xrGetActionStatePose with subactionPath empty: must be inactive"
                            " (previously bound controller now off, and no other controller to bind to)");

                        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                        getInfo.action = poseAction;
                        getInfo.subactionPath = XR_NULL_PATH;

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
                             GetSimpleInteractionProfile().BindingPaths);

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

                // should not get a null path, not really much else we can assert here.
                REQUIRE_THAT(enumerateResult, !Catch::Matchers::VectorContains(XrPath{}));
                // Can we assert that we don't enumerate duplicates? Would be weird to return duplicates but may not be strictly forbidden.
                REQUIRE_THAT(enumerateResult, VectorHasOnlyUniqueElements<XrPath>{});

                XrInputSourceLocalizedNameGetInfo getInfo{XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
                getInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT;
                getInfo.sourcePath = enumerateResult[0];

                uint32_t sourceCountOutput;
                SECTION("Invalid components")
                {
                    getInfo.whichComponents = 0;
                    REQUIRE_RESULT(xrGetInputSourceLocalizedName(session, &getInfo, 0, &sourceCountOutput, nullptr),
                                   XR_ERROR_VALIDATION_FAILURE);
                }
                SECTION("Invalid path")
                {
                    getInfo.sourcePath = XR_NULL_PATH;
                    REQUIRE_RESULT(xrGetInputSourceLocalizedName(session, &getInfo, 0, &sourceCountOutput, nullptr), XR_ERROR_PATH_INVALID);
                    getInfo.sourcePath = (XrPath)0x1234;
                    REQUIRE_RESULT(xrGetInputSourceLocalizedName(session, &getInfo, 0, &sourceCountOutput, nullptr), XR_ERROR_PATH_INVALID);
                }

                SECTION("xrGetInputSourceLocalizedName-on-each")
                {
                    for (size_t i = 0; i < enumerateResult.size(); ++i) {
                        getInfo.sourcePath = enumerateResult[i];
                        CAPTURE(i);

                        auto s = PathToString(instance, getInfo.sourcePath);
                        CHECK(!s.empty());
                        CAPTURE(s);
                        std::vector<char> localizedStringResult;

                        {
                            CAPTURE(XrInputSourceLocalizedNameFlagsRefCPP(getInfo.whichComponents) =
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT);
                            localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo);
                            CHECK(localizedStringResult.size() > 1);  // more than null terminator
                            CHECK_THAT(localizedStringResult, NullTerminatedVec());
                        }
                        {
                            CAPTURE(XrInputSourceLocalizedNameFlagsRefCPP(getInfo.whichComponents) =
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT);
                            localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo);
                            CHECK(localizedStringResult.size() > 1);  // more than null terminator
                            CHECK_THAT(localizedStringResult, NullTerminatedVec());
                        }
                        {
                            CAPTURE(XrInputSourceLocalizedNameFlagsRefCPP(getInfo.whichComponents) =
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT);
                            localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo);
                            CHECK(localizedStringResult.size() > 1);  // more than null terminator
                            CHECK_THAT(localizedStringResult, NullTerminatedVec());
                        }
                        {
                            CAPTURE(XrInputSourceLocalizedNameFlagsRefCPP(getInfo.whichComponents) =
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT);
                            localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo);
                            CHECK(localizedStringResult.size() > 1);  // more than null terminator
                            CHECK_THAT(localizedStringResult, NullTerminatedVec());
                        }
                        {
                            CAPTURE(XrInputSourceLocalizedNameFlagsRefCPP(getInfo.whichComponents) =
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT);
                            localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo);
                            CHECK(localizedStringResult.size() > 1);  // more than null terminator
                            CHECK_THAT(localizedStringResult, NullTerminatedVec());
                        }
                        {
                            CAPTURE(XrInputSourceLocalizedNameFlagsRefCPP(getInfo.whichComponents) =
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT);
                            localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo);
                            CHECK(localizedStringResult.size() > 1);  // more than null terminator
                            CHECK_THAT(localizedStringResult, NullTerminatedVec());
                        }
                        {
                            CAPTURE(XrInputSourceLocalizedNameFlagsRefCPP(getInfo.whichComponents) =
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                        XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT);
                            localizedStringResult = REQUIRE_TWO_CALL(char, {}, xrGetInputSourceLocalizedName, session, &getInfo);
                            CHECK(localizedStringResult.size() > 1);  // more than null terminator
                            CHECK_THAT(localizedStringResult, NullTerminatedVec());
                        }
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
