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

#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/types_and_constants.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <string>

namespace Conformance
{

    TEST_CASE("xrGetInstanceProperties", "")
    {
        // XrResult xrGetInstanceProperties(XrInstance instance, XrInstanceProperties* instanceProperties);

        AutoBasicInstance instance;

        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        XrResult result;

        SECTION("xrGetInstanceProperties valid usage")
        {
            result = xrGetInstanceProperties(instance, &instanceProperties);
            REQUIRE(ValidateResultAllowed("xrGetInstanceProperties", result));
            CHECK_RESULT_SUCCEEDED(result);
            CHECK_MSG(instanceProperties.type == XR_TYPE_INSTANCE_PROPERTIES, "Struct type was modified by runtime");
            CHECK_MSG(instanceProperties.next == nullptr, "Struct next was modified by runtime");

            // Verify that the returned runtime name is valid.
            CHECK(ValidateFixedSizeString(instanceProperties.runtimeName, false));
        }

        SECTION("xrGetInstanceProperties unrecognized extension")
        {
            // Runtimes should ignore unrecognized struct extensins.
            InsertUnrecognizableExtension(&instanceProperties);
            result = xrGetInstanceProperties(instance, &instanceProperties);
            CHECK(ValidateResultAllowed("xrGetInstanceProperties", result));
            CHECK_RESULT_SUCCEEDED(result);
        }

        // Invalid handle validation
        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            SECTION("xrGetInstanceProperties NULL instance")
            {
                CHECK(xrGetInstanceProperties(XR_NULL_HANDLE_CPP, &instanceProperties) == XR_ERROR_HANDLE_INVALID);
            }

            SECTION("xrGetInstanceProperties invalid instance")
            {
                GlobalData& globalData = GetGlobalData();
                result = xrGetInstanceProperties(globalData.invalidInstance, &instanceProperties);
                REQUIRE(ValidateResultAllowed("xrGetInstanceProperties", result));
                CHECK(result == XR_ERROR_HANDLE_INVALID);
            }
        }
    }
}  // namespace Conformance
