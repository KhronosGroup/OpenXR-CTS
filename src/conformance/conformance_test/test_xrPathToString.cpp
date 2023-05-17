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

#include "utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

namespace Conformance
{

    TEST_CASE("xrPathToString", "")
    {
        // XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char*
        // buffer);

        AutoBasicInstance instance;

        XrPath path;
        XrResult result;
        const char pathStr[] = "/abc/def";

        // We did some testing of xrPathToString already within the xrStringToPath test.
        // Let's exercise the API parameters here.
        result = xrStringToPath(instance, pathStr, &path);
        REQUIRE(ValidateResultAllowed("xrStringToPath", result));
        REQUIRE(result == XR_SUCCESS);
        CHECK(path != XR_NULL_PATH);

        // Exercise the various capacity/size options behavior.
        char buffer[XR_MAX_PATH_LENGTH];
        uint32_t length;

        result = xrPathToString(instance, path, 0, &length, nullptr);
        REQUIRE(ValidateResultAllowed("xrPathToString", result));
        REQUIRE(result == XR_SUCCESS);
        CHECK(length == sizeof(pathStr));

        result = xrPathToString(instance, path, 1, &length, buffer);
        REQUIRE(ValidateResultAllowed("xrPathToString", result));
        REQUIRE(result == XR_ERROR_SIZE_INSUFFICIENT);
        CHECK(length == sizeof(pathStr));

        result = xrPathToString(instance, path, sizeof(buffer), &length, buffer);
        REQUIRE(result == XR_SUCCESS);
        CHECK(length == sizeof(pathStr));
        CHECK(strequal(buffer, pathStr));
    }

}  // namespace Conformance
