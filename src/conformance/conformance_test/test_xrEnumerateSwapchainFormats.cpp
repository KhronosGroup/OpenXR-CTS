// Copyright (c) 2019-2026 The Khronos Group Inc.
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

#include "conformance_utils.h"
#include "conformance_framework.h"
#include "graphics_plugin.h"
#include "two_call.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <vector>

namespace Conformance
{
    TEST_CASE("xrEnumerateSwapchainFormats")
    {
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession);

        if (!GetGlobalData().IsUsingGraphicsPlugin()) {
            INFO("Headless shouldn't provide any swapchain formats");
            auto formats = REQUIRE_TWO_CALL(int64_t, {}, xrEnumerateSwapchainFormats, session.GetSession());
            REQUIRE(formats.empty());
            return;
        }

        auto formats = REQUIRE_TWO_CALL(int64_t, {}, xrEnumerateSwapchainFormats, session.GetSession());
        REQUIRE(formats.size() > 0);

        // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#xrEnumerateSwapchainFormats
        //
        // Texture formats should be in order from highest to lowest runtime preference. The
        // application should use the highest preference format that it supports for optimal
        // performance and quality.
        SECTION("format order")
        {
            if (formats.size() > 1) {
                bool inNumericalOrder = true;
                for (size_t i = 1; i < formats.size(); ++i) {
                    if (formats[i] < formats[i - 1]) {
                        inNumericalOrder = false;
                    }
                }
                if (inNumericalOrder) {
                    WARN(
                        "swapchain formats are listed in numerical order; this is not inherently a conformance failure, but potentially indicates that the runtime is not indicating a preference.");
                }
            }
        }

        SECTION("common depth format support")
        {
            auto graphicsPlugin = GetGlobalData().GetGraphicsPlugin();
            int64_t selectedFormat = graphicsPlugin->SelectDepthSwapchainFormat(false, formats);
            if (selectedFormat < 0) {
                WARN("No commonly used depth format enumerated. It is recommended to support at least one commonly used depth format.");
            }
        }
    }

}  // namespace Conformance
