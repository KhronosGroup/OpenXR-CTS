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
#include "utilities/types_and_constants.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>

namespace Conformance
{

    TEST_CASE("xrStructureTypeToString", "")
    {
        // XrResult xrStructureTypeToString(XrInstance instance, XrStructureType value, char buffer[XR_MAX_STRUCTURE_NAME_SIZE]);

        AutoBasicInstance instance;

        XrResult result;
        char buffer[XR_MAX_RESULT_STRING_SIZE];

        // valid structure types
        {
            // Exercise every known core structure type.
            const std::map<XrStructureType, const char*> structureTypeStringMap{XR_LIST_ENUM_XrStructureType(XRC_ENUM_NAME_PAIR)};

            for (auto value : structureTypeStringMap) {
                result = xrStructureTypeToString(instance, value.first, buffer);
                REQUIRE(ValidateResultAllowed("xrStructureTypeToString", result));
                REQUIRE(result == XR_SUCCESS);
                bool allowGeneratedName = false;
                uint64_t ext_num = 0;
                if (value.first >= XR_EXTENSION_ENUM_BASE) {
                    // This is an extension
                    ext_num = (value.first - XR_EXTENSION_ENUM_BASE) / XR_EXTENSION_ENUM_STRIDE;
                    if (!IsInstanceExtensionEnabled(ext_num)) {
                        // It's not enabled, so not enforcing that it must be the real value.
                        allowGeneratedName = true;
                    }
                }
                std::string returnedString(buffer);
                if (allowGeneratedName) {
                    CHECK_THAT(returnedString,
                               In<std::string>({std::string(value.second), "XR_UNKNOWN_STRUCTURE_TYPE_" + std::to_string(value.first)}));
                }
                else {
                    CHECK(returnedString == value.second);
                }
            }
        }

        // exercise XR_UNKNOWN_STRUCTURE_TYPE_XXX
        {
            const int UnknownType = 0x7ffffffe;  // 0x7fffffff is XR_STRUCTURE_TYPE_MAX_ENUM
            std::string expectedUnknownType = ("XR_UNKNOWN_STRUCTURE_TYPE_" + std::to_string(UnknownType));
            result = xrStructureTypeToString(instance, static_cast<XrStructureType>(UnknownType), buffer);
            REQUIRE(ValidateResultAllowed("xrStructureTypeToString", result));
            REQUIRE(result == XR_SUCCESS);
            CHECK(std::string(buffer) == expectedUnknownType);
        }

        // Exercise invalid handles
        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            // Exercise null instance
            {
                result = xrStructureTypeToString(XR_NULL_HANDLE_CPP, XR_TYPE_UNKNOWN, buffer);
                REQUIRE(ValidateResultAllowed("xrStructureTypeToString", result));
                REQUIRE(result == XR_ERROR_HANDLE_INVALID);
            }

            // Exercise invalid instance
            {
                GlobalData& globalData = GetGlobalData();
                result = xrStructureTypeToString(globalData.invalidInstance, XR_TYPE_UNKNOWN, buffer);
                REQUIRE(ValidateResultAllowed("xrStructureTypeToString", result));
                REQUIRE(result == XR_ERROR_HANDLE_INVALID);
            }
        }
    }

}  // namespace Conformance
