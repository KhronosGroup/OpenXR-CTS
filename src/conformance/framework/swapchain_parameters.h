// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#include <utils.h>
#include <vector>
#include <string>

namespace Conformance
{
    enum SwapchainFormatMutability
    {
        IMMUTABLE,
        MUTABLE,
    };
    enum SwapchainFormatSupportsMutability
    {
        NO_MUT_SUPPORT,
        MUT_SUPPORT,
    };
    enum SwapchainFormatIsColor
    {
        NON_COLOR,
        COLOR,
    };
    enum SwapchainFormatIsCompressed
    {
        UNCOMPRESSED,
        COMPRESSED,
    };

    /// Defines XrSwapchainCreateInfo test parameters for a single given image format.
    /// Sometimes values are zeroed, for the case that use of them is invalid or unsupportable.
    ///
    /// @ingroup cts_framework
    struct SwapchainCreateTestParameters
    {
        /// String-ified version of the C identifier.
        std::string imageFormatName;

        /// Whether the image format is a mutable (a.k.a. typeless) type.
        SwapchainFormatMutability mutableFormat;

        /// Whether the image format supports creation with XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT.
        SwapchainFormatSupportsMutability supportsMutableFormat;

        /// Whether the format is a color-specific format, as opposed to a depth-specific format.
        SwapchainFormatIsColor colorFormat;

        /// Whether the format is a compressed format.
        SwapchainFormatIsCompressed compressedFormat;

        /// The graphics-specific created image format returned by xrCreateSwapchain, may be different from imageFormat in some cases.
        int64_t expectedCreatedImageFormat;

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
    };

}  // namespace Conformance
