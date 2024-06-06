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

#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/types_and_constants.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <cmath>

namespace Conformance
{

    TEST_CASE("XR_EXT_thermal_query", "[XR_EXT_thermal_query]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported("XR_EXT_thermal_query")) {
            SKIP(XR_EXT_THERMAL_QUERY_EXTENSION_NAME " not supported");
        }

        // Set up the session we will use for the testing
        AutoBasicInstance instance({"XR_EXT_thermal_query"});
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

        XrResult result;
        XrPerfSettingsNotificationLevelEXT notificationLevel;
        float tempHeadroom;
        float tempSlope;

        // Since this is an extension, get a function pointer via `xrGetInstanceProcAddr`
        // so that runtimes that don't support the extension entry point can still compile against this
        auto _xrThermalGetTemperatureTrendEXT =
            GetInstanceExtensionFunction<PFN_xrThermalGetTemperatureTrendEXT>(session.GetInstance(), "xrThermalGetTemperatureTrendEXT");
        CHECK(_xrThermalGetTemperatureTrendEXT != nullptr);

        for (int i = 0; i < 100; ++i) {
            // XR_PERF_SETTINGS_DOMAIN_CPU_EXT
            notificationLevel = XR_PERF_SETTINGS_NOTIFICATION_LEVEL_MAX_ENUM_EXT;
            tempHeadroom = FP_NAN;
            tempSlope = FP_NAN;
            result =
                _xrThermalGetTemperatureTrendEXT(session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, &notificationLevel, &tempHeadroom, &tempSlope);
            CHECK(ValidateResultAllowed("xrThermalGetTemperatureTrendEXT", result));
            CHECK(result == XR_SUCCESS);
            CAPTURE(notificationLevel);
            CAPTURE(tempHeadroom);
            CAPTURE(tempSlope);
            CHECK(((notificationLevel == XR_PERF_SETTINGS_NOTIF_LEVEL_NORMAL_EXT) ||
                   (notificationLevel == XR_PERF_SETTINGS_NOTIF_LEVEL_WARNING_EXT) ||
                   (notificationLevel == XR_PERF_SETTINGS_NOTIF_LEVEL_IMPAIRED_EXT)));
            CHECK(tempHeadroom < 100000);  // Check that values are reasonable
            CHECK(tempSlope < 1000);

            // XR_PERF_SETTINGS_DOMAIN_GPU_EXT
            notificationLevel = XR_PERF_SETTINGS_NOTIFICATION_LEVEL_MAX_ENUM_EXT;
            tempHeadroom = FP_NAN;
            tempSlope = FP_NAN;
            result =
                _xrThermalGetTemperatureTrendEXT(session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, &notificationLevel, &tempHeadroom, &tempSlope);
            CHECK(ValidateResultAllowed("xrThermalGetTemperatureTrendEXT", result));
            CHECK(result == XR_SUCCESS);
            CAPTURE(notificationLevel);
            CAPTURE(tempHeadroom);
            CAPTURE(tempSlope);
            CHECK(((notificationLevel == XR_PERF_SETTINGS_NOTIF_LEVEL_NORMAL_EXT) ||
                   (notificationLevel == XR_PERF_SETTINGS_NOTIF_LEVEL_WARNING_EXT) ||
                   (notificationLevel == XR_PERF_SETTINGS_NOTIF_LEVEL_IMPAIRED_EXT)));
            CHECK(tempHeadroom < 100000);  // Check that values are reasonable
            CHECK(tempSlope < 1000);
        }

        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            // XR_PERF_SETTINGS_DOMAIN_CPU_EXT
            result = _xrThermalGetTemperatureTrendEXT(XR_NULL_HANDLE_CPP, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, &notificationLevel,
                                                      &tempHeadroom, &tempSlope);
            CHECK(ValidateResultAllowed("xrThermalGetTemperatureTrendEXT", result));
            CHECK(result == XR_ERROR_HANDLE_INVALID);

            result = _xrThermalGetTemperatureTrendEXT(globalData.invalidSession, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, &notificationLevel,
                                                      &tempHeadroom, &tempSlope);
            CHECK(ValidateResultAllowed("xrThermalGetTemperatureTrendEXT", result));
            CHECK(result == XR_ERROR_HANDLE_INVALID);

            // XR_PERF_SETTINGS_DOMAIN_GPU_EXT
            result = _xrThermalGetTemperatureTrendEXT(XR_NULL_HANDLE_CPP, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, &notificationLevel,
                                                      &tempHeadroom, &tempSlope);
            CHECK(ValidateResultAllowed("xrThermalGetTemperatureTrendEXT", result));
            CHECK(result == XR_ERROR_HANDLE_INVALID);

            result = _xrThermalGetTemperatureTrendEXT(globalData.invalidSession, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, &notificationLevel,
                                                      &tempHeadroom, &tempSlope);
            CHECK(ValidateResultAllowed("xrThermalGetTemperatureTrendEXT", result));
            CHECK(result == XR_ERROR_HANDLE_INVALID);
        }
    }

}  // namespace Conformance
