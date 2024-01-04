// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>
#include <string>

namespace Conformance
{
    struct IntentExtrasData
    {
        bool shouldAddXmlOutput = true;
        std::string xmlFilename{"openxr_conformance.xml"};
        std::vector<std::string> arguments;
    };
    IntentExtrasData parseIntentExtras(void* vm, void* activity);
}  // namespace Conformance
