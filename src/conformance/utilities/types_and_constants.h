// Copyright (c) 2019-2024, The Khronos Group Inc.
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
#include <cstdint>
#include <iosfwd>
#include <utility>
#include <type_traits>

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
        explicit XrPosefCPP(const XrPosef& pose) : XrPosef(pose)
        {
        }
    };
    bool operator==(const XrPosefCPP& lhs, const XrPosefCPP& rhs);

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
        MAKE_CONVERSION_FUNCTION(XrSpatialAnchorMSFT)
        MAKE_CONVERSION_FUNCTION(XrSpatialGraphNodeBindingMSFT)
        MAKE_CONVERSION_FUNCTION(XrHandTrackerEXT)
        MAKE_CONVERSION_FUNCTION(XrBodyTrackerFB)
        MAKE_CONVERSION_FUNCTION(XrSceneObserverMSFT)
        MAKE_CONVERSION_FUNCTION(XrSceneMSFT)
        MAKE_CONVERSION_FUNCTION(XrFacialTrackerHTC)
        MAKE_CONVERSION_FUNCTION(XrFoveationProfileFB)
        MAKE_CONVERSION_FUNCTION(XrTriangleMeshFB)
        MAKE_CONVERSION_FUNCTION(XrPassthroughFB)
        MAKE_CONVERSION_FUNCTION(XrPassthroughLayerFB)
        MAKE_CONVERSION_FUNCTION(XrGeometryInstanceFB)
        MAKE_CONVERSION_FUNCTION(XrSpatialAnchorStoreConnectionMSFT)
        MAKE_CONVERSION_FUNCTION(XrSpaceUserFB)
        MAKE_CONVERSION_FUNCTION(XrFaceTrackerFB)
        MAKE_CONVERSION_FUNCTION(XrEyeTrackerFB)
        MAKE_CONVERSION_FUNCTION(XrVirtualKeyboardMETA)
        MAKE_CONVERSION_FUNCTION(XrPassthroughColorLutMETA)
        MAKE_CONVERSION_FUNCTION(XrPassthroughHTC)
        MAKE_CONVERSION_FUNCTION(XrPlaneDetectorEXT)
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

        /// Explicit constructor from handle, if we don't need a destroyer instance.
        explicit ScopedHandle(HandleType h, std::enable_if<std::is_default_constructible<Destroyer>::value>* = nullptr) : h_(h)
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
        ScopedHandle(ScopedHandle&& other) noexcept : h_(std::move(other.h_)), d_(std::move(other.d_))
        {
            other.h_ = XR_NULL_HANDLE_CPP;
        }

        /// Move-assignable
        ScopedHandle& operator=(ScopedHandle&& other) noexcept
        {
            if (&other == this) {
                return *this;
            }
            reset();
            std::swap(h_, other.h_);
            std::swap(d_, other.d_);
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
        void adopt(HandleType h)
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

    namespace detail
    {
        void OutputHandle(std::ostream& os, uint64_t handle);
    }

    /// Outputs a formatted handle to a stream.
    template <typename T>
    static inline void OutputHandle(std::ostream& os, T handle)
    {
#if XR_PTR_SIZE == 8
        detail::OutputHandle(os, reinterpret_cast<uint64_t>(handle));
#else
        detail::OutputHandle(os, static_cast<uint64_t>(handle));
#endif
    }
}  // namespace Conformance
