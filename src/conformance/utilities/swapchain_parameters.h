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

#include <vector>
#include <string>
#include <cstdint>

namespace Conformance
{
    namespace SwapchainFormat
    {
        namespace Flags
        {
            enum Mutability
            {
                IMMUTABLE,
                MUTABLE,
            };
            enum SupportsMutability
            {
                NO_MUT_SUPPORT,
                MUT_SUPPORT,
            };
            enum IsColor
            {
                NON_COLOR,
                COLOR,
            };
            enum IsCompressed
            {
                UNCOMPRESSED,
                COMPRESSED,
            };
            enum SupportsRendering
            {
                NO_RENDERING_SUPPORT,
                RENDERING_SUPPORT,
            };
        }  // namespace Flags

        /// The components defined by the texture during sampling
        /// (i.e. not just returned as the default value)
        enum RawColorComponents : uint8_t
        {
            Unknown = 0,
            r = 1 << 0,
            g = 1 << 1,
            b = 1 << 2,
            a = 1 << 3,
        };

        /// Textures whose output is interpreted as an integer
        /// and not mapped to a fixed- or floating-point value.
        /// This does not yet account for integer aspects of
        /// non-color formats, but could be renamed and extended.
        enum ColorIntegerRange
        {
            NoIntegerColor = 0,
            u8,
            s8,
            u16,
            s16,
            u32,
            s32,
            uRGB10A2,
        };
        uint8_t ColorIntegerRangeBits(ColorIntegerRange colorIntegerRange);
        bool ColorIntegerRangeIsSigned(ColorIntegerRange colorIntegerRange);
    }  // namespace SwapchainFormat

    /// Defines XrSwapchainCreateInfo test parameters for a single given image format.
    /// Sometimes values are zeroed, for the case that use of them is invalid or unsupportable.
    ///
    /// @ingroup cts_framework
    struct SwapchainCreateTestParameters
    {
        /// String-ified version of the C identifier.
        std::string imageFormatName;

        /// Whether the image format is a mutable (a.k.a. typeless) type.
        SwapchainFormat::Flags::Mutability mutableFormat;

        /// Whether the image format supports creation with XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT.
        SwapchainFormat::Flags::SupportsMutability supportsMutableFormat;

        /// Whether the format is a color-specific format, as opposed to a depth-specific format.
        SwapchainFormat::Flags::IsColor colorFormat;

        /// Whether the format is a compressed format.
        SwapchainFormat::Flags::IsCompressed compressedFormat;

        /// Whether the image format can be rendered to.
        SwapchainFormat::Flags::SupportsRendering supportsRendering;

        /// The graphics-specific created image format returned by xrCreateSwapchain, may be different from imageFormat in some cases.
        int64_t expectedCreatedImageFormat;

        /// The color components that, when sampled, will not just be set to default values.
        SwapchainFormat::RawColorComponents colorComponents;

        /// For integer (not floating point or normalised) color images, the bit depth of each color/alpha component.
        SwapchainFormat::ColorIntegerRange colorIntegerRange;

        /// XrSwapchainUsageFlags to exercise for this format.
        std::vector<uint64_t> usageFlagsVector;

        /// XrSwapchainCreateFlags
        std::vector<uint64_t> createFlagsVector;

        /// Array values to exercise, with 1 meaning no array in OpenXR.
        std::vector<uint32_t> arrayCountVector;

        /// Used only by color buffers.
        std::vector<uint32_t> sampleCountVector;

        /// Used only by color buffers.
        std::vector<uint32_t> mipCountVector;

        /// Is this format usable as a depth buffer?
        bool useAsDepth = false;

        /// Is this format usable as a stencil buffer?
        bool useAsStencil = false;
    };

}  // namespace Conformance
