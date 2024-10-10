// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#define TINYGLTF_IMPLEMENTATION

#if defined(_MSC_VER)
#pragma warning(disable : 4018)  // signed/unsigned mismatch
#pragma warning(disable : 4189)  // local variable is initialized but not referenced
#endif                           // defined(_MSC_VER)

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif

#include "cts_tinygltf.h"
