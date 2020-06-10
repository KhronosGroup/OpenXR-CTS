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

    TEST_CASE("xrResultToString", "")
    {
        // XrResult xrResultToString(XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE]);

        AutoBasicInstance instance;

        XrResult result;
        char buffer[XR_MAX_RESULT_STRING_SIZE];

        // Exercise every known core xrResult.
        const ResultStringMap& resultStringMap = GetResultStringMap();

        for (auto value : resultStringMap) {
            result = xrResultToString(instance, value.first, buffer);
            REQUIRE(ValidateResultAllowed("xrResultToString", result));
            REQUIRE(result == XR_SUCCESS);
            CHECK(std::string(buffer) == value.second);
        }

        // Exercise XR_UNKNOWN_SUCCESS_XXX
        {
            const int UnknownSuccess = 0x7ffffffe;  // 0x7fffffff is XR_RESULT_MAX_ENUM.
            std::string expectedUnknownSuccess = ("XR_UNKNOWN_SUCCESS_" + std::to_string(UnknownSuccess));
            result = xrResultToString(instance, static_cast<XrResult>(UnknownSuccess), buffer);
            REQUIRE(ValidateResultAllowed("xrResultToString", result));
            REQUIRE(result == XR_SUCCESS);
            CHECK(std::string(buffer) == expectedUnknownSuccess);
        }

        // Exercise XR_UNKNOWN_FAILURE_XXX
        {
            std::string expectedUnknownFailure = ("XR_UNKNOWN_FAILURE_" + std::to_string((int)0x80000000));
            result = xrResultToString(instance, static_cast<XrResult>(0x80000000), buffer);
            REQUIRE(ValidateResultAllowed("xrResultToString", result));
            REQUIRE(result == XR_SUCCESS);
            CHECK(std::string(buffer) == expectedUnknownFailure);
        }

        // Exercise invalid handles
        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {

            // Exercise null instance
            {
                result = xrResultToString(XR_NULL_HANDLE_CPP, XR_SUCCESS, buffer);
                REQUIRE(ValidateResultAllowed("xrResultToString", result));
                REQUIRE(result == XR_ERROR_HANDLE_INVALID);
            }

            // Exercise invalid instance
            {
                GlobalData& globalData = GetGlobalData();
                result = xrResultToString(globalData.invalidInstance, XR_SUCCESS, buffer);
                REQUIRE(ValidateResultAllowed("xrResultToString", result));
                REQUIRE(result == XR_ERROR_HANDLE_INVALID);
            }
        }
    }

}  // namespace Conformance
