// Copyright (c) 2019-2025 The Khronos Group Inc.
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

#include "graphics_plugin.h"
#include "interaction_info.h"
#include "platform_plugin.h"
#include "report.h"
#include "two_call_util.h"
#include "utilities/colors.h"
#include "utilities/feature_availability.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"
#include "utilities/uuid_utils.h"

#include <catch2/catch_tostring.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <exception>
#include <inttypes.h>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string.h>
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

    std::string ConformanceReport::GetReportString() const
    {
        GlobalData& globalData = GetGlobalData();
        std::string reportString;

        AppendSprintf(reportString, "Random seed used: %" PRIu64 "\n", globalData.randEngine.GetSeed());
        AppendSprintf(reportString, "API version: %u.%u.%u\n", XR_VERSION_MAJOR(apiVersion), XR_VERSION_MINOR(apiVersion),
                      XR_VERSION_PATCH(apiVersion));
        AppendSprintf(reportString, "Present API layers:\n");
        for (const char* const& apiLayerName : globalData.enabledAPILayerNames) {
            AppendSprintf(reportString, "    %s\n", apiLayerName);
        }
        if (globalData.enabledAPILayerNames.empty()) {
            AppendSprintf(reportString, "    <none>\n");
        }
        AppendSprintf(reportString, "Instance extensions enabled for all tests:\n");
        AppendSprintf(reportString, "    (Individual tests enable additional extensions)\n");
        for (const char* const& extensionName : globalData.enabledInstanceExtensionNames) {
            AppendSprintf(reportString, "    %s\n", extensionName);
        }
        if (globalData.enabledInstanceExtensionNames.empty()) {
            AppendSprintf(reportString, "    <none>\n");
        }
        Options::Get().AppendToReportString(reportString);
        AppendSprintf(reportString, "Test Success Count: %zu\n", static_cast<size_t>(TestSuccessCount()));
        AppendSprintf(reportString, "Test Failure Count: %zu\n", static_cast<size_t>(TestFailureCount()));
        if (TestFailureCount() > 0) {
            AppendSprintf(reportString, "Tests with failures:\n");
            for (auto const& pair : results) {
                const auto& name = pair.first;
                const auto& score = pair.second;
                if (score.testFailureCount > 0) {
                    AppendSprintf(reportString, "    %s\n", name.c_str());
                }
            }
        }
        if (!unmatchedTestSpecs.empty()) {
            AppendSprintf(reportString, "Unmatched Test Specs:\n");
            for (auto const& spec : unmatchedTestSpecs) {
                AppendSprintf(reportString, "    %s\n", spec.c_str());
            }
        }

        return reportString;
    }

    uint64_t ConformanceReport::TestSuccessCount() const
    {
        uint64_t success{};
        for (auto const& pair : results) {
            success += pair.second.testSuccessCount;
        }
        return success;
    }

    uint64_t ConformanceReport::TestFailureCount() const
    {
        uint64_t failure{};
        for (auto const& pair : results) {
            failure += pair.second.testFailureCount;
        }
        return failure;
    }

    bool GlobalData::Initialize(Options& options)
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
                if (!options.invalidHandleValidation) {
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

        options.PopulateDefaultEnvironmentBlendMode(autoInstance, systemId);

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
        return IsGraphicsPluginRequired() || !Options::Get().graphicsPlugin.empty();
    }

    bool GlobalData::IsUsingConformanceAutomation() const
    {
        return IsInstanceExtensionEnabled(XR_EXT_CONFORMANCE_AUTOMATION_EXTENSION_NAME);
    }

    void GlobalData::PushSwapchainFormat(int64_t format, const std::string& name)
    {
        std::unique_lock<std::recursive_mutex> lock(dataMutex);
        conformanceReport.swapchainFormats.emplace_back(format, name);
    }

    XrColor4f GlobalData::GetClearColorForBackground() const
    {
        // TODO move over to Options?
        switch (Options::Get().environmentBlendModeValue) {
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
        out = FeatureSet(Options::Get().desiredApiVersionValue);
        for (const XrExtensionProperties& extProp : availableInstanceExtensions) {
            out.SetByExtensionNameString(extProp.extensionName);
        }
    }

    void GlobalData::PopulateVersionAndEnabledExtensions(FeatureSet& out) const
    {
        out = FeatureSet(Options::Get().desiredApiVersionValue);
        for (const auto& ext : enabledInstanceExtensionNames) {
            out.SetByExtensionNameString(ext);
        }
    }
}  // namespace Conformance

std::string Catch::StringMaker<XrUuidEXT>::convert(XrUuidEXT const& value)
{
    return to_string(value);
}

std::string Catch::StringMaker<XrQuaternionf>::convert(XrQuaternionf const& value)
{
    std::ostringstream oss;
    oss << "(w=" << value.w;
    oss << ", xyz=(" << value.x;
    oss << ", " << value.y;
    oss << ", " << value.z;
    oss << "))";
    return oss.str();
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
