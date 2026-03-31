// Copyright (c) 2019-2026 The Khronos Group Inc.
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

#include <openxr/openxr.h>

#include <string>
#include <vector>
#include <chrono>

#if defined(NDEBUG)
#define XRC_DEFAULT_DEBUG_MODE false
#else
#define XRC_DEFAULT_DEBUG_MODE true
#endif

namespace Conformance
{

    /// Specifies runtime options for the application.
    ///
    /// String options, other than interaction profile, are case-insensitive.
    ///
    /// Each of these can be specified from the command line via a command of the same name as
    /// the variable name. For example, the application can be run with --graphicsPlugin "vulkan"
    /// String vector options are specified by repeated arguments. For example, the app could be
    /// run with `--enabledAPILayers api_validation --enabledAPILayers handle_validation`
    struct Options
    {
        /// Get singleton - minimize use of this, prefer accepting `const Options& options` as a parameter.
        static Options& Get();

        void AppendToReportString(std::string& reportString) const;

        /// Describes the option set in a way suitable for printing.
        std::string DescribeOptions() const;

        /// Valid values include: "vulkan" "d3d11" d3d12" "opengl" "opengles" "metal"
        /// Default is none. Must be manually specified.
        std::string graphicsPlugin{};

        /// @name Minimum API Version
        /// Valid values include: "1.0" "1.1"
        /// Default is 1.0.
        /// @{

        /// String value matching @ref minApiVersionValue.
        /// To assign, call @ref SetMinApiVersion
        std::string minApiVersion{"1.0"};

        /// Will contain the results of XR_MAKE_VERSION using the requested major and minor version
        /// combined with the patch component of XR_CURRENT_API_VERSION.
        /// To assign, call @ref SetMinApiVersion
        XrVersion minApiVersionValue{XR_API_VERSION_1_0};

        /// Set the value of @ref minApiVersion and @ref minApiVersionValue
        /// @return false if a parsing error
        bool SetMinApiVersion(const std::string& arg);

        /// The recognized strings accepted by @ref SetMinApiVersion, delimited by |, for CLI usage help.
        static const char* AvailableMinApiVersions();
        /// @}

        /// @name Form Factor
        /// Valid values include "hmd" "handheld". See enum XrFormFactor.
        /// Default is hmd.
        /// @{

        /// String value matching @ref formFactorValue.
        /// To assign, call @ref SetFormFactor
        std::string formFactor{"Hmd"};

        /// Enumerant value matching @ref formFactor.
        /// To assign, call @ref SetFormFactor
        XrFormFactor formFactorValue{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};

        /// Set the value of @ref formFactor and @ref formFactorValue
        /// Valid values include "hmd" "handheld". See enum XrFormFactor.
        /// @return false if a parsing error
        bool SetFormFactor(const std::string& arg);

        /// The recognized strings accepted by @ref SetFormFactor, delimited by |, for CLI usage help.
        static const char* AvailableFormFactors();
        /// @}

        /// @name Enabled Hands
        /// Which hands have been selected for test. This is to allow for devices which only have
        /// one controller, and also to allow skipping one of the controllers during development.
        /// @{

        /// The string value.
        /// Valid values are "left", "right", and "both".
        /// Default is "both".
        /// To assign, call @ref SetEnabledHands
        std::string enabledHands{"both"};

        /// Whether testing is enabled for /user/hand/left
        /// To assign, call @ref SetEnabledHands
        bool leftHandEnabled{true};

        /// Whether testing is enabled for /user/hand/right
        /// To assign, call @ref SetEnabledHands
        bool rightHandEnabled{true};

        /// Set the value of @ref enabledHands, @ref leftHandEnabled, and @ref rightHandEnabled.
        /// Valid values are "left", "right", and "both".
        /// @return false if a parsing error
        bool SetEnabledHands(const std::string& arg);

        /// The recognized strings accepted by @ref SetEnabledHands, delimited by |, for CLI usage help.
        static const char* AvailableEnabledHands();
        /// @}

        /// Description of how long to wait before skipping tests which support auto skip
        /// or 0 when auto skip is disabled.
        std::chrono::milliseconds autoSkipTimeout{0};

        /// @name View Configuration
        /// Valid values include "stereo" "mono" "foveatedInset" "firstPersonObserver". See enum XrViewConfigurationType.
        /// Default is stereo.
        /// @{

        /// String value matching @ref viewConfigurationValue.
        /// To assign, call @ref SetViewConfiguration
        std::string viewConfiguration{"Stereo"};

        /// Enumerant value matching @ref viewConfiguration.
        /// To assign, call @ref SetViewConfiguration
        XrViewConfigurationType viewConfigurationValue{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};

        /// Set the value of @ref viewConfiguration and @ref viewConfigurationValue
        /// Valid values include "stereo" "mono" "foveatedInset" "firstPersonObserver". See enum XrViewConfigurationType.
        /// @return false if a parsing error
        bool SetViewConfiguration(const std::string& arg);

        /// The recognized strings accepted by @ref SetViewConfiguration, delimited by |, for CLI usage help.
        static const char* AvailableViewConfigurations();
        /// @}

        /// @ref PopulateDefaultEnvironmentBlendMode sets to the first enumerated value if still at the (invalid) default of 0.
        XrEnvironmentBlendMode environmentBlendModeValue{(XrEnvironmentBlendMode)0};

        /// If the environment blend mode was not set, auto-detect.
        ///
        /// If @ref environmentBlendModeValue == 0 (the default and an invalid value),
        /// assigns the value of @ref environmentBlendMode and @ref environmentBlendModeValue
        /// based on the first blend mode enumerated by the runtime (preferred).
        ///
        /// Throws on error.
        void PopulateDefaultEnvironmentBlendMode(XrInstance instance, XrSystemId systemId);
        /// @}

        /// Options can vary depending on their platform availability. If a requested API layer is
        /// not supported then the test fails.
        /// Default is empty.
        std::vector<std::string> enabledAPILayers;

        /// Valid values include at least any of the documented extensions. The runtime supported extensions
        /// are enumerated by xrEnumerateApiLayerProperties. If a requested extension is not supported
        /// then the test fails.
        /// Default is empty.
        std::vector<std::string> enabledInstanceExtensions;

        /// Valid values include at least any of the documented interaction profiles.
        /// The conformance tests will generically test the runtime supports each of the provided
        /// interaction profile.
        /// Default is /interaction_profiles/khr/simple_controller alone.
        std::vector<std::string> enabledInteractionProfiles;

        /// Indicates if the runtime should be tested to ensure it returns XR_ERROR_HANDLE_INVALID
        /// upon usage of invalid handles that are not undefined behavior to read.
        /// The OpenXR specification does not require this because it cannot (uninitialized memory
        /// used as a handle may trigger undefined behavior at the C level), but some runtimes will
        /// attempt to identify bad handles where they can.
        /// Default is false.
        bool invalidHandleValidation{false};

        /// Indicates if the runtime should be tested to ensure it returns XR_ERROR_VALIDATION_FAILURE
        /// upon passing structs with invalid .type fields.
        /// The OpenXR specification does not require this check, but some runtimes will.
        /// Default is false.
        bool invalidTypeValidation{false};

        /// Indicates if the runtime supports disconnecting a device, specifically left and right devices.
        /// Some input tests depends on the side-effects of device disconnection to test various features.
        /// If true the runtime does not support disconnectable devices.
        bool nonDisconnectableDevices{false};

        /// If true then all test diagnostics are reported with the file/line that they occurred on.
        /// Default is true (enabled).
        bool fileLineLoggingEnabled{true};

        /// If true then xrGetSystem will be attempted repeatedly for a limited time at the beginning of a run
        /// before beginning a test case.
        bool pollGetSystem{false};

        /// Defines if executing in debug mode. By default this follows the build type.
        bool debugMode{XRC_DEFAULT_DEBUG_MODE};
    };
}  // namespace Conformance
