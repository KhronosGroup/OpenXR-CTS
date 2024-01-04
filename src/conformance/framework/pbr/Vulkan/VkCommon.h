// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "utilities/vulkan_scoped_handle.h"

namespace Pbr
{
    struct VulkanTextureBundle
    {
        uint32_t width, height;
        uint32_t mipLevels;
        uint32_t layerCount;
        Conformance::ScopedVkImage image{};
        VkImageLayout imageLayout{};
        Conformance::ScopedVkImageView view{};
        Conformance::ScopedVkDeviceMemory deviceMemory{};
    };
}  // namespace Pbr
