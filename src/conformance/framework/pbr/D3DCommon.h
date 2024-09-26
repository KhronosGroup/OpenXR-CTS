// Copyright 2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include <utilities/image.h>

#include <dxgiformat.h>
#include <d3d11.h>

#include <unordered_map>

namespace Pbr
{
    DXGI_FORMAT ToDXGIFormat(Conformance::Image::FormatParams format, bool throwIfNotFound = true);
    const std::unordered_map<Conformance::Image::FormatParams, DXGI_FORMAT, Conformance::Image::FormatParamsHash>& GetDXGIFormatMap();
}  // namespace Pbr
