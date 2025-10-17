// Copyright (c) 2019-2025 The Khronos Group Inc.
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
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "composition_utils.h"
#include "input_testinputdevice.h"
#include "interaction_info.h"
#include "matchers.h"
#include "utilities/utils.h"
#include <catch2/catch_test_macros.hpp>

#include <openxr/openxr.h>

using namespace Conformance;

namespace Conformance
{
    TEST_CASE("XR_KHR_generic_controller-select", "[XR_KHR_generic_controller][actions][interactive]")
    {
        // Test summary:
        // If Oculus or Index are selected, then Generic must be selected in equivalent situations.

        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_KHR_GENERIC_CONTROLLER_EXTENSION_NAME)) {
            SKIP(XR_KHR_GENERIC_CONTROLLER_EXTENSION_NAME " not supported");
        }

        // adapted from the logic in xrSuggestInteractionProfileBindings_order
        auto canBecomeCurrent = [&](InteractionProfileIndex interactionProfileIndex, FeatureSet additionalFeatures = FeatureSet()) -> bool {
            FeatureSet enabled;
            globalData.PopulateMinVersionAndEnabledExtensions(enabled);
            FeatureSet available;
            globalData.PopulateMaxSupportedVersionAndAvailableExtensions(available);
            FeatureSet active = enabled + additionalFeatures;

            const InteractionProfileAvailMetadata& interactionProfile = GetInteractionProfile(interactionProfileIndex);

            // this is a consistency check - by this point, we should have skipped if a required profile is unavailable
            REQUIRE(active.IsSatisfiedBy(available));
            // and make sure the base profile requirements are covered by the features we chose
            REQUIRE(kInteractionAvailabilities[(size_t)interactionProfile.Availability].IsSatisfiedBy(active));

            CompositionHelper compositionHelper("XR_KHR_generic_controller-select", additionalFeatures.GetExtensions());

            XrInstance instance = compositionHelper.GetInstance();
            XrSession session = compositionHelper.GetSession();
            compositionHelper.BeginSession();

            ActionLayerManager actionLayerManager(compositionHelper);

            XrPath interactionProfilePath = StringToPath(instance, interactionProfile.InteractionProfilePathString);

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

            bool bindingSuggested = false;
            for (auto& bindings : interactionProfile.BindingPaths) {
                // We use the same pattern as the rest of the action conformance suite
                // and only bind the boolean actions. Note that we bind "boolAction"
                // to *every* boolean input, not just the first one.
                if (bindings.Type != XR_ACTION_TYPE_BOOLEAN_INPUT) {
                    continue;
                }
                if (!kInteractionAvailabilities[(size_t)bindings.Availability].IsSatisfiedBy(active)) {
                    continue;
                }
                XrActionSuggestedBinding binding = {boolAction, StringToPath(instance, bindings.Path)};
                interactionManager.AddActionBindings(interactionProfilePath, {binding});
                bindingSuggested = true;
            }
            REQUIRE(bindingSuggested);  // at least one bool binding should have been available

            bool leftUnderTest = globalData.leftHandUnderTest;
            const char* topLevelPathString = leftUnderTest ? "/user/hand/left" : "/user/hand/right";
            XrPath topLevelXrPath{StringToPath(instance, topLevelPathString)};
            std::shared_ptr<IInputTestDevice> inputDevice =
                CreateTestDevice(&actionLayerManager, &compositionHelper.GetInteractionManager(), instance, session,
                                 StringToPath(instance, GetSimpleInteractionProfile().InteractionProfilePathString), topLevelXrPath,
                                 GetSimpleInteractionProfile().BindingPaths);

            // This function calls xrSuggestInteractionProfileBindings() before attaching the actionsets
            interactionManager.AttachActionSets();

            // boolAction is used to detect when the device becomes active
            inputDevice->SetDeviceActive(/*state = */ true, /*skipInteraction = */ false, boolAction, actionSet);
            actionLayerManager.WaitForSessionFocusWithMessage();
            XrInteractionProfileState interactionProfileState{XR_TYPE_INTERACTION_PROFILE_STATE};
            REQUIRE_RESULT(xrGetCurrentInteractionProfile(session, StringToPath(instance, topLevelPathString), &interactionProfileState),
                           XR_SUCCESS);

            // no other interaction profiles have been suggested, so they must not be selected:
            // "The runtime must return only interaction profiles for which the application has provided
            // suggested bindings with xrSuggestInteractionProfileBindings or XR_NULL_PATH."
            REQUIRE_THAT(interactionProfileState.interactionProfile, In<XrPath>({interactionProfilePath, XR_NULL_PATH}));

            return interactionProfileState.interactionProfile == interactionProfilePath;
        };

        if (!(canBecomeCurrent(InteractionProfileIndex::Profile_oculus_touch_controller) ||
              canBecomeCurrent(InteractionProfileIndex::Profile_valve_index_controller))) {
            SKIP("Neither touch nor index became active, nothing to assert");
        }
        REQUIRE(canBecomeCurrent(InteractionProfileIndex::Profile_khr_generic_controller,
                                 FeatureSet{FeatureBitIndex::BIT_XR_KHR_generic_controller}));
    }

}  // namespace Conformance
