// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>

namespace Conformance
{
    /// The equivalent of C++17 std::size. A helper to get the dimension for an array.
    template <typename T, std::size_t Size>
    constexpr std::size_t ArraySize(const T (&unused)[Size]) noexcept
    {
        (void)unused;
        return Size;
    }
}  // namespace Conformance
