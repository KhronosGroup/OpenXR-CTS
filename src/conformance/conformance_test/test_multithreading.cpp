// Copyright (c) 2017 The Khronos Group Inc.
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
#include <string>
#include <thread>
#include <condition_variable>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <chrono>
#include <random>
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    // The way we do the primary test here, we create an instance and session, then exercise
    // API calls from multiple threads with the given instance/session.
    class ThreadTestEnvironment;

    enum class CallRequirement
    {
        global,    // The function can can be called without an instance.
        instance,  // The function requires an instance to be active.
        systemId,  // The function requires a systemID (and thus also an instance) to be active.
        session    // The function requires a session (and thus also a systemId and instance) to be active.
    };

    // Exercises a function, returns resulting error count.
    typedef void (*ExerciseFunction)(ThreadTestEnvironment& env);

    struct ThreadTestFunction
    {
        const char* functionName;
        CallRequirement callRequirement;
        ExerciseFunction exerciseFunction;
    };

    extern const std::vector<ThreadTestFunction> globalTestFunctionVector;

    // ThreadTestEnvironment
    //
    // Defines the environment in which a multithreaded test occurs.
    //
    class ThreadTestEnvironment
    {
    public:
        ThreadTestEnvironment(uint32_t invocationCountInitial)
            : envMutex()
            , autoBasicSession(AutoBasicSession::none)  // Do nothing yet.
            , shouldBegin(false)
            , threadStartSignal()
            , invocationCount(invocationCountInitial)
            , outputText()
            , errorCount(0)
            , threadVector()
            , testFunctionVector(globalTestFunctionVector)  // Just copy global one for now.
        {
            if (!GetGlobalData().IsUsingGraphicsPlugin()) {
                // Remove functions that won't work in headless.
                const std::string swapchain{"Swapchain"};
                const std::string waitframe{"WaitFrame"};
                testFunctionVector.erase(std::remove_if(testFunctionVector.begin(), testFunctionVector.end(),
                                                        [&](const ThreadTestFunction& elt) {
                                                            auto name = std::string{elt.functionName};
                                                            return std::string::npos != name.find(swapchain) ||
                                                                   std::string::npos != name.find(waitframe);
                                                        }),
                                         testFunctionVector.end());
            }
        }

    public:
        AutoBasicSession& GetAutoBasicSession()
        {
            return autoBasicSession;
        }

        void WaitToBegin()
        {
            for (;;) {
                std::unique_lock<std::mutex> lock(envMutex);
                if (shouldBegin)
                    break;
                threadStartSignal.wait(lock);
            }
        }

        void SignalBegin()
        {
            std::unique_lock<std::mutex> lock(envMutex);
            shouldBegin = true;
            threadStartSignal.notify_all();
        }

        uint32_t InvocationCount() const
        {
            return invocationCount;
        }

        const std::string& OutputText() const
        {
            return outputText;
        }

        // See AppendOutputText for text handling.
        void AppendError(const char* text)
        {
            std::unique_lock<std::mutex> lock(envMutex);
            errorCount++;
            outputText += text;
            outputText += '\n';
        }

        uint32_t ErrorCount() const
        {
            return errorCount;
        }

        std::vector<std::thread>& ThreadVector()
        {
            return threadVector;
        }

        std::vector<ThreadTestFunction>& TestFunctionVector()
        {
            return testFunctionVector;
        }

    protected:
        // Guards access to all the member data below.
        std::mutex envMutex;

        // The instance may be XR_NULL_HANDLE if the environment is testing the case of instance not being active.
        // The session and systemId may be XR_NULL_HANDLE if the environment is testing the case of session not being active.
        AutoBasicSession autoBasicSession;

        // If true then each thread should begin executing.
        bool shouldBegin;

        // Threads should wait until this is signaled before beginning.
        std::condition_variable threadStartSignal;

        // The number of times each of the threads should invoke functions before exiting.
        uint32_t invocationCount;

        // Any text to be displayed upon completing the tests. Catch2 can't currently handle
        // multithreaded testing, so we need to do the testing without Catch2 in threads, then
        // when done print the test.
        std::string outputText;

        // The sum of errors produced by all functions from all threads.
        std::atomic<std::uint32_t> errorCount;

        // All the threads currently executing with this ThreadTestEnvironment.
        std::vector<std::thread> threadVector;

        // Constant for the life of the ThreadTestEnvironment
        std::vector<ThreadTestFunction> testFunctionVector;
    };

    // SessionThreadFunction
    //
    // Executes a single thread of a multithreading test.
    // Works by invoking random Exercise functions a limited number of times.
    // Returns the error count.
    void SessionThreadFunction(ThreadTestEnvironment& env)
    {
        RandEngine& randEngine = GetGlobalData().GetRandEngine();

        env.WaitToBegin();

        for (uint32_t i = 0; i < env.InvocationCount(); ++i) {
            size_t functionIndex = randEngine.RandSizeT(0, env.TestFunctionVector().size());
            const ThreadTestFunction& testFunction = env.TestFunctionVector()[functionIndex];

            try {
                if ((testFunction.callRequirement == CallRequirement::session) &&
                    (env.GetAutoBasicSession().GetSession() != XR_NULL_HANDLE)) {
                    testFunction.exerciseFunction(env);
                }
                else if ((testFunction.callRequirement == CallRequirement::systemId) &&
                         (env.GetAutoBasicSession().GetSystemId() != XR_NULL_SYSTEM_ID)) {
                    testFunction.exerciseFunction(env);
                }
                else if ((testFunction.callRequirement == CallRequirement::instance) &&
                         (env.GetAutoBasicSession().GetInstance() != XR_NULL_HANDLE)) {
                    testFunction.exerciseFunction(env);
                }
                else if (testFunction.callRequirement == CallRequirement::global) {
                    testFunction.exerciseFunction(env);
                }
                else {    // Else we can't call this function due to the environment.
                    --i;  // Don't count it as an invocation.
                }
            }
            catch (const std::exception& ex) {
                env.AppendError(ex.what());
            }
        }
    }

    TEST_CASE("multithreading", "")
    {
        // As of May 2019, Catch2 documents that multithreaded tests must not access test primitives (e.g. REQUIRE)
        // from multiple threads simultaneously, though it is planned to be supported at some point in the future.
        // The problem is that there's static state which is not protected nor synchronized across threads.
        // As a result, we need to write tests such at the threads either save their results for later serialization
        // or that we implement manual serialization around Catch2 usage. How to properly accomplish the latter is
        // not currently understood by us, and it may well involve more than just wrapping REQUIRE in a THREADSAFE_REQUIRE,
        // because Catch2 macros are not independent entities but rather depend on the state/environment they are
        // executed within.
        //
        // See the Threading Behavior section of the OpenXR specification for documentation.
        const size_t threadCount = 2;        // 10;         // To do: Make this configurable.
        const size_t invocationCount = 100;  // 10000;  // To do: Make this configurable.

        auto RunTestEnvironment = [threadCount](ThreadTestEnvironment& env) -> void {
            std::vector<std::thread>& threadVector = env.ThreadVector();

            for (size_t i = 0; i < threadCount; ++i)
                threadVector.emplace_back(std::thread(SessionThreadFunction, std::ref(env)));

            env.SignalBegin();

            for (size_t i = 0; i < threadCount; ++i)
                threadVector[i].join();

            REQUIRE_MSG(env.ErrorCount() == 0, env.OutputText())
        };

        // Exerise instanceless multithreading
        {
            // Leave instance and session NULL.
            ThreadTestEnvironment env(invocationCount);

            RunTestEnvironment(env);
        }

        // Exercise instance without session multithreading
        {
            ThreadTestEnvironment env(invocationCount);
            env.GetAutoBasicSession().Init(AutoBasicSession::createInstance);

            RunTestEnvironment(env);
        }

        // Exercise session multithreading.
        {
            ThreadTestEnvironment env(invocationCount);
            env.GetAutoBasicSession().Init(AutoBasicSession::beginSession | AutoBasicSession::createActions |
                                           AutoBasicSession::createSpaces | AutoBasicSession::createSwapchains);

            RunTestEnvironment(env);
        }
    }

#if defined(_MSC_VER)
#pragma warning(disable : 4189)
#endif  // defined(_MSC_VER)

    // To consider: We could have exercise functions below auto-add themselves to a vector on startup.
    // A challenge with that is that code linkers will often elide such auto-add functions unless you
    // annotate them specially [e.g. GCC's __attribute__((constructor)) ] See XRC_BEGIN_ON_STARTUP.

    void Exercise_xrGetInstanceProcAddr(ThreadTestEnvironment& env)
    {
        PFN_xrVoidFunction voidFunction;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(env.GetAutoBasicSession().GetInstance(), "xrPollEvent", &voidFunction));
    }

    void Exercise_xrEnumerateInstanceExtensionProperties(ThreadTestEnvironment&)
    {
        std::vector<XrExtensionProperties> properties(16, {XR_TYPE_EXTENSION_PROPERTIES});
        uint32_t propertyCountOutput;
        XRC_CHECK_THROW_XRCMD(
            xrEnumerateInstanceExtensionProperties(nullptr, (uint32_t)properties.size(), &propertyCountOutput, properties.data()));
    }

    void Exercise_xrEnumerateApiLayerProperties(ThreadTestEnvironment&)
    {
        // XrResult xrEnumerateApiLayerProperties(uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrApiLayerProperties* properties);
        std::vector<XrApiLayerProperties> properties(16, {XR_TYPE_API_LAYER_PROPERTIES});
        uint32_t propertyCountOutput;
        XRC_CHECK_THROW_XRCMD(xrEnumerateApiLayerProperties((uint32_t)properties.size(), &propertyCountOutput, properties.data()));
    }

    void Exercise_xrCreateInstance(ThreadTestEnvironment&)
    {
        XrInstance instance;
        XrResult result = CreateBasicInstance(&instance);
        XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);

        if (XR_SUCCEEDED(result)) {
            SleepMs(50);
            XRC_CHECK_THROW_XRCMD(xrDestroyInstance(instance));
        }
    }

    void Exercise_xrDestroyInstance(ThreadTestEnvironment& env)
    {
        Exercise_xrCreateInstance(env);
    }

    void Exercise_xrGetInstanceProperties(ThreadTestEnvironment& env)
    {
        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProperties(env.GetAutoBasicSession().GetInstance(), &instanceProperties));
    }

    void Exercise_xrPollEvent(ThreadTestEnvironment& env)
    {
        // We can't likely exercise this well unless multiple threads are dequeing messages at
        // the same time. We need a means to tell the runtime to queue such messages.
        XrEventDataBuffer eventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
        XRC_CHECK_THROW_XRCMD(xrPollEvent(env.GetAutoBasicSession().GetInstance(), &eventDataBuffer));
    }

    void Exercise_xrResultToString(ThreadTestEnvironment& env)
    {
        char buffer[XR_MAX_RESULT_STRING_SIZE];
        RandEngine& randEngine = GetGlobalData().GetRandEngine();
        XrResult value = (XrResult)randEngine.RandInt32(-45, 9);  // Need a better way to id the min/max values,
        XRC_CHECK_THROW_XRCMD(xrResultToString(env.GetAutoBasicSession().GetInstance(), value, buffer));  // but this can be inaccurate.
    }

    void Exercise_xrStructureTypeToString(ThreadTestEnvironment& env)
    {
        char buffer[XR_MAX_STRUCTURE_NAME_SIZE];
        RandEngine& randEngine = GetGlobalData().GetRandEngine();
        XrStructureType value = (XrStructureType)randEngine.RandInt32(0, 57);  // Need a better way to id the min/max values,
        XRC_CHECK_THROW_XRCMD(xrStructureTypeToString(env.GetAutoBasicSession().GetInstance(), value,
                                                      buffer));  // but this can be inaccurate.
    }

    void Exercise_xrGetSystem(ThreadTestEnvironment& env)
    {
        GlobalData& globalData = GetGlobalData();
        XrSystemGetInfo getInfo{XR_TYPE_SYSTEM_GET_INFO, nullptr, globalData.GetOptions().formFactorValue};
        XrSystemId systemId;
        XRC_CHECK_THROW_XRCMD(xrGetSystem(env.GetAutoBasicSession().GetInstance(), &getInfo, &systemId));
    }

    void Exercise_xrGetSystemProperties(ThreadTestEnvironment& env)
    {
        XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
        XRC_CHECK_THROW_XRCMD(
            xrGetSystemProperties(env.GetAutoBasicSession().GetInstance(), env.GetAutoBasicSession().GetSystemId(), &properties));
    }

    void Exercise_xrEnumerateEnvironmentBlendModes(ThreadTestEnvironment& env)
    {
        GlobalData& globalData = GetGlobalData();

        std::array<XrEnvironmentBlendMode, 8> environmentBlendModes;
        uint32_t environmentBlendModeCountOutput;

        XRC_CHECK_THROW_XRCMD(
            xrEnumerateEnvironmentBlendModes(env.GetAutoBasicSession().GetInstance(), env.GetAutoBasicSession().GetSystemId(),
                                             globalData.GetOptions().viewConfigurationValue, (uint32_t)environmentBlendModes.size(),
                                             &environmentBlendModeCountOutput, environmentBlendModes.data()));
    }

    void Exercise_xrCreateSession(ThreadTestEnvironment& env)
    {
        GlobalData& globalData = GetGlobalData();
        const XrBaseInStructure* graphicsBinding = globalData.graphicsPlugin->GetGraphicsBinding();

        XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
        createInfo.next = graphicsBinding;
        createInfo.systemId = env.GetAutoBasicSession().GetSystemId();
        XrSession session;
        XrResult result = xrCreateSession(env.GetAutoBasicSession().GetInstance(), &createInfo, &session);
        XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);

        if (XR_SUCCEEDED(result)) {
            SleepMs(50);
            XRC_CHECK_THROW_XRCMD(xrDestroySession(session));
        }
    }

    void Exercise_xrDestroySession(ThreadTestEnvironment& env)
    {
        return Exercise_xrCreateSession(env);
    }

    void Exercise_xrEnumerateReferenceSpaces(ThreadTestEnvironment& env)
    {
        std::array<XrReferenceSpaceType, 16> spaces;
        uint32_t spaceCountOutput;
        XRC_CHECK_THROW_XRCMD(
            xrEnumerateReferenceSpaces(env.GetAutoBasicSession().GetSession(), (uint32_t)spaces.size(), &spaceCountOutput, spaces.data()));
    }

    void Exercise_xrCreateReferenceSpace(ThreadTestEnvironment& env)
    {
        // To do: make the reference space type dynamically chosen.
        XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr, XR_REFERENCE_SPACE_TYPE_VIEW, XrPosefCPP()};
        XrSpace space;
        XrResult result = xrCreateReferenceSpace(env.GetAutoBasicSession().GetSession(), &createInfo, &space);
        XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);

        if (XR_SUCCEEDED(result)) {
            SleepMs(50);
            XRC_CHECK_THROW_XRCMD(xrDestroySpace(space));
        }
    }

    void Exercise_xrGetReferenceSpaceBoundsRect(ThreadTestEnvironment& env)
    {
        // To do: make the reference space type dynamically chosen.
        XrExtent2Df bounds{};
        XRC_CHECK_THROW_XRCMD(
            xrGetReferenceSpaceBoundsRect(env.GetAutoBasicSession().GetSession(), XR_REFERENCE_SPACE_TYPE_LOCAL, &bounds));
    }

    void Exercise_xrCreateActionSpace(ThreadTestEnvironment& /*env*/)
    {
// TODO: Need an action handle.
#if 0
        XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceCreateInfo.subactionPath = XR_NULL_PATH;
        actionSpaceCreateInfo.poseInActionSpace = XrPosefCPP();

        XrSpace space;
        XrResult result = xrCreateActionSpace(XrAction action, &actionSpaceCreateInfo, XrSpace * space));
        XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);

            SleepMs(50);
            XRC_CHECK_THROW_XRCMD(xrDestroySpace(space));
#endif
    }

    void Exercise_xrLocateSpace(ThreadTestEnvironment& env)
    {
        // XrTime Conformance::GetCurrentXrTime(XrInstance instance)
        // XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceRelation* relation);
        (void)env;
        // return false;
    }

    void Exercise_xrDestroySpace(ThreadTestEnvironment& env)
    {
        return Exercise_xrCreateReferenceSpace(env);
    }

    void Exercise_xrEnumerateViewConfigurations(ThreadTestEnvironment& env)
    {
        std::array<XrViewConfigurationType, 16> viewConfigurationTypeArray;
        uint32_t countOutput;
        XRC_CHECK_THROW_XRCMD(
            xrEnumerateViewConfigurations(env.GetAutoBasicSession().GetInstance(), env.GetAutoBasicSession().GetSystemId(),
                                          (uint32_t)viewConfigurationTypeArray.size(), &countOutput, viewConfigurationTypeArray.data()));
    }

    void Exercise_xrGetViewConfigurationProperties(ThreadTestEnvironment& env)
    {
        const GlobalData& globalData = GetGlobalData();
        XrViewConfigurationProperties viewConfigurationProperties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};

        XRC_CHECK_THROW_XRCMD(xrGetViewConfigurationProperties(env.GetAutoBasicSession().GetInstance(),
                                                               env.GetAutoBasicSession().GetSystemId(),
                                                               globalData.options.viewConfigurationValue, &viewConfigurationProperties));
    }

    void Exercise_xrEnumerateViewConfigurationViews(ThreadTestEnvironment& env)
    {
        const GlobalData& globalData = GetGlobalData();
        std::vector<XrViewConfigurationView> viewConfigurationViewArray(16, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        uint32_t countOutput;
        XrResult result = xrEnumerateViewConfigurationViews(
            env.GetAutoBasicSession().GetInstance(), env.GetAutoBasicSession().GetSystemId(), globalData.options.viewConfigurationValue,
            (uint32_t)viewConfigurationViewArray.size(), &countOutput, viewConfigurationViewArray.data());

        // Could potentially validate viewConfigurationViewArray.

        // return XR_SUCCEEDED(result);
    }

    void Exercise_xrEnumerateSwapchainFormats(ThreadTestEnvironment& env)
    {
        std::vector<int64_t> formatArray;
        uint32_t countOutput;
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainFormats(env.GetAutoBasicSession().GetSession(), 0, &countOutput, nullptr));

        formatArray.resize(countOutput);
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainFormats(env.GetAutoBasicSession().GetSession(), (uint32_t)formatArray.size(),
                                                          &countOutput, formatArray.data()));
    }

    void Exercise_xrCreateSwapchain(ThreadTestEnvironment& env)
    {
        GlobalData& globalData = GetGlobalData();
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin = globalData.GetGraphicsPlugin();
        XrSwapchain swapchain;
        XrExtent2Di widthHeight{0, 0};  // 0,0 means Use defaults.
        XrResult result = CreateColorSwapchain(env.GetAutoBasicSession().GetSession(), graphicsPlugin.get(), &swapchain, &widthHeight);
        XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);

        if (XR_SUCCEEDED(result)) {
            SleepMs(50);
            XRC_CHECK_THROW_XRCMD(xrDestroySwapchain(swapchain));
        }
    }

    void Exercise_xrDestroySwapchain(ThreadTestEnvironment& env)
    {
        return Exercise_xrCreateSwapchain(env);
    }

    void Exercise_xrEnumerateSwapchainImages(ThreadTestEnvironment& env)
    {
        GlobalData& globalData = GetGlobalData();
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin = globalData.GetGraphicsPlugin();
        XrSwapchainCreateInfo createInfo;
        XrSwapchain swapchain;
        XrExtent2Di widthHeight{0, 0};  // 0,0 means Use defaults.
        XRC_CHECK_THROW_XRCMD(CreateColorSwapchain(env.GetAutoBasicSession().GetSession(), graphicsPlugin.get(), &swapchain, &widthHeight,
                                                   1, false, &createInfo));

        uint32_t countOutput;
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr));

        std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs> p = graphicsPlugin->AllocateSwapchainImageStructs(countOutput, createInfo);
        uint32_t newCountOutput;
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, countOutput, &newCountOutput, p->imagePtrVector[0]));
        XRC_CHECK_THROW(newCountOutput == countOutput);
    }

    void Exercise_xrAcquireSwapchainImage(ThreadTestEnvironment& env)
    {
        GlobalData& globalData = GetGlobalData();
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin = globalData.GetGraphicsPlugin();
        XrSwapchain swapchain;
        XrExtent2Di widthHeight{0, 0};  // 0,0 means Use defaults.
        XRC_CHECK_THROW_XRCMD(CreateColorSwapchain(env.GetAutoBasicSession().GetSession(), graphicsPlugin.get(), &swapchain, &widthHeight));

        const size_t iterationCount = 100;  // To do: Make this configurable.

        for (size_t i = 0; i < iterationCount; ++i) {
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            uint32_t index;
            XRC_CHECK_THROW_XRCMD(xrAcquireSwapchainImage(swapchain, &acquireInfo, &index));
            SleepMs(5);

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = 10000000;  // 10ms
            XRC_CHECK_THROW_XRCMD(xrWaitSwapchainImage(swapchain, &waitInfo));
            SleepMs(5);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            XRC_CHECK_THROW_XRCMD(xrReleaseSwapchainImage(swapchain, &releaseInfo));
            SleepMs(5);
        }
    }

    void Exercise_xrWaitSwapchainImage(ThreadTestEnvironment& env)
    {
        return Exercise_xrAcquireSwapchainImage(env);
    }

    void Exercise_xrReleaseSwapchainImage(ThreadTestEnvironment& env)
    {
        return Exercise_xrAcquireSwapchainImage(env);
    }

    // XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo);
    // XrResult xrEndSession(XrSession session);
    // XrResult xrRequestExitSession(XrSession session);

    // XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState);
    // XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo);
    // XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo);

    // XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views);

    void Exercise_xrStringToPath(ThreadTestEnvironment& env)
    {
        RandEngine& randEngine = GetGlobalData().GetRandEngine();

        const size_t iterationCount = 100;  // To do: Make this configurable.
        std::vector<std::pair<XrPath, std::string>> strVector;

        for (size_t i = 0; i < iterationCount; ++i) {
            size_t value = randEngine.RandSizeT(0, 10000);  // To do: Make this configurable.
            std::string pathStr = std::string("/") + std::to_string(value);
            XrPath path = XR_NULL_PATH;

            XRC_CHECK_THROW_XRCMD(xrStringToPath(env.GetAutoBasicSession().GetInstance(), pathStr.c_str(), &path));
            strVector.push_back(std::make_pair(path, pathStr));

            for (size_t j = 0; j < 5; ++j) {
                size_t index = randEngine.RandSizeT(0, strVector.size());
                path = strVector[index].first;
                pathStr = strVector[index].second;
                char buffer[32];

                uint32_t bufferCount;
                XRC_CHECK_THROW_XRCMD(
                    xrPathToString(env.GetAutoBasicSession().GetInstance(), path, (uint32_t)pathStr.length() + 1, &bufferCount, buffer));
            }
        }
    }

    void Exercise_xrPathToString(ThreadTestEnvironment& env)
    {
        return Exercise_xrStringToPath(env);
    }

    void Exercise_xrCreateActionSet(ThreadTestEnvironment& env)
    {
        const size_t iterationCount = 100;  // To do: Make this configurable.
        std::vector<XrActionSet> actionSetVector;

        // Construct a unique action set name across any threads.
        std::string strBase = "actionset_" + std::to_string(reinterpret_cast<uintptr_t>(&actionSetVector)) + "_";

        for (size_t i = 0; i < iterationCount; ++i) {
            std::string actionSetName = strBase + std::to_string(i);

            XrActionSetCreateInfo createInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(createInfo.actionSetName, actionSetName.c_str());
            strcpy(createInfo.localizedActionSetName, actionSetName.c_str());

            XrActionSet actionSet;
            XrResult result = xrCreateActionSet(env.GetAutoBasicSession().GetInstance(), &createInfo, &actionSet);
            XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);
            if (XR_SUCCEEDED(result)) {
                actionSetVector.push_back(actionSet);
            }
        }

        for (size_t a = 0; a != actionSetVector.size(); ++a)
            XRC_CHECK_THROW_XRCMD(xrDestroyActionSet(actionSetVector[a]));
    }

    void Exercise_xrDestroyActionSet(ThreadTestEnvironment& env)
    {
        return Exercise_xrCreateActionSet(env);
    }

    void Exercise_xrCreateAction(ThreadTestEnvironment& env)
    {
        std::string strBase = "actionset_";  // Construct a unique action set name across any threads.
        std::string actionSetName = strBase + std::to_string(reinterpret_cast<uintptr_t>(&strBase));

        XrActionSetCreateInfo actionSetCreateInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetCreateInfo.actionSetName, actionSetName.c_str());
        strcpy(actionSetCreateInfo.localizedActionSetName, actionSetName.c_str());

        XrActionSet actionSet;
        XRC_CHECK_THROW_XRCMD(xrCreateActionSet(env.GetAutoBasicSession().GetInstance(), &actionSetCreateInfo, &actionSet));
        std::vector<XrAction> actionVector;
        const size_t iterationCount = 100;  // To do: Make this configurable.

        for (size_t i = 0; i < iterationCount; ++i) {
            std::string actionName = std::string("action_") + std::to_string(i);

            XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO};
            strcpy(actionCreateInfo.actionName, actionName.c_str());
            actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionCreateInfo.localizedActionName, actionName.c_str());
            // uint32_t countSubactionPaths; Anythning to exercise with this?
            // const XrPath* subactionPaths;

            XrAction action;
            XrResult result = xrCreateAction(actionSet, &actionCreateInfo, &action);
            XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);
            if (XR_SUCCEEDED(result)) {
                actionVector.push_back(action);
            }
        }

        for (size_t a = 0; a != actionVector.size(); ++a)
            XRC_CHECK_THROW_XRCMD(xrDestroyAction(actionVector[a]));

        XRC_CHECK_THROW_XRCMD(xrDestroyActionSet(actionSet));
    }

    void Exercise_xrDestroyAction(ThreadTestEnvironment& env)
    {
        return Exercise_xrCreateAction(env);
    }

    void Exercise_xrSyncActions(ThreadTestEnvironment& /*env*/)
    {
        XrResult result = XR_SUCCESS;

        /* Disabled while this API is still in flux.

        // References to AutoBasicSession members.
        XrSession& session = env.GetAutoBasicSession().session;
        XrActionSet& actionSet = env.GetAutoBasicSession().actionSet;
        std::vector<XrAction>& actionVector = env.GetAutoBasicSession().actionVector;  // This actions are part of the actionSet.
        std::array<XrPath, 2>& handSubactionArray = env.GetAutoBasicSession().handSubactionArray;

        std::vector<XrActiveActionSet> activeActionSetVector = {{actionSet, handSubactionArray[0]}, {actionSet, handSubactionArray[1]}};
        const size_t iterationCount = 100;  // To do: Make this configurable.

        for (size_t i = 0; i < iterationCount; ++i) {
            XrActionsSyncInfo actionsSyncInfo{XR_TYPE_ACTIONS_SYNC_INFO, 0, (uint32_t)activeActionSetVector.size(),
                                              activeActionSetVector.data()};

            result = xrSyncActions(session, &actionsSyncInfo);
            if (FAILED(result))
                break;

            // Call xrGetActionStateBoolean
            size_t a = GenerateRandomSizeT(0, actionVector.size());
            XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            actionStateGetInfo.action = actionVector[a];
            actionStateGetInfo.countSubactionPaths = (uint32_t)handSubactionArray.size();
            actionStateGetInfo.subactionPaths = handSubactionArray.data();
            XrActionStateBoolean actionStateBoolean{XR_TYPE_ACTION_STATE_BOOLEAN};
            result = xrGetActionStateBoolean(session, &actionStateGetInfo, &actionStateBoolean);
            if (FAILED(result))
                break;
            // actionStateBoolean.currentState; // Possibly validate these.
            // actionStateBoolean.changedSinceLastSync;
            // actionStateBoolean.lastChangeTime;
            // actionStateBoolean.isActive;

            // Call xrGetActionStateFloat
            a = GenerateRandomSizeT(0, actionVector.size());
            actionStateGetInfo.action = actionVector[a];
            XrActionStateFloat actionStateFloat{XR_TYPE_ACTION_STATE_FLOAT};
            result = xrGetActionStateFloat(session, &actionStateGetInfo, &actionStateFloat);
            if (FAILED(result))
                break;
            // actionStateBoolean.currentState; // Possibly validate these.
            // actionStateBoolean.changedSinceLastSync;
            // actionStateBoolean.lastChangeTime;
            // actionStateBoolean.isActive;

            // Call xrGetActionStateVector2f
            a = GenerateRandomSizeT(0, actionVector.size());
            actionStateGetInfo.action = actionVector[a];
            XrActionStateVector2f actionStateVector2F{XR_TYPE_ACTION_STATE_VECTOR2F};
            result = xrGetActionStateVector2f(session, &actionStateGetInfo, &actionStateVector2F);
            if (FAILED(result))
                break;
            // actionStateBoolean.currentState; // Possibly validate these.
            // actionStateBoolean.changedSinceLastSync;
            // actionStateBoolean.lastChangeTime;
            // actionStateBoolean.isActive;

            // To do:
            // result = xrGetActionStatePose(session, XrActionStateGetInfo* getInfo, XrActionStatePose* state);

            // Call xrGetBoundSourcesForAction
            XrBoundSourcesForActionGetInfo boundSources{XR_TYPE_BOUND_SOURCES_FOR_ACTION_GET_INFO};
            std::vector<XrPath> boundSourcePathVector;
            uint32_t countOutput;
            result = xrGetBoundSourcesForAction(session, &boundSources, 0, &countOutput, nullptr);
            if (FAILED(result))
                break;
            boundSourcePathVector.resize(countOutput);
            result = xrGetBoundSourcesForAction(session, &boundSources, countOutput, &countOutput, boundSourcePathVector.data());
            if (FAILED(result))
                break;

            if (countOutput) {  // If there were bound sources...
                // Call xrGetInputSourceLocalizedName
                XrInputSourceLocalizedNameGetInfo nameGetInfo{XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
                nameGetInfo.sourcePath = boundSourcePathVector[0];  // Could test others..
                nameGetInfo.whichComponents =
                    (XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                     XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT);
                char nameBuffer[512];
                result = xrGetInputSourceLocalizedName(session, &nameGetInfo, (uint32_t)sizeof(nameBuffer), &countOutput, nameBuffer);
                if (FAILED(result))
                    break;
                if (!ValidateStringUTF8(nameBuffer, sizeof(nameBuffer))) {
                    env.AppendError("xrGetInputSourceLocalizedName failure");
                    break;
                }
            }

            // To do:
            // result = xrSetInteractionProfileSuggestedBindings(session, const XrInteractionProfileSuggestedBinding* suggestedBindings);
            // if(FAILED(result))
            //     break;
            //
            // result = xrGetCurrentInteractionProfile(session, XrPath topLevelUserPath, XrInteractionProfileInfo* interactionProfile);
            // if(FAILED(result))
            //     break;
        }
*/
        // return XR_SUCCEEDED(result);
    }

    void Exercise_xrSetInteractionProfileSuggestedBindings(ThreadTestEnvironment& env)
    {
        return Exercise_xrSyncActions(env);
    }

    void Exercise_xrGetCurrentInteractionProfile(ThreadTestEnvironment& env)
    {
        return Exercise_xrSyncActions(env);
    }

    void Exercise_xrGetActionStateBoolean(ThreadTestEnvironment& env)
    {
        return Exercise_xrSyncActions(env);
    }

    void Exercise_xrGetActionStateVector1f(ThreadTestEnvironment& env)
    {
        return Exercise_xrSyncActions(env);
    }

    void Exercise_xrGetActionStateVector2f(ThreadTestEnvironment& env)
    {
        return Exercise_xrSyncActions(env);
    }

    void Exercise_xrGetActionStatePose(ThreadTestEnvironment& env)
    {
        return Exercise_xrSyncActions(env);
    }

    void Exercise_xrGetBoundSourcesForAction(ThreadTestEnvironment& env)
    {
        return Exercise_xrSyncActions(env);
    }

    void Exercise_xrGetInputSourceLocalizedName(ThreadTestEnvironment& env)
    {
        return Exercise_xrSyncActions(env);
    }

    void Exercise_xrApplyHapticFeedback(ThreadTestEnvironment& /*env*/)
    {
        // Test fails because the actionset is not bound to the session.
#if 0
        // Create an action set.
        XrActionSetCreateInfo actionSetInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
        std::string strActionSet = "actionset_" + std::to_string(reinterpret_cast<uintptr_t>(&actionSetInfo));
        strcpy(actionSetInfo.actionSetName, strActionSet.c_str());
        strcpy(actionSetInfo.localizedActionSetName, strActionSet.c_str());
        XrActionSet inGameActionSet;
        XrResult result = xrCreateActionSet(env.GetAutoBasicSession().GetInstance(), &actionSetInfo, &inGameActionSet);
        XRC_CHECK_THROW(XR_SUCCEEDED(result) || result == XR_ERROR_LIMIT_REACHED);
        if (result == XR_ERROR_LIMIT_REACHED) {
            return;  // Nothing more can be tested.
        }

        // Create a "player_hit" output action.
        XrActionCreateInfo actionCreateInfo = {XR_TYPE_ACTION_CREATE_INFO};
        std::string strAction = "action_" + std::to_string(reinterpret_cast<uintptr_t>(&actionSetInfo));
        strcpy(actionCreateInfo.actionName, strAction.c_str());
        actionCreateInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
        strcpy(actionCreateInfo.localizedActionName, strAction.c_str());

        XrAction hapticsAction;
        XRC_CHECK_THROW_XRCMD(xrCreateAction(inGameActionSet, &actionCreateInfo, &hapticsAction));

        XrPath hapticsPath;
        xrStringToPath(env.GetAutoBasicSession().GetInstance(), "/user/hand/right/output/haptic", &hapticsPath);

        XrHapticActionInfo hapticActionInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
        hapticActionInfo.action = hapticsAction;

        XrHapticVibration vibration = {XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = 0.5;
        vibration.duration = 200000000;  // 200ms
        vibration.frequency = 320;       // 320 cycles per second
        XRC_CHECK_THROW_XRCMD(
            xrApplyHapticFeedback(env.GetAutoBasicSession().GetSession(), &hapticActionInfo, (const XrHapticBaseHeader*)&vibration));

        // Possibly wait a little.
        XRC_CHECK_THROW_XRCMD(xrStopHapticFeedback(env.GetAutoBasicSession().GetSession(), &hapticActionInfo));

        XRC_CHECK_THROW_XRCMD(xrDestroyAction(hapticsAction));

        XRC_CHECK_THROW_XRCMD(xrDestroyActionSet(inGameActionSet));
#endif
    }

    void Exercise_xrStopHapticFeedback(ThreadTestEnvironment& env)
    {
        return Exercise_xrApplyHapticFeedback(env);
    }

    const std::vector<ThreadTestFunction> globalTestFunctionVector{
        {"xrGetInstanceProcAddr", CallRequirement::instance, Exercise_xrGetInstanceProcAddr},
        {"xrEnumerateInstanceExtensionProperties", CallRequirement::global, Exercise_xrEnumerateInstanceExtensionProperties},
        {"xrEnumerateApiLayerProperties", CallRequirement::global, Exercise_xrEnumerateApiLayerProperties},
        {"xrCreateInstance", CallRequirement::global, Exercise_xrCreateInstance},
        {"xDestroyInstance", CallRequirement::global, Exercise_xrDestroyInstance},
        {"xrGetInstanceProperties", CallRequirement::instance, Exercise_xrGetInstanceProperties},
        {"xrPollEvent", CallRequirement::instance, Exercise_xrPollEvent},
        {"xrResultToString", CallRequirement::instance, Exercise_xrResultToString},
        {"xrStructureTypeToString", CallRequirement::instance, Exercise_xrStructureTypeToString},
        {"xrGetSystem", CallRequirement::instance, Exercise_xrGetSystem},
        {"xrGetSystemProperties", CallRequirement::systemId, Exercise_xrGetSystemProperties},
        {"xrEnumerateEnvironmentBlendModes", CallRequirement::systemId, Exercise_xrEnumerateEnvironmentBlendModes},
        {"xrCreateSession", CallRequirement::systemId, Exercise_xrCreateSession},
        {"xrDestroySession", CallRequirement::systemId, Exercise_xrDestroySession},
        {"xrEnumerateReferenceSpaces", CallRequirement::session, Exercise_xrEnumerateReferenceSpaces},
        {"xrCreateReferenceSpace", CallRequirement::session, Exercise_xrCreateReferenceSpace},
        {"xrGetReferenceSpaceBoundsRect", CallRequirement::session, Exercise_xrGetReferenceSpaceBoundsRect},

        {"xrGetReferenceSpaceBoundsRect", CallRequirement::session, Exercise_xrGetReferenceSpaceBoundsRect},
        //{"xrCreateActionSpace", CallRequirement::session, Exercise_xrCreateActionSpace},
        //{"xrLocateSpace", CallRequirement::session, Exercise_xrLocateSpace},
        {"xrDestroySpace", CallRequirement::session, Exercise_xrDestroySpace},
        {"xrEnumerateViewConfigurations", CallRequirement::session, Exercise_xrEnumerateViewConfigurations},
        {"xrGetViewConfigurationProperties", CallRequirement::session, Exercise_xrGetViewConfigurationProperties},
        {"xrEnumerateViewConfigurationViews", CallRequirement::session, Exercise_xrEnumerateViewConfigurationViews},
        {"xrEnumerateSwapchainFormats", CallRequirement::session, Exercise_xrEnumerateSwapchainFormats},
        {"xrCreateSwapchain", CallRequirement::session, Exercise_xrCreateSwapchain},
        {"xrDestroySwapchain", CallRequirement::session, Exercise_xrDestroySwapchain},
        {"xrEnumerateSwapchainImages", CallRequirement::session, Exercise_xrEnumerateSwapchainImages},

        {"xrAcquireSwapchainImage", CallRequirement::session, Exercise_xrAcquireSwapchainImage},
        {"xrWaitSwapchainImage", CallRequirement::session, Exercise_xrWaitSwapchainImage},
        {"xrReleaseSwapchainImage", CallRequirement::session, Exercise_xrReleaseSwapchainImage},
        /*
        {"xrBeginSession", CallRequirement::session, Exercise_xrBeginSession},
        {"xrEndSession", CallRequirement::session, Exercise_xrEndSession},
        {"xrRequestExitSession", CallRequirement::session, Exercise_xrRequestExitSession},
        {"xrWaitFrame", CallRequirement::session, Exercise_xrWaitFrame},
        {"xrBeginFrame", CallRequirement::session, Exercise_xrBeginFrame},
        {"xrEndFrame", CallRequirement::session, Exercise_xrEndFrame},
        {"xrLocateViews", CallRequirement::session, Exercise_xrLocateViews},
        */
        {"xrStringToPath", CallRequirement::session, Exercise_xrStringToPath},
        {"xrPathToString", CallRequirement::session, Exercise_xrPathToString},
        {"xrCreateActionSet", CallRequirement::session, Exercise_xrCreateActionSet},
        {"xrDestroyActionSet", CallRequirement::session, Exercise_xrDestroyActionSet},
        {"xrCreateAction", CallRequirement::session, Exercise_xrCreateAction},
        {"xrDestroyAction", CallRequirement::session, Exercise_xrDestroyAction},
        {"xrSetInteractionProfileSuggestedBindings", CallRequirement::session, Exercise_xrSetInteractionProfileSuggestedBindings},
        {"xrGetCurrentInteractionProfile", CallRequirement::session, Exercise_xrGetCurrentInteractionProfile},
        {"xrGetActionStateBoolean", CallRequirement::session, Exercise_xrGetActionStateBoolean},
        {"xrGetActionStateVector1f", CallRequirement::session, Exercise_xrGetActionStateVector1f},
        {"xrGetActionStateVector2f", CallRequirement::session, Exercise_xrGetActionStateVector2f},
        {"xrGetActionStatePose", CallRequirement::session, Exercise_xrGetActionStatePose},
        {"xrSyncActions", CallRequirement::session, Exercise_xrSyncActions},
        {"xrGetBoundSourcesForAction", CallRequirement::session, Exercise_xrGetBoundSourcesForAction},
        {"xrGetInputSourceLocalizedName", CallRequirement::session, Exercise_xrGetInputSourceLocalizedName},
        {"xrApplyHapticFeedback", CallRequirement::session, Exercise_xrApplyHapticFeedback},
        {"xrStopHapticFeedback", CallRequirement::session, Exercise_xrStopHapticFeedback}};

}  // namespace Conformance
