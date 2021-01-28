// Copyright (c) 2019-2021, The Khronos Group Inc.
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

    TEST_CASE("xrGetSystemProperties", "")
    {
        auto &globalData = GetGlobalData();

        AutoBasicInstance instance;

        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};

        // Test invalid system id
        {
            REQUIRE(XR_ERROR_SYSTEM_INVALID == xrGetSystemProperties(instance, XR_NULL_SYSTEM_ID, &systemProperties));
        }

        XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO, nullptr, globalData.options.formFactorValue};
        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        REQUIRE(XR_SUCCESS == xrGetSystem(instance, &systemGetInfo, &systemId));

        REQUIRE(XR_SUCCESS == xrGetSystemProperties(instance, systemId, &systemProperties));
        CHECK(systemProperties.systemId == systemId);
        CHECK(strlen(systemProperties.systemName) > 0);
        CHECK(systemProperties.graphicsProperties.maxLayerCount >= XR_MIN_COMPOSITION_LAYERS_SUPPORTED);
        CHECK(systemProperties.graphicsProperties.maxSwapchainImageHeight > 0);
        CHECK(systemProperties.graphicsProperties.maxSwapchainImageWidth > 0);
    }
}  // namespace Conformance
