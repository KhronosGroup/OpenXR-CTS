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
#include "utilities/types_and_constants.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <cstring>
#include <string>
#include <thread>

namespace Conformance
{
    TEST_CASE("validApiLayer", "")
    {
        GlobalData& globalData = GetGlobalData();

        // Layer for run-time conformance (and anything else global)
        StringVec enabledApiLayers = globalData.enabledAPILayerNames;
        // plus our test layer
        enabledApiLayers.push_back("XR_APILAYER_KHRONOS_conformance_test_layer");

        // Enable only the required platform extensions by default
        auto enabledExtensions = StringVec(globalData.requiredPlatformInstanceExtensions);

        XrInstance instance = XR_NULL_HANDLE_CPP;
        CleanupInstanceOnScopeExit cleanup(instance);

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};

        strcpy(createInfo.applicationInfo.applicationName, "conformance test");
        createInfo.applicationInfo.applicationVersion = 1;
        // Leave engineName and engineVersion empty, which is valid usage.
        createInfo.applicationInfo.apiVersion = globalData.options.desiredApiVersionValue;

        if (globalData.requiredPlatformInstanceCreateStruct) {
            createInfo.next = globalData.requiredPlatformInstanceCreateStruct;
        }

        createInfo.enabledApiLayerCount = (uint32_t)enabledApiLayers.size();
        createInfo.enabledApiLayerNames = enabledApiLayers.data();
        createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
        createInfo.enabledExtensionNames = enabledExtensions.data();

        SECTION("XR_SUCCESS, only platform-required extensions enabled")
        {
            REQUIRE(XR_SUCCESS == xrCreateInstance(&createInfo, &instance));
        }
    }

}  // namespace Conformance
