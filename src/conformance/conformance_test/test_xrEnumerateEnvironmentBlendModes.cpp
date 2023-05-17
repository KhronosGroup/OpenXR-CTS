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
#include "matchers.h"
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

using Catch::Matchers::VectorContains;

#define AS_LIST(name, val) name,
constexpr XrViewConfigurationType KnownViewTypes[] = {XR_LIST_ENUM_XrViewConfigurationType(AS_LIST)};
#undef AS_LIST

namespace Conformance
{

    TEST_CASE("xrEnumerateEnvironmentBlendModes", "")
    {
        GlobalData& globalData = GetGlobalData();

        AutoBasicInstance instance(AutoBasicInstance::createSystemId);

        // Exercise all known view configurations types and ensure unsupported types fail.
        {
            // Get the list of supported view configurations
            uint32_t viewCount = 0;
            REQUIRE(XR_SUCCESS == xrEnumerateViewConfigurations(instance, instance.systemId, 0, &viewCount, nullptr));
            std::vector<XrViewConfigurationType> runtimeViewTypes(viewCount);
            REQUIRE(XR_SUCCESS ==
                    xrEnumerateViewConfigurations(instance, instance.systemId, viewCount, &viewCount, runtimeViewTypes.data()));

            // Test every view configuration type in the spec.
            for (XrViewConfigurationType viewType : KnownViewTypes) {
                CAPTURE(viewType);

                // Is this enum valid, check against enabled extensions.
                bool valid = IsViewConfigurationTypeEnumValid(viewType);

                const bool isSupportedType =
                    std::find(runtimeViewTypes.begin(), runtimeViewTypes.end(), viewType) != runtimeViewTypes.end();

                if (!valid) {
                    CHECK_MSG(valid == isSupportedType, "Can not support invalid view configuration type");
                }

                uint32_t countOutput;
                const XrResult res = xrEnumerateEnvironmentBlendModes(instance, instance.systemId, viewType, 0, &countOutput, nullptr);
                if (isSupportedType) {
                    REQUIRE_MSG(XR_SUCCESS == res, "Expected success for supported view configuration type " << viewType);
                    REQUIRE_MSG(countOutput > 0, "Expected non-zero list of blend modes");
                }
                else if (!valid) {
                    REQUIRE_THAT(res, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED}));
                    if (res == XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED) {
                        WARN(
                            "Runtime accepted an invalid enum value as unsupported, which makes it harder for apps to reason about the error.");
                    }
                }
                else {
                    REQUIRE_MSG(XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED == res,
                                "Unexpected return code for unsupported view config type");
                }
            }
        }

        XrResult result;
        std::vector<XrEnvironmentBlendMode> v;
        uint32_t countOutput;

        // Exercise zero input size.
        result = xrEnumerateEnvironmentBlendModes(instance, instance.systemId, globalData.options.viewConfigurationValue, 0, &countOutput,
                                                  nullptr);
        REQUIRE_MSG(result == XR_SUCCESS, "xrEnumerateEnvironmentBlendModes failure.");
        CHECK_MSG(countOutput >= 1, "xrEnumerateEnvironmentBlendModes must enumerate at least one blend mode");

        // Exercise XR_ERROR_SIZE_INSUFFICIENT
        if (countOutput >= 2) {  // Need at least two in order to exercise XR_ERROR_SIZE_INSUFFICIENT
            v.resize(countOutput, XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM);
            result = xrEnumerateEnvironmentBlendModes(instance, instance.systemId, globalData.options.viewConfigurationValue, 1,
                                                      &countOutput, v.data());
            REQUIRE(result == XR_ERROR_SIZE_INSUFFICIENT);
            REQUIRE_MSG(v[1] == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM,
                        "xrEnumerateEnvironmentBlendModes failure: data written beyond input count.");
            REQUIRE_MSG(countOutput == v.size(), "xrEnumerateEnvironmentBlendModes failure: required size changed unexpectectedly.");
        }

        // Exercise invalid system id
        {
            REQUIRE(XR_ERROR_SYSTEM_INVALID == xrEnumerateEnvironmentBlendModes(instance, XR_NULL_SYSTEM_ID,
                                                                                globalData.options.viewConfigurationValue, 0, &countOutput,
                                                                                nullptr));
        }

        // Exercise enough capacity
        v = std::vector<XrEnvironmentBlendMode>(countOutput, XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM);
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrEnumerateEnvironmentBlendModes(
            instance, instance.systemId, globalData.options.viewConfigurationValue, countOutput, &countOutput, v.data()));
        CHECK_THAT(v, VectorHasOnlyUniqueElements<XrEnvironmentBlendMode>());
        CHECK_THAT(v, !VectorContains(XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM));

        // To do: Verify that the values reported are within the set of valid values for the given runtime version.
        // This is best done in a forward-looking way via a generated table.
        // The following is close but not quite.
        if (globalData.runtimeMatchesAPIVersion) {
            for (auto blendMode : v) {
                if (blendMode < 1000000000) {  // If it's a core type
                    CHECK_THAT(blendMode, In<XrEnvironmentBlendMode>({XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE,
                                                                      XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND}));
                }
            }
        }
    }

}  // namespace Conformance
