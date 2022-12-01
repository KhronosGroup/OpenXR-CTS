// Copyright (c) 2019-2022, The Khronos Group Inc.
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
#include "report.h"
#include "utils.h"
#include "two_call_util.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>

#include <openxr/openxr.h>

#ifdef XR_USE_PLATFORM_ANDROID
#include <android/log.h>
#include <android/window.h>             // for AWINDOW_FLAG_KEEP_SCREEN_ON
#include <android/native_window_jni.h>  // for native window JNI
#include <android_native_app_glue.h>
#include <openxr/openxr_platform.h>
#endif  /// XR_USE_PLATFORM_ANDROID

namespace Conformance
{
    static std::unique_ptr<GlobalData> globalDataInstance;

    void ResetGlobalData()
    {
        globalDataInstance.reset();
    }

    GlobalData& GetGlobalData()
    {
        if (!globalDataInstance) {
            globalDataInstance = std::make_unique<GlobalData>();
        }
        return *globalDataInstance;
    }

    std::string Options::DescribeOptions() const
    {
        std::string result;

        AppendSprintf(result, "   graphicsPlugin: %s\n", graphicsPlugin.c_str());

        AppendSprintf(result, "   formFactor: %s\n", formFactor.c_str());

        AppendSprintf(result, "   hands: %s\n", enabledHands.c_str());

        AppendSprintf(result, "   environmentBlendMode: %s\n", environmentBlendMode.c_str());

        AppendSprintf(result, "   viewConfiguration: %s\n", viewConfiguration.c_str());

        AppendSprintf(result, "   enabledAPILayers:\n");
        for (auto& str : enabledAPILayers) {
            AppendSprintf(result, "      %s\n", str.c_str());
        }

        AppendSprintf(result, "   enabledInstanceExtensions:\n");
        for (auto& str : enabledInstanceExtensions) {
            AppendSprintf(result, "      %s\n", str.c_str());
        }

        AppendSprintf(result, "   invalidHandleValidation: %s\n", invalidHandleValidation ? "yes" : "no");

        AppendSprintf(result, "   fileLineLoggingEnabled: %s\n", fileLineLoggingEnabled ? "yes" : "no");

        AppendSprintf(result, "   debugMode: %s", debugMode ? "yes" : "no");

        return result;
    }

    std::string ConformanceReport::GetReportString() const
    {
        GlobalData& globalData = GetGlobalData();
        std::string reportString;

        AppendSprintf(reportString, "Random seed used: %llu\n", globalData.randEngine.GetSeed());
        AppendSprintf(reportString, "API version: %u.%u.%u\n", XR_VERSION_MAJOR(apiVersion), XR_VERSION_MINOR(apiVersion),
                      XR_VERSION_PATCH(apiVersion));
        AppendSprintf(reportString, "Graphics system: %s\n", globalData.options.graphicsPlugin.c_str());
        AppendSprintf(reportString, "Present API layers:\n");
        for (const std::string& apiLayerName : globalData.enabledAPILayerNames) {
            AppendSprintf(reportString, "    %s\n", apiLayerName.c_str());
        }
        if (globalData.enabledAPILayerNames.empty()) {
            AppendSprintf(reportString, "    <none>\n");
        }
        AppendSprintf(reportString, "Tested instance extensions:\n");
        for (const std::string& extensionName : globalData.enabledInstanceExtensionNames) {
            AppendSprintf(reportString, "    %s\n", extensionName.c_str());
        }
        if (globalData.enabledInstanceExtensionNames.empty()) {
            AppendSprintf(reportString, "    <none>\n");
        }
        AppendSprintf(reportString, "Tested form factor: %s\n", globalData.options.formFactor.c_str());
        AppendSprintf(reportString, "Tested hands: %s\n", globalData.options.enabledHands.c_str());
        AppendSprintf(reportString, "Tested view configuration: %s\n", globalData.options.viewConfiguration.c_str());
        AppendSprintf(reportString, "Tested environment blend mode: %s\n", globalData.options.environmentBlendMode.c_str());
        AppendSprintf(reportString, "Handle invalidation tested: %s\n", globalData.options.invalidHandleValidation ? "yes" : "no");
        AppendSprintf(reportString, "Non-disconnectable devices: %s\n", globalData.options.nonDisconnectableDevices ? "yes" : "no");
        AppendSprintf(reportString, "Test Success Count: %d\n", (int)testSuccessCount);
        AppendSprintf(reportString, "Test Failure Count: %d\n", (int)testFailureCount);

        return reportString;
    }

    bool GlobalData::Initialize()
    {
        // NOTE: Runs *after* population of command-line options.

        GlobalData& globalData = GetGlobalData();
        std::lock_guard<std::recursive_mutex> lock(dataMutex);

        if (isInitialized) {
            return false;
        }

        // Setup the platform-specific plugin first. This is required before creating any instances.
        platformPlugin = Conformance::CreatePlatformPlugin();
        if (!platformPlugin->Initialize()) {
            ReportStr("GlobalData::Initialize: PlatformPlugin::Initialize: platform plugin initialization failed.");
            return false;
        }

        requiredPlatformInstanceExtensions = platformPlugin->GetInstanceExtensions();
        for (auto& str : requiredPlatformInstanceExtensions) {
            globalData.enabledInstanceExtensionNames.push_back_unique(str);
        }

        if (globalData.enabledInteractionProfiles.empty()) {
            globalData.enabledInteractionProfiles.push_back("khr/simple_controller");
        }

        // Get all platform-specific extensions for the "next" fields in several structs
        requiredPlaformInstanceCreateStruct = platformPlugin->PopulateNextFieldForStruct(XR_TYPE_INSTANCE_CREATE_INFO);

        // If we need or were given a graphics plugin, set it up.
        if (IsUsingGraphicsPlugin()) {
            // Setup the graphics-specific plugin. OpenXR supports only a single graphics plugin per
            // session, and as of this writing the conformance test does not try to exercise a possible
            // runtime that supports multiple instances with sessions that use different graphics systems.

            try {
                // CreateGraphicsPlugin may throw a C++ exception.
                graphicsPlugin = Conformance::CreateGraphicsPlugin(options.graphicsPlugin.c_str(), platformPlugin);
            }
            catch (std::exception& e) {
                ReportF("GlobalData::Initialize: Conformance::CreateGraphicsPlugin failed: %s", e.what());
                return false;
            }

            if (!graphicsPlugin->Initialize()) {
                ReportStr("GlobalData::Initialize: GraphicsPlugin::Initialize: graphics plugin initialization failed.");
                return false;
            }

            requiredGraphicsInstanceExtensions = graphicsPlugin->GetInstanceExtensions();
            for (auto& str : requiredGraphicsInstanceExtensions) {
                globalData.enabledInstanceExtensionNames.push_back_unique(str);
            }
        }

        // Identify available API layers, and enable at least the conformance layer if available.
        bool useDebugMessenger = false;
        {
            XrResult result =
                doTwoCallInPlaceWithEmptyElement(availableAPILayers, {XR_TYPE_API_LAYER_PROPERTIES}, xrEnumerateApiLayerProperties);
            if (XR_FAILED(result)) {
                ReportF("GlobalData::Initialize: xrEnumerateApiLayerProperties failed with result: %s", ResultToString(result));
                return false;
            }
            availableAPILayerNames.clear();
            for (auto& value : availableAPILayers) {
                availableAPILayerNames.emplace_back(value.layerName);
            }

            static const auto CONFORMANCE_LAYER_NAME = "XR_APILAYER_KHRONOS_runtime_conformance";
            const auto e = availableAPILayerNames.end();
            bool hasConfLayer = (e != std::find(availableAPILayerNames.begin(), e, CONFORMANCE_LAYER_NAME));
            if (hasConfLayer) {
                enabledAPILayerNames.push_back_unique(CONFORMANCE_LAYER_NAME);
                useDebugMessenger = true;
            }
        }

        XrResult result = doTwoCallInPlaceWithEmptyElement(availableInstanceExtensions, {XR_TYPE_EXTENSION_PROPERTIES},
                                                           xrEnumerateInstanceExtensionProperties, nullptr);
        if (XR_FAILED(result)) {
            ReportF("GlobalData::Initialize: xrEnumerateInstanceExtensionProperties failed with result: %s", ResultToString(result));
            return false;
        }

        // Create an initial instance for the purpose of identifying available extensions. And API layers, in some platform configurations.
        AutoBasicInstance autoInstance(AutoBasicInstance::skipDebugMessenger);

        result = xrGetInstanceProperties(autoInstance, &instanceProperties);
        if (XR_FAILED(result)) {
            ReportF("GlobalData::Initialize: GetInstanceProperties failed with result: %s", ResultToString(result));
            return false;
        }

        /// @todo Also query extensions provided by any layers that are enabled.
        availableInstanceExtensionNames.clear();
        for (auto& value : availableInstanceExtensions) {
            availableInstanceExtensionNames.emplace_back(value.extensionName);
        }

        // This list of instance extensions is safe to always enable if available.
        std::vector<std::string> enableIfAvailableInstanceExtensionNames = {XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME,
                                                                            XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME};

        for (auto& value : enableIfAvailableInstanceExtensionNames) {
            auto& avail = availableInstanceExtensionNames;
            if (std::find(avail.begin(), avail.end(), value) != avail.end()) {
                enabledInstanceExtensionNames.push_back_unique(value);
            }
        }

        if (useDebugMessenger) {
            enabledInstanceExtensionNames.push_back_unique(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // Fill out the functions in functionInfoMap.
        // Keep trying all functions, only failing out at end if one of them failed.
        bool functionMapInitialized = true;
        const FunctionInfoMap& functionInfoMap = GetFunctionInfoMap();
        for (auto& functionInfo : functionInfoMap) {
            // We need to poke the address pointer into map entries.
            result = xrGetInstanceProcAddr(autoInstance, functionInfo.first.c_str(),
                                           &const_cast<FunctionInfo&>(functionInfo.second).functionPtr);

            if (XR_SUCCEEDED(result)) {
                // This doesn't actually prove the pointer is correct. However, we will exercise that later.
                if (functionInfo.second.functionPtr == nullptr) {
                    ReportF("GlobalData::Initialize: xrGetInstanceProcAddr for '%s' failed to return valid addr.",
                            functionInfo.first.c_str());
                    functionMapInitialized = false;
                }
            }
            else {
                if (!ValidateResultAllowed("xrGetInstanceProcAddr", result)) {
                    ReportF("GlobalData::Initialize: xrGetInstanceProcAddr for '%s' returned invalid XrResult.",
                            functionInfo.first.c_str());
                    functionMapInitialized = false;
                }

                // If we could not get a pointer to this function, then it should be because we didn't
                // enable the extension required by the function.
                if (result != XR_ERROR_FUNCTION_UNSUPPORTED) {
                    ReportF("GlobalData::Initialize: xrGetInstanceProcAddr for '%s' failed with result: %s.", functionInfo.first.c_str(),
                            ResultToString(result));
                    functionMapInitialized = false;
                }

                // At this point, result is XR_ERROR_FUNCTION_UNSUPPORTED.
                // Verify that the extension was *not* enabled.
                // functionInfo.second.requiredExtension
                // To do.
            }
        }

        if (!functionMapInitialized) {
            ReportF("GlobalData::Initialize: xrGetInstanceProcAddr failed for one or more functions.");
            return false;
        }

        isInitialized = true;
        return true;
    }

    bool GlobalData::IsInitialized() const
    {
        std::lock_guard<std::recursive_mutex> lock(dataMutex);

        return isInitialized;
    }

    void GlobalData::Shutdown()
    {
        std::lock_guard<std::recursive_mutex> lock(dataMutex);

        if (IsUsingGraphicsPlugin()) {
            if (graphicsPlugin->IsInitialized()) {
                graphicsPlugin->ShutdownDevice();
                graphicsPlugin->Shutdown();
            }
        }

        if (platformPlugin->IsInitialized()) {
            platformPlugin->Shutdown();
        }

        isInitialized = false;
    }

    RandEngine& GlobalData::GetRandEngine()
    {
        return randEngine;
    }

    const FunctionInfo& GlobalData::GetFunctionInfo(const char* functionName) const
    {
        std::lock_guard<std::recursive_mutex> lock(dataMutex);

        const FunctionInfoMap& functionInfoMap = GetFunctionInfoMap();

        auto it = functionInfoMap.find(functionName);

        if (it != functionInfoMap.end()) {
            return it->second;
        }

        return nullFunctionInfo;
    }

    const Options& GlobalData::GetOptions() const
    {
        return options;
    }

    const XrInstanceProperties& GlobalData::GetInstanceProperties() const
    {
        return instanceProperties;
    }

    const ConformanceReport& GlobalData::GetConformanceReport() const
    {
        return conformanceReport;
    }

    bool GlobalData::IsAPILayerEnabled(const char* layerName) const
    {
        std::lock_guard<std::recursive_mutex> lock(dataMutex);

        for (const char* name : enabledAPILayerNames) {
            if (strequal(name, layerName)) {
                return true;
            }
        }

        return false;
    }

    bool GlobalData::IsInstanceExtensionEnabled(const char* extensionName) const
    {
        std::lock_guard<std::recursive_mutex> lock(dataMutex);

        for (const char* name : enabledInstanceExtensionNames) {
            if (strequal(name, extensionName)) {
                return true;
            }
        }

        return false;
    }

    bool GlobalData::IsInstanceExtensionSupported(const char* extensionName) const
    {
        std::lock_guard<std::recursive_mutex> lock(dataMutex);

        for (const std::string& name : availableInstanceExtensionNames) {
            if (name == extensionName) {
                return true;
            }
        }

        return false;
    }

    std::shared_ptr<IPlatformPlugin> GlobalData::GetPlatformPlugin()
    {
        return platformPlugin;
    }

    std::shared_ptr<IGraphicsPlugin> GlobalData::GetGraphicsPlugin()
    {
        return graphicsPlugin;
    }

    bool GlobalData::IsGraphicsPluginRequired() const
    {
        // A graphics system must be specified unless a headless extension is enabled.
        if (IsInstanceExtensionEnabled(XR_MND_HEADLESS_EXTENSION_NAME)) {
            return false;
        }
#ifdef XR_KHR_headless
        if (IsInstanceExtensionEnabled(XR_KHR_HEADLESS_EXTENSION_NAME)) {
            return false;
        }
#endif
        return true;
    }

    bool GlobalData::IsUsingGraphicsPlugin() const
    {
        return IsGraphicsPluginRequired() || !options.graphicsPlugin.empty();
    }
}  // namespace Conformance
