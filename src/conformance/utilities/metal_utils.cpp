// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "metal_utils.h"

namespace Conformance
{

    simd::float4x4 LoadXrMatrixToMetal(const XrMatrix4x4f& matrix)
    {
        // Metal uses column-major matrix which is the same as XrMatrix4x4f
        return (simd::float4x4&)matrix;
    }

}  // namespace Conformance

#endif
