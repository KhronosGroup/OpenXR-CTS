// Copyright (c) 2019-2026 The Khronos Group Inc.
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
        inline float ToSRGB(float linear)
        {
            if (linear < 0.04045f / 12.92f) {
                return linear * 12.92f;
            }
            else {
                return 1.055f * std::pow(linear, (1.0f / 2.4f)) - 0.055f;
            }
        }
        inline XrColor4f ToSRGB(XrColor4f linear)
        {
            return XrColor4f{ToSRGB(linear.r), ToSRGB(linear.g), ToSRGB(linear.b), linear.a};
        }

        inline float FromSRGB(float srgb)
        {
            if (srgb < 0.04045f) {
                return srgb / 12.92f;
            }
            return std::pow((srgb + .055f) / 1.055f, 2.4f);
        }
        inline XrColor4f FromSRGB(XrColor4f srgb)
        {
            return XrColor4f{FromSRGB(srgb.r), FromSRGB(srgb.g), FromSRGB(srgb.b), srgb.a};
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
        constexpr XrColor4f Gray = {0.5f, 0.5f, 0.5f, 1};

        /// A list of unique colors, not including red which is a "failure color".
        constexpr std::array<XrColor4f, 6> UniqueColors{{Green, Blue, Yellow, Orange, Magenta, Gray}};

    }  // namespace Colors
}  // namespace Conformance
