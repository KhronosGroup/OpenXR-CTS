// Copyright (c) 2017-2024, The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace Conformance
{

    /// Returns the environment variable value for the given name.
    /// Returns an empty string if the environment variable doesn't exist or if it exists but is empty.
    /// Use @ref GetEnvSet to tell if it exists.
    /// The name is a case-sensitive UTF8 string.
    ///
    /// Like PlatformUtilsGetEnv
    std::string GetEnv(const char* name);

    /// Returns true if the given environment variable exists.
    /// The name is a case-sensitive UTF8 string.
    ///
    /// Like PlatformUtilsGetEnvSet
    bool GetEnvSet(const char* name);

    /// Sets an environment variable via UTF8 strings.
    /// The name is case-sensitive.
    /// Overwrites the variable if it already exists.
    /// @return true if it could be set.
    ///
    /// Like PlatformUtilsSetEnv
    bool SetEnv(const char* name, const char* value);
}  // namespace Conformance
