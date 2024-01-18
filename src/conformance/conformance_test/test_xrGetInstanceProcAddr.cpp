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
#include "matchers.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <cstring>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Conformance
{

    TEST_CASE("xrGetInstanceProcAddr", "")
    {
        GlobalData& globalData = GetGlobalData();
        const FunctionInfoMap& functionInfoMap = GetFunctionInfoMap();

        // Exercise XR_NULL_HANDLE
        {
            // Because an application can call xrGetInstanceProcAddr before creating an instance,
            // xrGetInstanceProcAddr returns a valid function pointer when the instance parameter is
            // XR_NULL_HANDLE and the name parameter is one of the following strings: ...

            for (auto& functionInfo : functionInfoMap) {
                CAPTURE(functionInfo.first);
                CAPTURE(functionInfo.second.nullInstanceOk);

                PFN_xrVoidFunction f;
                XrResult result = xrGetInstanceProcAddr(XR_NULL_HANDLE_CPP, functionInfo.first.c_str(), &f);

                // xrInitializeLoaderKHR support is optional and requires special handling.
                if (functionInfo.first == "xrInitializeLoaderKHR") {
                    CHECK_THAT(result, In<XrResult>({XR_SUCCESS, XR_ERROR_FUNCTION_UNSUPPORTED}));
                }
                else {
                    XrResult expectedResult = (functionInfo.second.nullInstanceOk ? XR_SUCCESS : XR_ERROR_HANDLE_INVALID);
                    CHECK(result == expectedResult);
                }

                if (result == XR_SUCCESS) {
                    CHECK_MSG(nullptr != f, "Unexpected null function pointer returned from successful xrGetInstanceProcAddr call");
                }
                else {
                    CHECK_MSG(nullptr == f, "Unexpected non-null function pointer returned from failed xrGetInstanceProcAddr call");
                }
                // To do: We should call the succeeding functions to verify they resolved OK.
            }
        }

        // Get all functions with a valid instance:
        {
            AutoBasicInstance instance;

            for (auto& functionInfo : functionInfoMap) {
                XrResult expectedResult = XR_SUCCESS;
                if (functionInfo.second.requiredExtension != nullptr) {
                    // this function belongs to an extension: if the extension was enabled, the function pointer
                    // should get returned, otherwise XR_ERROR_FUNCTION_UNSUPPORTED is expected

                    bool thisExtensionIsEnabled = false;
                    const char* const* enabledExtensions = globalData.enabledInstanceExtensionNames.data();
                    for (size_t i = 0; i < globalData.enabledInstanceExtensionNames.size(); ++i) {
                        if (strcmp(functionInfo.second.requiredExtension, enabledExtensions[i]) == 0) {
                            thisExtensionIsEnabled = true;
                            break;
                        }
                    }
                    if (!thisExtensionIsEnabled)
                        expectedResult = XR_ERROR_FUNCTION_UNSUPPORTED;
                }
                CAPTURE(functionInfo.first);

                PFN_xrVoidFunction f;
                XrResult result = xrGetInstanceProcAddr(instance, functionInfo.first.c_str(), &f);

                // xrInitializeLoaderKHR support is optional and requires special handling.
                if (functionInfo.first == "xrInitializeLoaderKHR") {
                    CHECK_THAT(result, In<XrResult>({XR_SUCCESS, XR_ERROR_FUNCTION_UNSUPPORTED}));
                }
                else {
                    CHECK(result == expectedResult);
                }

                if (result == XR_SUCCESS) {
                    CHECK_MSG(nullptr != f, "Unexpected null function pointer returned from successful xrGetInstanceProcAddr call");
                }
                else {
                    CHECK_MSG(nullptr == f, "Unexpected non-null function pointer returned from failed xrGetInstanceProcAddr call");
                }
            }
        }

        // Try to get not existing functions:
        {
            AutoBasicInstance instance;

            {
                // "name must be a null-terminated UTF-8 string"
                // this means that name == nullptr is a validation failure:
                PFN_xrVoidFunction f;
                XrResult result = xrGetInstanceProcAddr(instance, nullptr, &f);
                CHECK(result == XR_ERROR_VALIDATION_FAILURE);
            }

            // test some illegale function names:
            std::vector<const char*> invalidFunctionNames{
                "", "a", "xr", "not a function", "xr", "xr*", "xrGetSystemDoesNotEndLikeThis", "xrGetSystem string is not terminated yet"};
            for (auto& functionName : invalidFunctionNames) {
                CAPTURE(functionName);
                PFN_xrVoidFunction f;
                XrResult result = xrGetInstanceProcAddr(instance, functionName, &f);
                CHECK(result == XR_ERROR_FUNCTION_UNSUPPORTED);
                REQUIRE_MSG(nullptr == f, "A NULL pointer has to get returned");
            }
        }

        // Invalid handle validation
        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            for (auto& functionInfo : functionInfoMap) {
                PFN_xrVoidFunction f;
                XrResult result = xrGetInstanceProcAddr(globalData.invalidInstance, functionInfo.first.c_str(), &f);

                // The OpenXR loader returns XR_ERROR_HANDLE_INVALID for this case, so we currently
                // implement our test to reflect that. If the application were to provide its own
                // loader or the runtime was to be directly used without a loader, currently they
                // would be expected to follow suit, as they might otherwise return XR_ERROR_VALIDATION_FAILURE.
                CHECK_MSG(result == XR_ERROR_HANDLE_INVALID,
                          "While testing invalid handle xrGetInstanceProcAddr for " << functionInfo.first.c_str());

                // To do: We should call the succeeding functions to verify they resolved OK.
            }
        }
    }
}  // namespace Conformance
