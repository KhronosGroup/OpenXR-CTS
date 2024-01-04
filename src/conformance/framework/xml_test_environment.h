// Copyright (c) 2023-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>
#include <nonstd/span.hpp>

namespace Catch
{
    class XmlWriter;
}

namespace Conformance
{
    using nonstd::span_lite::span;

    class ConformanceReport;
    struct Options;
    class GlobalData;

    /// Write the xmlns:cts attribute to the currently open element
    void WriteXmlnsAttribute(Catch::XmlWriter& xml);

    /// Output conformance report summary data
    void WriteConformanceReportSummary(Catch::XmlWriter& xml, const ConformanceReport& cr);

    /// Output test environment and instance/runtime data
    void WriteTestEnvironment(Catch::XmlWriter& xml, GlobalData& globalData);

    // Helpers called by WriteTestEnvironment

    /// Write data about the API layers as a `cts:availableApiLayers` element
    /// containing a `cts:apiLayerProperties` element for each with data in its attributes.
    void WriteAvailableApiLayers(Catch::XmlWriter& xml, const span<XrApiLayerProperties> availableApiLayers);

    /// Write data about the instance extensions as a `cts:availableInstanceExtensions` element
    /// containing a `cts:extensionProperties` element for each with data in its attributes.
    void WriteAvailableInstanceExtensions(Catch::XmlWriter& xml, const span<XrExtensionProperties> availableInstanceExtensions);

    /// Write out instance properties as a `cts:runtimeInstanceProperties` element with two sub-elements
    void WriteInstanceProperties(Catch::XmlWriter& xml, const XrInstanceProperties& instanceProperties);

    /// Write out test options as a `cts:testOptions` element with one sub-element for each option:
    /// Both the string/cli version and the parsed version of each option are output as attributes.
    void WriteTestOptions(Catch::XmlWriter& xml, const Options& options);

    /// Write out active API layers and instance extensions. These include extensions and layers turned on by the tests themselves,
    /// not just those specified in the options.
    void WriteActiveApiLayersAndExtensions(Catch::XmlWriter& xml, const GlobalData& globalData);

}  // namespace Conformance
