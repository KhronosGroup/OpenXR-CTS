// Copyright (c) 2019-2022, The Khronos Group Inc.
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
#include "RuntimeFailure.h"
#include <loader_interfaces.h>

/////////////////
// ABI
/////////////////

XrResult ConformanceHooks::xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
{
    const XrResult result = ConformanceHooksBase::xrPollEvent(instance, eventData);
    if (result != XR_SUCCESS) {
        return result;
    }

    try {
        switch ((int)eventData->type) {  // int cast so compiler doesn't warn about other enumerants.
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            const auto instanceLossPending = reinterpret_cast<const XrEventDataInstanceLossPending*>(eventData);
            VALIDATE_XRTIME(instanceLossPending->lossTime);
            break;
        }
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            const auto sessionStateChanged = reinterpret_cast<const XrEventDataSessionStateChanged*>(eventData);
            VALIDATE_XRTIME(sessionStateChanged->time);
            VALIDATE_XRENUM(sessionStateChanged->state);
            session::SessionStateChanged(this, sessionStateChanged);
            break;
        }
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
            const auto referenceSpaceChagePending = reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(eventData);
            VALIDATE_XRTIME(referenceSpaceChagePending->changeTime);
            VALIDATE_QUATERNION(referenceSpaceChagePending->poseInPreviousSpace.orientation);
            VALIDATE_XRBOOL32(referenceSpaceChagePending->poseValid);
            VALIDATE_XRENUM(referenceSpaceChagePending->referenceSpaceType);
            (void)session::GetSessionState(referenceSpaceChagePending->session);  // Check handle is alive/valid.
            break;
        }
        case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
            const auto eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(eventData);
            NONCONFORMANT_IF(eventsLost->lostEventCount == 0, "lostEventCount must be > 0");
            break;
        }
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
            const auto interactionProfileChanged = reinterpret_cast<const XrEventDataInteractionProfileChanged*>(eventData);
            (void)session::GetSessionState(interactionProfileChanged->session);  // Check handle is alive/valid.
            break;
        }
        case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
            const auto perfSettings = reinterpret_cast<const XrEventDataPerfSettingsEXT*>(eventData);
            VALIDATE_XRENUM(perfSettings->domain);
            VALIDATE_XRENUM(perfSettings->subDomain);
            VALIDATE_XRENUM(perfSettings->fromLevel);
            VALIDATE_XRENUM(perfSettings->toLevel);
            break;
        }
        case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR: {
            const auto visibilityMaskChanged = reinterpret_cast<const XrEventDataVisibilityMaskChangedKHR*>(eventData);
            session::VisibilityMaskChanged(this, visibilityMaskChanged);  // Validate session handle and view data.
            break;
        }
        case XR_TYPE_EVENT_DATA_SPATIAL_ANCHOR_CREATE_COMPLETE_FB: {
            const auto completeEvent = reinterpret_cast<XrEventDataSpatialAnchorCreateCompleteFB*>(eventData);
            (void)completeEvent;
            // Event data used in gen_dispatch.cpp
            break;
        }
        case XR_TYPE_EVENT_DATA_SPACE_SET_STATUS_COMPLETE_FB: {
            const auto completeEvent = reinterpret_cast<XrEventDataSpaceSetStatusCompleteFB*>(eventData);
            (void)completeEvent;
            break;
        }
        case XR_TYPE_EVENT_DATA_SPACE_SAVE_COMPLETE_FB: {
            const auto completeEvent = reinterpret_cast<XrEventDataSpaceSaveCompleteFB*>(eventData);
            (void)completeEvent;
            break;
        }
        case XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB: {
            const auto results = reinterpret_cast<XrEventDataSpaceQueryResultsAvailableFB*>(eventData);
            (void)results;
            break;
        }
        case XR_TYPE_EVENT_DATA_SPACE_ERASE_COMPLETE_FB: {
            const auto completeEvent = reinterpret_cast<XrEventDataSpaceEraseCompleteFB*>(eventData);
            (void)completeEvent;
            break;
        }
        case XR_TYPE_EVENT_DATA_SPACE_QUERY_COMPLETE_FB: {
            const auto completeEvent = reinterpret_cast<XrEventDataSpaceQueryCompleteFB*>(eventData);
            (void)completeEvent;
            break;
        }
        default:
            POSSIBLE_NONCONFORMANT("Unsupported event type: %d", eventData->type);
            break;
        }
    }
    catch (const HandleException& ex) {  // Some event data struct has a handle value which is not tracked.
        NONCONFORMANT("Event type %s: %s", to_string(eventData->type), ex.what());
    }

    return result;
}
