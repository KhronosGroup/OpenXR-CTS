// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>

namespace Conformance
{
    /// Example usage:
    /// ```
    /// XrDuration timeout = 10_xrSeconds;
    /// ```
    inline constexpr XrDuration operator"" _xrSeconds(unsigned long long value)
    {
        return (static_cast<int64_t>(value) * 1000 * 1000 * 1000);  // Convert seconds to XrDuration nanoseconds.
    }

    /// Example usage:
    /// ```
    /// XrDuration timeout = 10_xrMilliseconds;
    /// ```
    inline constexpr XrDuration operator"" _xrMilliseconds(unsigned long long value)
    {
        return (static_cast<int64_t>(value) * 1000 * 1000);  // Convert milliseconds to XrDuration nanoseconds.
    }

    /// Example usage:
    /// ```
    /// XrDuration timeout = 10_xrMicroseconds;
    /// ```
    inline constexpr XrDuration operator"" _xrMicroseconds(unsigned long long value)
    {
        return (static_cast<int64_t>(value) * 1000);  // Convert microseconds to XrDuration nanoseconds.
    }

    /// Example usage:
    /// ```
    /// XrDuration timeout = 10_xrNanoseconds;
    /// ```
    inline constexpr XrDuration operator"" _xrNanoseconds(unsigned long long value)
    {
        return static_cast<int64_t>(value);  // XrDuration is already in nanoseconds
    }
}  // namespace Conformance
