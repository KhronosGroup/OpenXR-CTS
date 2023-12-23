// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>

#include <array>

namespace Conformance
{
    namespace Colors
    {

        constexpr XrColor4f Red = {1, 0, 0, 1};
        constexpr XrColor4f Green = {0, 1, 0, 1};
        constexpr XrColor4f GreenZeroAlpha = {0, 1, 0, 0};
        constexpr XrColor4f Blue = {0, 0, 1, 1};
        constexpr XrColor4f Yellow = {1, 1, 0, 1};
        constexpr XrColor4f Orange = {1, 0.65f, 0, 1};
        constexpr XrColor4f Magenta = {1, 0, 1, 1};
        constexpr XrColor4f Transparent = {0, 0, 0, 0};
        constexpr XrColor4f Black = {0, 0, 0, 1};

        /// A list of unique colors, not including red which is a "failure color".
        constexpr std::array<XrColor4f, 4> UniqueColors{Green, Blue, Yellow, Orange};

    }  // namespace Colors
}  // namespace Conformance
