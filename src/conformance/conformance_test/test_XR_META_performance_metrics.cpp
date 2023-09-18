// Copyright (c) 2019-2023, The Khronos Group Inc.
// Copyright (c) Meta Platforms, LLC and its affiliates. All rights reserved.
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
#include "two_call.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <chrono>
#include <ratio>
#include <string>
#include <vector>

namespace Conformance
{
    TEST_CASE("XR_META_performance_metrics", "")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_META_PERFORMANCE_METRICS_EXTENSION_NAME)) {
            SKIP(XR_META_PERFORMANCE_METRICS_EXTENSION_NAME " not supported");
        }

        AutoBasicInstance instance({XR_META_PERFORMANCE_METRICS_EXTENSION_NAME});

        auto xrEnumeratePerformanceMetricsCounterPathsMETA_ =
            GetInstanceExtensionFunction<PFN_xrEnumeratePerformanceMetricsCounterPathsMETA>(
                instance, "xrEnumeratePerformanceMetricsCounterPathsMETA");
        auto xrSetPerformanceMetricsStateMETA_ =
            GetInstanceExtensionFunction<PFN_xrSetPerformanceMetricsStateMETA>(instance, "xrSetPerformanceMetricsStateMETA");
        auto xrGetPerformanceMetricsStateMETA_ =
            GetInstanceExtensionFunction<PFN_xrGetPerformanceMetricsStateMETA>(instance, "xrGetPerformanceMetricsStateMETA");
        auto xrQueryPerformanceMetricsCounterMETA_ =
            GetInstanceExtensionFunction<PFN_xrQueryPerformanceMetricsCounterMETA>(instance, "xrQueryPerformanceMetricsCounterMETA");

        std::vector<XrPath> paths = CHECK_TWO_CALL(XrPath, {}, xrEnumeratePerformanceMetricsCounterPathsMETA_, instance);

        SECTION("Query metrics without starting")
        {
            AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

            for (const XrPath& path : paths) {
                XrPerformanceMetricsCounterMETA counter{XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
                // Expect some sort of failure here
                REQUIRE(XR_SUCCESS != xrQueryPerformanceMetricsCounterMETA_(session, path, &counter));
            }
        }

        SECTION("Query metrics without xrEndFrame")
        {
            AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

            XrPerformanceMetricsStateMETA perfMetricsState{XR_TYPE_PERFORMANCE_METRICS_STATE_META};
            perfMetricsState.enabled = XR_TRUE;

            REQUIRE(XR_SUCCESS == xrSetPerformanceMetricsStateMETA_(session, &perfMetricsState));

            perfMetricsState.enabled = XR_FALSE;
            REQUIRE(XR_SUCCESS == xrGetPerformanceMetricsStateMETA_(session, &perfMetricsState));
            REQUIRE(perfMetricsState.enabled == XR_TRUE);

            for (const XrPath& path : paths) {
                // It is not very interesting to query frame stats without a frame, but I guess it
                // also is not an error?
                XrPerformanceMetricsCounterMETA counter{XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
                REQUIRE(XR_SUCCESS == xrQueryPerformanceMetricsCounterMETA_(session, path, &counter));
            }
        }

        SECTION("Query metrics after xrEndFrame")
        {
            // Get a session started.
            AutoBasicSession session(AutoBasicSession::createInstance | AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                         AutoBasicSession::createSwapchains | AutoBasicSession::createSpaces,
                                     instance);

            // Enable perf metrics
            XrPerformanceMetricsStateMETA perfMetricsState{XR_TYPE_PERFORMANCE_METRICS_STATE_META};
            perfMetricsState.enabled = XR_TRUE;
            REQUIRE(XR_SUCCESS == xrSetPerformanceMetricsStateMETA_(session, &perfMetricsState));

            // Get frames iterating to the point of app focused state. This will draw frames along the way.
            FrameIterator frameIterator(&session);
            frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

            // Render one frame to some frame stats.
            FrameIterator::RunResult runResult = frameIterator.SubmitFrame();
            REQUIRE(runResult == FrameIterator::RunResult::Success);

            for (const XrPath& path : paths) {
                XrPerformanceMetricsCounterMETA counter{XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
                REQUIRE(XR_SUCCESS == xrQueryPerformanceMetricsCounterMETA_(session, path, &counter));

                XrPerformanceMetricsCounterMETA counterAgain{XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
                REQUIRE(XR_SUCCESS == xrQueryPerformanceMetricsCounterMETA_(session, path, &counterAgain));

                if ((counter.counterFlags & XR_PERFORMANCE_METRICS_COUNTER_UINT_VALUE_VALID_BIT_META) != 0) {
                    REQUIRE((counter.counterFlags & XR_PERFORMANCE_METRICS_COUNTER_ANY_VALUE_VALID_BIT_META) != 0);

                    // Querying the results for the same metric again should give the same result
                    REQUIRE(counter.counterFlags == counterAgain.counterFlags);
                    REQUIRE(counter.uintValue == counterAgain.uintValue);
                }
                if ((counter.counterFlags & XR_PERFORMANCE_METRICS_COUNTER_FLOAT_VALUE_VALID_BIT_META) != 0) {
                    REQUIRE((counter.counterFlags & XR_PERFORMANCE_METRICS_COUNTER_ANY_VALUE_VALID_BIT_META) != 0);

                    // Querying the results for the same metric again should give type of result
                    REQUIRE(counter.counterFlags == counterAgain.counterFlags);
                }
            }
        }
    }
}  // namespace Conformance
