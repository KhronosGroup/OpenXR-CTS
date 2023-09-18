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
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <cstring>
#include <string>
#include <vector>

namespace Conformance
{

    TEST_CASE("xrEnumerateInstanceExtensionProperties", "")
    {
        // XrResult xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput, uint32_t*
        // propertyCountOutput, XrExtensionProperties* properties);

        // We do not exercise calling xrEnumerateInstanceExtensionProperties for a specific API layer,
        // as that's the job of a layer-specific test.

        auto ValidateProperties = [](std::vector<XrExtensionProperties>& v, size_t countValid) -> void {
            size_t i;

            // Verify entries that should have been written
            for (i = 0; i < countValid; ++i) {
                CHECK(ValidateFixedSizeString(v[i].extensionName, false));
                // CHECK(v[i].specVersion >= ___); // specVersion is the extension version. How to test?
                CHECK(v[i].type == XR_TYPE_EXTENSION_PROPERTIES);
            }

            // Verify entries that should not have been written.
            for (i = countValid; i < v.size(); ++i) {
                CHECK(!v[i].extensionName[0]);
            }
        };

        // See the OpenXR Fundamentals section Buffer Size Parameter Behavior for more info.
        std::vector<XrExtensionProperties> v;

        uint32_t propertyCount;
        XrResult result;

        // "Independent of elementCapacityInput or elements parameters, elementCountOutput
        // must be a valid pointer, and the function sets elementCountOutput." - 2.11
        result = xrEnumerateInstanceExtensionProperties(nullptr, 0, nullptr, nullptr);
        REQUIRE(ValidateResultAllowed("xrEnumerateInstanceExtensionProperties", result));
        REQUIRE(result == XR_ERROR_VALIDATION_FAILURE);

        // Exercise typical two-call usage
        result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &propertyCount, nullptr);
        CAPTURE(propertyCount);
        REQUIRE(ValidateResultAllowed("xrEnumerateInstanceExtensionProperties", result));
        REQUIRE(result == XR_SUCCESS);

        REQUIRE_NOTHROW(v.resize(propertyCount, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES, nullptr}));

        result = xrEnumerateInstanceExtensionProperties(nullptr, propertyCount, &propertyCount, v.data());
        REQUIRE(ValidateResultAllowed("xrEnumerateInstanceExtensionProperties", result));
        REQUIRE(result == XR_SUCCESS);
        ValidateProperties(v, propertyCount);

        SECTION("xrEnumerateInstanceExtensionProperties unrecognized extension")
        {
            // Runtime/loader should ignore unrecognized struct extensions.
            InsertUnrecognizableExtensionArray(v.data(), v.size());
            result = xrEnumerateInstanceExtensionProperties(nullptr, propertyCount, &propertyCount, v.data());
            REQUIRE(ValidateResultAllowed("xrEnumerateInstanceExtensionProperties", result));
            REQUIRE(result == XR_SUCCESS);
        }

        // Exercise XR_ERROR_SIZE_INSUFFICIENT, which is returned if the input capacity is > 0 but
        // less than needed. If input capacity is 0 then XR_SUCCESS is returned.
        if (propertyCount > 1)  // No way to test XR_ERROR_SIZE_INSUFFICIENT unless propertyCount > 1.
        {
            v = std::vector<XrExtensionProperties>(propertyCount, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES, nullptr});
            result = xrEnumerateInstanceExtensionProperties(nullptr, propertyCount - 1, &propertyCount, v.data());
            REQUIRE(ValidateResultAllowed("xrEnumerateInstanceExtensionProperties", result));
            CHECK(result == XR_ERROR_SIZE_INSUFFICIENT);
        }

        // Exercise that the property count doesn't change based on the input capacity.
        uint32_t propertyCount2 = (propertyCount * 2);
        v = std::vector<XrExtensionProperties>(propertyCount2, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES, nullptr});
        result = xrEnumerateInstanceExtensionProperties(nullptr, propertyCount2, &propertyCount2, v.data());
        REQUIRE(ValidateResultAllowed("xrEnumerateInstanceExtensionProperties", result));
        REQUIRE(result == XR_SUCCESS);
        CHECK(propertyCount2 == propertyCount);
        ValidateProperties(v, propertyCount2);

        // ask for extensions of a not existing layer
        uint32_t layerPropertyCount;
        result = xrEnumerateInstanceExtensionProperties("NotARealLayerName_42", 0, &layerPropertyCount, nullptr);
        REQUIRE(ValidateResultAllowed("xrEnumerateInstanceExtensionProperties", result));
        REQUIRE(result == XR_ERROR_API_LAYER_NOT_PRESENT);

        SECTION("xrEnumerateInstanceExtensionProperties unrecognized extension")
        {
            // Runtimes should ignore unrecognized struct extensions.
            InsertUnrecognizableExtensionArray(v.data(), v.size());
            result = xrEnumerateInstanceExtensionProperties(nullptr, propertyCount, &propertyCount, v.data());
            REQUIRE(ValidateResultAllowed("xrEnumerateInstanceExtensionProperties", result));
            REQUIRE(result == XR_SUCCESS);
        }
    }

}  // namespace Conformance
