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
#include "throw_helpers.h"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    // It would be nice to have these functions as lambdas per test case or section but
    // lambdas will not account for XRAPI_CALL calling conventions for all systems.

    static XRAPI_ATTR XrBool32 XRAPI_CALL myOutputDebugString(XrDebugUtilsMessageSeverityFlagsEXT, XrDebugUtilsMessageTypeFlagsEXT,
                                                              const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
    {
        REQUIRE(userData == NULL);
        WARN(callbackData->message);
        return XR_FALSE;
    };

    static XRAPI_ATTR XrBool32 XRAPI_CALL myDebugBreak(XrDebugUtilsMessageSeverityFlagsEXT, XrDebugUtilsMessageTypeFlagsEXT,
                                                       const XrDebugUtilsMessengerCallbackDataEXT*, void* userData)
    {
        REQUIRE(userData == NULL);
        FAIL();
        return XR_FALSE;
    };

    static XRAPI_ATTR XrBool32 XRAPI_CALL myStdOutLogger(XrDebugUtilsMessageSeverityFlagsEXT, XrDebugUtilsMessageTypeFlagsEXT,
                                                         const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
    {
        REQUIRE(userData == NULL);
        INFO(callbackData->message);
        return XR_FALSE;
    };

    static XRAPI_ATTR XrBool32 XRAPI_CALL addToStringPairVector(XrDebugUtilsMessageSeverityFlagsEXT, XrDebugUtilsMessageTypeFlagsEXT,
                                                                const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
    {
        REQUIRE(userData != NULL);
        auto pMessages = reinterpret_cast<std::vector<std::pair<std::string, std::string>>*>(userData);

        std::string functionName = (callbackData->functionName != nullptr ? callbackData->functionName : "");
        std::string message = (callbackData->message != nullptr ? callbackData->message : "");
        std::pair<std::string, std::string> pair = std::make_pair<std::string, std::string>(std::move(functionName), std::move(message));
        pMessages->push_back(std::move(pair));

        return XR_FALSE;
    };

    static XRAPI_ATTR XrBool32 XRAPI_CALL createInstanceCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                                                 XrDebugUtilsMessageTypeFlagsEXT /* messageTypes */,
                                                                 const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
    {
        REQUIRE(userData == nullptr);
        if ((messageSeverity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
            FAIL_CHECK("Conformance layer error: " << callbackData->functionName << ": " << callbackData->message);
        }
        else {
            WARN("Conformance layer warning: " << callbackData->functionName << ": " << callbackData->message);
        }
        return XR_TRUE;
    };

    TEST_CASE("XR_EXT_debug_utils", "")
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

        SECTION("xrSubmitDebugUtilsMessageEXT")
        {
            std::vector<std::pair<std::string, std::string>> messages;

            auto testMessage = std::make_pair<std::string, std::string>("worker", "testing!");

            auto worker = [&](XrInstance instance) -> void {
                auto pfnSubmitDebugUtilsMessageEXT =
                    GetInstanceExtensionFunction<PFN_xrSubmitDebugUtilsMessageEXT>(instance, "xrSubmitDebugUtilsMessageEXT");

                XrDebugUtilsMessengerCallbackDataEXT callbackData{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
                callbackData.functionName = testMessage.first.c_str();
                callbackData.message = testMessage.second.c_str();
                REQUIRE_RESULT(XR_SUCCESS, pfnSubmitDebugUtilsMessageEXT(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                                                         XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callbackData));
            };

            AutoBasicInstance instance({XR_EXT_DEBUG_UTILS_EXTENSION_NAME});

            // Must call extension functions through a function pointer:
            auto pfnCreateDebugUtilsMessengerEXT =
                GetInstanceExtensionFunction<PFN_xrCreateDebugUtilsMessengerEXT>(instance, "xrCreateDebugUtilsMessengerEXT");

            XrDebugUtilsMessengerCreateInfoEXT callback = {
                XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,   // type
                NULL,                                            // next
                XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |  // messageSeverities
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |  // messageTypes
                    XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                    XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT,
                addToStringPairVector,              // userCallback
                reinterpret_cast<void*>(&messages)  // userData
            };
            XrDebugUtilsMessengerEXT messenger = XR_NULL_HANDLE;
            REQUIRE_RESULT(XR_SUCCESS, pfnCreateDebugUtilsMessengerEXT(instance, &callback, &messenger));

            worker(instance);

            REQUIRE(messages.size() > 0);

            REQUIRE(messages.end() != std::find(messages.begin(), messages.end(), testMessage));
        }

        SECTION("xrCreateInstance debug utils not enabled")
        {
            StringVec enabledApiLayers = globalData.enabledAPILayerNames;
            // Enable only the required platform extensions by default
            StringVec enabledExtensions = globalData.requiredPlatformInstanceExtensions;

            XrInstance instance{XR_NULL_HANDLE};
            CleanupInstanceOnScopeExit cleanup(instance);

            XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
            createInfo.next = globalData.requiredPlatformInstanceCreateStruct;

            strcpy(createInfo.applicationInfo.applicationName, "conformance test : XR_EXT_debug_utils");
            createInfo.applicationInfo.applicationVersion = 1;
            // Leave engineName and engineVersion empty, which is valid usage.
            createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

            createInfo.enabledApiLayerCount = (uint32_t)enabledApiLayers.size();
            createInfo.enabledApiLayerNames = enabledApiLayers.data();

            createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
            createInfo.enabledExtensionNames = enabledExtensions.data();

            REQUIRE_RESULT(XR_SUCCESS, xrCreateInstance(&createInfo, &instance));

            ValidateInstanceExtensionFunctionNotSupported(instance, "xrCreateDebugUtilsMessengerEXT");
        }

        SECTION("xrCreateInstance XrDebugUtilsMessengerCreateInfoEXT")
        {
            // To capture events that occur while creating or destroying an instance an application can link
            // an XrDebugUtilsMessengerCreateInfoEXT structure to the next element of the XrInstanceCreateInfo
            // structure given to xrCreateInstance.
            // Note that this behavior will be implicitly validated by AutoBasicInstance when skipDebugMessenger
            // is not passed as an option, but we have an explicit test for this behavior too.

            StringVec enabledApiLayers = globalData.enabledAPILayerNames;
            // Enable only the required platform extensions by default
            StringVec enabledExtensions = globalData.requiredPlatformInstanceExtensions;

            XrDebugUtilsMessengerCreateInfoEXT debugInfo{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debugInfo.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                          XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugInfo.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
            debugInfo.userCallback = createInstanceCallback;
            debugInfo.userData = nullptr;

            XrInstance instance{XR_NULL_HANDLE};
            CleanupInstanceOnScopeExit cleanup(instance);

            XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};

            enabledExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);

            strcpy(createInfo.applicationInfo.applicationName, "conformance test : XR_EXT_debug_utils");
            createInfo.applicationInfo.applicationVersion = 1;
            // Leave engineName and engineVersion empty, which is valid usage.
            createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

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

            REQUIRE_RESULT(XR_SUCCESS, xrDestroyInstance(instance));

            instance = XR_NULL_HANDLE;
        }

        // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_debug_utils
        // The OpenXR spec provides some examples of how to use the extension; they are not full
        // examples but let's make sure that something equivalent to them works.
        // Example 1 / multiple callbacks
        SECTION("Examples")
        {
#define CHK_XR(expr) XRC_CHECK_THROW_XRCMD(expr)
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

                // Wait until the runtime is ready for us to begin a session
                auto timeout = (GetGlobalData().options.debugMode ? 3600s : 10s);
                FrameIterator frameIterator(&session);
                FrameIterator::RunResult runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_READY, timeout);
                REQUIRE(runResult == FrameIterator::RunResult::Success);

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
#undef CHK_XR
        }
    }

}  // namespace Conformance
