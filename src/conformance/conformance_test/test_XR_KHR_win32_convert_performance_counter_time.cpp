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
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

// Include all dependencies of openxr_platform as configured
#include "xr_dependencies.h"
#include <openxr/openxr_platform.h>

#ifdef XR_USE_PLATFORM_WIN32
#include <windows.h>
#endif

namespace Conformance
{
#ifdef XR_USE_PLATFORM_WIN32
    TEST_CASE("XR_KHR_win32_convert_performance_counter_time", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported("XR_KHR_win32_convert_performance_counter_time")) {
            return;
        }

        AutoBasicInstance instance({"XR_KHR_win32_convert_performance_counter_time"});

        auto xrConvertWin32PerformanceCounterToTimeKHR = GetInstanceExtensionFunction<PFN_xrConvertWin32PerformanceCounterToTimeKHR>(
            instance, "xrConvertWin32PerformanceCounterToTimeKHR");

        auto xrConvertTimeToWin32PerformanceCounterKHR = GetInstanceExtensionFunction<PFN_xrConvertTimeToWin32PerformanceCounterKHR>(
            instance, "xrConvertTimeToWin32PerformanceCounterKHR");

        SECTION("Roundtrip")
        {
            LARGE_INTEGER qpcFreq;
            QueryPerformanceFrequency(&qpcFreq);
            CAPTURE(qpcFreq.QuadPart);  // QPC ticks/Sec
            const XrDuration nanosecondsPerQpcTick = 1_xrSeconds / qpcFreq.QuadPart;
            CAPTURE(nanosecondsPerQpcTick);

            LARGE_INTEGER li1;
            QueryPerformanceCounter(&li1);
            CAPTURE(li1.QuadPart);

            XrTime time1;
            CHECK_RESULT_SUCCEEDED(xrConvertWin32PerformanceCounterToTimeKHR(instance, &li1, &time1));
            CAPTURE(time1);

            LARGE_INTEGER li2;
            CHECK_RESULT_SUCCEEDED(xrConvertTimeToWin32PerformanceCounterKHR(instance, time1, &li2));
            CAPTURE(li2.QuadPart);

            XrTime time2;
            CHECK_RESULT_SUCCEEDED(xrConvertWin32PerformanceCounterToTimeKHR(instance, &li2, &time2));
            CAPTURE(time2);

            // XrTime is more granular than QPC so round-trip should have at most 1 tick difference.
            CHECK(abs(li1.QuadPart - li2.QuadPart) <= 1);
            // The 1 QPC tick difference will result in up to nanosecondsPerQpcTick difference.
            CHECK(abs(time1 - time2) <= nanosecondsPerQpcTick);

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // Exercise XR_ERROR_HANDLE_INVALID
                REQUIRE(xrConvertWin32PerformanceCounterToTimeKHR(XR_NULL_HANDLE_CPP, &li1, &time1) == XR_ERROR_HANDLE_INVALID);

                REQUIRE(xrConvertTimeToWin32PerformanceCounterKHR(GetGlobalData().invalidInstance, time1, &li2) == XR_ERROR_HANDLE_INVALID);
            }
        }

        SECTION("Invalid times")
        {
            LARGE_INTEGER li1;
            CHECK(xrConvertTimeToWin32PerformanceCounterKHR(instance, XrTime(0), &li1) == XR_ERROR_TIME_INVALID);

            CHECK(xrConvertTimeToWin32PerformanceCounterKHR(instance, XrTime(-1), &li1) == XR_ERROR_TIME_INVALID);
        }
    }
#endif  // XR_USE_PLATFORM_WIN32
}  // namespace Conformance
