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

#include "ConformanceHooks.h"
#include "CustomHandleState.h"
#include "RuntimeFailure.h"
#include <loader_interfaces.h>

#include <openxr/openxr_reflection_parent_structs.h>

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
#define MAKE_CASE(STRUCT_TYPE, TYPE_ENUM)                                   \
    case TYPE_ENUM: {                                                       \
        const auto typed = reinterpret_cast<const STRUCT_TYPE*>(eventData); \
        checkEventPayload(typed);                                           \
        break;                                                              \
    }
#define MAKE_UNAVAIL_CASE(STRUCT_TYPE, TYPE_ENUM)                                                                            \
    case TYPE_ENUM: {                                                                                                        \
        POSSIBLE_NONCONFORMANT(                                                                                              \
            "Recognized event type: %d but support for this type not compiled in to conformance layer; could not verify it", \
            eventData->type);                                                                                                \
        break;                                                                                                               \
    }
            XR_LIST_ALL_CHILD_STRUCTURE_TYPES_XrEventDataBaseHeader(MAKE_CASE, MAKE_UNAVAIL_CASE);

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

// Helpers
#define VALIDATE_EVENT_XRBOOL32(value) ValidateXrBool32(this, value, #value, "xrPollEvent")
#define VALIDATE_EVENT_FLOAT(value, min, max) ValidateFloat(this, value, min, max, #value, "xrPollEvent")
#define VALIDATE_EVENT_XRTIME(value) ValidateXrTime(this, value, #value, "xrPollEvent")
#define VALIDATE_EVENT_QUATERNION(value) ValidateXrQuaternion(this, value, #value, "xrPollEvent")
#define VALIDATE_EVENT_VECTOR3F(value) ValidateXrVector3f(this, value, #value, "xrPollEvent")
#define VALIDATE_EVENT_XRENUM(value) ValidateXrEnum(this, value, #value, "xrPollEvent")

void ConformanceHooks::checkEventPayload(const XrEventDataEventsLost* data)
{
    const auto eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(data);
    NONCONFORMANT_IF(eventsLost->lostEventCount == 0, "lostEventCount must be > 0");
}

void ConformanceHooks::checkEventPayload(const XrEventDataInstanceLossPending* data)
{
    VALIDATE_EVENT_XRTIME(data->lossTime);
}

void ConformanceHooks::checkEventPayload(const XrEventDataSessionStateChanged* data)
{
    VALIDATE_EVENT_XRTIME(data->time);
    VALIDATE_EVENT_XRENUM(data->state);
    session::SessionStateChanged(this, data);
}

void ConformanceHooks::checkEventPayload(const XrEventDataReferenceSpaceChangePending* data)
{
    VALIDATE_EVENT_XRTIME(data->changeTime);
    VALIDATE_EVENT_QUATERNION(data->poseInPreviousSpace.orientation);
    VALIDATE_EVENT_XRBOOL32(data->poseValid);
    VALIDATE_EVENT_XRENUM(data->referenceSpaceType);
    (void)session::GetSessionState(data->session);  // Check handle is alive/valid.
}

void ConformanceHooks::checkEventPayload(const XrEventDataInteractionProfileChanged* data)
{
    (void)session::GetSessionState(data->session);  // Check handle is alive/valid.
}

void ConformanceHooks::checkEventPayload(const XrEventDataVisibilityMaskChangedKHR* data)
{
    session::VisibilityMaskChanged(this, data);  // Validate session handle and view data.
}

void ConformanceHooks::checkEventPayload(const XrEventDataPerfSettingsEXT* data)
{
    VALIDATE_EVENT_XRENUM(data->domain);
    VALIDATE_EVENT_XRENUM(data->subDomain);
    VALIDATE_EVENT_XRENUM(data->fromLevel);
    VALIDATE_EVENT_XRENUM(data->toLevel);
}

void ConformanceHooks::checkEventPayload(const XrEventDataSpatialAnchorCreateCompleteFB* data)
{
    (void)data;
    // Event data used in gen_dispatch.cpp
}
