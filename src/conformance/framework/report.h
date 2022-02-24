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

#pragma once

#include <stdarg.h>
#include <iostream>
#include <streambuf>
#include <functional>

namespace Conformance
{
    extern std::function<void(const char*)> g_reportCallback;

    // Direct report function.
    // May include multiple lines separated by \n.
    // This function supplies the final newline.
    void ReportStr(const char* str);

    // Formatted report function via va_list.
    // May include multiple lines separated by \n.
    // This function supplies the final newline.
    void ReportV(const char* format, va_list args);

    // Formatted report function.
    // May include multiple lines separated by \n.
    // This function supplies the final newline.
    void ReportF(const char* format, ...);
}  // namespace Conformance
