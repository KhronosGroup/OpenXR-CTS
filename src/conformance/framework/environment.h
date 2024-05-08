// Copyright (c) 2017-2024, The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace Conformance
{
    /// Sets an environment variable via ascii strings.
    /// The name is case-sensitive.
    /// @return true if it could be set (or was already set)
    ///
    bool SetEnv(const char* name, const char* value, bool shouldOverride);
}  // namespace Conformance
