// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>

#include <string>

/// Equality comparison for UUIDs
bool operator==(const XrUuidEXT& lhs, const XrUuidEXT& rhs);

/// Inequality comparison for UUIDs
bool operator!=(const XrUuidEXT& lhs, const XrUuidEXT& rhs);

/// Convert UUIDs to strings
std::string to_string(const XrUuidEXT& uuid);
