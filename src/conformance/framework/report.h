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

#pragma once

#include <stdarg.h>
#include <iostream>
#include <streambuf>
#include <functional>

namespace Conformance
{
    /**
     * @defgroup cts_report Standalone message reporters
     * @ingroup cts_framework
     */
    ///@{

    extern std::function<void(const char*)> g_reportCallback;

    /// Formatted report function.
    /// May include multiple lines separated by \n.
    /// This function supplies the final newline.
    ///
    /// @todo Any code that uses this, must be modified to output to the CTS catch2 reporter.
    ///
    /// Do not write new code that uses this function!
    void ReportF(const char* format, ...);

    /// Formatted report function, like ReportF, but for console output only (when XML report output has another way of including this data)
    void ReportConsoleOnlyF(const char* format, ...);

    /// @}

}  // namespace Conformance
