// Copyright (c) 2019-2020 The Khronos Group Inc.
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

#define CATCH_CONFIG_NOSTDOUT
#ifdef XR_USE_PLATFORM_ANDROID
#define CATCH_CONFIG_NO_CPP11_TO_STRING
#define CATCH_CONFIG_FALLBACK_STRINGIFIER
#endif  /// XR_USE_PLATFORM_ANDROID

#define CATCH_CONFIG_RUNNER  // Tell catch2 that we will be supplying main() ourselves.
#include <catch2/catch.hpp>

#include <conformance_framework.h>
#include <conformance_utils.h>
#include <openxr/openxr.h>
#include <vector>
#include <string>
#include <string.h>
#include <unordered_map>

#include "conformance_test.h"
#include "report.h"
#include "utils.h"
#include "platform_utils.hpp"
#include "filesystem_utils.hpp"
#include "two_call_util.h"
#include "conformance_utils.h"

using namespace Conformance;

namespace
{
    const ConformanceLaunchSettings* g_conformanceLaunchSettings = nullptr;

    /// Console output redirection
    class ConsoleStream : public std::streambuf
    {
    public:
        ConsoleStream(MessageType messageType) : m_messageType(messageType)
        {
        }

        int overflow(int c) override
        {
            auto c_as_char = traits_type::to_char_type(c);
            if (c_as_char == '\n') {
                sync();  // flush on newlines
            }
            else {
                m_s += c_as_char;  // add to local buffer
            }
            return c;
        }

        int sync() override
        {
            // if our buffer has anything meaningful, flush to the conformance_test host.
            if (m_s.length() > 0) {
                g_conformanceLaunchSettings->message(m_messageType, m_s.c_str());
                m_s.clear();
            }
            return 0;
        }

    private:
        std::string m_s;
        const MessageType m_messageType;
    };

    void ReportTestHeader()
    {
        ReportStr("*********************************************");
        ReportF("OpenXR Conformance Test v%d.%d.%d", XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), XR_VERSION_MINOR(XR_CURRENT_API_VERSION),
                XR_VERSION_PATCH(XR_CURRENT_API_VERSION));
        ReportStr("*********************************************\n");
    }

    void ReportTestEnvironment()
    {
        GlobalData& globalData = GetGlobalData();

        // Report the runtime name and info.
        const XrInstanceProperties& instanceProperties = globalData.GetInstanceProperties();
        ReportF("Runtime instance properties:\n   Runtime name: %s\n   Runtime version %d.%d.%d", instanceProperties.runtimeName,
                XR_VERSION_MAJOR(instanceProperties.runtimeVersion), XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                XR_VERSION_PATCH(instanceProperties.runtimeVersion));

        // Report the users-selected options
        std::string optionsDescription = globalData.GetOptions().DescribeOptions();
        ReportF("Test options:\n%s", optionsDescription.c_str());

        // Report the available API layers.
        ReportF("Available API layers:");
        if (globalData.availableAPILayers.empty())
            ReportStr("    <none>");
        else {
            for (const XrApiLayerProperties& layerProperties : globalData.availableAPILayers) {
                ReportF("    %s, version %u, spec version %d.%d.%d", layerProperties.layerName, layerProperties.layerVersion,
                        XR_VERSION_MAJOR(layerProperties.specVersion), XR_VERSION_MINOR(layerProperties.specVersion),
                        XR_VERSION_PATCH(layerProperties.specVersion));
            }
        }

        ReportF("Available instance extensions:");
        if (globalData.availableInstanceExtensions.empty())
            ReportStr("    <none>");
        else {
            for (const XrExtensionProperties& extensionProperties : globalData.availableInstanceExtensions) {
                ReportF("    %s, extension version %d", extensionProperties.extensionName, extensionProperties.extensionVersion);
            }
        }
        ReportF("");
    }

    TEST_CASE("Describe Graphics Plugin", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (globalData.IsGraphicsPluginRequired()) {
            AutoBasicInstance instance(AutoBasicInstance::createSystemId);
            auto graphicsPlugin = globalData.GetGraphicsPlugin();
            if (graphicsPlugin) {
                // Initialize device so DescribeGraphics can return information about the GPU.
                if (graphicsPlugin->InitializeDevice(instance, instance.systemId)) {
                    ReportF("graphicsPlugin: %s", graphicsPlugin->DescribeGraphics().c_str());
                    graphicsPlugin->ShutdownDevice();
                }
            }
        }
    }

    // Ensure conformance is configured correctly.
    TEST_CASE("Validate Environment")
    {
        GlobalData& globalData = GetGlobalData();

        REQUIRE_MSG(globalData.IsAPILayerEnabled("XR_APILAYER_KHRONOS_runtime_conformance"),
                    "Conformance layer required to pass conformance");

        // Conformance listens for failures from the conformance layer through the debug messenger extension.
        REQUIRE_MSG(IsInstanceExtensionEnabled(XR_EXT_DEBUG_UTILS_EXTENSION_NAME), "Debug utils extension required by conformance layer");
    }

    Catch::clara::Parser MakeCLIParser(Conformance::GlobalData& globalData)
    {
        using namespace Catch::clara;
        auto& options = globalData.options;

        /// Handle rand seed arg
        auto const parseRandSeed = [&](std::string const& arg) {
            using namespace Conformance;
            GlobalData& globalData = GetGlobalData();
            uint64_t seedValue = std::strtoull(arg.c_str(), nullptr, 0);
            if (errno == ERANGE) {
                ReportF("invalid arg: %s", arg.c_str());
                return ParserResult::runtimeError("invalid uint64_t seed '" + arg + "' passed on command line");
            }

            globalData.randEngine.SetSeed(seedValue);
            return ParserResult::ok(ParseResultType::Matched);
        };

        /// Handle form factor arg
        auto const parseFormFactor = [&](std::string const& arg) {
            using namespace Conformance;
            GlobalData& globalData = GetGlobalData();
            globalData.options.formFactor = arg;
            if (striequal(globalData.options.formFactor.c_str(), "hmd"))
                globalData.options.formFactorValue = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            else if (striequal(globalData.options.formFactor.c_str(), "handheld"))
                globalData.options.formFactorValue = XR_FORM_FACTOR_HANDHELD_DISPLAY;
            else {
                ReportF("invalid arg: %s", globalData.options.formFactor.c_str());
                return ParserResult::runtimeError("invalid form factor '" + arg + "' passed on command line");
            }

            return ParserResult::ok(ParseResultType::Matched);
        };

        /// Handle view config arg
        auto const parseViewConfig = [&](std::string const& arg) {
            using namespace Conformance;
            GlobalData& globalData = GetGlobalData();
            globalData.options.viewConfiguration = arg;
            if (striequal(globalData.options.viewConfiguration.c_str(), "stereo"))
                globalData.options.viewConfigurationValue = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            else if (striequal(globalData.options.viewConfiguration.c_str(), "mono"))
                globalData.options.viewConfigurationValue = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
            else {
                ReportF("invalid arg: %s", globalData.options.viewConfiguration.c_str());
                return ParserResult::runtimeError("invalid view config '" + arg + "' passed on command line");
            }
            return ParserResult::ok(ParseResultType::Matched);
        };

        /// Handle blend mode arg
        auto const parseBlendMode = [&](std::string const& arg) {
            using namespace Conformance;
            GlobalData& globalData = GetGlobalData();
            globalData.options.environmentBlendMode = arg;
            if (striequal(globalData.options.environmentBlendMode.c_str(), "opaque"))
                globalData.options.environmentBlendModeValue = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            else if (striequal(globalData.options.environmentBlendMode.c_str(), "additive"))
                globalData.options.environmentBlendModeValue = XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
            else if (striequal(globalData.options.environmentBlendMode.c_str(), "alphablend"))
                globalData.options.environmentBlendModeValue = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
            else {
                ReportF("invalid arg: %s", globalData.options.environmentBlendMode.c_str());

                return ParserResult::runtimeError("invalid environment blend mode '" + arg + "' passed on command line");
            }
            return ParserResult::ok(ParseResultType::Matched);
        };

        // NOTE: End of line comments are to encourage clang-format to work the way we want it to for this mini embedded DSL.
        // Clara requires that the "short" args be a single letter - we use capital letters here to avoid colliding with Catch2-provided
        // options.
        auto cli =
            Opt(options.graphicsPlugin,
                "Vulkan|OpenGLES|OpenGL|D3D11|D3D12")  // graphics plugin
                ["-G"]["--graphicsPlugin"]             //
            ("Specify a graphics plugin to use. Required.")
                .required()

            | Opt(parseRandSeed, "uint64_t random seed")  // seed for default global rand.
                  ["-S"]["--randSeed"]                    //
              ("Specify a random seed to use (decimal or hex). Default is a dynamically chosen value.")
                  .optional()

            | Opt(parseFormFactor, "HMD|Handheld")  // form factor
                  ["-F"]["--formFactor"]            //
              ("Specify a form factor to use. Default is HMD.")
                  .optional()

            | Opt(parseViewConfig, "Stereo|Mono")  // view configuration
                  ["-V"]["--viewConfiguration"]    //
              ("Specify view configuration. Default is Stereo.")
                  .optional()

            | Opt(parseBlendMode, "Opaque|Additive|AlphaBlend")  // blend mode
                  ["-B"]["--environmentBlendMode"]               //
              ("Specify blend mode. Default is Opaque.")
                  .optional()

            | Opt(options.enabledAPILayers, "API layer name")  // API layers
                  ["-L"]["--enabledAPILayer"]                  //
              ("Specify API layer. May repeat for multiple layers. Default is none.")
                  .optional()

            | Opt(options.enabledInstanceExtensions, "extension name")  // Extensions
                  ["-E"]["--enabledInstanceExtension"]                  //
              ("Specify instance extension. May repeat for multiple extensions. Default is none.")
                  .optional()

            | Opt(options.enabledInteractionProfiles, "interaction profile")  // Interaction profiles
                  ["-I"]["--interactionProfiles"]                             //
              ("Specify interaction profiles. May repeat for multiple profiles. Default is /interaction_profiles/khr/simple_controller.")
                  .optional()

            | Opt(options.invalidHandleValidation)     // Invalid handle validation
                  ["-H"]["--invalidHandleValidation"]  //
              ("Enables testing of invalid handle checking.")
                  .optional()

            | Opt([&](bool /* flag */) { options.fileLineLoggingEnabled = false; })  // disable file/line logging
                  ["-F"]["--disableFileLineLogging"]                                 //
              ("Disables logging file/line data.")
                  .optional()

            | Opt([&](bool enabled) { options.debugMode = enabled; })["-D"]["--debugMode"]("Sets debug mode as enabled or disabled.")
                  .optional();

        return cli;
    }
    bool UpdateOptionsFromCommandLine(Catch::Session& catchSession, int argc, const char* argv[])
    {
        using namespace Conformance;
        auto& globalData = GetGlobalData();
        auto cli = MakeCLIParser(globalData)  // our options first
                   | catchSession.cli();      // Catch default options
        catchSession.cli(cli);
        auto result = catchSession.applyCommandLine(argc, argv);
        if (catchSession.configData().showHelp) {
            std::cout << "\n\n"
                         "Returns 0 if execution proceeded normally (regardless of test success/failure).\n"
                         "Return -1 if execution of tests failed.\n";
            return true;
        }

        globalData.enabledAPILayerNames = globalData.options.enabledAPILayers;
        globalData.enabledInstanceExtensionNames = globalData.options.enabledInstanceExtensions;
        globalData.enabledInteractionProfiles = globalData.options.enabledInteractionProfiles;

        // Check for required parameters.
        if (GetGlobalData().options.graphicsPlugin.empty()) {  // If no graphics system was specified...
            if (GetGlobalData().IsGraphicsPluginRequired()) {  // and if one is required...
                ReportStr("graphicsPlugin parameter is required.");
                return false;
            }
        }

        return result == 0;
    }

    // Implements a class that listens to the results of individual test runs. This is used for
    // collecting telemetry.
    struct ConformanceTestListener : Catch::TestEventListenerBase
    {
        using Base = Catch::TestEventListenerBase;

        using TestEventListenerBase::TestEventListenerBase;  // inherit constructor

        void testCaseEnded(Catch::TestCaseStats const& testCaseStats) override
        {
            Base::testCaseEnded(testCaseStats);

            Conformance::GlobalData& globalData = Conformance::GetGlobalData();
            globalData.conformanceReport.testSuccessCount += testCaseStats.totals.testCases.passed;
            globalData.conformanceReport.testFailureCount += testCaseStats.totals.testCases.failed;
        }

        void sectionStarting(Catch::SectionInfo const& sectionInfo) override
        {
            Base::sectionStarting(sectionInfo);

            // Track test progress by outputting the current test section.
            std::string indentStr(m_sectionIndent * 2, ' ');
            g_conformanceLaunchSettings->message(MessageType_TestSectionStarting,
                                                 (indentStr + "Executing \"" + sectionInfo.name + "\" tests...").c_str());
            m_sectionIndent++;
        }
        void sectionEnded(Catch::SectionStats const& sectionStats) override
        {
            // Show a summary if something failed but leave the details to the (e.g. console or xml) reporter.
            if (sectionStats.assertions.failed > 0) {
                std::string indentStr(m_sectionIndent * 2, ' ');
                g_conformanceLaunchSettings->message(
                    MessageType_AssertionFailed,
                    (indentStr + std::to_string(sectionStats.assertions.failed) + " assertion(s) failed").c_str());
            }

            Base::sectionEnded(sectionStats);
            m_sectionIndent--;
        }

        int m_sectionIndent{0};
    };
    CATCH_REGISTER_LISTENER(ConformanceTestListener)

    static Catch::Session catchSession;  // Only one Catch Session can ever be created.
}  // namespace

// We need to redirect catch2 output through the reporting infrastructure.
// Note that if "-o" is used, Catch will redirect the returned ostream to the file instead.
namespace Catch
{
    std::ostream& cout()
    {
        static ConsoleStream acs(MessageType_Stdout);
        static std::ostream ret(&acs);
        return ret;
    }
    std::ostream& clog()
    {
        static ConsoleStream acs(MessageType_Stdout);
        static std::ostream ret(&acs);
        return ret;
    }
    std::ostream& cerr()
    {
        static ConsoleStream acs(MessageType_Stderr);
        static std::ostream ret(&acs);
        return ret;
    }
}  // namespace Catch

XrcResult XRAPI_CALL xrcEnumerateTestCases(uint32_t capacityInput, uint32_t* countOutput, ConformanceTestCase* testCases)
{
    auto catchTestCases = Catch::getAllTestCasesSorted(catchSession.config());
    *countOutput = (uint32_t)catchTestCases.size();

    if (capacityInput == 0) {
        return XRC_SUCCESS;  // Request for size.
    }

    if (capacityInput < *countOutput) {
        return XRC_ERROR_SIZE_INSUFFICIENT;
    }

    int i = 0;
    for (const Catch::TestCase& testCase : catchTestCases) {
        strcpy(testCases[i].testName, testCase.name.c_str());
        strcpy(testCases[i].tags, testCase.tagsAsString().c_str());
        i++;
    }

    return XRC_SUCCESS;
}

XrcResult XRAPI_CALL xrcRunConformanceTests(const ConformanceLaunchSettings* conformanceLaunchSettings, uint32_t* failureCount)
{
    using namespace Conformance;

    // Reset the state of the catch session since catch session must be re-used across multiple calls
    // and cannot be recreated.
    catchSession.useConfigData({});
    catchSession.cli(Catch::makeCommandLineParser(catchSession.configData()));

    ResetGlobalData();
    g_conformanceLaunchSettings = conformanceLaunchSettings;

    XrcResult result = XRC_SUCCESS;
    bool conformanceTestsRun = false;
    try {
        Conformance::g_reportCallback = [&](const char* message) { conformanceLaunchSettings->message(MessageType_Stdout, message); };

        // Disable loader error output by default, as we intentionally generate errors.
        if (!PlatformUtilsGetEnvSet("XR_LOADER_DEBUG"))      // If not already set to something...
            PlatformUtilsSetEnv("XR_LOADER_DEBUG", "none");  // then set to disabled.

        // Search for layers in the conformance executable folder so that the conformance_layer is included automatically.
        PlatformUtilsSetEnv(OPENXR_API_LAYER_PATH_ENV_VAR, "./");

        ReportTestHeader();

        if (!UpdateOptionsFromCommandLine(catchSession, conformanceLaunchSettings->argc, conformanceLaunchSettings->argv)) {
            ReportStr("Test failure: Command line arguments were invalid or insufficient.");
            return XRC_ERROR_COMMAND_LINE_INVALID;
        }

        bool initialized = GetGlobalData().Initialize();
        if (initialized) {
            ReportTestEnvironment();
        }

        if (catchSession.configData().listTestNamesOnly) {
            // If we only want the test names, "run()" will just print them,
            // then we want to exit without dumping more mess on the screen.
            ReportStr("\nTest names:");
            catchSession.run();
        }

        if (initialized) {
            *failureCount = catchSession.run();
            conformanceTestsRun = true;
        }
        else {
            ReportStr("Test failure: Test data initialization failed.");
            result = XRC_ERROR_INITIALIZATION_FAILED;
        }
    }
    catch (std::exception& e) {
        ReportF("Test failure: C++ exception caught: %s.", e.what());
        result = XRC_ERROR_INTERNAL_ERROR;
    }
    catch (...) {
        ReportStr("Test failure: Unknown C++ exception caught.");
        result = XRC_ERROR_INTERNAL_ERROR;
    }

    if (conformanceTestsRun) {
        // Print a conformance report
        const ConformanceReport& cr = GetGlobalData().GetConformanceReport();
        std::string report = cr.GetReportString();
        ReportF(
            "*********************************************\n"
            "Conformance Report\n"
            "*********************************************\n"
            "%s",
            report.c_str());
    }

    g_conformanceLaunchSettings = nullptr;
    return result;
}
