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

#include <vector>
#include <string>
#include <sstream>
#include <cstring>

namespace Conformance
{

    TEST_CASE("xrStringToPath", "")
    {
        // XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* path);
        // XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char*
        // buffer);

        XrPath path;
        XrResult result;
        AutoBasicInstance instance;

        // A well-formed path name string must conform to the following rules:
        //   - Path name strings must be constructed entirely from characters on the following list.
        //       - Lower case ASCII letters : a - z
        //       - Numeric digits : 0 - 9
        //       - Dash : -
        //       - Underscore : _
        //       - Period : .
        //       - Forward Slash : /
        //   - Path name strings must start with a single forward slash character.
        //   - Path name strings must not contain two or more adjacent forward slash characters.
        //   - Path name strings must not contain two forward slash characters that are separated by only period characters.
        //   - Path name strings must not contain only period characters following the final forward slash character in the string.
        //   - The maximum string length for a path name string, including the terminating \0 character, is defined by
        //   XR_MAX_PATH_LENGTH.
        const char maxPathStr[] =
            "/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcdef"
            "/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcdef"
            "/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcdef"
            "/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcde";
        static_assert(sizeof(maxPathStr) == XR_MAX_PATH_LENGTH, "maxPathStr is not the required size");

        const char exceededMaxPathStr[] =
            "/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcdef"
            "/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcdef"
            "/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcdef"
            "/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcdef";
        static_assert(sizeof(exceededMaxPathStr) == (XR_MAX_PATH_LENGTH + 1), "exceededMaxPathStr is not the required size");

        SECTION("Checking expected results")
        {
            struct ExpectedResult
            {
                const char* pathStr;
                XrResult expectedResult;
                XrPath path;
            };

            std::vector<ExpectedResult> expectedStringToPathResults{{"/foo", XR_SUCCESS},
                                                                    {"/f/o", XR_SUCCESS},
                                                                    {"/foo/bar/baz", XR_SUCCESS},
                                                                    {"/.f", XR_SUCCESS},
                                                                    {"/f.", XR_SUCCESS},
                                                                    {"/a./.a/.a./a.a", XR_SUCCESS},
                                                                    {"/.....ok", XR_SUCCESS},
                                                                    {"/999", XR_SUCCESS},
                                                                    {"/a_9-z.", XR_SUCCESS},
                                                                    {"/-/_", XR_SUCCESS},
                                                                    {maxPathStr, XR_SUCCESS},

                                                                    {"", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"//", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/dont_end_with_slash/", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/a//a", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"\\a", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/ ", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/wha?", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/aaA", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"foo", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"oof/", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/../foo", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/.", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/../..", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {"/.", XR_ERROR_PATH_FORMAT_INVALID},
                                                                    {exceededMaxPathStr, XR_ERROR_PATH_FORMAT_INVALID}};

            // expected validity
            for (auto& value : expectedStringToPathResults) {
                CAPTURE(value.pathStr);
                result = xrStringToPath(instance, value.pathStr, &value.path);
                REQUIRE(ValidateResultAllowed("xrStringToPath", result));
                CHECK(result == value.expectedResult);
                if (result == XR_SUCCESS) {
                    CHECK(value.path != XR_NULL_PATH);
                }
            }

            // Validate that the above successfully created paths are returned as the same strings.
            for (auto& value : expectedStringToPathResults) {
                CAPTURE(value.pathStr);
                CAPTURE(value.path);
                char buffer[XR_MAX_PATH_LENGTH];
                uint32_t length;
                result = xrPathToString(instance, value.path, sizeof(buffer), &length, buffer);
                if (value.path != XR_NULL_PATH) {
                    REQUIRE(ValidateResultAllowed("xrPathToString", result));
                    REQUIRE(result == XR_SUCCESS);
                    CHECK(strequal(buffer, value.pathStr));
                }
            }
        }
        SECTION("Try exceeding path count")
        {
            // Given that there is no way to free an XrPath, some runtimes may not be able to deal with
            // future path creation for a different instance if the capacity is previously exceeded.
            // We may need to make this test optional because some runtimes will exhaust memory.

            std::vector<XrPath> pathVector;
            const size_t maxCountToTest = 1000;  // Could be as much as SIZE_MAX.
            auto makePathString = [](size_t i) {
                std::ostringstream oss;
                oss << "/" << i;
                return oss.str();
            };
            for (size_t i = 0; i < maxCountToTest; ++i) {

                std::string pathStr = makePathString(i);
                CAPTURE(result = xrStringToPath(instance, pathStr.c_str(), &path));
                REQUIRE(ValidateResultAllowed("xrStringToPath", result));

                if (XR_FAILED(result)) {  // At some point this must fail, but for what reason?
                    CHECK(result == XR_ERROR_PATH_COUNT_EXCEEDED);
                    break;
                }

                pathVector.push_back(path);
            }

            // Now validate that all the above successfully created are resolvable.
            for (size_t i = 0; i < pathVector.size(); ++i) {
                std::string expectedPathStr = makePathString(i);

                char buffer[XR_MAX_PATH_LENGTH];
                uint32_t length;
                REQUIRE(xrPathToString(instance, pathVector[i], sizeof(buffer), &length, buffer) == XR_SUCCESS);
                CHECK(buffer == expectedPathStr);
            }
        }

        // Invalid handle validation
        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            // xrStringToPath doesn't *require* runtimes to identify invalid instance handles.
            // Exercise XR_ERROR_HANDLE_INVALID
            result = xrStringToPath(XR_NULL_HANDLE_CPP, "/abcd", &path);
            REQUIRE(ValidateResultAllowed("xrStringToPath", result));
            REQUIRE(result == XR_ERROR_HANDLE_INVALID);

            result = xrStringToPath(GetGlobalData().invalidInstance, "/abcd", &path);  // To do: pick a better handle.
            REQUIRE(ValidateResultAllowed("xrStringToPath", result));
            REQUIRE(result == XR_ERROR_HANDLE_INVALID);
        }
    }

}  // namespace Conformance
