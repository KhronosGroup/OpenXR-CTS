// Copyright (c) 2019-2024, The Khronos Group Inc.
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

#include "types_and_constants.h"

#include <stddef.h>
#include <sstream>
#include <iomanip>
#include <string>

namespace Conformance
{
    constexpr size_t HEX_DIGITS_FOR_HANDLE = 8;

    std::ostream& operator<<(std::ostream& os, NullHandleType const& /*unused*/)
    {
        os << "XR_NULL_HANDLE";
        return os;
    }
    namespace detail
    {
        void OutputHandle(std::ostream& os, uint64_t handle)
        {
            if (handle == 0) {
                os << "XR_NULL_HANDLE";
            }
            else {
                std::ostringstream oss;
                oss << "0x" << std::hex << std::setw(HEX_DIGITS_FOR_HANDLE) << std::setfill('0');
                oss << handle;
                os << oss.str();
            }
        }
    }  // namespace detail

}  // namespace Conformance
