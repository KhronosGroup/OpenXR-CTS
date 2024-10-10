// Copyright 2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include <utilities/image.h>

#include <vulkan/vulkan_core.h>

#include <unordered_map>

namespace Pbr
{
    VkFormat ToVkFormat(Conformance::Image::FormatParams format, bool throwIfNotFound = true);
    const std::unordered_map<Conformance::Image::FormatParams, VkFormat, Conformance::Image::FormatParamsHash>& GetVkFormatMap();
}  // namespace Pbr
