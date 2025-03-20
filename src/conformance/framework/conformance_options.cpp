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

#include "conformance_options.h"
#include "two_call_util.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"

#include <openxr/openxr.h>

#include <string.h>  // IWYU pragma: keep
#include <initializer_list>

namespace Conformance
{

    Options& Options::Get()
    {
        static Options opt;
        return opt;
    }

    void Options::AppendToReportString(std::string& reportString) const
    {
        AppendSprintf(reportString, "Graphics system: %s\n", graphicsPlugin.c_str());
        AppendSprintf(reportString, "Tested form factor: %s\n", formFactor.c_str());
        AppendSprintf(reportString, "Tested hands: %s\n", enabledHands.c_str());
        AppendSprintf(reportString, "Tested view configuration: %s\n", viewConfiguration.c_str());
        AppendSprintf(reportString, "Tested environment blend mode: %s\n", environmentBlendMode.c_str());
        AppendSprintf(reportString, "Handle invalidation tested: %s\n", invalidHandleValidation ? "yes" : "no");
        AppendSprintf(reportString, "Type invalidation tested: %s\n", invalidTypeValidation ? "yes" : "no");
        AppendSprintf(reportString, "Non-disconnectable devices: %s\n", nonDisconnectableDevices ? "yes" : "no");
    }

    std::string Options::DescribeOptions() const
    {
        std::string result;

        AppendSprintf(result, "   minApiVersion: %s\n", minApiVersion.c_str());

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

    /// Perform processing on a string to recognize value names and set the corresponding two fields in an @ref Options object.
    ///
    /// @tparam T Enum or integer type described by the strings. Deduced automatically.
    ///
    /// @param values A list of name, value pairs to recognize (case insensitive)
    /// @param arg The argument string to recognize
    /// @param outParam The enum/int variable to set after recognizing the string. Unchanged if returns false.
    /// @param outParamString The string variable to set to @p arg after recognizing the string. Unchanged if returns false.
    ///
    /// @return true if @p arg was recognized as matching an element from @p values and @p outParam and @p outParamString were updated.
    /// @return false if @p arg was not recognized, and @p outParam and @p outParamString are unmodified
    template <typename T>
    static inline bool ParseEnumFromString(const std::initializer_list<std::pair<const char*, T>>& values, const char* arg, T& outParam,
                                           std::string& outParamString)
    {
        for (auto& nameAndVal : values) {
            if (striequal(arg, nameAndVal.first)) {
                outParam = nameAndVal.second;
                outParamString = arg;
                return true;
            }
        }
        return false;
    }

    bool Options::SetMinApiVersion(const std::string& arg)
    {
        return ParseEnumFromString(
            {
                {"1.0", XR_API_VERSION_1_0},
                {"1.1", XR_API_VERSION_1_1},
            },
            arg.c_str(), minApiVersionValue, minApiVersion);
    }

    const char* Options::AvailableMinApiVersions()
    {
        return "1.0|1.1";
    }

    bool Options::SetFormFactor(const std::string& arg)
    {
        return ParseEnumFromString(
            {
                {"hmd", XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY},
                {"handheld", XR_FORM_FACTOR_HANDHELD_DISPLAY},
            },
            arg.c_str(), formFactorValue, formFactor);
    }

    const char* Options::AvailableFormFactors()
    {
        return "HMD|Handheld";
    }

    bool Options::SetEnabledHands(const std::string& arg)
    {

        if (striequal(arg.c_str(), "left")) {
            leftHandEnabled = true;
            rightHandEnabled = false;
        }
        else if (striequal(arg.c_str(), "right")) {
            leftHandEnabled = false;
            rightHandEnabled = true;
        }
        else if (striequal(arg.c_str(), "both")) {
            leftHandEnabled = true;
            rightHandEnabled = true;
        }
        else {
            return false;
        }
        enabledHands = arg;
        return true;
    }

    const char* Options::AvailableEnabledHands()
    {
        return "left|right|both";
    }

    bool Options::SetViewConfiguration(const std::string& arg)
    {
        return ParseEnumFromString(
            {
                {"stereo", XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO},
                {"stereoFoveated", XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET},
                {"mono", XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO},
            },
            arg.c_str(), viewConfigurationValue, viewConfiguration);
    }

    const char* Options::AvailableViewConfigurations()
    {
        return "Stereo|StereoFoveated|Mono";
    }

    bool Options::SetEnvironmentBlendMode(const std::string& arg)
    {
        return ParseEnumFromString(
            {
                {"opaque", XR_ENVIRONMENT_BLEND_MODE_OPAQUE},
                {"additive", XR_ENVIRONMENT_BLEND_MODE_ADDITIVE},
                {"alphablend", XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND},
            },
            arg.c_str(), environmentBlendModeValue, environmentBlendMode);
    }

    const char* Options::AvailableEnvironmentBlendModes()
    {
        return "Opaque|Additive|AlphaBlend";
    }

    void Options::PopulateDefaultEnvironmentBlendMode(XrInstance instance, XrSystemId systemId)
    {
        if (environmentBlendModeValue != (XrEnvironmentBlendMode)0) {
            // explicitly specified
            return;
        }

        // Find available blend modes
        std::vector<XrEnvironmentBlendMode> availableBlendModes;
        XrResult result =
            doTwoCallInPlace(availableBlendModes, xrEnumerateEnvironmentBlendModes, instance, systemId, viewConfigurationValue);
        XRC_CHECK_THROW_XRRESULT(result, "xrEnumerateEnvironmentBlendModes");

        if (environmentBlendMode.empty()) {
            // Default to the first enumerated blend mode
            environmentBlendModeValue = availableBlendModes.front();
            // convert to string, indicating auto selection
            switch (environmentBlendModeValue) {
            case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
                environmentBlendMode = "opaque (auto-selected)";
                break;
            case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
                environmentBlendMode = "additive (auto-selected)";
                break;
            case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
                environmentBlendMode = "alphablend (auto-selected)";
                break;
            default:
                XRC_THROW("Got unrecognized environment blend mode value as the front of the enumerated list.");
                break;
            }
        }
    }
}  // namespace Conformance
