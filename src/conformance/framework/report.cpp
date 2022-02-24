// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#include "report.h"
#include <string>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef XR_USE_PLATFORM_ANDROID
#include <android/log.h>
#endif  /// XR_USE_PLATFORM_ANDROID

namespace Conformance
{
    std::function<void(const char*)> g_reportCallback;

    void ReportStr(const char* str)
    {
#if defined(_WIN32)
        OutputDebugStringA(str);
        OutputDebugStringA("\n");
#endif

        if (g_reportCallback) {
            g_reportCallback(str);
        }
    }

    void ReportV(const char* format, va_list args)
    {
        // We first try writing into this buffer. If it's not enough then use a string.
        // We have to do an initial vsnprintf in any case, so it's cheap to try into this buffer.
        char buffer[1024];

        va_list tmp_args;
        va_copy(tmp_args, args);
        const int requiredStrlen = std::vsnprintf(buffer, sizeof(buffer), format, tmp_args);
        va_end(tmp_args);

        if (requiredStrlen >= 0) {
            if (requiredStrlen < (int)sizeof(buffer)) {  // If the entire result fits into the buffer.
                ReportStr(buffer);
            }
            else {
                std::string result(requiredStrlen, '\0');
                std::vsnprintf(&result[0], result.size(), format, args);
                ReportStr(result.c_str());
            }
        }
        else {
            ReportF("Malformed printf format: \"%s\"", format);
        }
    }

    void ReportF(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        ReportV(format, args);
        va_end(args);
    }

}  // namespace Conformance
