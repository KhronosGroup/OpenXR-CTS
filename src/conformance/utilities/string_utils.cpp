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

#include "string_utils.h"
#include <cstring>

namespace Conformance
{
    // Internal implementations that actually do the work
    static bool starts_with(const char* str, size_t strLen, const char* prefix, size_t prefixLen)
    {
        if (prefixLen > strLen) {
            return false;
        }
        return 0 == strncmp(str, prefix, prefixLen);
    }
    static bool ends_with(const char* str, size_t strLen, const char* suffix, size_t suffixLen)
    {
        if (suffixLen > strLen) {
            return false;
        }
        return 0 == strncmp(str + strLen - suffixLen, suffix, suffixLen);
    }

    // Exported wrapper functions.

    bool starts_with(const char* str, const char* prefix)
    {
        return starts_with(str, strlen(str), prefix, strlen(prefix));
    }
    bool starts_with(const std::string& str, const char* prefix)
    {
        return starts_with(str.data(), str.size(), prefix, std::strlen(prefix));
    }
    bool starts_with(const std::string& str, const std::string& prefix)
    {
        return starts_with(str.data(), str.size(), prefix.data(), prefix.size());
    }

    bool ends_with(const char* str, const char* suffix)
    {
        return ends_with(str, strlen(str), suffix, strlen(suffix));
    }
    bool ends_with(const std::string& str, const char* suffix)
    {
        return ends_with(str.data(), str.size(), suffix, std::strlen(suffix));
    }
    bool ends_with(const std::string& str, const std::string& suffix)
    {
        return ends_with(str.data(), str.size(), suffix.data(), suffix.size());
    }

}  // namespace Conformance
