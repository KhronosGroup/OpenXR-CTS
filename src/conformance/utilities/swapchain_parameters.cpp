// Copyright (c) 2019-2025 The Khronos Group Inc.
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

#include "swapchain_parameters.h"

#include <stdexcept>

namespace Conformance
{
    namespace SwapchainFormat
    {
        uint8_t ColorIntegerRangeBits(ColorIntegerRange colorIntegerRange)
        {
            switch (colorIntegerRange) {
            case ColorIntegerRange::NoIntegerColor:
                throw std::logic_error("ColorIntegerRangeBits only valid for integer colors");
            case ColorIntegerRange::u8:
            case ColorIntegerRange::s8:
                return 8;
            case ColorIntegerRange::u16:
            case ColorIntegerRange::s16:
                return 16;
            case ColorIntegerRange::u32:
            case ColorIntegerRange::s32:
                return 32;
            case ColorIntegerRange::uRGB10A2:
                return 10;
            default:
                throw std::logic_error("Missing case in implementation of ColorIntegerRangeBits");
            }
        }

        bool ColorIntegerRangeIsSigned(ColorIntegerRange colorIntegerRange)
        {
            switch (colorIntegerRange) {
            case ColorIntegerRange::NoIntegerColor:
                throw std::logic_error("ColorIntegerRangeIsSigned only valid for integer colors");
            case ColorIntegerRange::u8:
            case ColorIntegerRange::u16:
            case ColorIntegerRange::u32:
            case ColorIntegerRange::uRGB10A2:
                return false;
            case ColorIntegerRange::s8:
            case ColorIntegerRange::s16:
            case ColorIntegerRange::s32:
                return true;
            default:
                throw std::logic_error("Missing case in implementation of ColorIntegerRangeIsSigned");
            }
        }
    }  // namespace SwapchainFormat
}  // namespace Conformance
