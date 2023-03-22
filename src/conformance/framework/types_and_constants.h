// Copyright (c) 2019-2023, The Khronos Group Inc.
// Copyright (c) 2019 Collabora, Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <openxr/openxr.h>

#include <map>
#include <iosfwd>
#include <stdint.h>
#include <cassert>

namespace Conformance
{
/**
 * @defgroup cts_constants CTS framework constants
 * @ingroup cts_framework
 */
/// @{

/// Identifies the default invalid `XrInstance` value which isn't `XR_NULL_HANDLE`.
#define XRC_INVALID_INSTANCE_VALUE XrInstance(UINT64_C(0xbaaaaaaaaaaaaaad))

/// Identifies the default invalid `XrSession` value which isn't `XR_NULL_HANDLE`.
#define XRC_INVALID_SESSION_VALUE XrSession(UINT64_C(0xbaaaaaaaaaaaaaad))

/// Identifies the default invalid `XrSpace` value which isn't `XR_NULL_HANDLE`.
#define XRC_INVALID_SPACE_VALUE XrSpace(UINT64_C(0xbaaaaaaaaaaaaaad))

/// Identifies the default invalid `XrSwapchain` value which isn't `XR_NULL_HANDLE`.
#define XRC_INVALID_SWAPCHAIN_VALUE XrSwapchain(UINT64_C(0xbaaaaaaaaaaaaaad))

/// Identifies the default invalid `XrActionSet` value which isn't `XR_NULL_HANDLE`.
#define XRC_INVALID_ACTION_SET_VALUE XrActionSet(UINT64_C(0xbaaaaaaaaaaaaaad))

/// Identifies the default invalid `XrAction` value which isn't `XR_NULL_HANDLE`.
#define XRC_INVALID_ACTION_VALUE XrAction(UINT64_C(0xbaaaaaaaaaaaaaad))

/// Identifies the default invalid `SystemId` value which isn't `XR_NULL_SYSTEM_ID`.
#define XRC_INVALID_SYSTEM_ID_VALUE UINT64_C(0xbaaaaaaaaaaaaaad)

/// Identifies the default invalid `XrPath` value which isn't `XR_NULL_PATH`.
#define XRC_INVALID_PATH_VALUE UINT64_C(0xbaaaaaaaaaaaaaad)

/// Identifies an invalid image format.
///
/// This is graphics API-specific, but there are no graphics APIs which define
/// an image format of INT64_MAX, so that currently works for all APIs.
#define XRC_INVALID_IMAGE_FORMAT INT64_MAX

/// Specifies a structure type for an extension which is unknowable by any
/// application or runtime.
///
/// Used for validating that runtimes properly ignore unrecognized extension structs.
#define XRC_UNRECOGNIZABLE_STRUCTURE_TYPE ((XrStructureType)-1)

    /// @}

    /// Implements an auto-initializing XrPosef via C++ construction.
    /// @ingroup cts_framework
    struct XrPosefCPP : public XrPosef
    {
        XrPosefCPP() : XrPosef{{0, 0, 0, 1}, {0, 0, 0}}
        {
        }
    };

    /**
     * @defgroup cts_handle_helpers Handle-type utilities
     * @ingroup cts_framework
     */

    /**
     * @defgroup cts_handle_internals Implementation details of the handle utilities
     * @ingroup cts_handle_helpers
     */

    /// Proxy type used to provide a unique identity for `XR_NULL_HANDLE`, for comparisons, etc.
    ///
    /// Implicitly convertible to `XR_NULL_HANDLE` in all the places you want.
    ///
    /// Typically just use the instance @ref XR_NULL_HANDLE_CPP
    ///
    /// @ingroup cts_handle_internals
    struct NullHandleType
    {
#if XR_PTR_SIZE == 8

#define MAKE_CONVERSION_FUNCTION(T) \
    operator T() const              \
    {                               \
        return XR_NULL_HANDLE;      \
    }
        MAKE_CONVERSION_FUNCTION(XrInstance)
        MAKE_CONVERSION_FUNCTION(XrSession)
        MAKE_CONVERSION_FUNCTION(XrSpace)
        MAKE_CONVERSION_FUNCTION(XrAction)
        MAKE_CONVERSION_FUNCTION(XrSwapchain)
        MAKE_CONVERSION_FUNCTION(XrActionSet)
        MAKE_CONVERSION_FUNCTION(XrDebugUtilsMessengerEXT)
#else
        // 32-bit, just a uint64_t
        operator uint64_t() const
        {
            return XR_NULL_HANDLE;
        }
#endif
    };

    /// A constant null handle convertible to all handle types: like nullptr but for handles.
    ///
    /// Use instead of `XR_NULL_HANDLE` in the conformance suite
    ///
    /// @ingroup cts_handle_helpers
    constexpr NullHandleType XR_NULL_HANDLE_CPP{};

    /// @relates NullHandleType
    std::ostream& operator<<(std::ostream& os, NullHandleType const& /*unused*/);

    template <typename HandleType, typename Destroyer>
    class ScopedHandle;

    /// Used by ScopedHandle to allow it to be set "directly" by functions taking a pointer to a handle.
    ///
    /// @ingroup cts_handle_internals
    template <typename HandleType, typename Destroyer>
    class ScopedHandleResetProxy
    {
    public:
        explicit ScopedHandleResetProxy(ScopedHandle<HandleType, Destroyer>& parent) : parent_(parent), active_(true)
        {
        }
        ~ScopedHandleResetProxy();

        ScopedHandleResetProxy(ScopedHandleResetProxy const&) = delete;
        ScopedHandleResetProxy& operator=(ScopedHandleResetProxy const&) = delete;
        ScopedHandleResetProxy& operator=(ScopedHandleResetProxy&&) = delete;
        ScopedHandleResetProxy(ScopedHandleResetProxy&& other) noexcept : parent_(other.parent_)
        {
            std::swap(active_, other.active_);
            std::swap(addressGot_, other.addressGot_);
            std::swap(handle_, other.handle_);
        }

        operator HandleType*()
        {
            assert(!addressGot_);
            addressGot_ = true;
            return &handle_;
        }

    private:
        ScopedHandle<HandleType, Destroyer>& parent_;
        bool active_ = false;
        bool addressGot_ = false;
        HandleType handle_ = XR_NULL_HANDLE;
    };

    /// A unique-ownership RAII helper for OpenXR handles.
    ///
    /// @tparam HandleType The handle type to wrap
    /// @tparam Destroyer a functor type that destroys the handle
    ///
    /// @ingroup cts_handle_helpers
    template <typename HandleType, typename Destroyer>
    class ScopedHandle
    {
    public:
        /// Default (empty) constructor
        ScopedHandle() = default;

        /// Empty constructor when we need a destroyer instance.
        explicit ScopedHandle(Destroyer d) : h_(XR_NULL_HANDLE), d_(d)
        {
        }

        /// Explicit constructor from handle
        explicit ScopedHandle(HandleType h) : h_(h)
        {
        }
        /// Constructor from handle when we need a destroyer instance.
        ScopedHandle(HandleType h, Destroyer d) : h_(h), d_(d)
        {
        }

        /// Destructor
        ~ScopedHandle()
        {
            reset();
        }

        /// Non-copyable
        ScopedHandle(ScopedHandle const&) = delete;

        /// Non-copy-assignable
        ScopedHandle& operator=(ScopedHandle const&) = delete;

        /// Move-constructible
        ScopedHandle(ScopedHandle&& other) noexcept
        {
            std::swap(h_, other.h_);
        }

        /// Move-assignable
        ScopedHandle& operator=(ScopedHandle&& other) noexcept
        {
            std::swap(h_, other.h_);
            other.reset();
            return *this;
        }

        /// Is this handle valid?
        explicit operator bool() const
        {
            return h_ != XR_NULL_HANDLE;
        }

        /// Destroy the owned handle, if any.
        void reset()
        {
            if (h_ != XR_NULL_HANDLE_CPP) {
                d_(h_);
                h_ = XR_NULL_HANDLE_CPP;
            }
        }

        /// Assign a new handle into this object's control, destroying the old one if applicable.
        void reset(HandleType h)
        {
            reset();
            h_ = h;
        }

        /// Access the raw handle without affecting ownership or lifetime.
        HandleType get() const
        {
            return h_;
        }

        /// Release the handle from this object's control.
        HandleType release()
        {
            HandleType ret = h_;
            h_ = XR_NULL_HANDLE_CPP;
            return ret;
        }

        /// Call in a parameter that requires a pointer to a handle, to set it "directly" in here.
        ScopedHandleResetProxy<HandleType, Destroyer> resetAndGetAddress()
        {
            reset();
            return ScopedHandleResetProxy<HandleType, Destroyer>(*this);
        }

    private:
        HandleType h_ = XR_NULL_HANDLE_CPP;
        Destroyer d_;
    };

    /// Equality comparison between a scoped handle and a null handle
    /// @relates ScopedHandle
    template <typename HandleType, typename Destroyer>
    inline bool operator==(ScopedHandle<HandleType, Destroyer> const& handle, NullHandleType const&)
    {
        return handle.get() == XR_NULL_HANDLE_CPP;
    }

    /// Equality comparison between a scoped handle and a null handle
    /// @relates ScopedHandle
    template <typename HandleType, typename Destroyer>
    inline bool operator==(NullHandleType const&, ScopedHandle<HandleType, Destroyer> const& handle)
    {
        return handle.get() == XR_NULL_HANDLE_CPP;
    }

    /// Inequality comparison between a scoped handle and a null handle
    /// @relates ScopedHandle
    template <typename HandleType, typename Destroyer>
    inline bool operator!=(ScopedHandle<HandleType, Destroyer> const& handle, NullHandleType const&)
    {
        return handle.get() != XR_NULL_HANDLE_CPP;
    }

    /// Inequality comparison between a scoped handle and a null handle
    /// @relates ScopedHandle
    template <typename HandleType, typename Destroyer>
    inline bool operator!=(NullHandleType const&, ScopedHandle<HandleType, Destroyer> const& handle)
    {
        return handle.get() != XR_NULL_HANDLE_CPP;
    }

    template <typename HandleType, typename Destroyer>
    inline ScopedHandleResetProxy<HandleType, Destroyer>::~ScopedHandleResetProxy()
    {
        if (active_) {
            assert(addressGot_ && "Called resetAndGetAddress() without passing the result to a pointer-taking function.");
            parent_.reset(handle_);
        }
    }

    struct AutoBasicInstance;

    /// Output operator for the `XrInstance` handle in a @ref AutoBasicInstance
    /// @relates AutoBasicInstance
    std::ostream& operator<<(std::ostream& os, AutoBasicInstance const& inst);

    struct AutoBasicSession;

    /// Output operator for the `XrSession` handle in a @ref AutoBasicSession
    /// @relates AutoBasicSession
    std::ostream& operator<<(std::ostream& os, AutoBasicSession const& sess);

}  // namespace Conformance
