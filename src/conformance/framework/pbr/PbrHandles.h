// Copyright 2019-2023, The Khronos Group, Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "framework/graphics_plugin_impl_helpers.h"

#include "../graphics_plugin_impl_helpers.h"

#include <nonstd/type.hpp>

#include <limits>

namespace Pbr
{

    namespace detail
    {
        // Policy to default-init to max so we can tell that a "null" handle is bad.
        // Otherwise, a default-init would be 0 which is often a perfectly fine index.
        // using custom_default_max_uint16 = nonstd::custom_default_t<uint16_t, std::numeric_limits<uint16_t>::max()>;

        // using custom_default_max_uint32 = nonstd::custom_default_t<uint32_t, std::numeric_limits<uint32_t>::max()>;
        using custom_default_max_uint64 = nonstd::custom_default_t<uint64_t, std::numeric_limits<uint64_t>::max()>;
    }  // namespace detail

    using MaterialHandle = nonstd::equality<uint64_t, struct MaterialHandleTag, detail::custom_default_max_uint64>;

    template <typename MaterialType>
    using MaterialCollection = Conformance::VectorWithGenerationCountedHandles<MaterialType, MaterialHandle>;

    using PrimitiveHandle = nonstd::equality<uint64_t, struct PrimitiveTag, detail::custom_default_max_uint64>;
    // static_assert(sizeof(PrimitiveHandle) == sizeof(uint64_t), "The handle should be 64 bit");
    template <typename PrimitiveType>
    using PrimitiveCollection = Conformance::VectorWithGenerationCountedHandles<PrimitiveType, PrimitiveHandle>;

}  // namespace Pbr
