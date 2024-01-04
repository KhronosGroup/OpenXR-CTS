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

    TEST_CASE("xrCreateInstance", "")
    {
        GlobalData& globalData = GetGlobalData();

        // XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance);
        // XrResult xrDestroyInstance(XrInstance instance);

        XrInstance instance = XR_NULL_HANDLE_CPP;
        CleanupInstanceOnScopeExit cleanup(instance);

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};

        strcpy(createInfo.applicationInfo.applicationName, "conformance test");
        createInfo.applicationInfo.applicationVersion = 1;
        // Leave engineName and engineVersion empty, which is valid usage.
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

        if (globalData.requiredPlatformInstanceCreateStruct)
            createInfo.next = globalData.requiredPlatformInstanceCreateStruct;

        // Layers enabled at least for run-time conformance
        StringVec enabledApiLayers = globalData.enabledAPILayerNames;

        // Call this to update createInfo after modifying enabledApiLayers.
        auto updateCreateInfoApiLayers = [&] {
            createInfo.enabledApiLayerCount = (uint32_t)enabledApiLayers.size();
            createInfo.enabledApiLayerNames = enabledApiLayers.data();
        };
        updateCreateInfoApiLayers();

        // Enable only the required platform extensions by default
        auto enabledExtensions = StringVec(globalData.requiredPlatformInstanceExtensions);

        // Call this to update createInfo after modifying enabledExtensions.
        auto updateCreateInfoExtensions = [&] {
            createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
            createInfo.enabledExtensionNames = enabledExtensions.data();
        };
        updateCreateInfoExtensions();

        SECTION("XR_SUCCESS, only platform-required extensions enabled")
        {
            REQUIRE_RESULT_SUCCEEDED(xrCreateInstance(&createInfo, &instance));
        }

        SECTION("all configured extensions enabled")
        {
            enabledExtensions = globalData.enabledInstanceExtensionNames;
            updateCreateInfoExtensions();

            SECTION("XR_SUCCESS")
            {
                CHECK_RESULT_SUCCEEDED(xrCreateInstance(&createInfo, &instance));
            }

            SECTION("xrCreateInstance unrecognized extension")
            {
                InsertUnrecognizableExtension(&createInfo);
                CHECK_RESULT_SUCCEEDED(xrCreateInstance(&createInfo, &instance));
                RemoveUnrecognizableExtension(&createInfo);
            }

            SECTION("XR_SUCCESS in repetition")
            {
                for (int i = 0; i < 20; ++i) {
                    INFO("Iteration " << i);
                    AutoBasicInstance instanceTemp(((i % 4) < 2) ? AutoBasicInstance::createSystemId : 0);
                }
            }
            SECTION("XR_ERROR_EXTENSION_NOT_PRESENT, due to name case difference")
            {
                if (!enabledExtensions.empty()) {  // If there's anything to test...
                    std::string extensionNameFlipped(enabledExtensions[0]);
                    FlipCase(extensionNameFlipped);
                    enabledExtensions.set(0, extensionNameFlipped);
                    updateCreateInfoExtensions();

                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_EXTENSION_NOT_PRESENT);
                }
                else {
                    WARN("Skipped, no extensions enabled");
                }
            }

            SECTION("XR_ERROR_EXTENSION_NOT_PRESENT, due to bogus name")
            {
                enabledExtensions.push_back("nonexistant_extension");
                updateCreateInfoExtensions();

                CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_EXTENSION_NOT_PRESENT);
            }

            SECTION("XR_ERROR_API_LAYER_NOT_PRESENT, due to name case difference")
            {
                if (!enabledApiLayers.empty()) {  // If there's anything to test...
                    std::string apiLayerNameFlipped(enabledApiLayers[0]);
                    FlipCase(apiLayerNameFlipped);
                    enabledApiLayers.set(0, apiLayerNameFlipped);
                    updateCreateInfoApiLayers();

                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_API_LAYER_NOT_PRESENT);
                }
            }

            SECTION("XR_ERROR_API_LAYER_NOT_PRESENT, due to bogus name")
            {
                enabledApiLayers.push_back("nonexistant_api_layer");
                updateCreateInfoApiLayers();
                CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_API_LAYER_NOT_PRESENT);
            }

            SECTION("Supply extreme but valid additional data in applicationInfo")
            {
                strcpy(createInfo.applicationInfo.applicationName, XRC_UTF8_VALID_EXERCISE_STR);
                strcpy(createInfo.applicationInfo.engineName, XRC_UTF8_VALID_EXERCISE_STR);
                createInfo.applicationInfo.engineVersion = UINT32_MAX;

                REQUIRE_RESULT_SUCCEEDED(xrCreateInstance(&createInfo, &instance));
            }

            SECTION("API version with different major version than the runtime version")
            {
                // Currently there's no core API to get the runtime's API version.
                // We have our GetRuntimeMajorMinorVersion function which we could use.
                const uint32_t runtimeMajorAPIVersion = 99;

                SECTION("Application requesting too high of API")
                {
                    // Test application API version that's higher than the runtime supported api version, so XR_ERROR_API_VERSION_UNSUPPORTED.
                    createInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(runtimeMajorAPIVersion + 1, 0, 0);
                    CAPTURE(createInfo.applicationInfo.apiVersion);
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_API_VERSION_UNSUPPORTED);
                }

                // Test application API version that's lower than the runtime supported api version, so XR_ERROR_API_VERSION_UNSUPPORTED.
                SECTION("Application requesting too low of API")
                {
                    createInfo.applicationInfo.apiVersion = 1;
                    CAPTURE(createInfo.applicationInfo.apiVersion);
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_API_VERSION_UNSUPPORTED);
                }
                // Test application API version that's lower than the runtime supported api version, so XR_ERROR_API_VERSION_UNSUPPORTED.
                SECTION("Application requesting version 0")
                {
                    createInfo.applicationInfo.apiVersion = 0;
                    CAPTURE(createInfo.applicationInfo.apiVersion);
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_API_VERSION_UNSUPPORTED);
                }
            }

            SECTION("No createInfo")
            {
                CHECK(xrCreateInstance(nullptr, &instance) == XR_ERROR_VALIDATION_FAILURE);
            }

            SECTION("No instance")
            {
                CHECK(xrCreateInstance(&createInfo, nullptr) == XR_ERROR_VALIDATION_FAILURE);
            }

            SECTION("Invalid createInfo")
            {
                SECTION("Invalid createInfo.type")
                {
                    CAPTURE(createInfo.type = XR_TYPE_SYSTEM_GET_INFO);  // wrong type on purpose!
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_VALIDATION_FAILURE);
                }

                SECTION("Invalid createInfo.createFlags")
                {
                    CAPTURE(createInfo.createFlags = 0x42);
                    // "There are currently no instance creation flags. This is reserved for future use."
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_VALIDATION_FAILURE);
                }
                SECTION("Empty application name")
                {
                    createInfo.applicationInfo.applicationName[0] = 0;
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_NAME_INVALID);
                }
                SECTION("Empty engine name")
                {
                    createInfo.applicationInfo.engineName[0] = 0;
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_SUCCESS);
                }
                SECTION("Too long application name")
                {
                    for (size_t i = 0; i < XR_MAX_APPLICATION_NAME_SIZE; ++i) {
                        createInfo.applicationInfo.applicationName[i] = 'a';
                    }
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_NAME_INVALID);
                }
                SECTION("Too long engine name")
                {
                    for (size_t i = 0; i < XR_MAX_APPLICATION_NAME_SIZE; ++i) {
                        createInfo.applicationInfo.engineName[i] = 'e';
                    }
                    CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_NAME_INVALID);
                }
                /*
                     * This test is required for Android only:
                     * "The XrInstanceCreateInfoAndroidKHR struct must be provided as the next pointer in 
                     *  the XrInstanceCreateInfo struct when calling xrCreateInstance."
                     * "If the XrInstanceCreateInfo struct contains a platform-specific extension for a platform 
                     *  other than the target platform, XR_ERROR_INITIALIZATION_FAILED will be returned. The same 
                     *  is true if a mandatory platform-specific extension is defined for the target platform but 
                     *  no matching extension struct is provided in XrInstanceCreateInfo."
                     * 
                     * TODO: Do platform specific tests.
                    SECTION("Missing platform struct")
                    {
                        const void* ext = createInfo.next;
                        createInfo.next = nullptr;
                        CHECK(xrCreateInstance(&createInfo, &instance) == XR_ERROR_INITIALIZATION_FAILED);
                        createInfo.next = ext;
                    }
                    */
                // TODO:
                // Test for (if this requirement sticks):
                // "If the XrInstanceCreateInfo struct contains a platform-specific extension for a platform other
                // than the target platform, XR_ERROR_INITIALIZATION_FAILED will be returned."
            }

            // Exercise creating an instance with API layers and instance extensions.
            // To do: Enable any layers and extensions available.
        }
    }

    TEST_CASE("xrDestroyInstance", "")
    {
        SECTION("null handle")
        {
            // destruction of a real instance is done during these test over and over again,
            // only test missing: try to destroy NULL
            CHECK(xrDestroyInstance(XR_NULL_HANDLE_CPP) == XR_ERROR_HANDLE_INVALID);
        }

        SECTION("destroy on a different thread to create")
        {
            for (int i = 0; i < 2; ++i) {
                CAPTURE(i);
                AutoBasicInstance instance;
                XrResult destroyResult = XR_ERROR_RUNTIME_FAILURE;
                std::thread t([&destroyResult, &instance] { destroyResult = xrDestroyInstance(instance); });
                t.join();
                REQUIRE(destroyResult == XR_SUCCESS);
            }
        }

        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            GlobalData& globalData = GetGlobalData();
            SECTION("invalid handle")
            {
                CHECK(xrDestroyInstance(globalData.invalidInstance) == XR_ERROR_HANDLE_INVALID);
            }
            SECTION("second destroy of instance")
            {
                AutoBasicInstance instance;
                CHECK(xrDestroyInstance(instance) == XR_SUCCESS);
                CHECK(xrDestroyInstance(instance) == XR_ERROR_HANDLE_INVALID);
            }
        }
    }

}  // namespace Conformance
