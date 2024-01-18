// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "bitmask_to_string.h"

#include "common/hex_and_handles.h"

#include <openxr/openxr_reflection.h>

#include <cstdint>
#include <sstream>
#include <string>

namespace Conformance
{
    namespace detail
    {

#define XRC_STRINGIFY_FLAG_BITS(FLAG, VAL) {FLAG, #FLAG},

#define XRC_DEFINE_BIT_NAME_PAIR(FLAG)                                                   \
    std::string BitmaskToString(XrFlags64 val, const FLAG##Tag &)                        \
    {                                                                                    \
        return BitmaskToStringImpl(val, {XR_LIST_BITS_##FLAG(XRC_STRINGIFY_FLAG_BITS)}); \
    }

        XRC_FOR_EACH_WRAPPED_BITMASK_TYPE(XRC_DEFINE_BIT_NAME_PAIR)  // NOLINT(cert-err58-cpp)

#undef XRC_DEFINE_BIT_NAME_PAIR
#undef XRC_STRINGIFY_FLAG_BITS

        std::string BitmaskToStringImpl(uint64_t value, const std::initializer_list<BitNamePair> &bits)
        {
            if (value == 0) {
                return "0";
            }
            bool hadFirst = false;
            std::ostringstream os;
            for (const auto &bitnamepair : bits) {
                if ((value & bitnamepair.first) != 0) {
                    if (hadFirst) {
                        os << " | ";
                    }
                    os << bitnamepair.second;

                    // clear that bit in the value
                    value &= ~bitnamepair.first;

                    hadFirst = true;
                }
            }
            if (value != 0) {
                // we had leftover bits
                if (hadFirst) {
                    os << " | ";
                }
                os << to_hex(value);
            }
            return os.str();
        }

    }  // namespace detail

}  // namespace Conformance
