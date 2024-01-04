// Copyright (c) 2017-2024, The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "environment.h"

#if defined(XR_OS_LINUX) || defined(XR_OS_APPLE)

#include <unistd.h>
#include <fcntl.h>
#include <iostream>

#elif defined(XR_OS_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <catch2/catch_test_macros.hpp>

#endif

namespace Conformance
{

#if defined(XR_OS_LINUX) || defined(XR_OS_APPLE)

    namespace detail
    {

        static inline char* ImplGetEnv(const char* name)
        {
            return getenv(name);
        }

        static inline int ImplSetEnv(const char* name, const char* value, int overwrite)
        {
            return setenv(name, value, overwrite);
        }

    }  // namespace detail

    std::string GetEnv(const char* name)
    {
        auto str = detail::ImplGetEnv(name);
        if (str == nullptr) {
            return {};
        }
        return str;
    }

    bool GetEnvSet(const char* name)
    {
        return detail::ImplGetEnv(name) != nullptr;
    }

    bool SetEnv(const char* name, const char* value)
    {
        const int shouldOverwrite = 1;
        int result = detail::ImplSetEnv(name, value, shouldOverwrite);
        return (result == 0);
    }

#elif defined(XR_OS_WINDOWS)

    static inline std::wstring utf8_to_wide(const std::string& utf8Text)
    {
        if (utf8Text.empty()) {
            return {};
        }

        std::wstring wideText;
        const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), (int)utf8Text.size(), nullptr, 0);
        if (wideLength == 0) {
            WARN("utf8_to_wide get size error: " + std::to_string(::GetLastError()));
            return {};
        }

        // MultiByteToWideChar returns number of chars of the input buffer, regardless of null terminator
        wideText.resize(wideLength, 0);
        wchar_t* wideString = const_cast<wchar_t*>(wideText.data());  // mutable data() only exists in c++17
        const int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), (int)utf8Text.size(), wideString, wideLength);
        if (length != wideLength) {
            WARN("utf8_to_wide convert string error: " + std::to_string(::GetLastError()));
            return {};
        }

        return wideText;
    }

    static inline std::string wide_to_utf8(const std::wstring& wideText)
    {
        if (wideText.empty()) {
            return {};
        }

        std::string narrowText;
        int narrowLength = ::WideCharToMultiByte(CP_UTF8, 0, wideText.data(), (int)wideText.size(), nullptr, 0, nullptr, nullptr);
        if (narrowLength == 0) {
            WARN("wide_to_utf8 get size error: " + std::to_string(::GetLastError()));
            return {};
        }

        // WideCharToMultiByte returns number of chars of the input buffer, regardless of null terminator
        narrowText.resize(narrowLength, 0);
        char* narrowString = const_cast<char*>(narrowText.data());  // mutable data() only exists in c++17
        const int length =
            ::WideCharToMultiByte(CP_UTF8, 0, wideText.data(), (int)wideText.size(), narrowString, narrowLength, nullptr, nullptr);
        if (length != narrowLength) {
            WARN("wide_to_utf8 convert string error: " + std::to_string(::GetLastError()));
            return {};
        }

        return narrowText;
    }

    bool GetEnvSet(const char* name)
    {
        const std::wstring wname = utf8_to_wide(name);
        const DWORD valSize = ::GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
        // GetEnvironmentVariable returns 0 when environment variable does not exist or there is an error.
        return 0 != valSize;
    }

    std::string GetEnv(const char* name)
    {
        const std::wstring wname = utf8_to_wide(name);
        const DWORD valSize = ::GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
        // GetEnvironmentVariable returns 0 when environment variable does not exist or there is an error.
        // The size includes the null-terminator, so a size of 1 is means the variable was explicitly set to empty.
        if (valSize == 0 || valSize == 1) {
            return {};
        }

        // GetEnvironmentVariable returns size including null terminator for "query size" call.
        std::wstring wValue(valSize, 0);
        wchar_t* wValueData = &wValue[0];

        // GetEnvironmentVariable returns string length, excluding null terminator for "get value"
        // call if there was enough capacity. Else it returns the required capacity (including null terminator).
        const DWORD length = ::GetEnvironmentVariableW(wname.c_str(), wValueData, (DWORD)wValue.size());
        if ((length == 0) || (length >= wValue.size())) {  // If error or the variable increased length between calls...
            WARN("GetEnvironmentVariable get value error: " + std::to_string(::GetLastError()));
            return {};
        }

        wValue.resize(length);  // Strip the null terminator.

        return wide_to_utf8(wValue);
    }

    bool SetEnv(const char* name, const char* value)
    {
        const std::wstring wname = utf8_to_wide(name);
        const std::wstring wvalue = utf8_to_wide(value);
        BOOL result = ::SetEnvironmentVariableW(wname.c_str(), wvalue.c_str());
        return (result != 0);
    }

#elif defined(XR_OS_ANDROID)

    bool GetEnvSet(const char* /* name */)
    {
        // Stub func
        return false;
    }

    std::string GetEnv(const char* /* name */)
    {
        // Stub func
        return {};
    }

    bool SetEnv(const char* /* name */, const char* /* value */)
    {
        // Stub func
        return false;
    }
#else
#error "Port needed"
#endif
}  // namespace Conformance
