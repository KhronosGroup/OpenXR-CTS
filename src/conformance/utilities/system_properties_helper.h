// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>
#include <type_traits>
#include "throw_helpers.h"

namespace Conformance
{
    /// A generic wrapper for xrGetSystemProperties that returns only a single member of an extension struct.
    ///
    /// Mainly for use by @ref SystemPropertiesChecker and @ref SystemPropertiesBoolChecker
    template <typename SysPropsExtStruct, typename MemberType>
    static inline MemberType GetSystemPropertiesValue(const SysPropsExtStruct& emptyExtStruct,
                                                      MemberType SysPropsExtStruct::*pointerToMember, XrInstance instance,
                                                      XrSystemId systemId)
    {
        SysPropsExtStruct extSystemProperties{emptyExtStruct};
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES, &extSystemProperties};
        XrResult result = xrGetSystemProperties(instance, systemId, &systemProperties);
        if (result != XR_SUCCESS) {
            XRC_THROW_XRRESULT(result, xrGetSystemProperties);
        }

        return (&extSystemProperties)->*pointerToMember;
    }

    /// A functor class template that you can call with XrInstance and XrSystemId to get the value of a field in a struct chained to XrSystemProperties.
    ///
    /// @see MakeSystemPropertiesChecker for easy creation in the absence of C++17 class type parameter deduction.
    template <typename SysPropsExtStruct, typename MemberType>
    class SystemPropertiesChecker
    {
    public:
        /// Pointer to data member type
        using MemberObjectPointer = MemberType SysPropsExtStruct::*;

        // std::is_pod is deprecated in C++20.
        static_assert(std::is_standard_layout<SysPropsExtStruct>{}, "Extension structs must be plain-old data");
        static_assert(std::is_trivial<SysPropsExtStruct>{}, "Extension structs must be plain-old data");

        /// Constructor
        ///
        /// @param emptyExtStruct An empty but initialized extension struct to chain on to XrSystemProperties.
        ///     Make sure `type` is initialized. The `next` pointer will be cleared before use.
        /// @param memberToReturn A pointer to data member corresponding to the member of the extension struct to retrieve.
        SystemPropertiesChecker(const SysPropsExtStruct& emptyExtStruct, MemberObjectPointer memberToReturn) noexcept
            : m_emptyExtStruct(emptyExtStruct), m_memberToReturn(memberToReturn)
        {
            m_emptyExtStruct.next = nullptr;
        }

        /// Functor call operator: Will call xrGetSystemProperties with your instance and systemId, and your struct chained on.
        /// Will return the value of the member for which you provided a pointer
        MemberType operator()(XrInstance instance, XrSystemId systemId) const
        {
            return GetSystemPropertiesValue(m_emptyExtStruct, m_memberToReturn, instance, systemId);
        }

    private:
        SysPropsExtStruct m_emptyExtStruct;
        MemberObjectPointer m_memberToReturn;
    };

    /// Create a functor that you can call with XrInstance and XrSystemId to get the value of a member in a struct chained to XrSystemProperties.
    ///
    /// Helper function to deduce the type params of @ref SystemPropertiesChecker from the empty struct passed as the first argument and the pointer to data member passed as the second.
    ///
    /// @param emptyExtStruct An empty but initialized extension struct to chain on to XrSystemProperties. Make sure `type` is initialized. The `next` pointer will be cleared before use.
    /// @param memberToReturn A pointer to data member corresponding to the member of the extension struct to retrieve.
    template <typename SysPropsExtStruct, typename MemberType = XrBool32>
    static inline SystemPropertiesChecker<SysPropsExtStruct, MemberType>
    MakeSystemPropertiesChecker(const SysPropsExtStruct& emptyExtStruct, MemberType SysPropsExtStruct::*memberToReturn) noexcept
    {
        return SystemPropertiesChecker<SysPropsExtStruct, MemberType>{emptyExtStruct, memberToReturn};
    }

    /// A functor class template that you can call with XrInstance and XrSystemId to get the value of a boolean field in a struct chained to XrSystemProperties.
    ///
    /// Like @ref SystemPropertiesChecker but for bools only, with conversion built in.
    ///
    /// @see MakeSystemPropertiesBoolChecker for easy creation in the absence of C++17 class type parameter deduction.
    template <typename SysPropsExtStruct>
    class SystemPropertiesBoolChecker
    {
    public:
        /// Pointer to data member type: member must be an XrBool32
        using MemberObjectPointer = XrBool32 SysPropsExtStruct::*;

        static_assert(std::is_pod<SysPropsExtStruct>{}, "Extension structs must be plain-old data");

        /// Constructor
        ///
        /// @param emptyExtStruct An empty but initialized extension struct to chain on to XrSystemProperties.
        ///     Make sure `type` is initialized. The `next` pointer will be cleared before use.
        /// @param memberToReturn A pointer to data member corresponding to the member of the extension struct to retrieve.
        SystemPropertiesBoolChecker(const SysPropsExtStruct& emptyExtStruct, MemberObjectPointer memberToReturn) noexcept
            : m_emptyExtStruct(emptyExtStruct), m_memberToReturn(memberToReturn)
        {
            m_emptyExtStruct.next = nullptr;
        }

        /// Functor call operator: Will call xrGetSystemProperties with your instance and systemId, and your struct chained on.
        /// Will return the value of the XrBool32 member for which you provided a pointer, converted to bool
        bool operator()(XrInstance instance, XrSystemId systemId) const
        {
            return GetSystemPropertiesValue(m_emptyExtStruct, m_memberToReturn, instance, systemId) == XR_TRUE;
        }

    private:
        SysPropsExtStruct m_emptyExtStruct;
        MemberObjectPointer m_memberToReturn;
    };

    /// Create a functor that you can call with XrInstance and XrSystemId to get the value of a boolean member in a struct chained to XrSystemProperties.
    ///
    /// Helper function to deduce the type param of @ref SystemPropertiesBoolChecker from the empty struct passed as the first argument, and convert the result from XrBool32 to bool.
    ///
    /// @param emptyExtStruct An empty but initialized extension struct to chain on to XrSystemProperties. Make sure `type` is initialized. The `next` pointer will be cleared before use.
    /// @param memberToReturn A pointer to data member corresponding to the member of the extension struct to retrieve.
    template <typename SysPropsExtStruct>
    static inline SystemPropertiesBoolChecker<SysPropsExtStruct>
    MakeSystemPropertiesBoolChecker(const SysPropsExtStruct& emptyExtStruct, XrBool32 SysPropsExtStruct::*memberToReturn) noexcept
    {
        return SystemPropertiesBoolChecker<SysPropsExtStruct>{emptyExtStruct, memberToReturn};
    }

}  // namespace Conformance
