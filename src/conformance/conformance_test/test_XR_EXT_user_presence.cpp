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

#include "utilities/utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/system_properties_helper.h"
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>
#include <algorithm>

using namespace Conformance;

namespace Conformance
{
    static const auto SystemSupportsUserPresence =
        MakeSystemPropertiesBoolChecker(XrSystemUserPresencePropertiesEXT{XR_TYPE_SYSTEM_USER_PRESENCE_PROPERTIES_EXT},
                                        &XrSystemUserPresencePropertiesEXT::supportsUserPresence);

    TEST_CASE("XR_EXT_user_presence", "[XR_EXT_user_presence]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_USER_PRESENCE_EXTENSION_NAME)) {
            SKIP(XR_EXT_USER_PRESENCE_EXTENSION_NAME " not supported");
        }

        AutoBasicInstance instance({XR_EXT_USER_PRESENCE_EXTENSION_NAME}, AutoBasicInstance::createSystemId);

        if (!SystemSupportsUserPresence(instance, instance.systemId)) {
            // If the system does not support user presence sensing, the runtime must:
            // return ename:XR_FALSE for pname:supportsUserPresence and must: not queue the
            // slink:XrEventDataUserPresenceChangedEXT event for any session on this
            // system.

            SKIP("System does not support user presence sensing.");
        }

        AutoBasicSession session(AutoBasicSession::createSession, instance);

        FrameIterator frameIterator(&session);
        frameIterator.RunToSessionState(XR_SESSION_STATE_READY);

        XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
        sessionBeginInfo.primaryViewConfigurationType = GetGlobalData().GetOptions().viewConfigurationValue;
        REQUIRE(XR_SUCCESS == xrBeginSession(session, &sessionBeginInfo));

        // The runtime must: queue this event upon a successful call to the
        // flink:xrBeginSession function, regardless of the value of
        // pname:isUserPresent, so that the application can be in sync on the state
        // when a session begins running.

        bool foundUserPresenceEvent = false;
        constexpr std::chrono::nanoseconds duration = 1s;

        CountdownTimer countdown(duration);
        XrResult pollResult = XR_SUCCESS;
        while (!countdown.IsTimeUp() && pollResult == XR_SUCCESS) {
            XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
            std::fill(eventData.varying, eventData.varying + sizeof(eventData.varying), std::numeric_limits<uint8_t>::max());

            pollResult = xrPollEvent(instance, &eventData);
            REQUIRE(XR_SUCCEEDED(pollResult));

            if (eventData.type == XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT) {
                foundUserPresenceEvent = true;

                // We don't require a user to be present for running automated tests,
                // so we are not validating the value here...
                break;
            }
        }

        REQUIRE(foundUserPresenceEvent == true);
    }
}  // namespace Conformance
