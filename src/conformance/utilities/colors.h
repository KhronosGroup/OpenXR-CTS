// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>

#include <array>
#include <cmath>

namespace Conformance
{
    namespace ColorUtils
    {
        inline double ToSRGB(double linear)
        {
            if (linear < 0.04045 / 12.92)
                return linear * 12.92;
            else
                return 1.055 * std::pow(linear, (1.0 / 2.4)) - 0.055;
        }
        inline XrColor4f ToSRGB(XrColor4f linear)
        {
            return XrColor4f{(float)ToSRGB(linear.r), (float)ToSRGB(linear.g), (float)ToSRGB(linear.b), (float)ToSRGB(linear.a)};
        }

        inline double FromSRGB(double srgb)
        {
            if (srgb < 0.04045)
                return srgb / 12.92;
            return std::pow((srgb + .055) / 1.055, 2.4);
        }
        inline XrColor4f FromSRGB(XrColor4f srgb)
        {
            return XrColor4f{(float)FromSRGB(srgb.r), (float)FromSRGB(srgb.g), (float)FromSRGB(srgb.b), (float)FromSRGB(srgb.a)};
        }
    }  // namespace ColorUtils

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
