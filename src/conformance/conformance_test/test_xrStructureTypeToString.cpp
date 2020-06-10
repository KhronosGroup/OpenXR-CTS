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
#include <openxr/openxr_reflection.h>

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
                CHECK(std::string(buffer) == value.second);
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
