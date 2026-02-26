// Copyright (c) 2019-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

#include <string>
#include <map>

namespace Conformance
{
    /// We keep a private auto-generated map of all results and their string versions.
    typedef std::map<XrResult, const char*> ResultStringMap;

    const ResultStringMap& GetResultStringMap();

    /// @addtogroup cts_framework
    /// @{

    /// Returns a string for a given XrResult, based on our accounting of the result strings, and not
    /// based on the xrResultToString function.
    /// Returns "<unknown>" if the result is not recognized.
    ///
    /// Example usage:
    /// ```
    /// XrResult result = xrPollEvent(instance, &eventData);
    /// printf("%d: %s", result, ResultToString(result));
    /// ```
    const char* ResultToString(XrResult result);

    /// Helper to convert an XrVersion to a string formatted as major.minor.patch
    std::string VersionToString(XrVersion version);

    /// @addtogroup cts_framework
    /// @{

#define XR_ENUM_CASE_STR(name, val) \
    case name:                      \
        return #name;
#define XR_ENUM_STR(enumType)                                                       \
    constexpr const char* XrEnumStr(enumType e)                                     \
    {                                                                               \
        switch (e) {                                                                \
            XR_LIST_ENUM_##enumType(XR_ENUM_CASE_STR) default : return "<unknown>"; \
        }                                                                           \
    }

    XR_ENUM_STR(XrSpatialCapabilityEXT)
    XR_ENUM_STR(XrSpatialCapabilityFeatureEXT)
    XR_ENUM_STR(XrSpatialComponentTypeEXT)
    XR_ENUM_STR(XrSpatialPersistenceScopeEXT)
    XR_ENUM_STR(XrSpatialPersistenceContextResultEXT)

#define XRC_CHECK_STRINGIFY(x) #x
#define XRC_TO_STRING(x) XRC_CHECK_STRINGIFY(x)

/// Represents a compile time file and line location as a single string.
#define XRC_FILE_AND_LINE __FILE__ ":" XRC_TO_STRING(__LINE__)

}  // namespace Conformance
