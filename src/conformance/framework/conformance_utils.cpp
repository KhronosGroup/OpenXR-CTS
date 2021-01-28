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

#include <catch2/catch.hpp>
#include "utils.h"
#include "report.h"
#include "two_call_util.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "xr_dependencies.h"
#include "openxr/openxr_platform.h"
#include "openxr/openxr_reflection.h"
#include "graphics_plugin.h"
#include <openxr/openxr_reflection.h>
#include <map>
#include <algorithm>
#include <iostream>
#include <assert.h>
#include <cstring>
#include <time.h>
#include <sstream>
#include <iomanip>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _MSC_VER
#pragma warning(disable : 4312)  // <function-style-cast>': conversion from 'int' to 'XrInstance' of greater size)
#endif

#ifdef Success
#undef Success  // Some platforms #define Success as 0, breaking its usage as an enumerant.
#endif

namespace Conformance
{
    constexpr size_t HEX_DIGITS_FOR_HANDLE = 8;

    // We keep our own copy of this as opposed to calling the xrResultToString function, because our
    // purpose here it to validate the runtime's implementation of xrResultToString.
    const ResultStringMap resultStringMap{XR_LIST_ENUM_XrResult(XRC_ENUM_NAME_PAIR)};

    const ResultStringMap& GetResultStringMap()
    {
        return resultStringMap;
    }

    const std::map<uint64_t, const char*> GetNumberExtensionMap()
    {
#define MAKE_EXTENSION_NUMBER_MAP(NAME, NUM) {NUM, #NAME},
        static const std::map<uint64_t, const char*> myMap = {XR_LIST_EXTENSIONS(MAKE_EXTENSION_NUMBER_MAP)};
        return myMap;
    }

    const char* ResultToString(XrResult result)
    {
        auto it = resultStringMap.find(result);

        if (it != resultStringMap.end()) {
            return it->second;
        }

        return "<unknown>";
    }

    std::string PathToString(XrInstance instance, XrPath path)
    {
        uint32_t count = 0;
        if (XR_SUCCEEDED(xrPathToString(instance, path, 0, &count, nullptr))) {
            std::vector<char> buff(count);
            xrPathToString(instance, path, count, &count, buff.data());
            return std::string(buff.data());
        }
        return "<unknown XrPath " + std::to_string(uint64_t(path)) + ">";
    }

    bool ValidateResultAllowed(const char* functionName, XrResult result)
    {
        GlobalData& globalData = GetGlobalData();

        const FunctionInfo& functionInfo = globalData.GetFunctionInfo(functionName);
        const bool found =
            std::find(functionInfo.validResults.begin(), functionInfo.validResults.end(), result) != functionInfo.validResults.end();
        return found;
    }

    CleanupInstanceOnScopeExit::~CleanupInstanceOnScopeExit()
    {
        if (instance != XR_NULL_HANDLE) {
            INFO("Auto-destroying instance");
            CHECK_RESULT_SUCCEEDED(xrDestroyInstance(instance));
            instance = XR_NULL_HANDLE;
        }
    }

    void CleanupInstanceOnScopeExit::Destroy()
    {
        if (instance != XR_NULL_HANDLE) {
            INFO("Destroying instance on request");
            CHECK_RESULT_SUCCEEDED(xrDestroyInstance(instance));
            instance = XR_NULL_HANDLE;
        }
    }
    CleanupSessionOnScopeExit::~CleanupSessionOnScopeExit()
    {
        if (session != XR_NULL_HANDLE) {
            INFO("Auto-destroying session");
            CHECK_RESULT_SUCCEEDED(xrDestroySession(session));
            session = XR_NULL_HANDLE;
        }
    }

    void CleanupSessionOnScopeExit::Destroy()
    {
        if (session != XR_NULL_HANDLE) {
            INFO("Destroying session on request");
            CHECK_RESULT_SUCCEEDED(xrDestroySession(session));
            session = XR_NULL_HANDLE;
        }
    }

    void InstanceDeleteCHECK::operator()(XrInstance i)
    {
        if (i != XR_NULL_HANDLE) {
            XrResult result = xrDestroyInstance(i);
            CHECK(result == XR_SUCCESS);
        }
    }

    void InstanceDeleteREQUIRE::operator()(XrInstance i)
    {
        if (i != XR_NULL_HANDLE) {
            XrResult result = xrDestroyInstance(i);
            REQUIRE(result == XR_SUCCESS);
        }
    }

    void SessionDeleteCHECK::operator()(XrSession s)
    {
        if (s != XR_NULL_HANDLE) {
            XrResult result = xrDestroySession(s);
            CHECK(result == XR_SUCCESS);
        }
    }

    void SessionDeleteREQUIRE::operator()(XrSession s)
    {
        if (s != XR_NULL_HANDLE) {
            XrResult result = xrDestroySession(s);
            REQUIRE(result == XR_SUCCESS);
        }
    }

    void SpaceDeleteCHECK::operator()(XrSpace s)
    {
        if (s != XR_NULL_HANDLE) {
            XrResult result = xrDestroySpace(s);
            CHECK(result == XR_SUCCESS);
        }
    }

    void SpaceDeleteREQUIRE::operator()(XrSpace s)
    {
        if (s != XR_NULL_HANDLE) {
            XrResult result = xrDestroySpace(s);
            REQUIRE(result == XR_SUCCESS);
        }
    }

    void SwapchainDeleteCHECK::operator()(XrSwapchain s)
    {
        if (s != XR_NULL_HANDLE) {
            XrResult result = xrDestroySwapchain(s);
            CHECK(result == XR_SUCCESS);
        }
    }

    void SwapchainDeleteREQUIRE::operator()(XrSwapchain s)
    {
        if (s != XR_NULL_HANDLE) {
            XrResult result = xrDestroySwapchain(s);
            REQUIRE(result == XR_SUCCESS);
        }
    }

    std::ostream& operator<<(std::ostream& os, NullHandleType const& /*unused*/)
    {
        os << "XR_NULL_HANDLE";
        return os;
    }

    static XrBaseInStructure unrecognizedExtension{XRC_UNRECOGNIZABLE_STRUCTURE_TYPE, nullptr};

    const void* GetUnrecognizableExtension()
    {
        return &unrecognizedExtension;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Stopwatch
    ////////////////////////////////////////////////////////////////////////////////////////////////

    Stopwatch::Stopwatch(bool start) : startTime(), endTime(), running(false)
    {
        if (start)
            Restart();
    }

    void Stopwatch::Restart()
    {
        startTime = std::chrono::system_clock::now();
        running = true;
    }

    void Stopwatch::Stop()
    {
        endTime = std::chrono::system_clock::now();
        running = false;
    }

    bool Stopwatch::IsStarted() const
    {
        return running;
    }

    std::chrono::nanoseconds Stopwatch::Elapsed() const
    {
        std::chrono::time_point<std::chrono::system_clock> lastTime;

        if (running)
            lastTime = std::chrono::system_clock::now();
        else
            lastTime = endTime;

        return lastTime - startTime;
    }
    static XRAPI_ATTR XrBool32 XRAPI_CALL ConformanceLayerCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                                                   XrDebugUtilsMessageTypeFlagsEXT /* messageTypes */,
                                                                   const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                                   void* /* userData */)
    {
        if ((messageSeverity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
            FAIL_CHECK("Conformance layer error: " << callbackData->functionName << ": " << callbackData->message);
        }
        else {
            WARN("Conformance layer warning: " << callbackData->functionName << ": " << callbackData->message);
        }
        return XR_TRUE;
    }
    static inline XrDebugUtilsMessengerCreateInfoEXT MakeMessengerCreateInfo()
    {
        return {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                nullptr,
                XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT,
                &ConformanceLayerCallback,
                nullptr};
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // CreateBasicInstance
    ////////////////////////////////////////////////////////////////////////////////////////////////

    XrResult CreateBasicInstance(XrInstance* instance, bool permitDebugMessenger, std::vector<const char*> additionalEnabledExtensions)
    {
        GlobalData& globalData = GetGlobalData();

        XrDebugUtilsMessengerCreateInfoEXT debugInfo = MakeMessengerCreateInfo();
        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.applicationInfo.applicationVersion = 1;
        strcpy(createInfo.applicationInfo.applicationName, "conformance test");
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        createInfo.enabledApiLayerCount = (uint32_t)globalData.enabledAPILayerNames.size();
        createInfo.enabledApiLayerNames = globalData.enabledAPILayerNames.data();

        StringVec extensions(globalData.enabledInstanceExtensionNames);
        for (const char* enabledExt : additionalEnabledExtensions) {
            extensions.push_back_unique(enabledExt);
        }

        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.enabledExtensionNames = extensions.data();

        if (globalData.requiredPlaformInstanceCreateStruct != nullptr) {
            createInfo.next = globalData.requiredPlaformInstanceCreateStruct;
        }
        if (permitDebugMessenger) {
            debugInfo.next = createInfo.next;
            createInfo.next = &debugInfo;
        }
        XrResult result = xrCreateInstance(&createInfo, instance);
        if (XR_FAILED(result)) {
            *instance = XR_NULL_HANDLE;
        }

        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // AutoBasicInstance
    ////////////////////////////////////////////////////////////////////////////////////////////////

    AutoBasicInstance::AutoBasicInstance(const std::vector<const char*>& additionalEnabledExtensions, int optionFlags)
    {
        Initialize(optionFlags, XR_NULL_HANDLE, additionalEnabledExtensions);
    }

    AutoBasicInstance::AutoBasicInstance(int optionFlags, XrInstance instance_ /* = XR_NULL_HANDLE */)
    {
        Initialize(optionFlags, instance_);
    }

    void AutoBasicInstance::Initialize(int optionFlags, XrInstance instance_, const std::vector<const char*>& additionalEnabledExtensions)
    {
        const bool permitDebugMessenger =
            IsInstanceExtensionEnabled(XR_EXT_DEBUG_UTILS_EXTENSION_NAME) && ((optionFlags & skipDebugMessenger) == 0);

        if (instance_ != XR_NULL_HANDLE) {
            assert(additionalEnabledExtensions.size() == 0);
            instance = instance_;
        }
        else {
            instanceCreateResult = CreateBasicInstance(&instance, permitDebugMessenger, additionalEnabledExtensions);
            XRC_CHECK_THROW_XRRESULT(instanceCreateResult, "CreateBasicInstance");
        }

        if (permitDebugMessenger) {
            XrDebugUtilsMessengerCreateInfoEXT debugInfo = MakeMessengerCreateInfo();

            auto xrCreateDebugUtilsMessengerEXT_ =
                GetInstanceExtensionFunction<PFN_xrCreateDebugUtilsMessengerEXT>(instance, "xrCreateDebugUtilsMessengerEXT");
            XrResult result = xrCreateDebugUtilsMessengerEXT_(instance, &debugInfo, &debugMessenger);
            if (XR_FAILED(result)) {
                debugMessenger = XR_NULL_HANDLE_CPP;
            }
        }
        if ((optionFlags & createSystemId) != 0) {
            XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO, nullptr, GetGlobalData().options.formFactorValue};
            XrResult getSystemResult = xrGetSystem(instance, &systemGetInfo, &systemId);

            if (XR_FAILED(getSystemResult)) {
                xrDestroyInstance(instance);
                instance = XR_NULL_HANDLE;
                systemId = XR_NULL_SYSTEM_ID;

                XRC_CHECK_THROW(getSystemResult == XR_SUCCESS);
            }
        }
    }

    AutoBasicInstance::~AutoBasicInstance()
    {
        if (debugMessenger != XR_NULL_HANDLE_CPP) {
            auto xrDestroyDebugUtilsMessengerEXT_ =
                GetInstanceExtensionFunction<PFN_xrDestroyDebugUtilsMessengerEXT>(instance, "xrDestroyDebugUtilsMessengerEXT");
            xrDestroyDebugUtilsMessengerEXT_(debugMessenger);
            debugMessenger = XR_NULL_HANDLE_CPP;
        }
        if (instance != XR_NULL_HANDLE) {
            xrDestroyInstance(instance);
        }
    }

    bool AutoBasicInstance::operator==(NullHandleType const& /*unused*/) const
    {
        return !IsValidHandle();
    }

    bool AutoBasicInstance::operator!=(NullHandleType const& /*unused*/) const
    {
        return IsValidHandle();
    }

    template <typename T>
    static inline void OutputHandle(std::ostream& os, T handle)
    {
        if (handle == XR_NULL_HANDLE) {
            os << "XR_NULL_HANDLE";
        }
        else {
            std::ostringstream oss;
            oss << "0x" << std::hex << std::setw(HEX_DIGITS_FOR_HANDLE) << std::setfill('0');
#if XR_PTR_SIZE == 8
            oss << reinterpret_cast<uint64_t>(handle);
#else
            oss << static_cast<uint64_t>(handle);
#endif
            os << oss.str();
        }
    }

    std::ostream& operator<<(std::ostream& os, AutoBasicInstance const& inst)
    {
        OutputHandle(os, inst.GetInstance());
        return os;
    }

    XrResult CreateBasicSession(XrInstance instance, XrSystemId* systemId, XrSession* session, bool enableGraphicsSystem)
    {
        GlobalData& globalData = GetGlobalData();

        XrResult result;

        XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO, nullptr, globalData.options.formFactorValue};
        result = xrGetSystem(instance, &systemGetInfo, systemId);

        if (XR_SUCCEEDED(result)) {
            auto graphicsPlugin = globalData.GetGraphicsPlugin();
            const XrBaseInStructure* graphicsBinding = nullptr;

            // Normally the testing requires a graphics plugin. However, there's currently one case in
            // which that's not true: when a headless extension is enabled. In that case the
            // runtime supports creating a session without a graphics system. See XR_MND_headless and/or XR_KHR_headless doc.
            if (graphicsPlugin && enableGraphicsSystem) {
                // If the following fails then this app has a bug, not the runtime.
                assert(graphicsPlugin->IsInitialized());

                if (!graphicsPlugin->InitializeDevice(instance, *systemId)) {
                    // This isn't real. It may mislead this test if encountered. We have to decide our policy in this.
                    return XR_ERROR_RUNTIME_FAILURE;
                }

                graphicsBinding = graphicsPlugin->GetGraphicsBinding();
                assert(graphicsBinding);  // If this fails then this app has a bug, not the runtime.
            }
            else if (globalData.IsGraphicsPluginRequired()) {
                // We should have bailed out of testing on startup.
                assert(false);  // If this fails then this app has a bug, not the runtime.
                return XR_ERROR_RUNTIME_FAILURE;
            }

            XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO, graphicsBinding, 0, *systemId};
            result = xrCreateSession(instance, &sessionCreateInfo, session);
        }

        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // AutoBasicSession
    ////////////////////////////////////////////////////////////////////////////////////////////////
    AutoBasicSession::AutoBasicSession(int optionFlags_, XrInstance instance_) : optionFlags(optionFlags_)
    {
        Init(optionFlags_, instance_);
    }

    void AutoBasicSession::Init(int optionFlags_, XrInstance instance_)
    {
        GlobalData& globalData = GetGlobalData();

        if (instance)
            Shutdown();

        try {
            // Some flags imply parent flags
            if (optionFlags_ & beginSession)
                optionFlags_ |= (createInstance | createSession);
            if (optionFlags_ & createSwapchains)
                optionFlags_ |= (createInstance | createSession);
            if (optionFlags_ & createActions)
                optionFlags_ |= (createInstance | createSession);
            if (optionFlags_ & createSpaces)
                optionFlags_ |= (createInstance | createSession);
            if (optionFlags_ & createSession)
                optionFlags_ |= (createInstance);

            instance = instance_;
            optionFlags = optionFlags_;

            if (optionFlags & createInstance) {
                if (instance_ == XR_NULL_HANDLE) {
                    XRC_CHECK_THROW_XRCMD(CreateBasicInstance(&instance));
                    instanceOwned = true;
                }

                assert(instance != XR_NULL_HANDLE);

                if (optionFlags & createSession) {
                    bool enableGraphics = ((optionFlags & skipGraphics) == 0);

                    XRC_CHECK_THROW_XRCMD(CreateBasicSession(instance, &systemId, &session, enableGraphics));

                    assert(systemId != XR_NULL_SYSTEM_ID);
                    assert(session != XR_NULL_HANDLE);

                    if (optionFlags & beginSession) {
                        // The session starts in (or gets directly transitioned to) the
                        // XR_SESSION_STATE_IDLE state and will get transitioned to
                        // XR_SESSION_STATE_READY by the runtime. But before that has not happened,
                        // xrBeginSession() below can return XR_ERROR_SESSION_NOT_READY.
                        // So just calling xrBeginSession might fail without it being a conformance
                        // failure. The correct way is to wait until the runtime tells us via an event
                        // that the session is ready.

                        // timeout in case the runtime will never transition to READY: 10s in release, no practical limit in debug
                        std::chrono::nanoseconds timeout = (GetGlobalData().options.debugMode ? 3600_sec : 10_sec);
                        CountdownTimer countdownTimer(timeout);

                        while ((sessionState != XR_SESSION_STATE_READY) && (!countdownTimer.IsTimeUp())) {
                            XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
                            XrResult result = xrPollEvent(instance, &eventData);
                            XRC_CHECK_THROW_XRCMD(result);

                            if (XR_UNQUALIFIED_SUCCESS(result)) {
                                switch (eventData.type) {
                                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                                    XrEventDataSessionStateChanged sessionStateChanged;
                                    memcpy(&sessionStateChanged, &eventData, sizeof(sessionStateChanged));
                                    sessionState = sessionStateChanged.state;
                                    break;
                                default:
                                    break;  // Ignored event type.
                                }
                            }
                        }

                        if (sessionState != XR_SESSION_STATE_READY) {
                            XRC_THROW("Time out waiting for XR_SESSION_STATE_READY session state change");
                        }

                        XrSessionBeginInfo sessionBeginInfo{
                            XR_TYPE_SESSION_BEGIN_INFO,
                            globalData.GetPlatformPlugin()->PopulateNextFieldForStruct(XR_TYPE_SESSION_BEGIN_INFO),
                            globalData.options.viewConfigurationValue};
                        XRC_CHECK_THROW_XRCMD(xrBeginSession(session, &sessionBeginInfo));
                    }

                    // Set up the enumerated types
                    XRC_CHECK_THROW_XRCMD(doTwoCallInPlace(swapchainFormatVector, xrEnumerateSwapchainFormats, session));
                    XRC_CHECK_THROW_XRCMD(doTwoCallInPlace(spaceTypeVector, xrEnumerateReferenceSpaces, session));
                    XRC_CHECK_THROW_XRCMD(xrStringToPath(instance, "/user/hand/left", &handSubactionArray[0]));
                    XRC_CHECK_THROW_XRCMD(xrStringToPath(instance, "/user/hand/right", &handSubactionArray[1]));

                    // Note that while we are enumerating this, normally our testing is done via a pre-chosen one (globalData.options.viewConfigurationValue).
                    XRC_CHECK_THROW_XRCMD(doTwoCallInPlace(viewConfigurationTypeVector, xrEnumerateViewConfigurations, instance, systemId));

                    // We use globalData.options.viewConfigurationValue as the type we enumerate with, despite that the runtime may support others.
                    XRC_CHECK_THROW_XRCMD(doTwoCallInPlaceWithEmptyElement(
                        viewConfigurationViewVector, {XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr, 0, 0, 0, 0, 0, 0},
                        xrEnumerateViewConfigurationViews, instance, systemId, globalData.options.viewConfigurationValue));

                    XRC_CHECK_THROW_XRCMD(doTwoCallInPlace(environmentBlendModeVector, xrEnumerateEnvironmentBlendModes, instance, systemId,
                                                           globalData.options.viewConfigurationValue));

                    if ((optionFlags & createSwapchains) && globalData.IsUsingGraphicsPlugin()) {
                        auto graphicsPlugin = globalData.GetGraphicsPlugin();
                        if (graphicsPlugin != nullptr) {
                            XrSwapchain swapchain;
                            swapchainExtent = {(int32_t)viewConfigurationViewVector[0].recommendedImageRectWidth,
                                               (int32_t)viewConfigurationViewVector[0].recommendedImageRectHeight};
                            XRC_CHECK_THROW_XRCMD(CreateColorSwapchain(session, graphicsPlugin.get(), &swapchain, &swapchainExtent));
                            // Maybe create as many of them as there are views.
                            swapchainVector.push_back(swapchain);
                        }
                    }

                    if (optionFlags & createActions) {
                        XRC_CHECK_THROW_XRCMD(
                            CreateActionSet(instance, &actionSet, &actionVector, handSubactionArray.data(), handSubactionArray.size()));
                    }

                    if (optionFlags & createSpaces) {
                        std::vector<XrReferenceSpaceType> referenceSpaceTypes;
                        XRC_CHECK_THROW_XRCMD(doTwoCallInPlace(referenceSpaceTypes, xrEnumerateReferenceSpaces, session));

                        for (XrReferenceSpaceType referenceSpace : referenceSpaceTypes) {
                            XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr, referenceSpace,
                                                                  XrPosefCPP()};
                            XrSpace space;
                            XRC_CHECK_THROW_XRCMD(xrCreateReferenceSpace(session, &createInfo, &space));
                            spaceVector.push_back(space);
                        }
                    }
                }
            }
        }
        catch (...) {
            Shutdown();
            throw;
        }
    }

    AutoBasicSession::~AutoBasicSession()
    {
        Shutdown();
    }

    void AutoBasicSession::Shutdown()
    {
        bool sessionCreated = (optionFlags & createSession) != 0;
        bool graphicsSkipped = (optionFlags & skipGraphics) != 0;

        optionFlags = 0;
        systemId = XR_NULL_SYSTEM_ID;
        sessionCreateResult = XR_SUCCESS;
        //handSubactionArray - nothing to do.
        swapchainFormatVector.clear();  // Let parent session destroy this.
        actionSet = XR_NULL_HANDLE;     // Let parent session destroy this.
        actionVector.clear();           // Let parent session destroy this.
        spaceTypeVector.clear();        // Let parent session destroy this.
        viewConfigurationTypeVector.clear();
        viewConfigurationViewVector.clear();
        environmentBlendModeVector.clear();

        if (session != XR_NULL_HANDLE) {
            xrDestroySession(session);
            session = XR_NULL_HANDLE;
        }

        // Shutdown the device initialized by CreateBasicSession
        // after the session is destroyed.
        if (sessionCreated && !graphicsSkipped) {
            GlobalData& globalData = GetGlobalData();
            auto graphicsPlugin = globalData.GetGraphicsPlugin();
            if (graphicsPlugin->IsInitialized()) {
                graphicsPlugin->ShutdownDevice();
            }
        }

        if (instanceOwned) {
            if (instance != XR_NULL_HANDLE)  // Should be true.
                xrDestroyInstance(instance);
            instanceOwned = false;
        }

        instance = XR_NULL_HANDLE;
    }

    bool AutoBasicSession::operator==(NullHandleType const& /*unused*/) const
    {
        return !IsValidHandle();
    }

    bool AutoBasicSession::operator!=(NullHandleType const& /*unused*/) const
    {
        return IsValidHandle();
    }

    std::ostream& operator<<(std::ostream& os, AutoBasicSession const& sess)
    {
        OutputHandle(os, sess.GetSession());
        return os;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // FrameIterator
    ////////////////////////////////////////////////////////////////////////////////////////////////

    FrameIterator::FrameIterator(AutoBasicSession* autoBasicSession_)
        : autoBasicSession(autoBasicSession_)
        , sessionState(autoBasicSession->GetSessionState())
        , countdownTimer()
        , frameState()
        , viewVector()
    {
    }

    void FrameIterator::SetAutoBasicSession(AutoBasicSession* autoBasicSession_)
    {
        autoBasicSession = autoBasicSession_;
    }

    XrSessionState FrameIterator::GetCurrentSessionState() const
    {
        return sessionState;
    }

    FrameIterator::TickResult FrameIterator::PollEvent()
    {
        // App must have called SetAutoBasicSession and set flags enabling these.
        if (!autoBasicSession)
            return TickResult::Error;
        if (!autoBasicSession->instance)
            return TickResult::Error;

        XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
        XrResult result = xrPollEvent(autoBasicSession->instance, &eventData);

        switch (result) {
        case XR_SUCCESS: {
            switch ((int)eventData.type) {  // Cast to int to avoid warnings about unhandled events.
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged sessionStateChanged;
                memcpy(&sessionStateChanged, &eventData, sizeof(sessionStateChanged));
                sessionState = sessionStateChanged.state;
                return TickResult::SessionStateChanged;
            }
            }

            return TickResult::SessionStateUnchanged;
        }

        case XR_EVENT_UNAVAILABLE: {
            // Nothing to do.
            return TickResult::SessionStateUnchanged;
        }

        case XR_ERROR_INSTANCE_LOST:
        case XR_ERROR_RUNTIME_FAILURE:
        case XR_ERROR_HANDLE_INVALID:
        case XR_ERROR_VALIDATION_FAILURE: {
            return TickResult::Error;  // Error result.
        }

        default: {
            return TickResult::Error;  // Unexpected result.
        }
        }
    }

    FrameIterator::RunResult FrameIterator::CycleToNextSwapchainImage()
    {
        // App must have called SetAutoBasicSession and set flags enabling these.
        if (!autoBasicSession)
            return RunResult::Error;
        if (autoBasicSession->swapchainVector.empty())
            return RunResult::Error;

        // Call the helper function for this.
        const XrDuration twoSeconds = 2_xrSeconds;
        XrResult result = Conformance::CycleToNextSwapchainImage(autoBasicSession->swapchainVector.data(),
                                                                 autoBasicSession->swapchainVector.size(), twoSeconds);

        if (XR_FAILED(result))
            return RunResult::Error;

        if (result == XR_TIMEOUT_EXPIRED)
            return RunResult::Timeout;

        return RunResult::Success;
    }

    FrameIterator::RunResult FrameIterator::WaitAndBeginFrame()
    {
        // App must have called SetAutoBasicSession and set flags enabling these.
        if (!autoBasicSession)
            return RunResult::Error;
        if (!autoBasicSession->session)
            return RunResult::Error;
        if (autoBasicSession->spaceVector.empty())
            return RunResult::Error;
        if (autoBasicSession->viewConfigurationTypeVector.empty())
            return RunResult::Error;

        XrResult result;

        // xrWaitFrame may block.
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        frameState = XrFrameState{XR_TYPE_FRAME_STATE};
        result = xrWaitFrame(autoBasicSession->session, &frameWaitInfo, &frameState);
        if (XR_FAILED(result))
            return RunResult::Error;

        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = autoBasicSession->viewConfigurationTypeVector[0];
        viewLocateInfo.displayTime = frameState.predictedDisplayTime;
        viewLocateInfo.space = autoBasicSession->spaceVector[0];
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCount = (uint32_t)autoBasicSession->viewConfigurationViewVector.size();
        viewVector.resize(viewCount, {XR_TYPE_VIEW});
        result = xrLocateViews(autoBasicSession->session, &viewLocateInfo, &viewState, viewCount, &viewCount, viewVector.data());
        if (XR_FAILED(result))
            return RunResult::Error;
        viewVector.resize(viewCount);

        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        result = xrBeginFrame(autoBasicSession->session, &frameBeginInfo);
        if (XR_FAILED(result))
            return RunResult::Error;

        return RunResult::Success;
    }

    FrameIterator::RunResult FrameIterator::PrepareFrameEndInfo()
    {
        // App must have called SetAutoBasicSession and set flags enabling these.
        if (!autoBasicSession)
            return RunResult::Error;
        if (autoBasicSession->spaceVector.empty())
            return RunResult::Error;
        if (autoBasicSession->environmentBlendModeVector.empty())
            return RunResult::Error;
        if (autoBasicSession->swapchainVector.empty())
            return RunResult::Error;

        frameEndInfo = XrFrameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = autoBasicSession->environmentBlendModeVector[0];
        frameEndInfo.layerCount = 0;    // To be filled in later by whoever will be constructing the layers.
        frameEndInfo.layers = nullptr;  // To be filled in later by ...

        projectionViewVector.resize(viewVector.size(), {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
        for (size_t v = 0; v < viewVector.size(); ++v) {
            projectionViewVector[v].pose = viewVector[v].pose;
            projectionViewVector[v].fov = viewVector[v].fov;
            // Currently this swapchain handling is dumb; we just use the first swapchain image.
            projectionViewVector[v].subImage.swapchain =
                autoBasicSession->swapchainVector[0];  // Intentionally use just [0], in order to simplify our logic here.
            projectionViewVector[v].subImage.imageRect = {0, 0, (int32_t)autoBasicSession->swapchainExtent.width,
                                                          (int32_t)autoBasicSession->swapchainExtent.height};
            projectionViewVector[v].subImage.imageArrayIndex = 0;
        }

        compositionLayerProjection = XrCompositionLayerProjection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        compositionLayerProjection.layerFlags = 0;
        compositionLayerProjection.space = autoBasicSession->spaceVector[0];
        compositionLayerProjection.viewCount = (uint32_t)projectionViewVector.size();
        compositionLayerProjection.views = projectionViewVector.data();

        return RunResult::Success;
    }

    FrameIterator::RunResult FrameIterator::PrepareSubmitFrame()
    {
        RunResult runResult = WaitAndBeginFrame();
        if (runResult != RunResult::Success)
            return runResult;

        runResult = CycleToNextSwapchainImage();
        if (runResult != RunResult::Success)
            return runResult;

        runResult = PrepareFrameEndInfo();
        if (runResult != RunResult::Success)
            return runResult;

        return RunResult::Success;
    }

    FrameIterator::RunResult FrameIterator::SubmitFrame()
    {
        RunResult runResult = PrepareSubmitFrame();
        if (runResult != RunResult::Success)
            return runResult;

        const XrCompositionLayerBaseHeader* headerPtrArray[1] = {
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&compositionLayerProjection)};
        frameEndInfo.layerCount = 1;
        frameEndInfo.layers = headerPtrArray;

        XrResult result = xrEndFrame(autoBasicSession->session, &frameEndInfo);
        if (XR_FAILED(result))
            return RunResult::Error;

        return RunResult::Success;
    }

    // Runs until the given XrSessionState is achieved or timesout before so.
    // targetSessionState may be any XrSessionState, but some session states may require
    // special handling in order to get to, such as XR_SESSION_STATE_LOSS_PENDING.
    FrameIterator::RunResult FrameIterator::RunToSessionState(XrSessionState targetSessionState, std::chrono::nanoseconds timeout)
    {
        if (!autoBasicSession)  // App must have called SetAutoBasicSession.
            return RunResult::Error;

        countdownTimer.Restart(timeout);

        while (!countdownTimer.IsTimeUp()) {
            TickResult tickResult = PollEvent();

            if (tickResult == TickResult::Error)
                return RunResult::Error;

            if (sessionState == targetSessionState)
                return RunResult::Success;

            if ((sessionState == XR_SESSION_STATE_LOSS_PENDING) || (sessionState == XR_SESSION_STATE_EXITING) ||
                (sessionState == XR_SESSION_STATE_STOPPING)) {
                return RunResult::Timeout;
            }

            // At this point sessionState is one of XR_SESSION_STATE_UNKNOWN,
            // XR_SESSION_STATE_IDLE, XR_SESSION_STATE_READY, XR_SESSION_STATE_SYNCHRONIZED,
            // XR_SESSION_STATE_VISIBLE, XR_SESSION_STATE_FOCUSED. We proceed based on the
            // current state.

            switch (sessionState) {
            case XR_SESSION_STATE_UNKNOWN:
                // Wait until we timeout or are moved to a new state.
                break;

            case XR_SESSION_STATE_IDLE:
                break;

            case XR_SESSION_STATE_READY:
                if (tickResult == TickResult::SessionStateChanged) {
                    // If we just transitioned to READY then we will call begin session, otherwise we will be stuck.
                    // If the caller of this function does not desire this, it should use targetSessionState=XR_SESSION_STATE_READY
                    // so that it can handle it differently.
                    GlobalData& globalData = GetGlobalData();
                    XrSessionBeginInfo sessionBeginInfo{
                        XR_TYPE_SESSION_BEGIN_INFO, globalData.GetPlatformPlugin()->PopulateNextFieldForStruct(XR_TYPE_SESSION_BEGIN_INFO),
                        globalData.options.viewConfigurationValue};
                    XrResult result = xrBeginSession(autoBasicSession->session, &sessionBeginInfo);
                    if (XR_FAILED(result))
                        return RunResult::Error;
                }

                // Fall-through because frames must be submitted to get promoted from READY to SYNCHRONIZED.

            case XR_SESSION_STATE_SYNCHRONIZED:
            case XR_SESSION_STATE_VISIBLE:
            case XR_SESSION_STATE_FOCUSED: {
                if (GetGlobalData().IsUsingGraphicsPlugin()) {
                    // In these states we need to submit frames. Otherwise the runtime won't
                    // necessarily move us from synchronized to visible or focused.
                    RunResult runResult = SubmitFrame();

                    if (runResult == RunResult::Error)
                        return RunResult::Error;
                }
                // Just keep going. We haven't reached the target state yet.
                break;
            }

            case XR_SESSION_STATE_STOPPING:
            case XR_SESSION_STATE_LOSS_PENDING:
            case XR_SESSION_STATE_EXITING:
            case XR_SESSION_STATE_MAX_ENUM:
                break;
            }
        }

        return RunResult::Timeout;
    }

    bool WaitUntilPredicateWithTimeout(std::function<bool()> predicate, const std::chrono::nanoseconds timeout,
                                       const std::chrono::nanoseconds delay)
    {
        const auto timeoutTime = std::chrono::system_clock::now() + timeout;

        while (!predicate()) {
            if (std::chrono::system_clock::now() >= timeoutTime) {
                return false;
            }

            std::this_thread::sleep_for(delay);
        }

        return true;
    }

    XrResult GetAvailableAPILayers(std::vector<XrApiLayerProperties>& availableAPILayers)
    {
        availableAPILayers.clear();

        uint32_t propertyCount = 0;
        XrResult result = xrEnumerateApiLayerProperties(0, &propertyCount, nullptr);

        if (XR_FAILED(result))
            return result;

        availableAPILayers.resize(propertyCount, XrApiLayerProperties{XR_TYPE_API_LAYER_PROPERTIES});
        result = xrEnumerateApiLayerProperties(propertyCount, &propertyCount, availableAPILayers.data());

        return result;
    }

    XrResult GetAvailableInstanceExtensions(std::vector<XrExtensionProperties>& availableInstanceExtensions, const char* layerName)
    {
        availableInstanceExtensions.clear();

        uint32_t propertyCount = 0;
        XrResult result = xrEnumerateInstanceExtensionProperties(layerName, 0, &propertyCount, nullptr);

        if (XR_FAILED(result))
            return result;

        availableInstanceExtensions.resize(propertyCount, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
        result = xrEnumerateInstanceExtensionProperties(layerName, propertyCount, &propertyCount, availableInstanceExtensions.data());

        return result;
    }

    bool IsInstanceExtensionEnabled(const char* extensionName)
    {
        GlobalData& globalData = GetGlobalData();

        auto caseInsensitivePredicate = [&extensionName](const std::string& str) -> bool { return striequal(extensionName, str.c_str()); };

        auto it = std::find_if(globalData.enabledInstanceExtensionNames.begin(), globalData.enabledInstanceExtensionNames.end(),
                               caseInsensitivePredicate);

        return (it != globalData.enabledInstanceExtensionNames.end());
    }

    bool IsInstanceExtensionEnabled(uint64_t extensionNumber)
    {
        auto& map = GetNumberExtensionMap();
        auto it = map.find(extensionNumber);
        if (it == map.end()) {
            return false;
        }
        return IsInstanceExtensionEnabled(it->second);
    }

    bool IsInteractionProfileEnabled(const char* ipName)
    {
        GlobalData& globalData = GetGlobalData();

        auto caseInsensitivePredicate = [&ipName](const std::string& str) -> bool { return striequal(ipName, str.c_str()); };

        auto it = std::find_if(globalData.enabledInteractionProfiles.begin(), globalData.enabledInteractionProfiles.end(),
                               caseInsensitivePredicate);

        return (it != globalData.enabledInteractionProfiles.end());
    }

    bool IsExtensionFunctionEnabled(const char* functionName)
    {
        const FunctionInfoMap& functionInfoMap = GetFunctionInfoMap();

        auto it = functionInfoMap.find(functionName);

        if (it != functionInfoMap.end()) {
            const FunctionInfo& functionInfo = it->second;
            return IsInstanceExtensionEnabled(functionInfo.requiredExtension);
        }

        return false;  // Function is unknown. Was it case-mismatched?
    }

    bool IsViewConfigurationTypeEnumValid(XrViewConfigurationType viewType)
    {
        //! @todo This function should be auto-generated from the spec.

        switch (viewType) {
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
            // The two valid view configurations in unextended OpenXR.
            return true;
        case XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM:
            // This is never a valid XrViewConfigurationType.
            return false;
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO:
            return IsInstanceExtensionEnabled("XR_VARJO_quad_views");
        case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
            return IsInstanceExtensionEnabled("XR_MSFT_first_person_observer");
        default:
            assert(false);
            return false;
        }
    }

    // Encapsulates xrEnumerateSwapchainFormats/xrCreateSwapchain
    XrResult CreateColorSwapchain(XrSession session, IGraphicsPlugin* graphicsPlugin, XrSwapchain* swapchain, XrExtent2Di* widthHeight,
                                  uint32_t arraySize, bool cubemap, XrSwapchainCreateInfo* createInfoReturn)
    {
        std::vector<int64_t> formatArray;
        uint32_t countOutput;
        XrResult result = xrEnumerateSwapchainFormats(session, 0, &countOutput, nullptr);

        if (result == XR_SUCCESS) {  // This should succeed
            if (widthHeight->width < 1)
                widthHeight->width = 256;
            if (widthHeight->height < 1)
                widthHeight->height = 256;

            formatArray.resize(countOutput);
            result = xrEnumerateSwapchainFormats(session, (uint32_t)formatArray.size(), &countOutput, formatArray.data());

            if (result == XR_SUCCESS) {
                XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};

                XrSwapchainUsageFlags usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                if (graphicsPlugin->DescribeGraphics() != "OpenGL") {
                    // mutability exists in GL but isn't used in the conformance tests, so don't require it
                    usageFlags |= XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
                }

                createInfo.faceCount = cubemap ? 6 : 1;
                createInfo.createFlags = 0;  // XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT or XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT
                createInfo.usageFlags = usageFlags;
                createInfo.format = graphicsPlugin->SelectColorSwapchainFormat(formatArray.data(), formatArray.size());
                createInfo.sampleCount = 1;
                createInfo.width = (uint32_t)widthHeight->width;
                createInfo.height = (uint32_t)widthHeight->height;
                createInfo.arraySize = arraySize;
                createInfo.mipCount = 1;

                if (createInfoReturn) {
                    *createInfoReturn = createInfo;
                }

                result = xrCreateSwapchain(session, &createInfo, swapchain);
            }
        }

        return result;
    }

    // Encapsulates xrEnumerateSwapchainFormats/xrCreateSwapchain
    XrResult CreateDepthSwapchain(XrSession session, IGraphicsPlugin* graphicsPlugin, XrSwapchain* swapchain, XrExtent2Di* widthHeight,
                                  uint32_t arraySize)
    {
        std::vector<int64_t> formatArray;
        uint32_t countOutput;
        XrResult result = xrEnumerateSwapchainFormats(session, 0, &countOutput, nullptr);

        if (result == XR_SUCCESS) {  // This should succeed
            if (widthHeight->width < 1)
                widthHeight->width = 256;
            if (widthHeight->height < 1)
                widthHeight->height = 256;

            formatArray.resize(countOutput);
            result = xrEnumerateSwapchainFormats(session, (uint32_t)formatArray.size(), &countOutput, formatArray.data());

            if (result == XR_SUCCESS) {
                XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};

                XrSwapchainUsageFlags usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                if (graphicsPlugin->DescribeGraphics() != "OpenGL") {
                    // mutability exists in GL but isn't used in the conformance tests, so don't require it
                    usageFlags |= XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
                }

                createInfo.faceCount = 1;
                createInfo.createFlags = 0;  // XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT or XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT
                createInfo.usageFlags = usageFlags;
                createInfo.format = graphicsPlugin->SelectDepthSwapchainFormat(formatArray.data(), formatArray.size());
                createInfo.sampleCount = 1;
                createInfo.width = (uint32_t)widthHeight->width;
                createInfo.height = (uint32_t)widthHeight->height;
                createInfo.arraySize = arraySize;
                createInfo.mipCount = 1;

                result = xrCreateSwapchain(session, &createInfo, swapchain);
            }
        }

        return result;
    }

    XrResult CycleToNextSwapchainImage(XrSwapchain* swapchainArray, size_t count, XrDuration timeoutNs)
    {
        XrResult result = XR_SUCCESS;
        bool timeoutOccurred = false;

        for (size_t i = 0; (i < count) && !timeoutOccurred; ++i) {
            XrSwapchain swapchain = swapchainArray[i];
            uint32_t index;

            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            result = xrAcquireSwapchainImage(swapchain, &acquireInfo, &index);
            if (XR_FAILED(result))
                return result;

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = timeoutNs;
            result = xrWaitSwapchainImage(swapchain, &waitInfo);
            if (XR_FAILED(result))
                return result;

            if (result == XR_TIMEOUT_EXPIRED) {
                // In this case we call xrReleaseSwapchainImage so as
                // not to leave the texture in an acquired state.
                // But if we get a failure in the release call below then that takes precedence.
                timeoutOccurred = true;
            }

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            result = xrReleaseSwapchainImage(swapchain, &releaseInfo);
            if (XR_FAILED(result))
                return result;
        }

        if (timeoutOccurred) {
            assert(XR_SUCCEEDED(result));  // Should be impossible for this to fail.
            result = XR_TIMEOUT_EXPIRED;
        }

        return result;
    }

    // Encapsulates xrCreateActionSet/xrCreateAction
    XrResult CreateActionSet(XrInstance instance, XrActionSet* actionSet, std::vector<XrAction>* actionVector,
                             const XrPath* subactionPathArray, size_t subactionPathArraySize)
    {
        XrActionSetCreateInfo createInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(createInfo.actionSetName, "test_action_set");
        strcpy(createInfo.localizedActionSetName, "TestActionSet");

        XrResult result = xrCreateActionSet(instance, &createInfo, actionSet);  // Should succeed.
        if (XR_SUCCEEDED(result)) {
            XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
            strcpy(actionCreateInfo.actionName, "test_action");
            actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionCreateInfo.localizedActionName, "TestAction");
            actionCreateInfo.countSubactionPaths = (uint32_t)subactionPathArraySize;
            actionCreateInfo.subactionPaths = subactionPathArray;

            XrAction action;
            result = xrCreateAction(*actionSet, &actionCreateInfo, &action);  // Should succeed.
            if (XR_SUCCEEDED(result))
                actionVector->push_back(action);
            else {
                xrDestroyActionSet(*actionSet);
                *actionSet = XR_NULL_HANDLE;
            }
        }
        else {
            actionSet = XR_NULL_HANDLE;
        }

        return result;
    }

    bool GetRuntimeMajorMinorVersion(XrVersion& version)
    {
        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        XrInstance instance;

        for (int32_t major = 10; major >= 0; major--) {
            createInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(major, 0, 0);

            XrResult result = xrCreateInstance(&createInfo, &instance);
            if (result == XR_ERROR_API_VERSION_UNSUPPORTED) {
                continue;  // Try the next lower number.
            }
            if (XR_SUCCEEDED(result)) {  // This is the highest major version. Try minor versions now.
                xrDestroyInstance(instance);

                // Try successive minor versions.
                for (int32_t minor = 99; minor >= 0; minor--) {
                    createInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(major, minor, 0);
                    result = xrCreateInstance(&createInfo, &instance);
                    if (result == XR_ERROR_API_VERSION_UNSUPPORTED) {
                        continue;  // Try the next lower number.
                    }
                    if (XR_SUCCEEDED(result)) {  // This is the highest major version. Try minor versions now.
                        xrDestroyInstance(instance);
                        version = createInfo.applicationInfo.apiVersion;
                        return true;
                    }

                    break;
                }
            }
            else {
                break;
            }
        }

        version = 0;
        return false;  // Error occurred above.
    };

}  // namespace Conformance
