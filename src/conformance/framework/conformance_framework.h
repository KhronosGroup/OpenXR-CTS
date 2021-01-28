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

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <stdarg.h>
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include "utils.h"
#include "conformance_utils.h"
#include "platform_plugin.h"
#include "graphics_plugin.h"
#include <catch2/catch.hpp>

#ifdef XR_USE_PLATFORM_WIN32
#include "windows.h"
#endif

#ifdef XR_USE_PLATFORM_ANDROID
#include <android/log.h>
#include <stdlib.h>  // abort
#endif               /// XR_USE_PLATFORM_ANDROID

// CHECK_MSG
// If you're checking XR_SUCCEEDED(result), see CHECK_RESULT_SUCCEEDED.
// Example usage:
//     CAPTURE(result = xrCreateSession(instance, &session, ...));
//     CHECK_MSG(session != XR_NULL_HANDLE_CPP, "xrCreateSession failed");
//
#define CHECK_MSG(expr, msg) \
    {                        \
        INFO(msg);           \
        CHECK(expr);         \
    }  // Need to create scope or else the INFO leaks into other failures.

// REQUIRE_MSG
// If you're checking XR_SUCCEEDED(result), see REQUIRE_RESULT_SUCCEEDED.
// Example usage:
//     CAPTURE(result = xrCreateSession(instance, &session, ...));
//     REQUIRE_MSG(session != XR_NULL_HANDLE_CPP, "xrCreateSession failed");
//
#define REQUIRE_MSG(expr, msg) \
    {                          \
        INFO(msg);             \
        REQUIRE(expr);         \
    }  // Need to create scope or else the INFO leaks into other failures.

// REQUIRE_RESULT
// Expects result to be exactly equal to expectedResult
//
#define REQUIRE_RESULT(result, expectedResult) REQUIRE(result == expectedResult)

// CHECK_RESULT_SUCCEEDED
// Expects XR_SUCCEEDED(result) (any kind of success, not necessarily XR_SUCCESS)
//
#define CHECK_RESULT_SUCCEEDED(result) CHECK(result >= 0)

// REQUIRE_RESULT_SUCCEEDED
// Expects XR_SUCCEEDED(result) (any kind of success, not necessarily XR_SUCCESS)
//
#define REQUIRE_RESULT_SUCCEEDED(result) REQUIRE(result >= 0)

// CHECK_RESULT_UNQUALIFIED_SUCCESS
// Expects XR_UNQUALIFIED_SUCCESS(result) (exactly equal to XR_SUCCESS)
//
#define CHECK_RESULT_UNQUALIFIED_SUCCESS(result) CHECK(result == XR_SUCCESS)

// REQUIRE_RESULT_UNQUALIFIED_SUCCESS
// Expects XR_UNQUALIFIED_SUCCESS(result) (exactly equal to XR_SUCCESS)
//
#define REQUIRE_RESULT_UNQUALIFIED_SUCCESS(result) REQUIRE(result == XR_SUCCESS)

// XRC_FILE_AND_LINE
// Represents a compile-time file and line location as a single string.
//
#define XRC_CHECK_STRINGIFY(x) #x
#define XRC_TO_STRING(x) XRC_CHECK_STRINGIFY(x)
#define XRC_FILE_AND_LINE __FILE__ ":" XRC_TO_STRING(__LINE__)

#if defined(XR_USE_PLATFORM_ANDROID)
void Conformance_Android_Attach_Current_Thread();
void Conformance_Android_Detach_Current_Thread();
#define ATTACH_THREAD Conformance_Android_Attach_Current_Thread()
#define DETACH_THREAD Conformance_Android_Detach_Current_Thread()
#else
// We put an expression here so that forgetting the semicolon is an error on all platforms.
#define ATTACH_THREAD \
    do {              \
    } while (0)
#define DETACH_THREAD \
    do {              \
    } while (0)
#endif

namespace Conformance
{

    // The following are copied from the HelloXR project. Let's make a shared location version of
    // this which can be shared and uses shareable conventions. They aren't possible to use directly
    // from HelloXR because of collisions, but we can look resolving that.

    [[noreturn]] inline void Throw(std::string failureMessage, const char* originator = nullptr, const char* sourceLocation = nullptr)
    {
        if (originator != nullptr) {
            failureMessage += StringSprintf("\n    Origin: %s", originator);
        }

        if (sourceLocation != nullptr) {
            failureMessage += StringSprintf("\n    Source: %s", sourceLocation);
        }
#ifdef XR_USE_PLATFORM_ANDROID
        /// write to the log too
        __android_log_write(ANDROID_LOG_ERROR, "OpenXR_Conformance_Throw", failureMessage.c_str());
#endif
        throw std::logic_error(failureMessage);
    }

#define XRC_THROW(msg) ::Conformance::Throw(msg, nullptr, XRC_FILE_AND_LINE);

#define XRC_CHECK_THROW(exp)                                \
    {                                                       \
        if (!(exp)) {                                       \
            Throw("Check failed", #exp, XRC_FILE_AND_LINE); \
        }                                                   \
    }

#define XRC_CHECK_THROW_MSG(exp, msg)            \
    {                                            \
        if (!(exp)) {                            \
            Throw(msg, #exp, XRC_FILE_AND_LINE); \
        }                                        \
    }

    [[noreturn]] inline void ThrowXrResult(XrResult res, const char* originator = nullptr,
                                           const char* sourceLocation = nullptr) noexcept(false)
    {
        Throw(StringSprintf("XrResult failure [%d]", res), originator, sourceLocation);
    }

    inline XrResult CheckThrowXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) noexcept(false)
    {
        if (XR_FAILED(res)) {
            ThrowXrResult(res, originator, sourceLocation);
        }

        return res;
    }

#define XRC_THROW_XRRESULT(xr, cmd) ::Conformance::ThrowXrResult(xr, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_XRCMD(cmd) ::Conformance::CheckThrowXrResult(cmd, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_XRRESULT(res, cmdStr) ::Conformance::CheckThrowXrResult(res, cmdStr, XRC_FILE_AND_LINE);

#ifdef XR_USE_PLATFORM_WIN32

    [[noreturn]] inline void ThrowHResult(HRESULT hr, const char* originator = nullptr,
                                          const char* sourceLocation = nullptr) noexcept(false)
    {
        Throw(StringSprintf("HRESULT failure [%x]", hr), originator, sourceLocation);
    }

    inline HRESULT CheckThrowHResult(HRESULT hr, const char* originator = nullptr, const char* sourceLocation = nullptr) noexcept(false)
    {
        if (FAILED(hr)) {
            ThrowHResult(hr, originator, sourceLocation);
        }

        return hr;
    }

#define XRC_THROW_HR(hr, cmd) ::Conformance::ThrowHResult(hr, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_HRCMD(cmd) ::Conformance::CheckThrowHResult(cmd, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_HRESULT(res, cmdStr) ::Conformance::CheckThrowHResult(res, cmdStr, XRC_FILE_AND_LINE);

#endif  // XR_USE_PLATFORM_WIN32

    // Specifies runtime options for the application.
    // String options are case-insensitive.
    // Each of these can be specified from the command line via a command of the same name as
    // the variable name. For example, the application can be run with --graphicsPlugin "vulkan"
    // String vector options are specified space delimited strings. For example, the app could be
    // run with --enabledAPILayers "api_validation handle_validation"
    //
    struct Options
    {
        // Describes the option set in a way suitable for printing.
        std::string DescribeOptions() const;

        // Options include: "vulkan" "d3d11" d3d12" "opengl" "opengles"
        // Default is none. Must be manually specified.
        std::string graphicsPlugin{};

        // Options include "hmd" "handheld". See enum XrFormFactor.
        // Default is hmd.
        std::string formFactor{"Hmd"};
        XrFormFactor formFactorValue{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};

        // Options include "stereo" "mono". See enum XrViewConfigurationType.
        // Default is stereo.
        std::string viewConfiguration{"Stereo"};
        XrViewConfigurationType viewConfigurationValue{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};

        // Options include "opaque" "additive" "alphablend". See enum XrEnvironmentBlendMode.
        // Default is opaque.
        std::string environmentBlendMode{"Opaque"};
        XrEnvironmentBlendMode environmentBlendModeValue{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};

        // Options can vary depending on their platform availability. If a requested API layer is
        // not supported then the test fails.
        // Default is empty.
        std::vector<std::string> enabledAPILayers;

        // Options include at least any of the documented extensions. The runtime supported extensions
        // are enumerated by xrEnumerateApiLayerProperties. If a requested extension is not supported
        // then the test fails.
        // Default is empty.
        std::vector<std::string> enabledInstanceExtensions;

        // Options include at least any of the documented interaction profiles.
        // The conformance tests will generically test the runtime supports each of the provided
        // interaction profile.
        // Default is /interaction_profiles/khr/simple_controller alone.
        std::vector<std::string> enabledInteractionProfiles;

        // Indicates if the runtime returns XR_ERROR_HANDLE_INVALID upon usage of invalid handles.
        // Note that as of 4/2019 the OpenXR specification is inconsistent in its requirement for
        // functions returning XR_ERROR_HANDLE_INVALID. Some functions must return it, some may, with
        // no rationale. Originally it was all must, but there was some debate...
        // Default is false.
        bool invalidHandleValidation{false};

        // Indicates if the runtime supports disconnecting a device, specifically left and right devices.
        // Some input tests depends on the side-effects of device disconnection to test various features.
        // If true the runtime does not support disconnectable devices.
        bool nonDisconnectableDevices{false};

        // If true then all test diagnostics are reported with the file/line that they occurred on.
        // Default is true (enabled).
        bool fileLineLoggingEnabled{true};

        // Defines if executing in debug mode. By default this follows the build type.
        bool debugMode
        {
#if defined(NDEBUG)
            false
#else
            true
#endif
        };
    };

    // Records and produces a conformance report.
    // Conformance isn't a black-and-white result. Conformance is against a given specification version,
    // against a selected set of extensions, with a subset of graphics systems and image formats.
    // We want to produce a report of this upon completion of the tests.
    class ConformanceReport
    {
    public:
        // Generates a report string.
        std::string GetReportString() const;

    public:
        XrVersion apiVersion{XR_CURRENT_API_VERSION};
        size_t testSuccessCount{};
        size_t testFailureCount{};
    };

    // A single place where all singleton data hangs off of.
    class GlobalData
    {
    public:
        GlobalData() = default;

        // Non-copyable
        GlobalData(const GlobalData&) = delete;
        GlobalData& operator=(const GlobalData&) = delete;

        // Sets up global data for usage. Required before use of GlobalData.
        // Returns false if already Initialized.
        bool Initialize();

        bool IsInitialized() const;

        // Matches a successful call to Initialize.
        void Shutdown();

        // Returns the default random number engine.
        RandEngine& GetRandEngine();

        const FunctionInfo& GetFunctionInfo(const char* functionName) const;

        const Options& GetOptions() const;

        const ConformanceReport& GetConformanceReport() const;

        const XrInstanceProperties& GetInstanceProperties() const;

        // case sensitive check.
        bool IsAPILayerEnabled(const char* layerName) const;

        // case sensitive check.
        bool IsInstanceExtensionEnabled(const char* extensionName) const;

        // case sensitive check.
        bool IsInstanceExtensionSupported(const char* extensionName) const;

        // Returns a copy of the IPlatformPlugin
        std::shared_ptr<IPlatformPlugin> GetPlatformPlugin();

        // Returns a copy of the IGraphicsPlugin.
        std::shared_ptr<IGraphicsPlugin> GetGraphicsPlugin();

        // Returns true if under the current test environment we require a graphics plugin. This may
        // be false, for example, if the XR_KHR_headless is enabled.
        bool IsGraphicsPluginRequired() const;

        // Returns true if a graphics plugin was supplied, or if IsGraphicsPluginRequired() is true.
        bool IsUsingGraphicsPlugin() const;

    public:
        // Guards all member data.
        mutable std::recursive_mutex dataMutex;

        // Indicates if Init has succeeded.
        bool isInitialized{};

        // The default random number generation engine we use. Thread safe.
        RandEngine randEngine;

        // User selected options for the program execution.
        Options options;

        ConformanceReport conformanceReport;

        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES, nullptr};

        FunctionInfo nullFunctionInfo;

        std::shared_ptr<IPlatformPlugin> platformPlugin;

        std::shared_ptr<IGraphicsPlugin> graphicsPlugin;

        // If true then we assume the runtime API version is the same as the API version the
        // conformance was built against. If true then we can exercise the runtime more fully because
        // we know the API it was built against.
        bool runtimeMatchesAPIVersion{true};

        // Specifies invalid values, which aren't XR_NULL_HANDLE. Used to exercise invalid handles.
        XrInstance invalidInstance{XRC_INVALID_INSTANCE_VALUE};
        XrSession invalidSession{XRC_INVALID_SESSION_VALUE};
        XrSpace invalidSpace{XRC_INVALID_SPACE_VALUE};
        XrSwapchain invalidSwapchain{XRC_INVALID_SWAPCHAIN_VALUE};
        XrActionSet invalidActionSet{XRC_INVALID_ACTION_SET_VALUE};
        XrAction invalidAction{XRC_INVALID_ACTION_VALUE};
        XrSystemId invalidSystemId{XRC_INVALID_SYSTEM_ID_VALUE};
        XrPath invalidPath{XRC_INVALID_PATH_VALUE};

        // The API layers currently available.
        std::vector<XrApiLayerProperties> availableAPILayers;
        std::vector<std::string> availableAPILayerNames;

        // The API layers that have been requested to be enabled. Suitable for passing to OpenXR.
        StringVec enabledAPILayerNames;

        // The instance extensions currently available.
        std::vector<XrExtensionProperties> availableInstanceExtensions;
        std::vector<std::string> availableInstanceExtensionNames;

        // The instance extensions that are required by the platform (IPlatformPlugin).
        std::vector<std::string> requiredPlatformInstanceExtensions;

        // The instance extensions that are required by the graphics system (IGraphicsPlugin).
        std::vector<std::string> requiredGraphicsInstanceExtensions;

        // The instance extensions that have been requested to be enabled. Suitable for passing to OpenXR.
        StringVec enabledInstanceExtensionNames;

        // The interaction profiles that have been requested to be tested.
        StringVec enabledInteractionProfiles;

        // Required instance creation extension struct, or nullptr.
        // This is a pointer into IPlatformPlugin-provided memory.
        XrBaseInStructure* requiredPlaformInstanceCreateStruct{};
    };

    // Returns the default singleton global data.
    GlobalData& GetGlobalData();

    // Reset global data for a subsequent test run.
    void ResetGlobalData();

}  // namespace Conformance

// GetInstanceExtensionFunction
//
// Returns a pointer to an extension function retrieved via xrGetInstanceProcAddr.
//
// Example usage:
//     XrInstance instance; // ... a valid instance
//     auto _xrPollEvent = GetInstanceExtensionFunction<PFN_xrPollEvent>(instance, "xrPollEvent");
//     CHECK(_xrPollEvent != nullptr);
//
template <typename FunctionType, bool requireSuccess = true>
FunctionType GetInstanceExtensionFunction(XrInstance instance, const char* functionName)
{
    using namespace Conformance;
    XRC_CHECK_THROW(instance != XR_NULL_HANDLE_CPP);
    XRC_CHECK_THROW(functionName != nullptr);
    FunctionType f;
    XrResult result = xrGetInstanceProcAddr(instance, functionName, (PFN_xrVoidFunction*)&f);
    if (requireSuccess) {
        XRC_CHECK_THROW(result == XR_SUCCESS);
    }

    if (XR_SUCCEEDED(result)) {
        XRC_CHECK_THROW(f != nullptr);
    }

    return f;
}

#define OPTIONAL_INVALID_HANDLE_VALIDATION_INFO            \
    if (GetGlobalData().options.invalidHandleValidation) { \
        INFO("Invalid handle validation (optional)")       \
    }                                                      \
    if (GetGlobalData().options.invalidHandleValidation)

#define OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION       \
    if (GetGlobalData().options.invalidHandleValidation) \
    SECTION("Invalid handle validation (optional)")

#define OPTIONAL_DISCONNECTABLE_DEVICE_INFO                  \
    if (!GetGlobalData().options.nonDisconnectableDevices) { \
        INFO("Disconnectable device (optional)")             \
    }                                                        \
    if (!GetGlobalData().options.nonDisconnectableDevices)

#define OPTIONAL_DISCONNECTABLE_DEVICE_SECTION             \
    if (!GetGlobalData().options.nonDisconnectableDevices) \
    SECTION("Disconnectable device (optional)")

// Stringification for Catch2.
// See https://github.com/catchorg/Catch2/blob/master/docs/tostring.md.
#define ENUM_CASE_STR(name, val) \
    case name:                   \
        return #name;

#define MAKE_ENUM_TO_STRING_FUNC(enumType)                    \
    inline const char* enum_to_string(enumType e)             \
    {                                                         \
        switch (e) {                                          \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR);           \
        default:                                              \
            return "Unknown " #enumType;                      \
        }                                                     \
    }                                                         \
    namespace Catch                                           \
    {                                                         \
        template <>                                           \
        struct StringMaker<enumType>                          \
        {                                                     \
            static std::string convert(enumType const& value) \
            {                                                 \
                return enum_to_string(value);                 \
            }                                                 \
        };                                                    \
    }  // namespace Catch

MAKE_ENUM_TO_STRING_FUNC(XrResult);
MAKE_ENUM_TO_STRING_FUNC(XrSessionState);
