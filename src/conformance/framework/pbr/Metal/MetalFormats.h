// Copyright 2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include <utilities/image.h>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <unordered_map>

namespace Pbr
{
    bool IsKnownFormatSupportedByDriver(MTL::Device* device, MTL::PixelFormat format);
    MTL::PixelFormat ToMetalFormat(Conformance::Image::FormatParams format, bool throwIfNotFound = true);
    const std::unordered_map<Conformance::Image::FormatParams, MTL::PixelFormat, Conformance::Image::FormatParamsHash>& GetMetalFormatMap();
}  // namespace Pbr
