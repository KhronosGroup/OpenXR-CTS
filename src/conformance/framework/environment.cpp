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

#endif

namespace Conformance
{

#if defined(XR_OS_LINUX) || defined(XR_OS_APPLE)

    bool SetEnv(const char* name, const char* value, bool shouldOverride)
    {
        const int overwrite = shouldOverride ? 1 : 0;
        int result = setenv(name, value, overwrite);
        return (result == 0);
    }

#elif defined(XR_OS_WINDOWS)

    bool SetEnv(const char* name, const char* value, bool shouldOverride)
    {
        if (!shouldOverride) {
            const DWORD valSize = ::GetEnvironmentVariableA(name, nullptr, 0);
            // GetEnvironmentVariable returns 0 when environment variable does not exist or there is an error.
            if (valSize != 0) {
                return true;
            }
        }

        BOOL result = ::SetEnvironmentVariableA(name, value);
        return (result != 0);
    }

#elif defined(XR_OS_ANDROID)

    bool SetEnv(const char* /* name */, const char* /* value */, bool /* shouldOverride */)
    {
        // Stub func
        return false;
    }

#else
#error "Port needed"
#endif
}  // namespace Conformance
