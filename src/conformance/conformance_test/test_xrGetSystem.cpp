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

    TEST_CASE("xrGetSystem", "")
    {
        // XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId);
        auto &globalData = GetGlobalData();

        AutoBasicInstance instance;

        XrResult result;
        XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO, nullptr, globalData.options.formFactorValue};

        // We require that for the conformance test to successfully complete, a system of the specified form factor must be present.
        XrSystemId systemId;
        result = xrGetSystem(instance, &systemGetInfo, &systemId);
        CHECK(ValidateResultAllowed("xrGetSystem", result));
        REQUIRE(result == XR_SUCCESS);

        SECTION("XR_ERROR_FORM_FACTOR_UNSUPPORTED")
        {
            // Exercise XR_ERROR_FORM_FACTOR_UNSUPPORTED
            systemGetInfo.formFactor = XR_FORM_FACTOR_MAX_ENUM;
            result = xrGetSystem(instance, &systemGetInfo, &systemId);
            CHECK(ValidateResultAllowed("xrGetSystem", result));
            CHECK(result == XR_ERROR_FORM_FACTOR_UNSUPPORTED);
        }

        // We don't have a good way to test XR_ERROR_FORM_FACTOR_UNAVAILABLE without
        // being able to conspire with the runtime to make it so.

        SECTION("xrGetSystem unrecognized extension")
        {
            // Runtimes should ignore unrecognized struct extensins.
            InsertUnrecognizableExtension(&systemGetInfo);
            result = xrGetSystem(instance, &systemGetInfo, &systemId);
            CHECK(ValidateResultAllowed("xrGetSystem", result));
            REQUIRE(result == XR_SUCCESS);
        }

        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            // Invalid handle validation
            {
                // Invalid handle validation
                {
                    // Exercise XR_ERROR_HANDLE_INVALID
                    result = xrGetSystem(XR_NULL_HANDLE_CPP, &systemGetInfo, &systemId);
                    CHECK(ValidateResultAllowed("xrGetSystem", result));
                    CHECK(result == XR_ERROR_HANDLE_INVALID);

                    result = xrGetSystem(globalData.invalidInstance, &systemGetInfo, &systemId);
                    CHECK(ValidateResultAllowed("xrGetSystem", result));
                    CHECK(result == XR_ERROR_HANDLE_INVALID);
                }
            }
        }
    }
}  // namespace Conformance
