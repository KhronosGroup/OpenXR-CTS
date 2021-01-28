// Copyright (c) 2019-2021, The Khronos Group Inc.
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
#include <openxr/openxr.h>
#include <catch2/catch.hpp>

namespace Conformance
{
    TEST_CASE("xrRequestExitSession", "")
    {
        AutoBasicSession session(AutoBasicSession::createSession);

        SECTION("Session Not Running")
        {
            REQUIRE(XR_ERROR_SESSION_NOT_RUNNING == xrRequestExitSession(session));
        }

        // Successful case is tested in test_SessionState.cpp
    }

}  // namespace Conformance
