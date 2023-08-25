// Copyright (c) 2019-2023, The Khronos Group Inc.
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

        static const XrVisibilityMaskKHR visibilityMask{XR_TYPE_VISIBILITY_MASK_KHR};
        static const auto data = TwoCallStruct(visibilityMask,
                                               CapacityInputCountOutput(NAME_AND_MEMPTR(XrVisibilityMaskKHR::indexCapacityInput),
                                                                        NAME_AND_MEMPTR(XrVisibilityMaskKHR::indexCountOutput))
                                                   .Array(NAME_AND_MEMPTR(XrVisibilityMaskKHR::indices)),
                                               CapacityInputCountOutput(NAME_AND_MEMPTR(XrVisibilityMaskKHR::vertexCapacityInput),
                                                                        NAME_AND_MEMPTR(XrVisibilityMaskKHR::vertexCountOutput))
                                                   .Array(NAME_AND_MEMPTR(XrVisibilityMaskKHR::vertices)));
        return data;
    }

    /// Get the two-call-struct metadata for XrControllerModelPropertiesMSFT
    static inline auto getTwoCallStructData(const XrControllerModelPropertiesMSFT&)
    {
        static const XrControllerModelPropertiesMSFT modelProperties{XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT};
        static const auto data = TwoCallStruct(
            modelProperties,
            CapacityInputCountOutput(NAME_AND_MEMPTR(XrControllerModelPropertiesMSFT::nodeCapacityInput),
                                     NAME_AND_MEMPTR(XrControllerModelPropertiesMSFT::nodeCountOutput))
                .Array(NAME_AND_MEMPTR(XrControllerModelPropertiesMSFT::nodeProperties), {XR_TYPE_CONTROLLER_MODEL_NODE_PROPERTIES_MSFT}));
        return data;
    }

    /// Get the two-call-struct metadata for XrControllerModelStateMSFT
    static inline auto getTwoCallStructData(const XrControllerModelStateMSFT&)
    {
        static const XrControllerModelStateMSFT modelState{XR_TYPE_CONTROLLER_MODEL_STATE_MSFT};
        static const auto data = TwoCallStruct(
            modelState, CapacityInputCountOutput(NAME_AND_MEMPTR(XrControllerModelStateMSFT::nodeCapacityInput),
                                                 NAME_AND_MEMPTR(XrControllerModelStateMSFT::nodeCountOutput))
                            .Array(NAME_AND_MEMPTR(XrControllerModelStateMSFT::nodeStates), {XR_TYPE_CONTROLLER_MODEL_NODE_STATE_MSFT}));
        return data;
    }

#undef NAME_AND_MEMPTR

    /// Get the two-call-struct metadata for the type parameter.
    template <typename T>
    static inline auto getTwoCallStructData()
    {
        return getTwoCallStructData(std::remove_const_t<std::remove_cv_t<T>>{});
    }
}  // namespace Conformance
