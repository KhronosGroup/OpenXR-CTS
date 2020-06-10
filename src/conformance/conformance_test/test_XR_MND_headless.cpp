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
    TEST_CASE("XR_MND_headless", "")
    {
        GlobalData& globalData = GetGlobalData();

        // When this extension is enabled, the behavior of existing functions that interact with the
        // graphics subsystem is altered. When calling the function xrCreateSession with no graphics
        // binding structure, the session will be created as headless.
        //
        // When operating with a headless session, the function xrEnumerateSwapchainFormats must return
        // an empty list of formats. Calls to functions xrCreateSwapchain, xrDestroySwapchain,
        // xrAcquireSwapchainImage, xrWaitFrame are invalid. All other functions, including those
        // related to tracking, input and haptics, are unaffected.
        if (!globalData.IsInstanceExtensionEnabled("XR_MND_headless")) {
            return;
        }

        AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::skipGraphics);

        SECTION("xrEnumerateSwapchainFormats should return XR_SUCCESS but zero formats.")
        {
            uint32_t countOutput = UINT32_MAX;
            REQUIRE(xrEnumerateSwapchainFormats(session, 0, &countOutput, nullptr) == XR_SUCCESS);
            REQUIRE(countOutput == 0);

            int64_t formats[100];
            countOutput = UINT32_MAX;
            REQUIRE(xrEnumerateSwapchainFormats(session, sizeof(formats) / sizeof(formats[0]), &countOutput, formats) == XR_SUCCESS);
            REQUIRE(countOutput == 0);
        }

        // Calls to functions xrCreateSwapchain, xrDestroySwapchain, xrAcquireSwapchainImage,
        // xrWaitFrame are invalid, but there isn't a specification for what happens when called.

        // We begin a session and call valid session functions.
        XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO, nullptr, globalData.options.viewConfigurationValue};
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrBeginSession(session, &sessionBeginInfo));

        // To do: call input and tracking functions here.
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrRequestExitSession(session));
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrEndSession(session));
    }
}  // namespace Conformance
