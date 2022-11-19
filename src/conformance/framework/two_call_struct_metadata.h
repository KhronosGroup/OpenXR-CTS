// Copyright (c) 2019-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "two_call_struct.h"
#include <openxr/openxr.h>

namespace Conformance
{

#define NAME_AND_MEMPTR(MEMBER) #MEMBER, &MEMBER

    /// Get the two-call-struct metadata for XrVisibilityMaskKHR
    static inline auto getTwoCallStructData(const XrVisibilityMaskKHR&)
    {

        XrVisibilityMaskKHR visibilityMask{XR_TYPE_VISIBILITY_MASK_KHR};
        return TwoCallStruct(visibilityMask,
                             CapacityInputCountOutput(NAME_AND_MEMPTR(XrVisibilityMaskKHR::indexCapacityInput),
                                                      NAME_AND_MEMPTR(XrVisibilityMaskKHR::indexCountOutput))
                                 .Array(NAME_AND_MEMPTR(XrVisibilityMaskKHR::indices)),
                             CapacityInputCountOutput(NAME_AND_MEMPTR(XrVisibilityMaskKHR::vertexCapacityInput),
                                                      NAME_AND_MEMPTR(XrVisibilityMaskKHR::vertexCountOutput))
                                 .Array(NAME_AND_MEMPTR(XrVisibilityMaskKHR::vertices)));
    }

#undef NAME_AND_MEMPTR

    /// Get the two-call-struct metadata for the type parameter.
    template <typename T>
    static inline auto getTwoCallStructData()
    {
        return getTwoCallStructData(std::remove_const_t<std::remove_cv_t<T>>{});
    }
}  // namespace Conformance
