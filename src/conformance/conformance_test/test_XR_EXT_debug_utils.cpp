// Copyright (c) 2017-2024, The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
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
#include "conformance_framework.h"
#include "utilities/throw_helpers.h"
#include "common/hex_and_handles.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>
#include <string>
#include <cstring>

namespace Conformance
{
    // It would be nice to have these functions as lambdas per test case or section but
    // lambdas will not account for XRAPI_ATTR, XRAPI_CALL calling conventions for all systems.

    static XRAPI_ATTR XrBool32 XRAPI_CALL myOutputDebugString(XrDebugUtilsMessageSeverityFlagsEXT, XrDebugUtilsMessageTypeFlagsEXT,
                                                              const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
    {
        REQUIRE(userData == nullptr);
        WARN(callbackData->message);
        return XR_FALSE;
    };

    static XRAPI_ATTR XrBool32 XRAPI_CALL myDebugBreak(XrDebugUtilsMessageSeverityFlagsEXT, XrDebugUtilsMessageTypeFlagsEXT,
                                                       const XrDebugUtilsMessengerCallbackDataEXT*, void* userData)
    {
        REQUIRE(userData == nullptr);
        FAIL();
        return XR_FALSE;
    };

    static XRAPI_ATTR XrBool32 XRAPI_CALL myStdOutLogger(XrDebugUtilsMessageSeverityFlagsEXT, XrDebugUtilsMessageTypeFlagsEXT,
                                                         const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
    {
        REQUIRE(userData == nullptr);
        INFO(callbackData->message);
        return XR_FALSE;
    };

    struct DebugUtilsCallbackInfo
    {
        XrDebugUtilsMessageSeverityFlagsEXT messageSeverity;
        XrDebugUtilsMessageTypeFlagsEXT messageTypes;
        XrDebugUtilsMessengerCallbackDataEXT callbackData;

        std::vector<XrDebugUtilsObjectNameInfoEXT> objects;
        std::vector<XrDebugUtilsLabelEXT> sessionLabels;

        // All of the debug utils structs contain strings which are not valid
        // for us to reference after the callback function has returned. We will
        // store a vector of strings that were used with each of our callbacks
        // to avoid this problem.
        std::vector<std::shared_ptr<std::string>> strings;
    };

    static XRAPI_ATTR XrBool32 XRAPI_CALL addToDebugUtilsCallbackInfoVector(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                                                            XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                                            const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                                            void* userData)
    {
        REQUIRE(userData != nullptr);
        auto pMessages = reinterpret_cast<std::vector<DebugUtilsCallbackInfo>*>(userData);

        DebugUtilsCallbackInfo callbackInfo;
        callbackInfo.messageSeverity = messageSeverity;
        callbackInfo.messageTypes = messageTypes;
        callbackInfo.callbackData = *callbackData;

        if (callbackData->messageId != nullptr) {
            auto tmp = std::make_shared<std::string>(callbackData->messageId);
            callbackInfo.strings.push_back(tmp);
            callbackInfo.callbackData.messageId = tmp->c_str();
        }

        if (callbackData->functionName != nullptr) {
            auto tmp = std::make_shared<std::string>(callbackData->functionName);
            callbackInfo.strings.push_back(tmp);
            callbackInfo.callbackData.functionName = tmp->c_str();
        }

        if (callbackData->message != nullptr) {
            auto tmp = std::make_shared<std::string>(callbackData->message);
            callbackInfo.strings.push_back(tmp);
            callbackInfo.callbackData.message = tmp->c_str();
        }

        for (uint32_t i = 0; i < callbackData->objectCount; ++i) {
            callbackInfo.objects.push_back(callbackData->objects[i]);
            if (callbackData->objects[i].objectName != nullptr) {
                auto tmp = std::make_shared<std::string>(callbackData->objects[i].objectName);
                callbackInfo.strings.push_back(tmp);
                callbackInfo.objects[i].objectName = tmp->c_str();
            }
        }
        callbackInfo.callbackData.objects = callbackInfo.objects.data();

        for (uint32_t i = 0; i < callbackData->sessionLabelCount; ++i) {
            callbackInfo.sessionLabels.push_back(callbackData->sessionLabels[i]);
            if (callbackData->sessionLabels[i].labelName != nullptr) {
                auto tmp = std::make_shared<std::string>(callbackData->sessionLabels[i].labelName);
                callbackInfo.strings.push_back(tmp);
                callbackInfo.sessionLabels[i].labelName = tmp->c_str();
            }
        }
        callbackInfo.callbackData.sessionLabels = callbackInfo.sessionLabels.data();

        pMessages->push_back(std::move(callbackInfo));

        return XR_FALSE;
    };

    static bool debugMessageExists(const std::vector<DebugUtilsCallbackInfo>& callbackInfos,
                                   XrDebugUtilsMessageSeverityFlagsEXT messageSeverity, XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                                   const XrDebugUtilsMessengerCallbackDataEXT* callbackData)
    {
        auto callbackDataMatches = [](const XrDebugUtilsMessengerCallbackDataEXT* a,
                                      const XrDebugUtilsMessengerCallbackDataEXT* b) -> bool {
            REQUIRE(a->type == XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT);
            REQUIRE(b->type == XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT);

            // We are not validating next chains match, but that should be ok.

            if ((a->messageId == nullptr) != (b->messageId == nullptr)) {
                return false;
            }
            if ((a->messageId != nullptr) && (b->messageId != nullptr)) {
                if (strcmp(a->messageId, b->messageId) != 0) {
                    return false;
                }
            }

            if ((a->functionName == nullptr) != (b->functionName == nullptr)) {
                return false;
            }
            if ((a->functionName != nullptr) && (b->functionName != nullptr)) {
                if (strcmp(a->functionName, b->functionName) != 0) {
                    return false;
                }
            }

            if ((a->message == nullptr) != (b->message == nullptr)) {
                return false;
            }
            if ((a->message != nullptr) && (b->message != nullptr)) {
                if (strcmp(a->message, b->message) != 0) {
                    return false;
                }
            }

            if (a->objectCount != b->objectCount) {
                return false;
            }
            for (uint32_t i = 0; i < a->objectCount; ++i) {
                REQUIRE(a->objects[i].type == XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
                REQUIRE(b->objects[i].type == XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);

                if (a->objects[i].objectType != b->objects[i].objectType) {
                    return false;
                }
                if (a->objects[i].objectHandle != b->objects[i].objectHandle) {
                    return false;
                }
                if ((a->objects[i].objectName == nullptr) != (b->objects[i].objectName == nullptr)) {
                    return false;
                }
                if ((a->objects[i].objectName != nullptr) && (b->objects[i].objectName != nullptr)) {
                    if (strcmp(a->objects[i].objectName, b->objects[i].objectName) != 0) {
                        return false;
                    }
                }
            }

            if (a->sessionLabelCount != b->sessionLabelCount) {
                return false;
            }
            for (uint32_t i = 0; i < a->sessionLabelCount; ++i) {
                REQUIRE(a->sessionLabels[i].type == XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
                REQUIRE(b->sessionLabels[i].type == XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);

                if ((a->sessionLabels[i].labelName == nullptr) != (b->sessionLabels[i].labelName == nullptr)) {
                    return false;
                }
                if ((a->sessionLabels[i].labelName != nullptr) && (b->sessionLabels[i].labelName != nullptr)) {
                    if (strcmp(a->sessionLabels[i].labelName, b->sessionLabels[i].labelName) != 0) {
                        return false;
                    }
                }
            }

            return true;
        };

        for (const auto& callbackInfo : callbackInfos) {
            if (callbackInfo.messageSeverity == messageSeverity && callbackInfo.messageTypes == messageTypes &&
                callbackDataMatches(&callbackInfo.callbackData, callbackData)) {
                return true;
            }
        }
        return false;
    }

    static const DebugUtilsCallbackInfo& findMessageByMessageId(const std::vector<DebugUtilsCallbackInfo>& callbackInfos,
                                                                const char* messageId)
    {
        {
            size_t messageMatchCount = 0;
            for (const auto& callbackInfo : callbackInfos) {
                if (strcmp(callbackInfo.callbackData.messageId, messageId) == 0) {
                    messageMatchCount++;
                }
            }
            REQUIRE(messageMatchCount == 1);
        }
        auto it = std::find_if(callbackInfos.begin(), callbackInfos.end(), [messageId](const auto& callbackInfo) {
            return strcmp(callbackInfo.callbackData.messageId, messageId) == 0;
        });
        REQUIRE(it != callbackInfos.end());
        return *it;
    }

    TEST_CASE("XR_EXT_debug_utils", "[XR_EXT_debug_utils]")
    {
        GlobalData& globalData = GetGlobalData();

        // The OpenXR loader implements XR_EXT_debug_utils so it should be very difficult for
        // a runtime to exist which doesn't support XR_EXT_debug_utils but let's check that it is
        // supported anyway.
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            // Runtime does not support extension - it should not be possible to get function pointers.
            AutoBasicInstance instance;
            ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreateDebugUtilsMessengerEXT");

            SKIP(XR_EXT_DEBUG_UTILS_EXTENSION_NAME " not supported");
        }

        SECTION("xrCreateInstance debug utils not enabled")
        {
            auto enabledApiLayers = StringVec(globalData.enabledAPILayerNames);
            // Enable only the required platform extensions by default
            auto enabledExtensions = StringVec(globalData.requiredPlatformInstanceExtensions);

            XrInstance instance{XR_NULL_HANDLE};
            CleanupInstanceOnScopeExit cleanup(instance);

            XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
            createInfo.next = globalData.requiredPlatformInstanceCreateStruct;

            strcpy(createInfo.applicationInfo.applicationName, "conformance test : XR_EXT_debug_utils");
            createInfo.applicationInfo.applicationVersion = 1;
            // Leave engineName and engineVersion empty, which is valid usage.
            createInfo.applicationInfo.apiVersion = globalData.options.desiredApiVersionValue;

            createInfo.enabledApiLayerCount = (uint32_t)enabledApiLayers.size();
            createInfo.enabledApiLayerNames = enabledApiLayers.data();

            createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
            createInfo.enabledExtensionNames = enabledExtensions.data();

            REQUIRE_RESULT(XR_SUCCESS, xrCreateInstance(&createInfo, &instance));

            ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreateDebugUtilsMessengerEXT");
        }

        SECTION("Create/Destroy with xrCreateInstance/xrDestroyInstance")
        {
            // To capture events that occur while creating or destroying an instance an application can link
            // an XrDebugUtilsMessengerCreateInfoEXT structure to the next element of the XrInstanceCreateInfo
            // structure given to xrCreateInstance.
            // Note that this behavior will be implicitly validated by AutoBasicInstance when skipDebugMessenger
            // is not passed as an option, but we have an explicit test for this behavior too.

            auto enabledApiLayers = StringVec(globalData.enabledAPILayerNames);

            // Enable only the required platform extensions by default
            auto enabledExtensions = StringVec(globalData.requiredPlatformInstanceExtensions);

            std::vector<DebugUtilsCallbackInfo> callbackInfo;

            XrDebugUtilsMessengerCreateInfoEXT debugInfo{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debugInfo.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                          XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugInfo.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugInfo.userCallback = addToDebugUtilsCallbackInfoVector;
            debugInfo.userData = reinterpret_cast<void*>(&callbackInfo);

            XrInstance instance{XR_NULL_HANDLE};
            CleanupInstanceOnScopeExit cleanup(instance);

            XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};

            enabledExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);

            strcpy(createInfo.applicationInfo.applicationName, "conformance test : XR_EXT_debug_utils");
            createInfo.applicationInfo.applicationVersion = 1;
            // Leave engineName and engineVersion empty, which is valid usage.
            createInfo.applicationInfo.apiVersion = globalData.options.desiredApiVersionValue;

            // Add debug info
            createInfo.next = &debugInfo;
            if (globalData.requiredPlatformInstanceCreateStruct) {
                debugInfo.next = globalData.requiredPlatformInstanceCreateStruct;
            }

            createInfo.enabledApiLayerCount = (uint32_t)enabledApiLayers.size();
            createInfo.enabledApiLayerNames = enabledApiLayers.data();

            createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
            createInfo.enabledExtensionNames = enabledExtensions.data();

            REQUIRE_RESULT(XR_SUCCESS, xrCreateInstance(&createInfo, &instance));

            {
                auto pfnCreateDebugUtilsMessengerEXT =
                    GetInstanceExtensionFunction<PFN_xrCreateDebugUtilsMessengerEXT>(instance, "xrCreateDebugUtilsMessengerEXT");
                REQUIRE(pfnCreateDebugUtilsMessengerEXT != nullptr);
            }
            {
                // Get a function pointer to the submit function to test
                PFN_xrSubmitDebugUtilsMessageEXT pfn_submit_dmsg;
                REQUIRE_RESULT(XR_SUCCESS, xrGetInstanceProcAddr(instance, "xrSubmitDebugUtilsMessageEXT",
                                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_submit_dmsg)));
                REQUIRE(pfn_submit_dmsg != nullptr);

                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "General Error";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "General Error";

                // Test the various items
                {
                    callback_data.messageId = "General Error";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Validation Warning";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                    REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Performance Info";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "General Verbose";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }
            }

            REQUIRE_RESULT(XR_SUCCESS, xrDestroyInstance(instance));

            instance = XR_NULL_HANDLE;
        }

        SECTION("Create/Destroy with explicit call (xrCreateDebugUtilsMessengerEXT/xrDestroyDebugUtilsMessengerEXT)")
        {
            AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});

            // Get a function pointer to the various debug utils functions to test
            auto pfn_create_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrCreateDebugUtilsMessengerEXT>(instance, "xrCreateDebugUtilsMessengerEXT");
            auto pfn_destroy_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrDestroyDebugUtilsMessengerEXT>(instance, "xrDestroyDebugUtilsMessengerEXT");
            auto pfn_submit_dmsg = GetInstanceExtensionFunction<PFN_xrSubmitDebugUtilsMessageEXT>(instance, "xrSubmitDebugUtilsMessageEXT");

            // Create the debug utils messenger
            std::vector<DebugUtilsCallbackInfo> callbackInfo;
            XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                           XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
            dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbg_msg_ci.userCallback = addToDebugUtilsCallbackInfoVector;
            dbg_msg_ci.userData = reinterpret_cast<void*>(&callbackInfo);

            XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
            REQUIRE_RESULT(XR_SUCCESS, pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger));

            XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
            callback_data.messageId = "General Error";
            callback_data.functionName = "MyTestFunctionName";
            callback_data.message = "General Error";

            // Test the various items
            {
                callback_data.messageId = "General Error";
                REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
            }
            {
                callback_data.messageId = "Validation Warning";
                REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                           XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
            }
            {
                callback_data.messageId = "Performance Info";
                REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                           XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
            }
            {
                callback_data.messageId = "General Verbose";
                REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
            }

            // Destroy what we created
            REQUIRE_RESULT(XR_SUCCESS, pfn_destroy_debug_utils_messager_ext(debug_utils_messenger));
        }

        SECTION("Make sure appropriate messages only received when registered")
        {
            AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});

            // Get a function pointer to the various debug utils functions to test
            auto pfn_create_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrCreateDebugUtilsMessengerEXT>(instance, "xrCreateDebugUtilsMessengerEXT");
            auto pfn_destroy_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrDestroyDebugUtilsMessengerEXT>(instance, "xrDestroyDebugUtilsMessengerEXT");
            auto pfn_submit_dmsg = GetInstanceExtensionFunction<PFN_xrSubmitDebugUtilsMessageEXT>(instance, "xrSubmitDebugUtilsMessageEXT");

            SECTION("Create the debug utils messenger, but only to receive general error messages")
            {
                // Create the debug utils messenger, but only to receive general error messages
                std::vector<DebugUtilsCallbackInfo> callbackInfo;
                XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
                dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
                dbg_msg_ci.userCallback = addToDebugUtilsCallbackInfoVector;
                dbg_msg_ci.userData = reinterpret_cast<void*>(&callbackInfo);

                XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
                REQUIRE_RESULT(XR_SUCCESS, pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger));

                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "General Error";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "General Error";

                // Test the various items
                {
                    callback_data.messageId = "General Error";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Validation Warning";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Performance Info";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "General Verbose";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }

                // Destroy what we created
                REQUIRE_RESULT(XR_SUCCESS, pfn_destroy_debug_utils_messager_ext(debug_utils_messenger));
            }

            SECTION("Create the debug utils messenger, but only to receive validation warning messages")
            {
                // Create the debug utils messenger, but only to receive validation warning messages
                std::vector<DebugUtilsCallbackInfo> callbackInfo;
                XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
                dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
                dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
                dbg_msg_ci.userCallback = addToDebugUtilsCallbackInfoVector;
                dbg_msg_ci.userData = reinterpret_cast<void*>(&callbackInfo);

                XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
                REQUIRE_RESULT(XR_SUCCESS, pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger));

                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "General Error";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "General Error";

                // Test the various items
                {
                    callback_data.messageId = "General Error";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Validation Warning";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                    REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Performance Info";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "General Verbose";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }

                // Destroy what we created
                REQUIRE_RESULT(XR_SUCCESS, pfn_destroy_debug_utils_messager_ext(debug_utils_messenger));
            }

            SECTION("Create the debug utils messenger, but only to receive performance verbose messages")
            {
                // Create the debug utils messenger, but only to receive performance verbose messages
                std::vector<DebugUtilsCallbackInfo> callbackInfo;
                XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
                dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
                dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                dbg_msg_ci.userCallback = addToDebugUtilsCallbackInfoVector;
                dbg_msg_ci.userData = reinterpret_cast<void*>(&callbackInfo);

                XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
                REQUIRE_RESULT(XR_SUCCESS, pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger));

                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "General Error";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "General Error";

                // Test the various items
                {
                    callback_data.messageId = "General Error";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Validation Warning";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Performance Info";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "General Verbose";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Performance Verbose";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                }

                // Destroy what we created
                REQUIRE_RESULT(XR_SUCCESS, pfn_destroy_debug_utils_messager_ext(debug_utils_messenger));
            }

            SECTION("Create the debug utils messenger, but only to info validation messages")
            {
                // Create the debug utils messenger, but only to receive validation info messages
                std::vector<DebugUtilsCallbackInfo> callbackInfo;
                XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
                dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
                dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
                dbg_msg_ci.userCallback = addToDebugUtilsCallbackInfoVector;
                dbg_msg_ci.userData = reinterpret_cast<void*>(&callbackInfo);

                XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
                REQUIRE_RESULT(XR_SUCCESS, pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger));

                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "General Error";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "General Error";

                // Test the various items
                {
                    callback_data.messageId = "General Error";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Validation Warning";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Performance Info";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "General Verbose";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                    REQUIRE(!debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));
                }
                {
                    callback_data.messageId = "Performance Verbose";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                    REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callback_data));
                }

                // Destroy what we created
                REQUIRE_RESULT(XR_SUCCESS, pfn_destroy_debug_utils_messager_ext(debug_utils_messenger));
            }
        }

        SECTION("Test Objects")
        {
            AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});
            AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::createSpaces | AutoBasicSession::createSwapchains,
                                     instance);

            // Get a function pointer to the various debug utils functions to test
            auto pfn_create_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrCreateDebugUtilsMessengerEXT>(instance, "xrCreateDebugUtilsMessengerEXT");
            auto pfn_destroy_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrDestroyDebugUtilsMessengerEXT>(instance, "xrDestroyDebugUtilsMessengerEXT");
            auto pfn_submit_dmsg = GetInstanceExtensionFunction<PFN_xrSubmitDebugUtilsMessageEXT>(instance, "xrSubmitDebugUtilsMessageEXT");

            // Create the debug utils messenger, but only to receive validation warning messages
            std::vector<DebugUtilsCallbackInfo> callbackInfo;
            XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
            dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbg_msg_ci.userCallback = addToDebugUtilsCallbackInfoVector;
            dbg_msg_ci.userData = reinterpret_cast<void*>(&callbackInfo);
            XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
            REQUIRE_RESULT(XR_SUCCESS, pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger));

            XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
            callback_data.messageId = "General Error";
            callback_data.functionName = "MyTestFunctionName";
            callback_data.message = "General Error";

            std::array<XrDebugUtilsObjectNameInfoEXT, 3> objects;
            objects.fill({XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT});
            objects[0].objectType = XR_OBJECT_TYPE_INSTANCE;
            objects[0].objectHandle = MakeHandleGeneric(instance.GetInstance());
            objects[0].objectName = nullptr;
            objects[1].objectType = XR_OBJECT_TYPE_SESSION;
            objects[1].objectHandle = MakeHandleGeneric(session.GetSession());
            objects[1].objectName = nullptr;
            objects[2].objectType = XR_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;
            objects[2].objectHandle = MakeHandleGeneric(debug_utils_messenger);
            objects[2].objectName = nullptr;
            callback_data.objects = objects.data();
            callback_data.objectCount = static_cast<uint32_t>(objects.size());

            REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                       XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
            REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                       XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));

            // Destroy what we created
            REQUIRE_RESULT(XR_SUCCESS, pfn_destroy_debug_utils_messager_ext(debug_utils_messenger));
        }

        SECTION("Test object names")
        {
            AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});

            auto pfn_create_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrCreateDebugUtilsMessengerEXT>(instance, "xrCreateDebugUtilsMessengerEXT");
            auto pfn_destroy_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrDestroyDebugUtilsMessengerEXT>(instance, "xrDestroyDebugUtilsMessengerEXT");
            auto pfn_submit_dmsg = GetInstanceExtensionFunction<PFN_xrSubmitDebugUtilsMessageEXT>(instance, "xrSubmitDebugUtilsMessageEXT");
            auto pfn_set_obj_name =
                GetInstanceExtensionFunction<PFN_xrSetDebugUtilsObjectNameEXT>(instance, "xrSetDebugUtilsObjectNameEXT");
            auto pfn_begin_debug_utils_label_region_ext = GetInstanceExtensionFunction<PFN_xrSessionBeginDebugUtilsLabelRegionEXT>(
                instance, "xrSessionBeginDebugUtilsLabelRegionEXT");
            auto pfn_end_debug_utils_label_region_ext =
                GetInstanceExtensionFunction<PFN_xrSessionEndDebugUtilsLabelRegionEXT>(instance, "xrSessionEndDebugUtilsLabelRegionEXT");
            auto pfn_insert_debug_utils_label_ext =
                GetInstanceExtensionFunction<PFN_xrSessionInsertDebugUtilsLabelEXT>(instance, "xrSessionInsertDebugUtilsLabelEXT");

            // Create the debug utils messenger, but only to receive validation warning messages
            std::vector<DebugUtilsCallbackInfo> callbackInfo;
            XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
            dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbg_msg_ci.userCallback = addToDebugUtilsCallbackInfoVector;
            dbg_msg_ci.userData = reinterpret_cast<void*>(&callbackInfo);
            XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
            REQUIRE_RESULT(XR_SUCCESS, pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger));

            XrDebugUtilsObjectNameInfoEXT object{XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            object.objectType = XR_OBJECT_TYPE_INSTANCE;
            object.objectHandle = MakeHandleGeneric(instance.GetInstance());
            object.objectName = "My Instance Obj";
            REQUIRE_RESULT(XR_SUCCESS, pfn_set_obj_name(instance, &object));

            {
                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "General Error";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "General Error";
                callback_data.objectCount = 1;
                callback_data.objects = &object;
                REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                REQUIRE(debugMessageExists(callbackInfo, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                           XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
            }

            {
                static const char first_individual_label_name[] = "First individual label";
                static const char second_individual_label_name[] = "Second individual label";
                static const char third_individual_label_name[] = "Third individual label";
                static const char first_label_region_name[] = "First Label Region";
                static const char second_label_region_name[] = "Second Label Region";

                AutoBasicSession session(
                    AutoBasicSession::createSession | AutoBasicSession::createSpaces | AutoBasicSession::createSwapchains, instance);
                FrameIterator frameIterator(&session);

                // Create a label struct for initial testing
                XrDebugUtilsLabelEXT first_label = {XR_TYPE_DEBUG_UTILS_LABEL_EXT};
                first_label.labelName = first_individual_label_name;

                // Set it up to put in the session and instance to any debug utils messages
                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "General Error";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "General Error";
                std::array<XrDebugUtilsObjectNameInfoEXT, 2> objects;
                objects.fill({XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT});
                objects[0].objectType = XR_OBJECT_TYPE_INSTANCE;
                objects[0].objectHandle = MakeHandleGeneric(instance.GetInstance());
                objects[0].objectName = nullptr;
                objects[1].objectType = XR_OBJECT_TYPE_SESSION;
                objects[1].objectHandle = MakeHandleGeneric(session.GetSession());
                objects[1].objectName = nullptr;
                callback_data.objectCount = static_cast<uint32_t>(objects.size());
                callback_data.objects = objects.data();

                // Start an individual label
                REQUIRE_RESULT(XR_SUCCESS, pfn_insert_debug_utils_label_ext(session, &first_label));

                // Trigger a message and make sure we see "First individual label"
                {
                    callback_data.messageId = "First Individual Label";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));

                    const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);
                    REQUIRE(cb.callbackData.sessionLabelCount == 1);
                    REQUIRE_THAT(cb.callbackData.sessionLabels[0].labelName, Catch::Matchers::Equals(first_individual_label_name));
                }

                // Begin a label region
                first_label.labelName = first_label_region_name;
                REQUIRE_RESULT(XR_SUCCESS, pfn_begin_debug_utils_label_region_ext(session, &first_label));

                // Trigger a message and make sure we see "Label Region" and not "First individual label"
                {
                    callback_data.messageId = "First Label Region";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);
                    REQUIRE(cb.callbackData.sessionLabelCount == 1);
                    REQUIRE_THAT(cb.callbackData.sessionLabels[0].labelName, Catch::Matchers::Equals(first_label_region_name));
                }

                // Begin the session now.
                {
                    frameIterator.RunToSessionState(XR_SESSION_STATE_READY);

                    XrSessionBeginInfo session_begin_info = {XR_TYPE_SESSION_BEGIN_INFO};
                    session_begin_info.primaryViewConfigurationType = GetGlobalData().GetOptions().viewConfigurationValue;
                    REQUIRE_RESULT(XR_SUCCESS, xrBeginSession(session, &session_begin_info));
                }

                XrDebugUtilsLabelEXT individual_label{XR_TYPE_DEBUG_UTILS_LABEL_EXT};
                individual_label.labelName = second_individual_label_name;
                REQUIRE_RESULT(XR_SUCCESS, pfn_insert_debug_utils_label_ext(session, &individual_label));

                // Trigger a message and make sure we see "Second individual" and "First Label Region" and not "First
                // individual label"
                {
                    callback_data.messageId = "Second Individual and First Region";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);
                    // From: https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#session-labels
                    // The labels listed inside sessionLabels are organized in time order, with the most recently
                    // generated label appearing first, and the oldest label appearing last.
                    REQUIRE(cb.callbackData.sessionLabelCount == 2);
                    REQUIRE_THAT(cb.callbackData.sessionLabels[0].labelName, Catch::Matchers::Equals(second_individual_label_name));
                    REQUIRE_THAT(cb.callbackData.sessionLabels[1].labelName, Catch::Matchers::Equals(first_label_region_name));
                }

                individual_label.labelName = third_individual_label_name;
                REQUIRE_RESULT(XR_SUCCESS, pfn_insert_debug_utils_label_ext(session, &individual_label));

                // Trigger a message and make sure we see "Third individual" and "First Label Region" and not "First
                // individual label" or "Second individual label"
                {
                    callback_data.messageId = "Third Individual and First Region";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);
                    REQUIRE(cb.callbackData.sessionLabelCount == 2);
                    REQUIRE_THAT(cb.callbackData.sessionLabels[0].labelName, Catch::Matchers::Equals(third_individual_label_name));
                    REQUIRE_THAT(cb.callbackData.sessionLabels[1].labelName, Catch::Matchers::Equals(first_label_region_name));
                }

                // Begin a label region
                {
                    XrDebugUtilsLabelEXT second_label_region = {XR_TYPE_DEBUG_UTILS_LABEL_EXT};
                    second_label_region.labelName = second_label_region_name;
                    REQUIRE_RESULT(XR_SUCCESS, pfn_begin_debug_utils_label_region_ext(session, &second_label_region));
                }

                // Trigger a message and make sure we see "Second Label Region" and "First Label Region"
                {
                    callback_data.messageId = "Second and First Label Regions";
                    pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                    XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data);
                    const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);
                    REQUIRE(cb.callbackData.sessionLabelCount == 2);
                    REQUIRE_THAT(cb.callbackData.sessionLabels[0].labelName, Catch::Matchers::Equals(second_label_region_name));
                    REQUIRE_THAT(cb.callbackData.sessionLabels[1].labelName, Catch::Matchers::Equals(first_label_region_name));
                }

                // End the last (most recent) label region
                {
                    REQUIRE_RESULT(XR_SUCCESS, pfn_end_debug_utils_label_region_ext(session));
                }

                // Trigger a message and make sure we see "First Label Region"
                {
                    callback_data.messageId = "First Label Region 2";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);
                    REQUIRE(cb.callbackData.sessionLabelCount == 1);
                    REQUIRE_THAT(cb.callbackData.sessionLabels[0].labelName, Catch::Matchers::Equals(first_label_region_name));
                }

                // Now clean-up (the session)
                {
                    REQUIRE_RESULT(XR_SUCCESS, xrRequestExitSession(session));

                    frameIterator.RunToSessionState(XR_SESSION_STATE_STOPPING);

                    REQUIRE_RESULT(XR_SUCCESS, xrEndSession(session));
                }

                // End the last label region
                {
                    REQUIRE_RESULT(XR_SUCCESS, pfn_end_debug_utils_label_region_ext(session));
                }

                // Trigger a message and make sure we see no labels
                {
                    callback_data.messageId = "No Labels";
                    REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data));
                    const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);
                    REQUIRE(cb.callbackData.sessionLabelCount == 0);
                }

                session.Shutdown();
            }

            // Destroy what we created
            REQUIRE_RESULT(XR_SUCCESS, pfn_destroy_debug_utils_messager_ext(debug_utils_messenger));
        }

        SECTION("Object naming")
        {
            AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});
            AutoBasicSession session(AutoBasicSession::createSession, instance);

            auto pfn_create_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrCreateDebugUtilsMessengerEXT>(instance, "xrCreateDebugUtilsMessengerEXT");
            auto pfn_destroy_debug_utils_messager_ext =
                GetInstanceExtensionFunction<PFN_xrDestroyDebugUtilsMessengerEXT>(instance, "xrDestroyDebugUtilsMessengerEXT");
            auto pfn_submit_dmsg = GetInstanceExtensionFunction<PFN_xrSubmitDebugUtilsMessageEXT>(instance, "xrSubmitDebugUtilsMessageEXT");
            auto pfn_set_obj_name =
                GetInstanceExtensionFunction<PFN_xrSetDebugUtilsObjectNameEXT>(instance, "xrSetDebugUtilsObjectNameEXT");

            // Create the debug utils messenger
            std::vector<DebugUtilsCallbackInfo> callbackInfo;

            XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
            dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
            dbg_msg_ci.userCallback = addToDebugUtilsCallbackInfoVector;
            dbg_msg_ci.userData = reinterpret_cast<void*>(&callbackInfo);

            XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
            REQUIRE_RESULT(XR_SUCCESS, pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger));

            // Set object name
            XrDebugUtilsObjectNameInfoEXT referenceObject{XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            referenceObject.objectType = XR_OBJECT_TYPE_INSTANCE;
            referenceObject.objectHandle = MakeHandleGeneric(instance.GetInstance());
            referenceObject.objectName = "My Instance Obj";
            REQUIRE_RESULT(XR_SUCCESS, pfn_set_obj_name(instance, &referenceObject));

            // Check object names
            {
                std::array<XrDebugUtilsObjectNameInfoEXT, 2> objects;
                objects.fill({XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT});
                // We pass an object with a name we expect to be overridden with the correct name
                objects[0].objectType = XR_OBJECT_TYPE_INSTANCE;
                objects[0].objectHandle = MakeHandleGeneric(instance.GetInstance());
                objects[0].objectName = "Not my instance";
                // and we pass an object with a name we expect to stay
                objects[1].objectType = XR_OBJECT_TYPE_SESSION;
                objects[1].objectHandle = MakeHandleGeneric(session.GetSession());
                objects[1].objectName = "My Session Obj";

                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "Object Name Test";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "Object name";
                callback_data.objectCount = static_cast<uint32_t>(objects.size());
                callback_data.objects = objects.data();
                REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));

                const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);

                REQUIRE(cb.callbackData.objectCount == 2);

                // We expect that the Instance name will be filled by the debug utils implementation
                REQUIRE(cb.callbackData.objects[0].objectName != nullptr);
                REQUIRE_THAT(cb.callbackData.objects[0].objectName, Catch::Matchers::Equals(referenceObject.objectName));

                // We expect that the passed name will not be overridden / removed
                REQUIRE(cb.callbackData.objects[1].objectName != nullptr);
                REQUIRE_THAT(cb.callbackData.objects[1].objectName, Catch::Matchers::Equals(objects[1].objectName));
            }

            // Unset object name
            // https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrSetDebugUtilsObjectNameEXT.html
            // If XrDebugUtilsObjectNameInfoEXT::objectName is an empty string, then any previously set name is removed.
            XrDebugUtilsObjectNameInfoEXT unsetObject{XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            unsetObject.objectType = XR_OBJECT_TYPE_INSTANCE;
            unsetObject.objectHandle = MakeHandleGeneric(instance.GetInstance());
            unsetObject.objectName = "";
            REQUIRE_RESULT(XR_SUCCESS, pfn_set_obj_name(instance, &unsetObject));

            {
                XrDebugUtilsObjectNameInfoEXT object{XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
                object.objectType = XR_OBJECT_TYPE_INSTANCE;
                object.objectHandle = MakeHandleGeneric(instance.GetInstance());
                object.objectName = nullptr;

                XrDebugUtilsMessengerCallbackDataEXT callback_data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callback_data.messageId = "Object Name Test Removed";
                callback_data.functionName = "MyTestFunctionName";
                callback_data.message = "Object name";
                callback_data.objectCount = 1;
                callback_data.objects = &object;
                REQUIRE_RESULT(XR_SUCCESS, pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callback_data));

                const auto& cb = findMessageByMessageId(callbackInfo, callback_data.messageId);

                REQUIRE(cb.callbackData.objectCount == 1);

                // We expect that the Instance name will NOT be filled by the debug utils implementation
                REQUIRE(cb.callbackData.objects[0].objectName == nullptr);
            }

            // Destroy what we created
            REQUIRE_RESULT(XR_SUCCESS, pfn_destroy_debug_utils_messager_ext(debug_utils_messenger));
        }

        SECTION("Invalid parameters")
        {
            AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});

            auto pfn_begin_debug_utils_label_region_ext = GetInstanceExtensionFunction<PFN_xrSessionBeginDebugUtilsLabelRegionEXT>(
                instance, "xrSessionBeginDebugUtilsLabelRegionEXT");
            auto pfn_end_debug_utils_label_region_ext =
                GetInstanceExtensionFunction<PFN_xrSessionEndDebugUtilsLabelRegionEXT>(instance, "xrSessionEndDebugUtilsLabelRegionEXT");
            auto pfn_insert_debug_utils_label_ext =
                GetInstanceExtensionFunction<PFN_xrSessionInsertDebugUtilsLabelEXT>(instance, "xrSessionInsertDebugUtilsLabelEXT");

            AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::createSpaces | AutoBasicSession::createSwapchains,
                                     instance);
            FrameIterator frameIterator(&session);

            {
                // auto pfn_set_obj_name =
                //     GetInstanceExtensionFunction<PFN_xrSetDebugUtilsObjectNameEXT>(instance, "xrSetDebugUtilsObjectNameEXT");
                // Cannot try invalid instance on set object name as loader will crash
                // REQUIRE_RESULT(XR_ERROR_HANDLE_INVALID, pfn_set_obj_name(XR_NULL_HANDLE, nullptr));
                // Cannot try nullptr for the object name info as loader will crash
                // REQUIRE_RESULT(XR_ERROR_HANDLE_INVALID, pfn_set_obj_name(instance, nullptr));
            }

            // Try invalid session on each of the label functions
            {
                // Create a label struct for initial testing
                XrDebugUtilsLabelEXT label = {XR_TYPE_DEBUG_UTILS_LABEL_EXT};
                label.labelName = "individual label";

                REQUIRE_RESULT(XR_ERROR_HANDLE_INVALID, pfn_begin_debug_utils_label_region_ext(XR_NULL_HANDLE, &label));
                REQUIRE_RESULT(XR_ERROR_HANDLE_INVALID, pfn_end_debug_utils_label_region_ext(XR_NULL_HANDLE));
                REQUIRE_RESULT(XR_ERROR_HANDLE_INVALID, pfn_insert_debug_utils_label_ext(XR_NULL_HANDLE, &label));
            }

            // Try with nullptr for the label
            {
                REQUIRE_RESULT(XR_ERROR_VALIDATION_FAILURE, pfn_begin_debug_utils_label_region_ext(session, nullptr));
                REQUIRE_RESULT(XR_ERROR_VALIDATION_FAILURE, pfn_insert_debug_utils_label_ext(session, nullptr));
            }

            // Try to end a label region that has not been started
            {
                // This seems like an error condition but the OpenXR Loader does not return an error
                // here so we need the same behavior.
                REQUIRE_RESULT(XR_SUCCESS, pfn_end_debug_utils_label_region_ext(session));
            }
        }

        // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XR_EXT_debug_utils
        // The OpenXR spec provides some examples of how to use the extension; they are not full
        // examples but let's make sure that something equivalent to them works.
        // Example 1 / multiple callbacks
#define CHK_XR(expr) XRC_CHECK_THROW_XRCMD(expr)

        SECTION("Examples")
        {
            SECTION("Example 1: Multiple callbacks")
            {
                AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});

                // Must call extension functions through a function pointer:
                PFN_xrCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT;
                CHK_XR(xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateDebugUtilsMessengerEXT)));

                PFN_xrDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT;
                CHK_XR(xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfnDestroyDebugUtilsMessengerEXT)));

                XrDebugUtilsMessengerCreateInfoEXT callback1 = {
                    XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,   // type
                    NULL,                                            // next
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |  // messageSeverities
                        XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                    XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |  // messageTypes
                        XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                    myOutputDebugString,  // userCallback
                    NULL                  // userData
                };
                XrDebugUtilsMessengerEXT messenger1 = XR_NULL_HANDLE;
                CHK_XR(pfnCreateDebugUtilsMessengerEXT(instance, &callback1, &messenger1));

                callback1.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                callback1.userCallback = myDebugBreak;
                callback1.userData = NULL;
                XrDebugUtilsMessengerEXT messenger2 = XR_NULL_HANDLE;
                CHK_XR(pfnCreateDebugUtilsMessengerEXT(instance, &callback1, &messenger2));

                XrDebugUtilsMessengerCreateInfoEXT callback3 = {
                    XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,    // type
                    NULL,                                             // next
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,  // messageSeverities
                    XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |     // messageTypes
                        XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                    myStdOutLogger,  // userCallback
                    NULL             // userData
                };
                XrDebugUtilsMessengerEXT messenger3 = XR_NULL_HANDLE;
                CHK_XR(pfnCreateDebugUtilsMessengerEXT(instance, &callback3, &messenger3));

                // ...

                // Remove callbacks when cleaning up
                pfnDestroyDebugUtilsMessengerEXT(messenger1);
                pfnDestroyDebugUtilsMessengerEXT(messenger2);
                pfnDestroyDebugUtilsMessengerEXT(messenger3);
            }

            SECTION("Example 2: Name for XrSpace")
            {
                AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});
                AutoBasicSession session(AutoBasicSession::beginSession | AutoBasicSession::createSpaces, instance);

                XrSpace space = session.spaceVector.front();

                // Must call extension functions through a function pointer:
                PFN_xrSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;
                CHK_XR(xrGetInstanceProcAddr(instance, "xrSetDebugUtilsObjectNameEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfnSetDebugUtilsObjectNameEXT)));

                // Set a name on the space
                const XrDebugUtilsObjectNameInfoEXT spaceNameInfo = {
                    XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,  // type
                    NULL,                                      // next
                    XR_OBJECT_TYPE_SPACE,                      // objectType
                    (uint64_t)space,                           // objectHandle
                    "My Object-Specific Space",                // objectName
                };

                pfnSetDebugUtilsObjectNameEXT(instance, &spaceNameInfo);

                // A subsequent error might print:
                //   Space "My Object-Specific Space" (0xc0dec0dedeadbeef) is used
                //   with an XrSession that is not it's parent
            }

            SECTION("Example 3: Label workload")
            {
                AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});
                AutoBasicSession session(AutoBasicSession::createSession, instance);

                FrameIterator frameIterator(&session);
                frameIterator.RunToSessionState(XR_SESSION_STATE_READY);

                // Must call extension functions through a function pointer:

                PFN_xrSessionBeginDebugUtilsLabelRegionEXT pfnSessionBeginDebugUtilsLabelRegionEXT;
                CHK_XR(xrGetInstanceProcAddr(instance, "xrSessionBeginDebugUtilsLabelRegionEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfnSessionBeginDebugUtilsLabelRegionEXT)));

                PFN_xrSessionEndDebugUtilsLabelRegionEXT pfnSessionEndDebugUtilsLabelRegionEXT;
                CHK_XR(xrGetInstanceProcAddr(instance, "xrSessionEndDebugUtilsLabelRegionEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfnSessionEndDebugUtilsLabelRegionEXT)));

                PFN_xrSessionInsertDebugUtilsLabelEXT pfnSessionInsertDebugUtilsLabelEXT;
                CHK_XR(xrGetInstanceProcAddr(instance, "xrSessionInsertDebugUtilsLabelEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfnSessionInsertDebugUtilsLabelEXT)));

                XrSessionBeginInfo session_begin_info = {XR_TYPE_SESSION_BEGIN_INFO, nullptr, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
                CHK_XR(xrBeginSession(session, &session_begin_info));

                const XrDebugUtilsLabelEXT session_active_region_label = {
                    XR_TYPE_DEBUG_UTILS_LABEL_EXT,  // type
                    NULL,                           // next
                    "Session active",               // labelName
                };

                // Start an annotated region of calls under the 'Session Active' name
                pfnSessionBeginDebugUtilsLabelRegionEXT(session, &session_active_region_label);

                // Brackets added for clarity
                {
                    XrDebugUtilsLabelEXT individual_label = {
                        XR_TYPE_DEBUG_UTILS_LABEL_EXT,  // type
                        NULL,                           // next
                        "WaitFrame",                    // labelName
                    };

                    const char wait_frame_label[] = "WaitFrame";
                    individual_label.labelName = wait_frame_label;
                    pfnSessionInsertDebugUtilsLabelEXT(session, &individual_label);

                    XrFrameWaitInfo wait_frame_info{XR_TYPE_FRAME_WAIT_INFO};
                    XrFrameState frame_state = {XR_TYPE_FRAME_STATE, nullptr};
                    CHK_XR(xrWaitFrame(session, &wait_frame_info, &frame_state));

                    // Do stuff 1

                    const XrDebugUtilsLabelEXT session_frame_region_label = {
                        XR_TYPE_DEBUG_UTILS_LABEL_EXT,  // type
                        NULL,                           // next
                        "Session Frame 123",            // labelName
                    };

                    // Start an annotated region of calls under the 'Session Frame 123' name
                    pfnSessionBeginDebugUtilsLabelRegionEXT(session, &session_frame_region_label);

                    // Brackets added for clarity
                    {

                        const char begin_frame_label[] = "BeginFrame";
                        individual_label.labelName = begin_frame_label;
                        pfnSessionInsertDebugUtilsLabelEXT(session, &individual_label);

                        XrFrameBeginInfo begin_frame_info{XR_TYPE_FRAME_BEGIN_INFO};
                        CHK_XR(xrBeginFrame(session, &begin_frame_info));

                        // Do stuff 2

                        const char end_frame_label[] = "EndFrame";
                        individual_label.labelName = end_frame_label;
                        pfnSessionInsertDebugUtilsLabelEXT(session, &individual_label);

                        XrFrameEndInfo end_frame_info{XR_TYPE_FRAME_END_INFO};
                        end_frame_info.displayTime = frame_state.predictedDisplayTime;
                        end_frame_info.environmentBlendMode = globalData.GetOptions().environmentBlendModeValue;
                        CHK_XR(xrEndFrame(session, &end_frame_info));
                    }

                    // End the session/begun region started above
                    // (in this case it's the "Session Frame 123" label)
                    pfnSessionEndDebugUtilsLabelRegionEXT(session);
                }

                // End the session/begun region started above
                // (in this case it's the "Session Active" label)
                pfnSessionEndDebugUtilsLabelRegionEXT(session);
            }
        }
#undef CHK_XR
    }

}  // namespace Conformance
