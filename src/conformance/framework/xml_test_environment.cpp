// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "xml_test_environment.h"
#include <chrono>

#include "catch2/internal/catch_xmlwriter.hpp"
#include "conformance_framework.h"
#include "hex_and_handles.h"

#define CTS_XML_NS_PREFIX "cts"
#define CTS_XML_NS_PREFIX_QUALIFIER CTS_XML_NS_PREFIX ":"
namespace Conformance
{
    void WriteXmlnsAttribute(Catch::XmlWriter& xml)
    {
        xml.writeAttribute("xmlns:" CTS_XML_NS_PREFIX, "https://github.com/KhronosGroup/OpenXR-CTS");
    }

    void WriteConformanceReportSummary(Catch::XmlWriter& xml, const ConformanceReport& cr)
    {
        auto e = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "ctsConformanceReport");
        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "apiVersion")
            .writeAttribute("major", XR_VERSION_MAJOR(cr.apiVersion))
            .writeAttribute("minor", XR_VERSION_MINOR(cr.apiVersion))
            .writeAttribute("patch", XR_VERSION_PATCH(cr.apiVersion))
            .writeText(to_hex(cr.apiVersion));
        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "results")
            .writeAttribute("testSuccessCount", cr.testSuccessCount)
            .writeAttribute("testFailureCount", cr.testFailureCount);
        if (cr.timedSubmission.IsValid()) {
            const auto& timing = cr.timedSubmission;
            using ms = std::chrono::duration<float, std::milli>;
            auto e2 = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "timedSubmission");
            xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "averageWaitTime")
                .writeAttribute("ms", std::chrono::duration_cast<ms>(timing.GetAverageWaitTime()).count());
            xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "averageAppFrameTime")
                .writeAttribute("ms", std::chrono::duration_cast<ms>(timing.GetAverageAppFrameTime()).count());
            xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "averageDisplayPeriod")
                .writeAttribute("ms", std::chrono::duration_cast<ms>(timing.GetAverageDisplayPeriod()).count());
            xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "averageBeginWaitTime")
                .writeAttribute("ms", std::chrono::duration_cast<ms>(timing.GetAverageBeginWaitTime()).count());
            xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "overhead").writeAttribute("percent", timing.GetOverheadFactor() * 100.f);
        }
        if (!cr.swapchainFormats.empty()) {
            auto e2 = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "swapchainFormats");
            for (const auto& formatAndName : cr.swapchainFormats) {
                xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "format")
                    .writeAttribute("name", formatAndName.second)
                    .writeAttribute("value", formatAndName.first);
            }
        }
    }

    void WriteInstanceProperties(Catch::XmlWriter& xml, const XrInstanceProperties& instanceProperties)
    {
        auto e = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "runtimeInstanceProperties");
        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "runtimeVersion")
            .writeAttribute("major", XR_VERSION_MAJOR(instanceProperties.runtimeVersion))
            .writeAttribute("minor", XR_VERSION_MINOR(instanceProperties.runtimeVersion))
            .writeAttribute("patch", XR_VERSION_PATCH(instanceProperties.runtimeVersion))
            .writeText(to_hex(instanceProperties.runtimeVersion));
        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "runtimeName").writeText(instanceProperties.runtimeName);
    }

    void WriteAvailableApiLayers(Catch::XmlWriter& xml, const span<XrApiLayerProperties> availableApiLayers)
    {

        auto e = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "availableApiLayers");

        for (const XrApiLayerProperties& layerProperties : availableApiLayers) {
            xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "apiLayerProperties")
                .writeAttribute("layerName", layerProperties.layerName)
                .writeAttribute("layerVersion", layerProperties.layerVersion)
                .writeAttribute("specVersionMajor", XR_VERSION_MAJOR(layerProperties.specVersion))
                .writeAttribute("specVersionMinor", XR_VERSION_MINOR(layerProperties.specVersion))
                .writeAttribute("specVersionPatch", XR_VERSION_PATCH(layerProperties.specVersion));
        }
    }

    void WriteAvailableInstanceExtensions(Catch::XmlWriter& xml, const span<XrExtensionProperties> availableInstanceExtensions)
    {
        auto e = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "availableInstanceExtensions");

        for (const XrExtensionProperties& extensionProperties : availableInstanceExtensions) {
            xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "extensionProperties")
                .writeAttribute("extensionName", extensionProperties.extensionName)
                .writeAttribute("extensionVersion", extensionProperties.extensionVersion);
        }
    }

    void WriteTestOptions(Catch::XmlWriter& xml, const Options& options)
    {
        auto e = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "testOptions");
        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "graphicsPlugin").writeAttribute("value", options.graphicsPlugin);

        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "formFactor")
            .writeAttribute("string", options.formFactor)
            .writeAttribute("value", enum_to_string(options.formFactorValue));

        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "enabledHands")
            .writeAttribute("string", options.enabledHands)
            .writeAttribute("leftHandEnabled", options.leftHandEnabled)
            .writeAttribute("rightHandEnabled", options.rightHandEnabled);

        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "viewConfiguration")
            .writeAttribute("string", options.viewConfiguration)
            .writeAttribute("value", enum_to_string(options.viewConfigurationValue));

        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "environmentBlendMode")
            .writeAttribute("string", options.environmentBlendMode)
            .writeAttribute("value", enum_to_string(options.environmentBlendModeValue));

        {
            auto e2 = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "enabledAPILayers");
            for (const auto& name : options.enabledAPILayers) {
                xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "layer").writeAttribute("name", name);
            }
        }

        {
            auto e2 = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "enabledInstanceExtensions");
            for (const auto& name : options.enabledInstanceExtensions) {
                xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "extension").writeAttribute("name", name);
            }
        }

        {
            auto e2 = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "enabledInteractionProfiles");
            for (const auto& name : options.enabledInteractionProfiles) {
                xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "interactionProfile").writeAttribute("path", name);
            }
        }

        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "invalidHandleValidation").writeAttribute("value", options.invalidHandleValidation);
        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "nonDisconnectableDevices").writeAttribute("value", options.nonDisconnectableDevices);
        if (options.nonDisconnectableDevices) {
            xml.writeComment("WARNING: turning off disconnectable devices results in skipping mandatory tests!");
        }

        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "fileLineLoggingEnabled").writeAttribute("value", options.fileLineLoggingEnabled);
        xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "debugMode").writeAttribute("value", options.debugMode);
    }

    void WriteActiveApiLayersAndExtensions(Catch::XmlWriter& xml, const GlobalData& globalData)
    {
        auto e = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "activeAPILayersAndExtensions");
        {
            auto e2 = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "activeAPILayers");

            for (const std::string& apiLayerName : globalData.enabledAPILayerNames) {
                xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "layer").writeAttribute("name", apiLayerName.c_str());
            }
        }
        {
            auto e2 = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "activeInstanceExtensions");

            for (const std::string& extensionName : globalData.enabledInstanceExtensionNames) {
                xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "extension").writeAttribute("name", extensionName);
            }
        }
    }

    void WriteTestEnvironment(Catch::XmlWriter& xml, GlobalData& globalData)
    {
        auto e = xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "ctsTestEnvironment");

        // Report the runtime name and info.
        WriteInstanceProperties(xml, globalData.GetInstanceProperties());

        // Report the users-selected options
        WriteTestOptions(xml, globalData.GetOptions());

        // Report the available API layers.
        WriteAvailableApiLayers(xml, globalData.availableAPILayers);

        WriteAvailableInstanceExtensions(xml, globalData.availableInstanceExtensions);

        if (globalData.IsGraphicsPluginRequired()) {
            AutoBasicInstance instance(AutoBasicInstance::createSystemId);
            auto graphicsPlugin = globalData.GetGraphicsPlugin();
            if (graphicsPlugin) {
                // DescribeGraphics may report only minimal info (name) due to not having a running instance, but this is OK for now.
                xml.scopedElement(CTS_XML_NS_PREFIX_QUALIFIER "graphicsPluginDescription")
                    .writeText(graphicsPlugin->DescribeGraphics(), Catch::XmlFormatting::None);
            }
        }
    }

}  // namespace Conformance
