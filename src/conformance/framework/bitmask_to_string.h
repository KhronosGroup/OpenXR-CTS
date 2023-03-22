// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "conformance_framework.h"

#include <initializer_list>
#include <string>
#include <utility>
#include "openxr/openxr.h"

namespace Conformance
{
/// This is intended as an X-Macro for those flag/bitmask types that we should generate a wrapper for,
/// for formatted output in Catch2
/// See the bottom of this file for details.
#define XRC_FOR_EACH_WRAPPED_BITMASK_TYPE(_) \
    _(XrSwapchainCreateFlags)                \
    _(XrSwapchainUsageFlags)                 \
    _(XrCompositionLayerFlags)               \
    _(XrViewStateFlags)                      \
    _(XrSpaceLocationFlags)

    namespace detail
    {
        using BitNamePair = std::pair<uint64_t, const char *>;

        /// Given a bitmask value and a list of (bit name, bit value) pairs, format the bitmask as bitwise OR of bit values (or 0)
        std::string BitmaskToStringImpl(uint64_t value, const std::initializer_list<BitNamePair> &bits);

        // Define tag types for the flags, as well as an overload of BitmaskToString that is selected based on the tag type.
#define XRC_DEFINE_BIT_NAME_PAIR(FLAG) \
    struct FLAG##Tag                   \
    {                                  \
    };                                 \
    std::string BitmaskToString(XrFlags64 val, const FLAG##Tag &);

        XRC_FOR_EACH_WRAPPED_BITMASK_TYPE(XRC_DEFINE_BIT_NAME_PAIR)  // NOLINT(cert-err58-cpp)

#undef XRC_DEFINE_BIT_NAME_PAIR
    }  // namespace detail

    /// Base type that wraps a bitmask flag value so that Catch2 can output a formatted version.
    ///
    /// We can't just write a FormatString implementation for the flags types since they are all typedefs
    /// of the same underlying type. Similarly, we can't parameterize this wrapper using only the flag type since
    /// they are all the same.
    ///
    /// Below, we generate a function to instantiate this for every bitmask type we want to handle this way,
    /// by just appending CPP to the end of the Flags name.
    template <typename Tag, typename FlagsType = XrFlags64>
    struct BitmaskWrapper
    {
        FlagsType value;

        explicit BitmaskWrapper(FlagsType val) : value(val)
        {
        }

        std::string ToString() const
        {
            return detail::BitmaskToString(value, Tag{});
        }

        operator FlagsType() const
        {
            return value;
        }
    };

    /// Base type that wraps a reference to a bitmask flag value, so that Catch2 can output a formatted version.
    ///
    /// We can't just write a FormatString implementation for the flags types since they are all typedefs
    /// of the same underlying type. Similarly, we can't parameterize this wrapper using only the flag type since
    /// they are all the same.
    ///
    /// Below, we generate a function to instantiate this for every bitmask type we want to handle this way,
    /// by just appending RefCPP to the end of the Flags name.
    template <typename Tag, typename FlagsType = XrFlags64>
    struct BitmaskRefWrapper
    {
        std::reference_wrapper<FlagsType> ref;

        typedef BitmaskWrapper<Tag, FlagsType> ValueWrapper;

        explicit BitmaskRefWrapper(FlagsType &val) : ref(std::ref(val))
        {
        }

        /// Access the referenced value as a reference
        FlagsType &Get()
        {
            return ref.get();
        }

        /// Access the referenced value as a reference to const
        const FlagsType &Get() const
        {
            return ref.get();
        }

        /// Access the referenced value as wrapped in @ref BitmaskWrapper
        ValueWrapper GetWrapped() const
        {
            return ValueWrapper(ref.get());
        }

        /// Transparently wrap assignment from a flag value
        const BitmaskRefWrapper &operator=(const FlagsType &newVal) const  // NOLINT(misc-unconventional-assign-operator)
        {
            ref.get() = newVal;
            return *this;
        }

        /// Transparently wrap assignment from a wrapped flag value
        const BitmaskRefWrapper &operator=(ValueWrapper newVal) const  // NOLINT(misc-unconventional-assign-operator)
        {
            ref.get() = newVal.value;
            return *this;
        }

        /// Format the value currently referenced as a string
        std::string ToString() const
        {
            return GetWrapped().ToString();
        }
    };

    /// Overloaded output stream operator for bitmask reference wrapper
    template <typename Os, typename Tag, typename FlagsType>
    static inline Os &operator<<(Os &stream, const BitmaskRefWrapper<Tag, FlagsType> &wrapped)
    {
        stream << wrapped.ToString();
        return stream;
    }

    /// Overloaded output stream operator for bitmask value wrapper
    template <typename Os, typename Tag, typename FlagsType>
    static inline Os &operator<<(Os &stream, const BitmaskWrapper<Tag, FlagsType> &wrapped)
    {
        stream << wrapped.ToString();
        return stream;
    }

    /*
For the Xr...Flags types described in XRC_FOR_EACH_WRAPPED_BITMASK_TYPE:

- Generate a function that can wrap a value of that type, whose name is the Flags type + "CPP"
  Use to wrap a value Catch2 is seeing and capturing somehow.
- Generate a function that can wrap a reference of that type, whose name is the Flags type + "RefCPP".
  Use on the left side of an assignment that Catch2 is capturing.

For example,

```
CAPTURE(XrSwapchainCreateFlagsRefCPP(createInfo.createFlags) = cf);
```

The result of both of these functions can be captured and formatted in a human-readable way by Catch2.
*/

#define XRC_MAKE_BITMASK_WRAPPER_DECLARATIONS(FLAG)                                  \
    /** wrap a bitmask value so it can be formatted by Catch2 */                     \
    static inline BitmaskWrapper<detail::FLAG##Tag, FLAG> FLAG##CPP(FLAG val)        \
    {                                                                                \
        return BitmaskWrapper<detail::FLAG##Tag, FLAG>{val};                         \
    }                                                                                \
    /** wrap a bitmask reference so its value can be formatted by Catch2 */          \
    static inline BitmaskRefWrapper<detail::FLAG##Tag, FLAG> FLAG##RefCPP(FLAG &val) \
    {                                                                                \
        return BitmaskRefWrapper<detail::FLAG##Tag, FLAG>{val};                      \
    }

    XRC_FOR_EACH_WRAPPED_BITMASK_TYPE(XRC_MAKE_BITMASK_WRAPPER_DECLARATIONS)

#undef XRC_MAKE_BITMASK_WRAPPER_DECLARATIONS

}  // namespace Conformance
