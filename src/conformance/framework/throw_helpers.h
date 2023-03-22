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

#pragma once

#include "utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"

#include <string>
#include <stdexcept>

#include <openxr/openxr.h>

#ifdef XR_USE_PLATFORM_WIN32
// Mostly for the cautious windows.h include
#include "xr_dependencies.h"
#endif

#ifdef XR_USE_PLATFORM_ANDROID
#include <android/log.h>
#include <stdlib.h>
#endif  // XR_USE_PLATFORM_ANDROID

namespace Conformance
{
    /**
     * @defgroup cts_throw Exception-throwing helpers
     * @brief Helpers for when you can't use a Catch2 macro.
     *
     * Code in the framework itself should generally use exceptions, rather than assertion macros, for thread safety.
     *
     * If a helper in the framework wants to report an error, and it might be called from something other than the "main" thread,
     * it must throw an exception rather than using a CHECK/REQUIRE macro, as use of Catch2 test state from multiple threads is undefined.
     *
     * Code directly in the conformance test should use CHECK/REQUIRE macros or a macro from @ref cts_assert_macros unless there is some reason why they cannot.
     *
     */
    /// @{

    [[noreturn]] inline void Throw(std::string failureMessage, const char* originator = nullptr, const char* sourceLocation = nullptr)
    {
        if (originator != nullptr) {
            failureMessage += StringSprintf("\n    Origin: %s", originator);
        }

        if (sourceLocation != nullptr) {
            failureMessage += StringSprintf("\n    Source: %s", sourceLocation);
        }
#ifdef XR_USE_PLATFORM_ANDROID
        // write to the log too
        __android_log_write(ANDROID_LOG_ERROR, "OpenXR_Conformance_Throw", failureMessage.c_str());
#endif
        throw std::logic_error(failureMessage);
    }

#define XRC_THROW(msg) ::Conformance::Throw(msg, nullptr, XRC_FILE_AND_LINE);

#define XRC_CHECK_THROW(exp)                                \
    {                                                       \
        if (!(exp)) {                                       \
            Throw("Check failed", #exp, XRC_FILE_AND_LINE); \
        }                                                   \
    }

#define XRC_CHECK_THROW_MSG(exp, msg)            \
    {                                            \
        if (!(exp)) {                            \
            Throw(msg, #exp, XRC_FILE_AND_LINE); \
        }                                        \
    }

    [[noreturn]] inline void ThrowXrResult(XrResult res, const char* originator = nullptr,
                                           const char* sourceLocation = nullptr) noexcept(false)
    {
        Throw(StringSprintf("XrResult failure [%s]", ResultToString(res)), originator, sourceLocation);
    }

    inline XrResult CheckThrowXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) noexcept(false)
    {
        if (XR_FAILED(res)) {
            ThrowXrResult(res, originator, sourceLocation);
        }

        return res;
    }

    inline XrResult CheckThrowXrResultUnqualifiedSuccess(XrResult res, const char* originator = nullptr,
                                                         const char* sourceLocation = nullptr) noexcept(false)
    {
        if (!XR_UNQUALIFIED_SUCCESS(res)) {
            ThrowXrResult(res, originator, sourceLocation);
        }

        return res;
    }

    static inline XrResult CheckThrowXrResultSuccessOrLimitReached(XrResult res, const char* originator = nullptr,
                                                                   const char* sourceLocation = nullptr) noexcept(false)
    {
        if (XR_FAILED(res) && res != XR_ERROR_LIMIT_REACHED) {
            Throw(StringSprintf("XrResult failure (and not XR_ERROR_LIMIT_REACHED) [%s]", ResultToString(res)), originator, sourceLocation);
        }
        return res;
    }

#define XRC_THROW_XRRESULT(xr, cmd) ::Conformance::ThrowXrResult(xr, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_XRCMD(cmd) ::Conformance::CheckThrowXrResult(cmd, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_XRCMD_UNQUALIFIED_SUCCESS(cmd) ::Conformance::CheckThrowXrResultUnqualifiedSuccess(cmd, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_XRRESULT(res, cmdStr) ::Conformance::CheckThrowXrResult(res, cmdStr, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_XRRESULT_SUCCESS_OR_LIMIT_REACHED(res, cmdStr) \
    ::Conformance::CheckThrowXrResultSuccessOrLimitReached(res, cmdStr, XRC_FILE_AND_LINE);

#if defined(_WIN32) || defined(XRC_DOXYGEN)

    [[noreturn]] inline void ThrowHResult(HRESULT hr, const char* originator = nullptr,
                                          const char* sourceLocation = nullptr) noexcept(false)
    {
        Throw(StringSprintf("HRESULT failure [%x]", hr), originator, sourceLocation);
    }

    inline HRESULT CheckThrowHResult(HRESULT hr, const char* originator = nullptr, const char* sourceLocation = nullptr) noexcept(false)
    {
        if (FAILED(hr)) {
            ThrowHResult(hr, originator, sourceLocation);
        }

        return hr;
    }

#define XRC_THROW_HR(hr, cmd) ::Conformance::ThrowHResult(hr, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_HRCMD(cmd) ::Conformance::CheckThrowHResult(cmd, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_HRESULT(res, cmdStr) ::Conformance::CheckThrowHResult(res, cmdStr, XRC_FILE_AND_LINE);

#endif  // defined(_WIN32) || defined(XRC_DOXYGEN)

    /// @}

}  // namespace Conformance
