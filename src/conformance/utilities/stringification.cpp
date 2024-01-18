// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "stringification.h"
#include "utils.h"

#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

#include <utility>

namespace Conformance
{

    // We keep our own copy of this as opposed to calling the xrResultToString function, because our
    // purpose here it to validate the runtime's implementation of xrResultToString.

    const ResultStringMap& GetResultStringMap()
    {
        static const ResultStringMap resultStringMap{XR_LIST_ENUM_XrResult(XRC_ENUM_NAME_PAIR)};
        return resultStringMap;
    }

    const char* ResultToString(XrResult result)
    {
        auto it = GetResultStringMap().find(result);

        if (it != GetResultStringMap().end()) {
            return it->second;
        }

        return "<unknown>";
    }
}  // namespace Conformance
