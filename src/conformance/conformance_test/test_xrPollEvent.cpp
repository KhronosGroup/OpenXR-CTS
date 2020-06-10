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
#include "conformance_utils.h"
#include "conformance_framework.h"
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

namespace Conformance
{

    void ValidateEventData(const XrEventDataBuffer* eventData)
    {
        GlobalData& globalData = GetGlobalData();

        const XrEventDataBuffer* chain = reinterpret_cast<const XrEventDataBuffer*>(eventData->next);

        if (chain) {
            size_t count = 0;

            // note: the next chain needs to fit inside of the XrEventDataBuffer struct:
            // "Runtimes may create valid next chains depending on enabled extensions, but they must
            //  guarantee that any such chains point only to objects which fit completely within the
            //  original XrEventDataBuffer pointed to by eventData." 2.20.1
            //
            // So once we followed more pointers as fit into the struct, we either got into a loop
            // or out of the original struct!
            const size_t maxPointersToFollow = (sizeof(XrEventDataBuffer) / sizeof(void*));

            while ((chain->next != nullptr) && (count < maxPointersToFollow)) {
                count++;
                chain = reinterpret_cast<const XrEventDataBuffer*>(chain->next);
            }

            if (count == maxPointersToFollow) {
                CHECK_MSG(false, "Event data contains an invalid next chain.");
            }
        }

        switch (eventData->type) {
            // To do: Auto-generate some data that would allow us to know all the event types so we can
            // easily maintain this in the future.

        case XR_TYPE_EVENT_DATA_BUFFER: {
            // This should never be returned.
            CHECK_MSG(false, "Event data is of unexpected type XR_TYPE_EVENT_DATA_BUFFER.");
            break;
        }

        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            const XrEventDataInstanceLossPending* ilp = reinterpret_cast<const XrEventDataInstanceLossPending*>(eventData);
            (void)ilp->lossTime;
            break;
        }

        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            const XrEventDataSessionStateChanged* ssc = reinterpret_cast<const XrEventDataSessionStateChanged*>(eventData);
            (void)ssc->session;
            (void)ssc->state;
            (void)ssc->time;
            (void)ssc->type;
            break;
        }

        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
            const XrEventDataReferenceSpaceChangePending* rscp = reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(eventData);
            (void)rscp->referenceSpaceType;
            (void)rscp->changeTime;
            (void)rscp->poseValid;
            (void)rscp->poseInPreviousSpace;
            break;
        }

        case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
            const XrEventDataEventsLost* el = reinterpret_cast<const XrEventDataEventsLost*>(eventData);
            (void)el->lostEventCount;
            break;
        }

        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
            const XrEventDataInteractionProfileChanged* pc = reinterpret_cast<const XrEventDataInteractionProfileChanged*>(eventData);
            (void)pc;
            break;
        }

        case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
            const XrEventDataPerfSettingsEXT* pse = reinterpret_cast<const XrEventDataPerfSettingsEXT*>(eventData);
            (void)pse->domain;
            (void)pse->subDomain;
            (void)pse->fromLevel;
            (void)pse->toLevel;
            break;
        }

        case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR: {
            const XrEventDataVisibilityMaskChangedKHR* vmck = reinterpret_cast<const XrEventDataVisibilityMaskChangedKHR*>(eventData);
            (void)vmck->viewConfigurationType;
            (void)vmck->viewIndex;
            break;
        }

        default: {  // The event is of some type that we don't know.
            if (globalData.runtimeMatchesAPIVersion) {
                // Since we are testing a runtime whose version matches our API version,
                // the event should not be a core type, because that would an event we don't
                // know about is being returned.

                if (globalData.instanceProperties.runtimeVersion == XR_CURRENT_API_VERSION) {
                    CHECK_MSG(eventData->type < 1000000000, "Runtime supports unexpected event type")
                }
            }

            break;
        }
        }
    }

    TEST_CASE("xrPollEvent", "")
    {
        // XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData);

        // We don't have a way to programmatically exercise delivery of any event types, as the delivery
        // of events is at the will of the runtime. As of OpenXR 1.0, there are no events that we can
        // trigger from the client side except XrEventDataSessionStateChanged.

        AutoBasicInstance instance;

        int counter;
        XrResult result;
        XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};

        // Wait for delivery of any possible events.
        SleepMs(500);

        // Poll event prior to session being created.
        counter = 0;
        do {
            result = xrPollEvent(instance, &eventData);
            CHECK(ValidateResultAllowed("xrPollEvent", result));
            CHECK(((result == XR_SUCCESS) || (result == XR_EVENT_UNAVAILABLE)));
            if (result == XR_SUCCESS) {
                ValidateEventData(&eventData);
            }
        } while ((result != XR_EVENT_UNAVAILABLE) && (++counter < 100));

        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

        SECTION("Unrecognized extension")
        {
            // Runtimes should ignore unrecognized struct extensins.
            InsertUnrecognizableExtension(&eventData);
            result = xrPollEvent(instance, &eventData);
            CHECK(ValidateResultAllowed("xrPollEvent", result));
            CHECK(((result == XR_SUCCESS) || (result == XR_EVENT_UNAVAILABLE)));
        }

        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            SECTION("NULL handle")
            {
                // Exercise XR_ERROR_HANDLE_INVALID
                result = xrPollEvent(XR_NULL_HANDLE_CPP, &eventData);
                CHECK(ValidateResultAllowed("xrPollEvent", result));
                CHECK(result == XR_ERROR_HANDLE_INVALID);
            }

            // Invalid handle validation
            {
                GlobalData& globalData = GetGlobalData();
                // Exercise XR_ERROR_HANDLE_INVALID
                result = xrPollEvent(globalData.invalidInstance, &eventData);
                CHECK(ValidateResultAllowed("xrPollEvent", result));
                CHECK(result == XR_ERROR_HANDLE_INVALID);

                // Poll event prior to session being created.
                counter = 0;
                do {
                    eventData = XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
                    result = xrPollEvent(instance, &eventData);
                    CHECK(ValidateResultAllowed("xrPollEvent", result));
                    CHECK(((result == XR_SUCCESS) || (result == XR_EVENT_UNAVAILABLE)));
                    if (result == XR_SUCCESS) {
                        ValidateEventData(&eventData);
                    }
                } while ((result != XR_EVENT_UNAVAILABLE) && (++counter < 100));

                /// @todo We are going to need a bit more work to exercise the session life-cycle events (XrEventDataSessionStateChanged).
            }
        }
    }
}  // namespace Conformance
