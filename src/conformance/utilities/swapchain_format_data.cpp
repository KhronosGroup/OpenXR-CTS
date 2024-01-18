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

#include "common/hex_and_handles.h"
#include "swapchain_parameters.h"
#include "swapchain_format_data.h"
#include "throw_helpers.h"

#include <openxr/openxr.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <sstream>

namespace Conformance
{

    // the app might request any combination of flags
    static constexpr auto XRC_COLOR_UA_COPY_SAMPLED_MUTABLE_USAGE_FLAGS = {
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
            XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
            XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
            XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
            XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
    };
    static constexpr auto XRC_COLOR_COPY_SAMPLED_USAGE_FLAGS = {
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
    };
    static constexpr auto XRC_COLOR_COPY_SAMPLED_MUTABLE_USAGE_FLAGS = {
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
    };
    static constexpr auto XRC_DEPTH_COPY_SAMPLED_USAGE_FLAGS = {
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
    };
    static constexpr auto XRC_COMPRESSED_SAMPLED_MUTABLE_USAGE_FLAGS = {
        XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
    };
    static constexpr auto XRC_COMPRESSED_SAMPLED_USAGE_FLAGS = {
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
    };

    static constexpr std::array<const uint32_t, 1> kArrayOf1{{1}};
    static constexpr std::array<const uint32_t, 2> kArrayOf1And2{{1, 2}};
    static constexpr std::array<const XrSwapchainCreateFlags, 4> kDefaultCreateFlags{
        {0, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT,
         XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT | XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT}};

    SwapchainFormatData::SwapchainFormatData(int64_t format, const char* name)
        : m_imageFormat(format)
        , m_imageFormatName(name)
        , m_expectedCreatedImageFormat(format)
        , m_createFlagsVector(kDefaultCreateFlags.begin(), kDefaultCreateFlags.end())
    {
    }

    SwapchainCreateTestParameters GetSwapchainCreateTestParameters(const SwapchainFormatDataMap& formatData, int64_t imageFormat)
    {
        SwapchainFormatDataMap::const_iterator it = formatData.find(imageFormat);

        // Verify that the image format is known. If it's not known then this test needs to be
        // updated to recognize new formats.
        if (it == formatData.end()) {
            XRC_THROW("Unknown image format: " + std::to_string(imageFormat));
        }

        // Verify that imageFormat is not a typeless type. Only regular types are allowed to
        // be returned by the runtime for enumerated image formats.
        if (it->second.IsTypeless()) {
            XRC_THROW("Typeless image formats must not be enumerated by runtimes: " + std::string{it->second.GetImageFormatName()});
        }

        // We may now proceed with creating swapchains with the format.
        return it->second.ToTestParameters();
    }

    const char* GetImageFormatName(const SwapchainFormatDataMap& formatData, int64_t imageFormat)
    {
        SwapchainFormatDataMap::const_iterator it = formatData.find(imageFormat);

        if (it != formatData.end()) {
            return it->second.GetImageFormatName();
        }

        return "unknown";
    }

    bool IsImageFormatKnown(const SwapchainFormatDataMap& formatData, int64_t imageFormat)
    {
        SwapchainFormatDataMap::const_iterator it = formatData.find(imageFormat);

        return it != formatData.end();
    }

    SwapchainCreateTestParametersBuilder::SwapchainCreateTestParametersBuilder(int64_t imageFormat, const char* imageFormatName)
        : m_data(imageFormat, imageFormatName)
    {
        UpdateDefaultUsageFlagVector();
    }

    std::pair<int64_t, SwapchainCreateTestParameters> SwapchainCreateTestParametersBuilder::Build() const
    {
        return m_data.Build();
    }

    std::pair<int64_t, SwapchainFormatData> SwapchainCreateTestParametersBuilder::ToPair() const
    {
        return m_data.ToPair();
    }

    std::string SwapchainCreateTestParametersBuilder::ToString() const
    {
        return m_data.ToString();
    }

    void SwapchainCreateTestParametersBuilder::UpdateDefaultUsageFlagVector()
    {
        if (m_data.m_isTypeless) {
            m_data.m_usageFlagsVector = {};
        }
        else if (m_data.m_compressedFormat) {

            if (m_data.m_supportsMutableFormat) {
                // compressed, mutable
                m_data.m_usageFlagsVector = XRC_COMPRESSED_SAMPLED_MUTABLE_USAGE_FLAGS;
            }
            else {
                // compressed, not mutable
                m_data.m_usageFlagsVector = XRC_COMPRESSED_SAMPLED_USAGE_FLAGS;
            }
        }
        else {  // not compressed
            if (m_data.m_colorFormat) {
                if (m_data.m_supportsMutableFormat) {
                    // not compressed, color, mutable
                    m_data.m_usageFlagsVector =
                        m_data.m_allowUA ? XRC_COLOR_UA_COPY_SAMPLED_MUTABLE_USAGE_FLAGS : XRC_COLOR_COPY_SAMPLED_MUTABLE_USAGE_FLAGS;
                }
                else {

                    // not compressed, color, not mutable
                    m_data.m_usageFlagsVector = XRC_COLOR_COPY_SAMPLED_USAGE_FLAGS;
                }
            }
            else {

                // not compressed, depth/stencil
                m_data.m_usageFlagsVector = XRC_DEPTH_COPY_SAMPLED_USAGE_FLAGS;
            }
        }
    }

    SwapchainCreateTestParameters SwapchainFormatData::ToTestParameters() const
    {

        span<const uint32_t> mipCountVector{kArrayOf1.begin(), kArrayOf1.end()};
        if (m_colorFormat && !m_compressedFormat) {
            mipCountVector = {kArrayOf1And2.begin(), kArrayOf1And2.end()};
        }

        span<const uint32_t> arrayCountVector{kArrayOf1And2.begin(), kArrayOf1And2.end()};
        return SwapchainCreateTestParameters{
            std::string{m_imageFormatName},                                                            //
            m_isTypeless ? SwapchainFormatMutability::MUTABLE : SwapchainFormatMutability::IMMUTABLE,  //
            m_supportsMutableFormat ? SwapchainFormatSupportsMutability::MUT_SUPPORT
                                    : SwapchainFormatSupportsMutability::NO_MUT_SUPPORT,  //
            m_colorFormat ? SwapchainFormatIsColor::COLOR : SwapchainFormatIsColor::NON_COLOR,
            m_compressedFormat ? SwapchainFormatIsCompressed::COMPRESSED : SwapchainFormatIsCompressed::UNCOMPRESSED,
            m_compressedFormat ? SwapchainFormatSupportsRendering::NO_RENDERING_SUPPORT
                               : SwapchainFormatSupportsRendering::RENDERING_SUPPORT,
            m_expectedCreatedImageFormat,
            {m_usageFlagsVector.begin(), m_usageFlagsVector.end()},
            {m_createFlagsVector.begin(), m_createFlagsVector.end()},
            {arrayCountVector.begin(), arrayCountVector.end()},
            {/* sampleCountVector - unused */},
            {mipCountVector.begin(), mipCountVector.end()},
            m_depthFormat,
            m_stencilFormat,
        };
    }

    std::pair<int64_t, SwapchainCreateTestParameters> SwapchainFormatData::Build() const
    {

        return std::make_pair(m_imageFormat, ToTestParameters());
    }

    std::pair<int64_t, SwapchainFormatData> SwapchainFormatData::ToPair() const
    {

        return {m_imageFormat, *this};
    }

    std::string SwapchainFormatData::ToString() const
    {

        std::ostringstream oss;
        oss << m_imageFormatName << " (" << to_hex(m_imageFormat) << "):";
        if (m_compressedFormat) {
            oss << " compressed";
        }

        // what kind of thing: color, depth, stencil
        if (m_colorFormat) {
            oss << " color";
        }
        else if (m_depthFormat && m_stencilFormat) {
            oss << " depth/stencil";
        }
        else if (m_depthFormat) {
            oss << " depth";
        }
        else if (m_stencilFormat) {
            oss << " stencil";
        }

        if (m_isTypeless) {
            oss << " typeless";
        }

        oss << " texture format";

        if (!m_supportsMutableFormat) {
            oss << " (no mutable format support)";
        }
        if (!m_allowUA) {
            oss << " (no UA support)";
        }
        if (m_expectedCreatedImageFormat != m_imageFormat) {
            oss << " (expected to be created as " << to_hex(m_expectedCreatedImageFormat) << ")";
        }
        return oss.str();
    }

}  // namespace Conformance
