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

#include "openxr/openxr_platform_defines.h"
#define CATCH_CONFIG_NOSTDOUT

#include "conformance_framework.h"
#include "conformance_test.h"
#include "conformance_utils.h"
#include "environment.h"
#include "graphics_plugin.h"
#include "platform_utils.hpp"  // for OPENXR_API_LAYER_PATH_ENV_VAR
#include "report.h"
#include "utilities/utils.h"

#include <openxr/openxr.h>

#include "catch_reporter_cts.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/internal/catch_clara.hpp>  // for customizing arg parsing
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include "xr_dependencies.h"
#include <openxr/openxr_platform.h>

#include <cstddef>
#include <string>
#include <cstring>
#include <streambuf>
#include <algorithm>

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
            m_s += c_as_char;  // add to local buffer
            if (c_as_char == '\n') {
                sync();  // flush on newlines
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
        ReportConsoleOnlyF("*********************************************");
        ReportConsoleOnlyF("OpenXR Conformance Test v%d.%d.%d", XR_VERSION_MAJOR(XR_CURRENT_API_VERSION),
                           XR_VERSION_MINOR(XR_CURRENT_API_VERSION), XR_VERSION_PATCH(XR_CURRENT_API_VERSION));
        ReportConsoleOnlyF("*********************************************\n");
    }

    /// Display test environment info on the console.
    /// @see WriteTestEnvironment for writing the same information to an XML output file.
    void ReportTestEnvironment()
    {
        GlobalData& globalData = GetGlobalData();

        // Report the runtime name and info.
        const XrInstanceProperties& instanceProperties = globalData.GetInstanceProperties();
        ReportConsoleOnlyF("Runtime instance properties:\n   Runtime name: %s\n   Runtime version %d.%d.%d", instanceProperties.runtimeName,
                           XR_VERSION_MAJOR(instanceProperties.runtimeVersion), XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                           XR_VERSION_PATCH(instanceProperties.runtimeVersion));

        // Report the users-selected options
        std::string optionsDescription = globalData.GetOptions().DescribeOptions();
        ReportConsoleOnlyF("Test options:\n%s", optionsDescription.c_str());

        // Report the available API layers.
        ReportConsoleOnlyF("Available API layers:");
        if (globalData.availableAPILayers.empty())
            ReportConsoleOnlyF("    <none>");
        else {
            for (const XrApiLayerProperties& layerProperties : globalData.availableAPILayers) {
                ReportConsoleOnlyF("    %s, version %u, spec version %d.%d.%d", layerProperties.layerName, layerProperties.layerVersion,
                                   XR_VERSION_MAJOR(layerProperties.specVersion), XR_VERSION_MINOR(layerProperties.specVersion),
                                   XR_VERSION_PATCH(layerProperties.specVersion));
            }
        }

        ReportConsoleOnlyF("Available instance extensions:");
        if (globalData.availableInstanceExtensions.empty())
            ReportConsoleOnlyF("    <none>");
        else {
            for (const XrExtensionProperties& extensionProperties : globalData.availableInstanceExtensions) {
                ReportConsoleOnlyF("    %s, extension version %d", extensionProperties.extensionName, extensionProperties.extensionVersion);
            }
        }
        ReportConsoleOnlyF("");
    }

    TEST_CASE("DescribeGraphicsPlugin", "")
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
    TEST_CASE("ValidateEnvironment")
    {
        // Ensure that the conformance layer is loaded (or print a warning if it
        // is not)
        SECTION("Conformance layer")
        {
            GlobalData& globalData = GetGlobalData();

            if (!globalData.options.invalidHandleValidation) {
                REQUIRE_MSG(globalData.IsAPILayerEnabled("XR_APILAYER_KHRONOS_runtime_conformance"),
                            "Conformance layer required to pass conformance");

                // Conformance listens for failures from the conformance layer through the debug messenger extension.
                REQUIRE_MSG(IsInstanceExtensionEnabled(XR_EXT_DEBUG_UTILS_EXTENSION_NAME),
                            "Debug utils extension required by conformance layer");
            }
            else {
                WARN("Conformance API layer not supported due to handle validation tests; do not submit this log for official conformance");
            }
        }

        uint32_t testCasesCount = 0;
        REQUIRE(XRC_SUCCESS == xrcEnumerateTestCases(0, &testCasesCount, nullptr));

        std::vector<ConformanceTestCase> testCases(testCasesCount);
        REQUIRE(XRC_SUCCESS == xrcEnumerateTestCases(testCasesCount, &testCasesCount, testCases.data()));

        SECTION("Validate Test Case Names")
        {
            for (const auto& testCase : testCases) {
                std::string testName = testCase.testName;

                // Spaces in test names break our Android runner
                INFO(testName);
                REQUIRE(testName.find(" ") == std::string::npos);
            }
        }

        SECTION("Validate Test Case Tags")
        {
            for (const auto& testCase : testCases) {
                std::string testTags = testCase.tags;
                INFO(testCase.testName);
                INFO(testTags);

                // readme.md instructions use [interactive] with [actions], [composition], and [scenario]
                // Let's ensure that these cover all of the possible test cases.
                const std::array<std::string, 3> interactiveTestTypes = {
                    "[actions]",
                    "[composition]",
                    "[scenario]",
                };
                if (testTags.find("[interactive]") != std::string::npos) {
                    {
                        bool foundInteractiveTestType = false;
                        for (const auto& testType : interactiveTestTypes) {
                            if (testTags.find(testType) != std::string::npos) {
                                foundInteractiveTestType = true;
                            }
                        }
                        INFO("An interactive test should also have a tag for either actions, composition, or scenario");
                        REQUIRE(foundInteractiveTestType);
                    }

                    {
                        INFO("Interactive tests are typically either [actions] or [no_auto]");
                        // [interactive] tests are almost always not automatable [no_auto] except when
                        // they are [actions] tests using `XR_EXT_conformance_automation`
                        bool isNoAuto = testTags.find("[no_auto]") != std::string::npos;
                        bool isActions = testTags.find("[actions]") != std::string::npos;
                        REQUIRE((isNoAuto || isActions));
                    }
                }
            }
        }
    }

    Catch::Clara::Parser MakeCLIParser(Conformance::GlobalData& globalData)
    {
        using namespace Catch::Clara;
        auto& options = globalData.options;

        /// Handle rand seed arg
        auto const parseRandSeed = [&](std::string const& arg) {
            using namespace Conformance;
            GlobalData& globalData = GetGlobalData();
            uint64_t seedValue = std::strtoull(arg.c_str(), nullptr, 0);
            if (errno == ERANGE) {
                ReportConsoleOnlyF("invalid arg: %s", arg.c_str());
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
                ReportConsoleOnlyF("invalid arg: %s", globalData.options.formFactor.c_str());
                return ParserResult::runtimeError("invalid form factor '" + arg + "' passed on command line");
            }

            return ParserResult::ok(ParseResultType::Matched);
        };

        /// Handle hands arg
        auto const parseHands = [&](std::string const& arg) {
            using namespace Conformance;
            GlobalData& globalData = GetGlobalData();
            globalData.options.enabledHands = arg;

            if (striequal(globalData.options.enabledHands.c_str(), "left")) {
                globalData.options.leftHandEnabled = true;
                globalData.options.rightHandEnabled = false;
            }
            else if (striequal(globalData.options.enabledHands.c_str(), "right")) {
                globalData.options.leftHandEnabled = false;
                globalData.options.rightHandEnabled = true;
            }
            else if (striequal(globalData.options.enabledHands.c_str(), "both")) {
                globalData.options.leftHandEnabled = true;
                globalData.options.rightHandEnabled = true;
            }
            else {
                ReportConsoleOnlyF("invalid arg: %s", globalData.options.enabledHands.c_str());
                return ParserResult::runtimeError("invalid hands '" + arg + "' passed on command line");
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
                ReportConsoleOnlyF("invalid arg: %s", globalData.options.viewConfiguration.c_str());
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
                ReportConsoleOnlyF("invalid arg: %s", globalData.options.environmentBlendMode.c_str());

                return ParserResult::runtimeError("invalid environment blend mode '" + arg + "' passed on command line");
            }
            return ParserResult::ok(ParseResultType::Matched);
        };

        // NOTE: End of line comments are to encourage clang-format to work the way we want it to for this mini embedded DSL.
        // Clara requires that the "short" args be a single letter - we use capital letters here to avoid colliding with Catch2-provided
        // options.
        auto cli =
            Opt(options.graphicsPlugin,
                "Vulkan|Vulkan2|OpenGLES|OpenGL|D3D11|D3D12")  // graphics plugin
                ["-G"]["--graphicsPlugin"]                     //
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

            | Opt(parseHands, "interaction profile")  // Hands
                  ["--hands"]                         //
              ("Choose which hands to test: left, right, or both. Default is both.")
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

            | Opt(options.nonDisconnectableDevices)  // Runtime supports disconnectable devices
                  ["--nonDisconnectableDevices"]     //
              ("Disables tests that requires disconnectable devices (for debugging).")
                  .optional()

            | Opt([&](bool /* flag */) { options.fileLineLoggingEnabled = false; })  // disable file/line logging
                  ["-F"]["--disableFileLineLogging"]                                 //
              ("Disables logging file/line data.")
                  .optional()

            | Opt(options.pollGetSystem)  // poll xrGetSystem at startup
                  ["--pollGetSystem"]     //
              ("Retry xrGetSystem until success or timeout expires before running tests.")
                  .optional()

            //
            | Opt([&](bool enabled) { options.debugMode = enabled; })  //
                  ["-D"]["--debugMode"]                                //
              ("Sets debug mode as enabled or disabled.")
                  .optional();

        return cli;
    }
    bool UpdateOptionsFromCommandLine(Catch::Session& catchSession, int argc, const char* const* argv)
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
        globalData.leftHandUnderTest = globalData.options.leftHandEnabled;
        globalData.rightHandUnderTest = globalData.options.rightHandEnabled;

        if (!(catchSession.configData().listTests || catchSession.configData().listTags || catchSession.configData().listListeners ||
              catchSession.configData().listReporters)) {
            // Check for required parameters, if we are actually going to run tests
            if (GetGlobalData().options.graphicsPlugin.empty()) {  // If no graphics system was specified...
                if (GetGlobalData().IsGraphicsPluginRequired()) {  // and if one is required...
                    ReportConsoleOnlyF("graphicsPlugin parameter is required.");
                    return false;
                }
            }
        }
        return result == 0;
    }

    // Implements a class that listens to the results of individual test runs. This is used for
    // collecting telemetry.
    struct ConformanceTestListener : Catch::EventListenerBase
    {
        using Base = Catch::EventListenerBase;

        using EventListenerBase::EventListenerBase;  // inherit constructor

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
            std::string indentStr(static_cast<long>(m_sectionIndent) * 2, ' ');
            g_conformanceLaunchSettings->message(MessageType_TestSectionStarting,
                                                 (indentStr + "Executing \"" + sectionInfo.name + "\" tests...\n").c_str());
            m_sectionIndent++;
        }
        void sectionEnded(Catch::SectionStats const& sectionStats) override
        {
            // Show a summary if something failed but leave the details to the (e.g. console or xml) reporter.
            if (sectionStats.assertions.failed > 0) {
                std::string indentStr(static_cast<long>(m_sectionIndent) * 2, ' ');
                g_conformanceLaunchSettings->message(
                    MessageType_AssertionFailed,
                    (indentStr + std::to_string(sectionStats.assertions.failed) + " assertion(s) failed\n").c_str());
            }

            Base::sectionEnded(sectionStats);
            m_sectionIndent--;
        }

        void noMatchingTestCases(Catch::StringRef /* unmatchedSpec */) override
        {
            Conformance::GlobalData& globalData = Conformance::GetGlobalData();
            globalData.conformanceReport.unmatchedTestSpecs = true;
        }

        void testRunEnded(Catch::TestRunStats const& testRunStats) override
        {
            Conformance::GlobalData& globalData = Conformance::GetGlobalData();
            globalData.conformanceReport.totals = testRunStats.totals;
        }

        int m_sectionIndent{0};
    };
    CATCH_REGISTER_LISTENER(ConformanceTestListener)
    CATCH_REGISTER_REPORTER("ctsxml", Catch::CTSReporter)

    // static Catch::Session catchSession;  // Only one Catch Session can ever be created.
    static std::shared_ptr<Catch::Session> catchSession;

    static Catch::Session& CreateOrGetCatchSession()
    {
        if (catchSession == nullptr) {
            catchSession = std::make_shared<Catch::Session>();
        }
        return *catchSession;
    }
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

XrcResult XRAPI_CALL xrcCleanup()
{
    GetGlobalData().Shutdown();
    Catch::cleanUp();
    catchSession = nullptr;
    return XRC_SUCCESS;
}

XrcResult XRAPI_CALL xrcEnumerateTestCases(uint32_t capacityInput, uint32_t* countOutput, ConformanceTestCase* testCases)
{
    auto catchTestCases = Catch::getAllTestCasesSorted(CreateOrGetCatchSession().config());
    *countOutput = (uint32_t)catchTestCases.size();

    if (capacityInput == 0) {
        return XRC_SUCCESS;  // Request for size.
    }

    if (capacityInput < *countOutput) {
        return XRC_ERROR_SIZE_INSUFFICIENT;
    }

    int i = 0;
    for (const Catch::TestCaseHandle& testCase : catchTestCases) {
        auto& testCaseInfo = testCase.getTestCaseInfo();
        strcpy(testCases[i].testName, testCaseInfo.name.c_str());
        strcpy(testCases[i].tags, testCaseInfo.tagsAsString().c_str());
        i++;
    }

    return XRC_SUCCESS;
}

XrcResult XRAPI_CALL xrcRunConformanceTests(const ConformanceLaunchSettings* conformanceLaunchSettings, XrcTestResult* testResult,
                                            uint64_t* failureCount)
{
    using namespace Conformance;

    // Reset the state of the catch session since catch session must be re-used across multiple calls
    // and cannot be recreated.
    CreateOrGetCatchSession().useConfigData({});
    CreateOrGetCatchSession().cli(Catch::makeCommandLineParser(CreateOrGetCatchSession().configData()));

    ResetGlobalData();
    g_conformanceLaunchSettings = conformanceLaunchSettings;

    XrcResult result = XRC_SUCCESS;
    *testResult = XRC_TEST_RESULT_SUCCESS;
    *failureCount = 0;
    bool conformanceTestsRun = false;
    try {
        Conformance::g_reportCallback = [&](const char* message) { conformanceLaunchSettings->message(MessageType_Stdout, message); };

        // Disable loader error output by default, as we intentionally generate errors.
        if (!GetEnvSet("XR_LOADER_DEBUG"))      // If not already set to something...
            SetEnv("XR_LOADER_DEBUG", "none");  // then set to disabled.

        // Search for layers in the conformance executable folder so that the conformance_layer is included automatically.
        SetEnv(OPENXR_API_LAYER_PATH_ENV_VAR, "./");

        ReportTestHeader();

#if defined(XR_USE_PLATFORM_ANDROID)
        PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
        if (XR_SUCCEEDED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)(&xrInitializeLoaderKHR))) &&
            xrInitializeLoaderKHR != NULL) {
            XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
            loaderInitializeInfoAndroid.applicationVM = Conformance_Android_Get_Application_VM();
            loaderInitializeInfoAndroid.applicationContext = Conformance_Android_Get_Application_Context();
            xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
        }
#endif  // defined(XR_USE_PLATFORM_ANDROID)

        if (!UpdateOptionsFromCommandLine(CreateOrGetCatchSession(), conformanceLaunchSettings->argc, conformanceLaunchSettings->argv)) {
            ReportConsoleOnlyF("Test failure: Command line arguments were invalid or insufficient.");
            return XRC_ERROR_COMMAND_LINE_INVALID;
        }
        auto& catchConfig = CreateOrGetCatchSession().config();
        auto& catchConfigData = CreateOrGetCatchSession().configData();
        bool skipActuallyTesting =
            catchConfigData.listTests || catchConfigData.listTags || catchConfigData.listListeners || catchConfigData.listReporters;
        bool initialized = true;
        if (!skipActuallyTesting) {
            initialized = GetGlobalData().Initialize();
            if (initialized) {
                ReportTestEnvironment();
            }
        }

        if (CreateOrGetCatchSession().configData().verbosity == Catch::Verbosity::Quiet) {
            // If we only want the test names, "run()" will just print them,
            // then we want to exit without dumping more mess on the screen.
            ReportConsoleOnlyF("\nTest names:");
            CreateOrGetCatchSession().run();
        }

        if (initialized) {
            int exitCode = CreateOrGetCatchSession().run();

            Conformance::GlobalData& globalData = Conformance::GetGlobalData();
            *failureCount = globalData.conformanceReport.testFailureCount;
            const auto& totals = globalData.conformanceReport.totals;
            conformanceTestsRun = true;

            // a list option was used so no tests could have run
            if (skipActuallyTesting) {
                *testResult = XRC_TEST_RESULT_SUCCESS;
            }
            else if (globalData.conformanceReport.unmatchedTestSpecs && catchConfig.warnAboutUnmatchedTestSpecs()) {
                *testResult = XRC_TEST_RESULT_UNMATCHED_TEST_SPEC;
            }
            else if (totals.testCases.total() == 0 && !catchConfig.zeroTestsCountAsSuccess()) {
                *testResult = XRC_TEST_RESULT_NO_TESTS_SELECTED;
            }
            else if (totals.testCases.total() > 0 && totals.testCases.total() == totals.testCases.skipped &&
                     !catchConfig.zeroTestsCountAsSuccess()) {
                *testResult = XRC_TEST_RESULT_ALL_TESTS_SKIPPED;
            }
            else if (exitCode != 0) {
                *testResult = XRC_TEST_RESULT_SOME_TESTS_FAILED;
            }
        }
        else {
            ReportF("Test failure: Test data initialization failed.");
            result = XRC_ERROR_INITIALIZATION_FAILED;
        }
    }
    catch (std::exception& e) {
        ReportF("Test failure: C++ exception caught: %s.", e.what());
        result = XRC_ERROR_INTERNAL_ERROR;
    }
    catch (...) {
        ReportF("Test failure: Unknown C++ exception caught.");
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
