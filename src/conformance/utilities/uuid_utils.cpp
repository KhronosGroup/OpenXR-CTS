// Copyright (c) 2019-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "uuid_utils.h"

#include "common/hex_and_handles.h"

#include <openxr/openxr.h>

#include <stdint.h>
#include <sstream>

bool operator==(const XrUuidEXT& lhs, const XrUuidEXT& rhs)
{
    return lhs.data[0] == rhs.data[0] && lhs.data[1] == rhs.data[1] && lhs.data[2] == rhs.data[2] && lhs.data[3] == rhs.data[3] &&
           lhs.data[4] == rhs.data[4] && lhs.data[5] == rhs.data[5] && lhs.data[6] == rhs.data[6] && lhs.data[7] == rhs.data[7] &&
           lhs.data[8] == rhs.data[8] && lhs.data[9] == rhs.data[9] && lhs.data[10] == rhs.data[10] && lhs.data[11] == rhs.data[11] &&
           lhs.data[12] == rhs.data[12] && lhs.data[13] == rhs.data[13] && lhs.data[14] == rhs.data[14] && lhs.data[15] == rhs.data[15];
}

bool operator!=(const XrUuidEXT& lhs, const XrUuidEXT& rhs)
{
    return !(lhs == rhs);
}

std::string to_string(const XrUuidEXT& uuid)
{
    std::ostringstream oss;
    // 8-4-4-4-12 format
    // each byte is two digits
    // accessing the bare impl of to_hex to drop the leading 0x
    oss << to_hex(&uuid.data[0], 1, false);
    oss << to_hex(&uuid.data[1], 1, false);
    oss << to_hex(&uuid.data[2], 1, false);
    oss << to_hex(&uuid.data[3], 1, false);
    oss << '-';
    oss << to_hex(&uuid.data[4], 1, false);
    oss << to_hex(&uuid.data[5], 1, false);
    oss << '-';
    oss << to_hex(&uuid.data[6], 1, false);
    oss << to_hex(&uuid.data[7], 1, false);
    oss << '-';
    oss << to_hex(&uuid.data[8], 1, false);
    oss << to_hex(&uuid.data[9], 1, false);
    oss << '-';
    oss << to_hex(&uuid.data[10], 1, false);
    oss << to_hex(&uuid.data[11], 1, false);
    oss << to_hex(&uuid.data[12], 1, false);
    oss << to_hex(&uuid.data[13], 1, false);
    oss << to_hex(&uuid.data[14], 1, false);
    oss << to_hex(&uuid.data[15], 1, false);
    return oss.str();
}
