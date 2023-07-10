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

#include "conformance_utils.h"
#include "conformance_framework.h"
#include "two_call.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <vector>

namespace Conformance
{
    TEST_CASE("xrEnumerateSwapchainFormats")
    {
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession);

        if (GetGlobalData().IsUsingGraphicsPlugin()) {
            INFO("A non-headless session should provide at least one swapchain format");
            auto formats = REQUIRE_TWO_CALL(int64_t, {}, xrEnumerateSwapchainFormats, session);
            REQUIRE(formats.size() > 0);
        }
        else {

            INFO("Headless shouldn't provide any swapchain formats");
            auto formats = REQUIRE_TWO_CALL(int64_t, {}, xrEnumerateSwapchainFormats, session);
            REQUIRE(formats.empty());
        }
    }

}  // namespace Conformance
