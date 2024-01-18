// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "uuid_utils.h"

#include <ios>
#include <sstream>
#include <iomanip>

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
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[0];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[1];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[2];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[3];
    oss << '-';
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[4];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[5];
    oss << '-';
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[6];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[7];
    oss << '-';
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[8];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[9];
    oss << '-';
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[10];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[11];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[12];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[13];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[14];
    oss << std::hex << std::setw(2) << std::setfill('0') << uuid.data[15];
    return oss.str();
}
