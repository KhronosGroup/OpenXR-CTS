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

#pragma once

#include <openxr/openxr.h>
#include <nonstd/span.hpp>
#include "swapchain_parameters.h"
#include "utils.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <cassert>
#include <map>

namespace Conformance
{
    using nonstd::span;

    /// Minimal data structure storing details about a swapchain image format.
    ///
    /// May eventually replace @ref SwapchainImageTestParam
    class SwapchainFormatData
    {
    public:
        /// The graphics-API-specific numeric value of the image format
        int64_t GetImageFormat() const
        {
            return m_imageFormat;
        }

        /// String-ified version of the C identifier.
        const char* GetImageFormatName() const
        {
            return m_imageFormatName;
        }

        /// The graphics-API-specific created image format returned by `xrCreateSwapchain`, may be different from @ref GetImageFormat()
        int64_t GetExpectedCreatedImageFormat() const
        {
            return m_expectedCreatedImageFormat;
        }

        /// Whether "unordered access" usage flag is allowed
        bool SupportsUnorderedAccess() const
        {
            return m_allowUA;
        }

        /// Whether the image format is a mutable (a.k.a. typeless) type.
        bool IsTypeless() const
        {
            return m_isTypeless;
        }

        /// Whether the image format supports creation with XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT.
        bool SupportsMutableFormatBit() const
        {
            return m_supportsMutableFormat;
        }

        /// Whether the format is a color-specific format
        bool GetColorFormat() const
        {
            return m_colorFormat;
        }

        /// Whether the format can be use as a depth buffer: implies not color
        bool GetDepthFormat() const
        {
            return m_depthFormat;
        }

        /// Whether the format can be use as a stencil buffer: implies not color
        bool GetStencilFormat() const
        {
            return m_stencilFormat;
        }

        /// Whether the format is a compressed format (and thus cannot be rendered to)
        bool GetCompressedFormat() const
        {
            return m_compressedFormat;
        }

        /// XrSwapchainUsageFlags to exercise for this format.
        /// Defaults to all combinations, including 0, of the core flags.
        span<const XrSwapchainUsageFlags> GetUsageFlagsTestValues() const
        {
            return m_usageFlagsVector;
        }

        /// XrSwapchainCreateFlags
        span<const XrSwapchainCreateFlags> GetCreateFlagsTestValues() const
        {
            return m_createFlagsVector;
        }

        /// Convert to a @ref SwapchainCreateTestParameters instance
        SwapchainCreateTestParameters ToTestParameters() const;

        /// Convert to a pair of the numeric format and @ref SwapchainCreateTestParameters instance
        std::pair<int64_t, SwapchainCreateTestParameters> Build() const;

        /// Return pair of the numeric format and and ourself.
        std::pair<int64_t, SwapchainFormatData> ToPair() const;

        /// Describe this entry
        std::string ToString() const;

    private:
        friend class SwapchainCreateTestParametersBuilder;
        SwapchainFormatData(int64_t format, const char* name);

        /// The graphics-API-specific numeric value of the image format
        int64_t m_imageFormat;

        /// String-ified version of the C identifier.
        const char* m_imageFormatName;

        /// The graphics-API-specific created image format returned by `xrCreateSwapchain`, may be different from @ref m_imageFormat in some cases.
        int64_t m_expectedCreatedImageFormat;

        /// Whether "unordered access" usage flag is allowed
        bool m_allowUA = true;

        /// Whether the image format is a mutable (a.k.a. typeless) type.
        bool m_isTypeless = false;

        /// Whether the image format supports creation with XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT.
        bool m_supportsMutableFormat = true;

        /// Whether the format is a color-specific format
        bool m_colorFormat = true;

        /// Whether the format can be use as a depth buffer: implies not color
        bool m_depthFormat = false;

        /// Whether the format can be use as a stencil buffer: implies not color
        bool m_stencilFormat = false;

        /// Whether the format is a compressed format (and thus cannot be rendered to)
        bool m_compressedFormat = false;

        /// XrSwapchainUsageFlags to exercise for this format.
        /// Defaults to all combinations, including 0, of the core flags.
        /// @todo Stop making so many copies of this, generate it from the other data instead
        span<const XrSwapchainUsageFlags> m_usageFlagsVector;

        /// XrSwapchainCreateFlags
        /// @todo Stop making so many copies of this, generate it from the other data instead
        span<const XrSwapchainCreateFlags> m_createFlagsVector;
    };

    /// A map of swapchain format (numeric value) to @ref SwapchainFormatData
    using SwapchainFormatDataMap = std::map<int64_t, SwapchainFormatData>;

    /// Look up the swapchain create test parameters in an map (API-specific).
    ///
    /// Throws if the format cannot be found.
    SwapchainCreateTestParameters GetSwapchainCreateTestParameters(const SwapchainFormatDataMap& formatData, int64_t imageFormat);

    /// Returns a name for an image format. Returns "unknown" for unknown formats.
    const char* GetImageFormatName(const SwapchainFormatDataMap& formatData, int64_t imageFormat);

    /// Returns true if the format is known to the plugin. Can be false if the runtime supports extra formats unknown to the conformance tests
    /// (e.g. in APIs which have optional extensions).
    bool IsImageFormatKnown(const SwapchainFormatDataMap& formatData, int64_t imageFormat);

    class SwapchainCreateTestParametersBuilder
    {
    public:
        SwapchainCreateTestParametersBuilder(int64_t imageFormat, const char* imageFormatName);

        using Self = SwapchainCreateTestParametersBuilder;

        /// Mark this as not supporting "unordered access"
        Self& NoUnorderedAccess()
        {
            m_data.m_allowUA = false;
            UpdateDefaultUsageFlagVector();
            return *this;
        }

        /// Mark this as being a "typeless" format (just channels of widths, no implied interpretation)
        ///
        /// Also sets some default usage flags.
        Self& Typeless()
        {
            m_data.m_isTypeless = true;
            /// @todo is this actually right? It's what the old d3d code did.
            m_data.m_createFlagsVector = {};
            UpdateDefaultUsageFlagVector();
            return *this;
        }

        /// Mark this as supporting depth buffer usage (and un-marking for color buffer usage)
        ///
        /// Also sets some default usage flags.
        Self& Depth()
        {
            m_data.m_depthFormat = true;
            NotColor();
            return *this;
        }

        /// Mark this as supporting stencil buffer usage (and un-marking for color buffer usage)
        ///
        /// Also sets some default usage flags.
        Self& Stencil()
        {
            m_data.m_stencilFormat = true;
            NotColor();
            return *this;
        }

        /// Mark this as supporting depth and stencil buffer usage (and un-marking for color buffer usage)
        ///
        /// Also sets some default usage flags.
        ///
        /// Equivalent to calling both @ref Depth() and @ref Stencil()
        Self& DepthStencil()
        {
            m_data.m_stencilFormat = true;
            m_data.m_depthFormat = true;
            NotColor();
            return *this;
        }

        /// Record that we expect the runtime to allocate this as the specified different format (normally a typeless version if one exists)
        Self& ExpectedFormat(int64_t format)
        {
            assert(format == m_data.m_expectedCreatedImageFormat || !m_data.m_isTypeless);
            m_data.m_expectedCreatedImageFormat = format;
            return *this;
        }

        /// Mark this as a format for which we should not test the `XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT`
        Self& NotMutable()
        {
            assert(!m_data.m_isTypeless);
            m_data.m_supportsMutableFormat = false;
            UpdateDefaultUsageFlagVector();
            return *this;
        }

        /// Mark this as a compressed format that we should not test rendering to
        Self& Compressed()
        {
            m_data.m_compressedFormat = true;
            UpdateDefaultUsageFlagVector();
            return *this;
        }

        /// Populate the usage flags combinations to test.
        ///
        /// @note Call this method *after* any other builder methods other than @ref Build, since many of them update the usage flags
        Self& UsageFlags(span<const XrSwapchainUsageFlags> usageFlagCombinationsToTest)
        {
            m_data.m_usageFlagsVector = usageFlagCombinationsToTest;
            return *this;
        }

        /// Populate the create flags combinations to test
        Self& CreateFlags(span<const XrSwapchainCreateFlags> createFlagCombinationsToTest)
        {
            m_data.m_createFlagsVector = createFlagCombinationsToTest;
            return *this;
        }

        /// Convert to a pair of the numeric format and @ref SwapchainCreateTestParameters instance
        std::pair<int64_t, SwapchainCreateTestParameters> Build() const;

        /// Return pair of the numeric format and and ourself.
        std::pair<int64_t, SwapchainFormatData> ToPair() const;

        /// Describe this entry
        std::string ToString() const;

    private:
        void NotColor()
        {
            m_data.m_colorFormat = false;
            UpdateDefaultUsageFlagVector();
        }

        void UpdateDefaultUsageFlagVector();
        SwapchainFormatData m_data;
    };

/// Wraps constructor of @ref SwapchainCreateTestParametersBuilder to stringify format name
#define XRC_SWAPCHAIN_FORMAT(FORMAT) (::Conformance::SwapchainCreateTestParametersBuilder(FORMAT, #FORMAT))

}  // namespace Conformance
