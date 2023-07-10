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

#include "utilities/utils.h"
#include "conformance_utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <cstring>
#include <string>
#include <vector>

namespace Conformance
{

    TEST_CASE("xrEnumerateApiLayerProperties", "")
    {
        // Question: Will the loader ever call a runtime implementation of xrEnumerateApiLayerProperties?
        //    or will it always handle it only internally. Oculus would like to be able to have internal
        //    API layers that can be used regardless of the loader.

        // XrResult xrEnumerateApiLayerProperties)(uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrApiLayerProperties*
        // properties);

        auto ValidateProperties = [](std::vector<XrApiLayerProperties>& v, size_t countValid) -> void {
            size_t i;

            // Verify entries that should have been written
            for (i = 0; i < countValid; ++i) {
                CHECK(ValidateFixedSizeString(v[i].layerName, false));

                // CHECK(v[i].layerVersion); Any number is valid, so nothing to test.
                CHECK(ValidateFixedSizeString(v[i].description, false));
                CHECK(v[i].type == XR_TYPE_API_LAYER_PROPERTIES);
            }

            // Verify entries that should not have been written.
            for (i = countValid; i < v.size(); ++i) {
                CHECK(!v[i].layerName[0]);
            }
        };

        // See the OpenXR Fundamentals section Buffer Size Parameter Behavior for more info.
        std::vector<XrApiLayerProperties> v;

        uint32_t propertyCount;
        XrResult result;

        // "Independent of elementCapacityInput or elements parameters, elementCountOutput must be a valid pointer,
        // and the function sets elementCountOutput." - 2.11
        result = xrEnumerateApiLayerProperties(0, nullptr, nullptr);
        REQUIRE(ValidateResultAllowed("xrEnumerateApiLayerProperties", result));
        REQUIRE(result == XR_ERROR_VALIDATION_FAILURE);

        // Exercise typical two-call usage
        result = xrEnumerateApiLayerProperties(0, &propertyCount, nullptr);
        REQUIRE(ValidateResultAllowed("xrEnumerateApiLayerProperties", result));
        REQUIRE(result == XR_SUCCESS);

        REQUIRE_NOTHROW(v.resize(propertyCount, XrApiLayerProperties{XR_TYPE_API_LAYER_PROPERTIES, nullptr}));

        result = xrEnumerateApiLayerProperties(propertyCount, &propertyCount, v.data());
        REQUIRE(ValidateResultAllowed("xrEnumerateApiLayerProperties", result));
        REQUIRE(result == XR_SUCCESS);
        {
            INFO("Shouldn't return more elements than requested.");
            REQUIRE(propertyCount <= static_cast<uint32_t>(v.size()));
        }
        // Resize to shrink, if required.
        v.resize(propertyCount);

        ValidateProperties(v, propertyCount);

        // "Independent of elementCapacityInput or elements parameters, elementCountOutput must be a valid pointer,
        // and the function sets elementCountOutput." - 2.11
        if (propertyCount > 0) {
            v = std::vector<XrApiLayerProperties>(propertyCount, XrApiLayerProperties{XR_TYPE_API_LAYER_PROPERTIES, nullptr});
            result = xrEnumerateApiLayerProperties(propertyCount, nullptr, v.data());
            REQUIRE(ValidateResultAllowed("xrEnumerateApiLayerProperties", result));
            REQUIRE(result == XR_ERROR_VALIDATION_FAILURE);
        }

        // Exercise XR_ERROR_SIZE_INSUFFICIENT, which is returned if the input capacity is > 0 but
        // less than needed. If input capacity is 0 then XR_SUCCESS is returned.
        if (propertyCount > 1)  // No way to test XR_ERROR_SIZE_INSUFFICIENT unless propertyCount > 1.
        {
            v = std::vector<XrApiLayerProperties>(propertyCount, XrApiLayerProperties{XR_TYPE_API_LAYER_PROPERTIES, nullptr});
            result = xrEnumerateApiLayerProperties(propertyCount - 1, &propertyCount, v.data());
            REQUIRE(ValidateResultAllowed("xrEnumerateApiLayerProperties", result));
            CHECK(result == XR_ERROR_SIZE_INSUFFICIENT);
        }

        // Exercise that the property count doesn't change based on the input capacity.
        uint32_t propertyCount2 = (propertyCount * 2);
        v = std::vector<XrApiLayerProperties>(propertyCount2, XrApiLayerProperties{XR_TYPE_API_LAYER_PROPERTIES, nullptr});
        result = xrEnumerateApiLayerProperties(propertyCount2, &propertyCount2, v.data());
        REQUIRE(ValidateResultAllowed("xrEnumerateApiLayerProperties", result));
        REQUIRE(result == XR_SUCCESS);
        CHECK(propertyCount2 == propertyCount);
        ValidateProperties(v, propertyCount2);

        SECTION("xrEnumerateApiLayerProperties unrecognized extension")
        {
            // Runtimes should ignore unrecognized struct extensions.
            InsertUnrecognizableExtensionArray(v.data(), v.size());
            result = xrEnumerateApiLayerProperties(propertyCount, &propertyCount, v.data());
            REQUIRE(ValidateResultAllowed("xrEnumerateApiLayerProperties", result));
            REQUIRE(result == XR_SUCCESS);
        }
    }

}  // namespace Conformance
