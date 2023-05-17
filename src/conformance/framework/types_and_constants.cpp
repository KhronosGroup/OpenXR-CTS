// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <catch2/catch_test_macros.hpp>
#include "utils.h"
#include "report.h"
#include "two_call_util.h"
#include "types_and_constants.h"
#include "conformance_framework.h"
#include "xr_dependencies.h"
#include "openxr/openxr_platform.h"
#include "openxr/openxr_reflection.h"
#include "graphics_plugin.h"

#include <map>
#include <algorithm>
#include <assert.h>
#include <cstring>
#include <time.h>
#include <sstream>
#include <iomanip>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _MSC_VER
#pragma warning(disable : 4312)  // <function-style-cast>': conversion from 'int' to 'XrInstance' of greater size)
#endif

#ifdef Success
#undef Success  // Some platforms #define Success as 0, breaking its usage as an enumerant.
#endif

namespace Conformance
{
    constexpr size_t HEX_DIGITS_FOR_HANDLE = 8;

    std::ostream& operator<<(std::ostream& os, NullHandleType const& /*unused*/)
    {
        os << "XR_NULL_HANDLE";
        return os;
    }

    template <typename T>
    static inline void OutputHandle(std::ostream& os, T handle)
    {
        if (handle == XR_NULL_HANDLE) {
            os << "XR_NULL_HANDLE";
        }
        else {
            std::ostringstream oss;
            oss << "0x" << std::hex << std::setw(HEX_DIGITS_FOR_HANDLE) << std::setfill('0');
#if XR_PTR_SIZE == 8
            oss << reinterpret_cast<uint64_t>(handle);
#else
            oss << static_cast<uint64_t>(handle);
#endif
            os << oss.str();
        }
    }

    std::ostream& operator<<(std::ostream& os, AutoBasicInstance const& inst)
    {
        OutputHandle(os, inst.GetInstance());
        return os;
    }

    std::ostream& operator<<(std::ostream& os, AutoBasicSession const& sess)
    {
        OutputHandle(os, sess.GetSession());
        return os;
    }

}  // namespace Conformance
