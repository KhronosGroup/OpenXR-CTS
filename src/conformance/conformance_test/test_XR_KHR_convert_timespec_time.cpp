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

#include "utilities/utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

// Include all dependencies of openxr_platform as configured
#include "xr_dependencies.h"
#include <openxr/openxr_platform.h>

#ifdef XR_USE_PLATFORM_WIN32
#include <windows.h>
#endif

namespace Conformance
{

    TEST_CASE("XR_KHR_convert_timespec_time", "")
    {
#ifndef XR_USE_TIMESPEC
        SKIP("XR_KHR_convert_timespec_time test not enabled in CTS");
#else
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported("XR_KHR_convert_timespec_time")) {
            SKIP(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME " not supported");
        }

        // XrResult xrConvertTimespecTimeToTimeKHR(XrInstance xrInstance, const struct timespec* timespecTime, XrTime* time);
        // XrResult xrConvertTimeToTimespecTimeKHR(XrInstance xrInstance, XrTime time, struct timespec* timespecTime);

        XrResult result;
        AutoBasicInstance instance({"XR_KHR_convert_timespec_time"});

        timespec ts1, ts2;
#ifdef XR_USE_PLATFORM_WIN32
        timespec_get(&ts1, TIME_UTC);
#else
        clock_gettime(CLOCK_MONOTONIC, &ts1);
#endif

        // Since this is an extension, get a function pointer via `xrGetInstanceProcAddr`
        // so that runtimes that don't support the extension entry point can still compile against this
        auto xrConvertTimespecTimeToTimeKHR =
            GetInstanceExtensionFunction<PFN_xrConvertTimespecTimeToTimeKHR>(instance, "xrConvertTimespecTimeToTimeKHR");
        REQUIRE(xrConvertTimespecTimeToTimeKHR != nullptr);

        auto xrConvertTimeToTimespecTimeKHR =
            GetInstanceExtensionFunction<PFN_xrConvertTimeToTimespecTimeKHR>(instance, "xrConvertTimeToTimespecTimeKHR");
        REQUIRE(xrConvertTimeToTimespecTimeKHR != nullptr);

        SECTION("Roundtrip")
        {
            XrTime time1, time2;

            result = xrConvertTimespecTimeToTimeKHR(instance, &ts1, &time1);
            CHECK(ValidateResultAllowed("xrConvertTimespecTimeToTimeKHR", result));
            INFO("xrConvertTimespecTimeToTimeKHR failed with result: " << ResultToString(result));
            CHECK_RESULT_SUCCEEDED(result);

            result = xrConvertTimeToTimespecTimeKHR(instance, time1, &ts2);
            CHECK(ValidateResultAllowed("xrConvertTimeToTimespecTimeKHR", result));
            INFO("xrConvertTimeToTimespecTimeKHR failed with result: " << ResultToString(result));
            CHECK_RESULT_SUCCEEDED(result);

            result = xrConvertTimespecTimeToTimeKHR(instance, &ts2, &time2);
            CHECK(ValidateResultAllowed("xrConvertTimespecTimeToTimeKHR", result));
            INFO("xrConvertTimespecTimeToTimeKHR failed with result: " << ResultToString(result));
            CHECK_RESULT_SUCCEEDED(result);

            // At this point ts1/ts22 and time1/time2 should be similar to each other. But since the
            // frequency of the two are not the same, the round trip could cause a shift by a value.
            CHECK(std::abs((int64_t)((ts1.tv_sec * 100000000ULL) + ts1.tv_nsec) - (int64_t)((ts2.tv_sec * 100000000ULL) + ts2.tv_nsec)) <
                  2);
            CHECK(std::abs(time1 - time2) < 2);

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // Exercise XR_ERROR_HANDLE_INVALID
                result = xrConvertTimespecTimeToTimeKHR(XR_NULL_HANDLE_CPP, &ts1, &time1);
                REQUIRE(ValidateResultAllowed("xrConvertTimespecTimeToTimeKHR", result));
                REQUIRE(result == XR_ERROR_HANDLE_INVALID);

                result = xrConvertTimeToTimespecTimeKHR(globalData.invalidInstance, time1, &ts2);
                REQUIRE(ValidateResultAllowed("xrConvertTimeToTimespecTimeKHR", result));
                REQUIRE(result == XR_ERROR_HANDLE_INVALID);
            }
        }

        SECTION("Invalid times")
        {
            result = xrConvertTimeToTimespecTimeKHR(instance, XrTime(0), &ts1);
            CHECK(ValidateResultAllowed("xrConvertTimeToTimespecTimeKHR", result));
            CHECK(result == XR_ERROR_TIME_INVALID);

            result = xrConvertTimeToTimespecTimeKHR(instance, XrTime(-1), &ts1);
            CHECK(ValidateResultAllowed("xrConvertTimeToTimespecTimeKHR", result));
            CHECK(result == XR_ERROR_TIME_INVALID);
        }

        SECTION("Matches frame timing")
        {
            auto queryXrTimeFromCurrentTime = [&]() -> XrTime {
                timespec ts;
#ifdef XR_USE_PLATFORM_WIN32
                timespec_get(&ts, TIME_UTC);
#else
                clock_gettime(CLOCK_MONOTONIC, &ts);
#endif

                XrTime t = 0;
                xrConvertTimespecTimeToTimeKHR(instance, &ts, &t);
                return t;
            };

            AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                         AutoBasicSession::createSpaces,
                                     instance);
            // Query timespec before we query the runtime for an independent XrTime
            XrTime timespecBefore = queryXrTimeFromCurrentTime();
            CAPTURE(timespecBefore);

            // Wait until the runtime is ready for us to begin a session
            FrameIterator frameIterator(&session);
            frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

            // Submit a frame and query the time for the next frame
            FrameIterator::RunResult runResult = frameIterator.SubmitFrame();
            REQUIRE(runResult == FrameIterator::RunResult::Success);
            XrTime nextFrameTime = frameIterator.frameState.predictedDisplayTime;

            // predicted display time is required to be a time in the future so it is fair to assume it is after now.
            REQUIRE(nextFrameTime >= timespecBefore);

            XrTime timespecAfter = queryXrTimeFromCurrentTime();
            CAPTURE(timespecAfter);

            REQUIRE(timespecAfter > timespecBefore);
        }
#endif  // XR_USE_TIMESPEC
    }
}  // namespace Conformance
