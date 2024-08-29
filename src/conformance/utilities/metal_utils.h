// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include <openxr/openxr.h>
#include <simd/simd.h>
#include "common/xr_linear.h"

namespace Conformance
{

    simd::float4x4 LoadXrMatrixToMetal(const XrMatrix4x4f& matrix);

}  // namespace Conformance

#endif
