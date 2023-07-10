// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace Conformance
{
    /// Rounds up the parameter size @p n to be a multiple of @p alignment
    template <std::uint32_t alignment>
    constexpr std::uint32_t AlignTo(std::uint32_t n)
    {
        static_assert((alignment & (alignment - 1)) == 0, "The alignment must be power-of-two");
        // Add one less than the alignment: this will give us the largest possible value that we might need to round to.
        // Then, mask off the bits that are lower order than the alignment.
        return (n + alignment - 1) & ~(alignment - 1);
    }
}  // namespace Conformance
