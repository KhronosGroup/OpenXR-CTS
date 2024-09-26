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

#include "composition_utils.h"  // for Colors
#include "graphics_plugin.h"
#include "interaction_info.h"
#include "platform_plugin.h"
#include "report.h"
#include "two_call_util.h"
#include "utilities/feature_availability.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"
#include "utilities/uuid_utils.h"

#include <openxr/openxr.h>

#include <algorithm>
#include <exception>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>

namespace Conformance
{

    /// This list of instance extensions is safe to always enable if available.
    static constexpr std::initializer_list<const char*> kEnableIfAvailableInstanceExtensionNames = {
        XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME, XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME};

    /// The name of the required conformance layer
    static constexpr auto kConformanceLayerName = "XR_APILAYER_KHRONOS_runtime_conformance";

    static constexpr auto kGetSystemPollingTimeout = std::chrono::seconds(10);

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

        AppendSprintf(result, "   apiVersion: %s\n", desiredApiVersion.c_str());

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

        AppendSprintf(result, "   invalidTypeValidation: %s\n", invalidTypeValidation ? "yes" : "no");

        AppendSprintf(result, "   fileLineLoggingEnabled: %s\n", fileLineLoggingEnabled ? "yes" : "no");

        AppendSprintf(result, "   pollGetSystem: %s\n", pollGetSystem ? "yes" : "no");

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
        for (const char* const& apiLayerName : globalData.enabledAPILayerNames) {
            AppendSprintf(reportString, "    %s\n", apiLayerName);
        }
        if (globalData.enabledAPILayerNames.empty()) {
            AppendSprintf(reportString, "    <none>\n");
        }
        AppendSprintf(reportString, "Tested instance extensions:\n");
        for (const char* const& extensionName : globalData.enabledInstanceExtensionNames) {
            AppendSprintf(reportString, "    %s\n", extensionName);
        }
        if (globalData.enabledInstanceExtensionNames.empty()) {
            AppendSprintf(reportString, "    <none>\n");
        }
        AppendSprintf(reportString, "Tested form factor: %s\n", globalData.options.formFactor.c_str());
        AppendSprintf(reportString, "Tested hands: %s\n", globalData.options.enabledHands.c_str());
        AppendSprintf(reportString, "Tested view configuration: %s\n", globalData.options.viewConfiguration.c_str());
        AppendSprintf(reportString, "Tested environment blend mode: %s\n", globalData.options.environmentBlendMode.c_str());
        AppendSprintf(reportString, "Handle invalidation tested: %s\n", globalData.options.invalidHandleValidation ? "yes" : "no");
        AppendSprintf(reportString, "Type invalidation tested: %s\n", globalData.options.invalidTypeValidation ? "yes" : "no");
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
            ReportF("GlobalData::Initialize: PlatformPlugin::Initialize: platform plugin initialization failed.");
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
        requiredPlatformInstanceCreateStruct = platformPlugin->PopulateNextFieldForStruct(XR_TYPE_INSTANCE_CREATE_INFO);

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
                ReportF("GlobalData::Initialize: GraphicsPlugin::Initialize: graphics plugin initialization failed.");
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

            const auto e = availableAPILayerNames.end();
            bool hasConfLayer = (e != std::find(availableAPILayerNames.begin(), e, kConformanceLayerName));
            if (hasConfLayer) {
                if (!globalData.options.invalidHandleValidation) {
                    enabledAPILayerNames.push_back_unique(kConformanceLayerName);
                    useDebugMessenger = true;
                }
                else {
                    ReportF("GlobalData::Initialize: not loading conformance layer due to handle validation mode");
                }
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

        for (auto& value : kEnableIfAvailableInstanceExtensionNames) {
            const auto& avail = availableInstanceExtensionNames;
            if (std::find(avail.begin(), avail.end(), value) != avail.end()) {
                enabledInstanceExtensionNames.push_back_unique(value);
            }
        }

        if (useDebugMessenger) {
            enabledInstanceExtensionNames.push_back_unique(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // Now that we know our available extensions, try to enable the requested interaction profile(s)
        {
            FeatureSet enabled;
            PopulateVersionAndEnabledExtensions(enabled);
            FeatureSet available;
            PopulateVersionAndAvailableExtensions(available);

            // Consistency check: enabled should always be a subset of available
            XRC_CHECK_THROW_MSG(enabled.IsSatisfiedBy(available), "An unavailable extension is enabled.");

            const auto beginProfiles = std::begin(GetAllInteractionProfiles());
            const auto endProfiles = std::end(GetAllInteractionProfiles());
            for (auto& str : globalData.enabledInteractionProfiles) {
                auto ipIt = std::find_if(beginProfiles, endProfiles, [&](const InteractionProfileAvailMetadata& ip) {
                    return strcmp(ip.InteractionProfileShortname, str) == 0;
                });

                if (ipIt == endProfiles) {
                    // Interaction profile path not found in the generated database, presumably missing from XML.
                    ReportF("GlobalData::Initialize: Interaction profile \"%s\" not supported by conformance test", str);
                    return false;
                }
                Availability availability = kInteractionAvailabilities[(size_t)ipIt->Availability];

                if (availability.IsSatisfiedBy(enabled)) {
                    // The currently enabled extensions are enough to get this profile, no need to add more.
                    continue;
                }

                // There may be multiple ways of enabling this profile, search for the first that the current version and available extensions
                // can satisfy.
                auto fsIt = std::find_if(std::begin(availability), std::end(availability),
                                         [&](const FeatureSet& featureSet) { return featureSet.IsSatisfiedBy(available); });

                if (fsIt == std::end(availability)) {
                    // Could not do it!
                    ReportF("GlobalData::Initialize: Cannot meet requirements for interaction profile \"%s\": need: %s, have: %s", str,
                            availability.ToString().c_str(), available.ToString().c_str());
                    return false;
                }

                for (const char* extension : fsIt->GetExtensions()) {
                    globalData.enabledInstanceExtensionNames.push_back_unique(extension);
                }
            }
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

        // Find XrSystemId (for later use and to ensure device is connected/available for whatever that means in a given runtime)
        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        XrSystemGetInfo systemGetInfo = {XR_TYPE_SYSTEM_GET_INFO};
        systemGetInfo.formFactor = options.formFactorValue;

        auto tryGetSystem = [&] {
            XrResult result = xrGetSystem(autoInstance, &systemGetInfo, &systemId);
            if (result != XR_SUCCESS && result != XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
                // Anything else is a real error
                ReportF("GlobalData::Initialize: xrGetSystem failed with result: %s.", ResultToString(result));
                return false;
            }
            return true;
        };

        if (options.pollGetSystem) {
            ReportF(
                "GlobalData::Initialize: xrGetSystem will be polled until success or timeout, as requested. This behavior may be less compatible with applications.");

            const auto timeout = std::chrono::steady_clock::now() + kGetSystemPollingTimeout;
            while (systemId == XR_NULL_SYSTEM_ID && std::chrono::steady_clock::now() < timeout) {
                if (!tryGetSystem()) {
                    return false;
                }
                // pause briefly before trying again
                std::this_thread::sleep_for(std::chrono::milliseconds{50});
            }

            if (systemId == XR_NULL_SYSTEM_ID) {
                ReportF("GlobalData::Initialize: xrGetSystem polling timed out without success after %f",
                        std::chrono::duration_cast<std::chrono::duration<float>>(kGetSystemPollingTimeout).count());
                return false;
            }
        }
        else {
            // just try once
            if (!tryGetSystem()) {
                return false;
            }
            if (systemId == XR_NULL_SYSTEM_ID) {
                ReportF("GlobalData::Initialize: xrGetSystem did not return a system ID on the first call, not proceeding with tests.");
                return false;
            }
        }

        // Find available blend modes
        result = doTwoCallInPlace(availableBlendModes, xrEnumerateEnvironmentBlendModes, autoInstance.GetInstance(), systemId,
                                  options.viewConfigurationValue);
        if (XR_FAILED(result)) {
            ReportF("GlobalData::Initialize: xrEnumerateEnvironmentBlendModes failed with result: %s", ResultToString(result));
            return false;
        }
        if (options.environmentBlendMode.empty()) {
            // Default to the first enumerated blend mode
            options.environmentBlendModeValue = availableBlendModes.front();
            // convert to string, indicating auto selection
            switch (options.environmentBlendModeValue) {
            case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
                options.environmentBlendMode = "opaque (auto-selected)";
                break;
            case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
                options.environmentBlendMode = "additive (auto-selected)";
                break;
            case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
                options.environmentBlendMode = "alphablend (auto-selected)";
                break;
            default:
                XRC_THROW("Got unrecognized environment blend mode value as the front of the enumerated list.");
                break;
            }
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

        if (IsUsingGraphicsPlugin() && graphicsPlugin) {
            if (graphicsPlugin->IsInitialized()) {
                graphicsPlugin->ShutdownDevice();
                graphicsPlugin->Shutdown();
            }
        }

        if (platformPlugin && platformPlugin->IsInitialized()) {
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
        return true;
    }

    bool GlobalData::IsUsingGraphicsPlugin() const
    {
        return IsGraphicsPluginRequired() || !options.graphicsPlugin.empty();
    }

    void GlobalData::PushSwapchainFormat(int64_t format, const std::string& name)
    {
        std::unique_lock<std::recursive_mutex> lock(dataMutex);
        conformanceReport.swapchainFormats.emplace_back(format, name);
    }

    XrColor4f GlobalData::GetClearColorForBackground() const
    {
        switch (options.environmentBlendModeValue) {
        case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
            return DarkSlateGrey;
        case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
            return Colors::Black;
        case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
            return Colors::Transparent;
        default:
            XRC_THROW("Encountered unrecognized environment blend mode value while determining background color.");
        }
    }

    void GlobalData::PopulateVersionAndAvailableExtensions(FeatureSet& out) const
    {
        out = FeatureSet(options.desiredApiVersionValue);
        for (const XrExtensionProperties& extProp : availableInstanceExtensions) {
            out.SetByExtensionNameString(extProp.extensionName);
        }
    }

    void GlobalData::PopulateVersionAndEnabledExtensions(FeatureSet& out) const
    {
        out = FeatureSet(options.desiredApiVersionValue);
        for (const auto& ext : enabledInstanceExtensionNames) {
            out.SetByExtensionNameString(ext);
        }
    }
}  // namespace Conformance

std::string Catch::StringMaker<XrUuidEXT>::convert(XrUuidEXT const& value)
{
    return to_string(value);
}

std::string Catch::StringMaker<XrVector3f>::convert(XrVector3f const& value)
{
    std::ostringstream oss;
    oss << "(" << value.x;
    oss << ", " << value.y;
    oss << ", " << value.z;
    oss << ")";
    return oss.str();
}

std::string Catch::StringMaker<XrPosef>::convert(XrPosef const& value)
{
    std::ostringstream oss;
    oss << "[pos = (" << value.position.x;
    oss << ", " << value.position.y;
    oss << ", " << value.position.z;
    oss << ") ori = (w=" << value.orientation.w;
    oss << ", xyz=(" << value.orientation.x;
    oss << ", " << value.orientation.y;
    oss << ", " << value.orientation.z;
    oss << "))]";
    return oss.str();
}
