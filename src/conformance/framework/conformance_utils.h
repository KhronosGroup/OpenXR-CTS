// Copyright (c) 2019-2020 The Khronos Group Inc.
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
#include <utility>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>
#include <chrono>
#include <stdint.h>
#include <iosfwd>
#include <utils.h>
#include <assert.h>

#include "event_reader.h"
#include "graphics_plugin.h"

namespace Conformance
{

// Identifies the default invalid XrInstance value which isn't XR_NULL_HANDLE.
#define XRC_INVALID_INSTANCE_VALUE XrInstance(UINT64_C(0xbaaaaaaaaaaaaaad))

// Identifies the default invalid XrSession value which isn't XR_NULL_HANDLE.
#define XRC_INVALID_SESSION_VALUE XrSession(UINT64_C(0xbaaaaaaaaaaaaaad))

// Identifies the default invalid XrSpace value which isn't XR_NULL_HANDLE.
#define XRC_INVALID_SPACE_VALUE XrSpace(UINT64_C(0xbaaaaaaaaaaaaaad))

// Identifies the default invalid XrSwapchain value which isn't XR_NULL_HANDLE.
#define XRC_INVALID_SWAPCHAIN_VALUE XrSwapchain(UINT64_C(0xbaaaaaaaaaaaaaad))

// Identifies the default invalid XrActionSet value which isn't XR_NULL_HANDLE.
#define XRC_INVALID_ACTION_SET_VALUE XrActionSet(UINT64_C(0xbaaaaaaaaaaaaaad))

// Identifies the default invalid XrAction value which isn't XR_NULL_HANDLE.
#define XRC_INVALID_ACTION_VALUE XrAction(UINT64_C(0xbaaaaaaaaaaaaaad))

// Identifies the default invalid SystemId value which isn't XR_NULL_SYSTEM_ID.
#define XRC_INVALID_SYSTEM_ID_VALUE UINT64_C(0xbaaaaaaaaaaaaaad)

// Identifies the default invalid XrPath value which isn't XR_NULL_PATH.
#define XRC_INVALID_PATH_VALUE UINT64_C(0xbaaaaaaaaaaaaaad)

// Identifies an invalid image format. This is graphics API-specific, but there are no
// graphics APIs which define an image format of INT64_MAX, so that currently works for all APIs.
#define XRC_INVALID_IMAGE_FORMAT INT64_MAX

// Specifies a structure type for an extension which is unknownable by any
// application or runtime. Used for validating that runtimes properly ignore
// unrecognized extension structs.
#define XRC_UNRECOGNIZABLE_STRUCTURE_TYPE ((XrStructureType)-1)

    // Forward declarations
    struct IGraphicsPlugin;

    // XrPosefCPP
    //
    // Implements an auto-initialing XrPosef via C++ construction.
    //
    struct XrPosefCPP : public XrPosef
    {
        XrPosefCPP() : XrPosef{{0, 0, 0, 1}, {0, 0, 0}}
        {
        }
    };

    // We keep a private auto-generated map of all results and their string versions.
    typedef std::map<XrResult, const char*> ResultStringMap;
    const ResultStringMap& GetResultStringMap();

    // ResultToString
    //
    // Returns a string for a given XrResult, based on our accounting of the result strings, and not
    // based on the xrResultToString function.
    // Returns "<unknown>" if the result is not recognized.
    //
    // Example usage:
    //    XrResult result = xrPollEvent(instance, &eventData);
    //    printf("%d: %s, resut, ResultToString(result));
    //
    const char* ResultToString(XrResult result);

    // PathToString
    //
    // Returns a string for a given XrPath if it exists, else "<unknown XrPath %u>"
    //
    // Example usage:
    //    std::string pathString = PathToString(instance, path);
    //
    std::string PathToString(XrInstance instance, XrPath path);

    // ValidateResultAllowed
    //
    // Returns true if the given function (e.g. "xrPollEvent") may return the given result (e.g. XR_ERROR_PATH_INVALID).
    //
    // Example usage:
    //    XrResult result = xrPollEvent(instance, &eventData);
    //    REQUIRE(ValidateResultAllowed("xrPollEvent", result));
    //
    bool ValidateResultAllowed(const char* functionName, XrResult result);

    // ValidateStructType
    //
    // Validates that a struct has a 'type' of the given expected type.
    //
    // Example usage:
    //    XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
    //    CHECK(ValidateStructType(&systemGetInfo, XR_TYPE_SYSTEM_GET_INFO));
    //
    template <typename Struct>
    bool ValidateStructType(const Struct* s, XrStructureType expectedType)
    {
        return (reinterpret_cast<const XrBaseOutStructure*>(s)->type == expectedType);
    }

    // ValidateStructArrayType
    //
    // Validates that an array of some struct has a 'type' of the given expected type.
    //
    // Example usage:
    //    std::array<XrSystemGetInfo, 4> systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
    //    CHECK(ValidateStructArrayType(systemGetInfo.data(), 4, XR_TYPE_SYSTEM_GET_INFO));
    //
    template <typename Struct>
    bool ValidateStructArrayType(const Struct* s, size_t count, XrStructureType expectedType)
    {
        for (const Struct* sEnd = s + count; s != sEnd; ++s) {
            if (reinterpret_cast<const XrBaseOutStructure*>(s)->type != expectedType) {
                return false;
            }
        }
        return true;
    }

    // ValidateStructArrayType
    //
    // Validates that a vector of some struct has a 'type' of the given expected type.
    //
    // Example usage:
    //    std::vector<XrSystemGetInfo> systemGetInfoVector(4, {XR_TYPE_SYSTEM_GET_INFO});
    //    CHECK(ValidateStructVectorType(systemGetInfoVector, XR_TYPE_SYSTEM_GET_INFO));
    //
    template <typename StructVector>
    bool ValidateStructVectorType(const StructVector& sv, XrStructureType expectedType)
    {
        return ValidateStructArrayType(sv.data(), sv.size(), expectedType);
    }

    // A sentinel object that takes a reference to an XrInstance, and destroys it at scope exit if it's not XR_NULL_HANDLE.
    class CleanupInstanceOnScopeExit
    {
    public:
        explicit CleanupInstanceOnScopeExit(XrInstance& inst) : instance(inst)
        {
        }
        ~CleanupInstanceOnScopeExit();

        // Destroy the instance if it's not XR_NULL_HANDLE
        void Destroy();

        // Can't copy, move, or assign - it's just a scope guard.
        CleanupInstanceOnScopeExit(CleanupInstanceOnScopeExit const&) = delete;
        CleanupInstanceOnScopeExit(CleanupInstanceOnScopeExit&&) = delete;
        CleanupInstanceOnScopeExit& operator=(CleanupInstanceOnScopeExit const&) = delete;
        CleanupInstanceOnScopeExit& operator=(CleanupInstanceOnScopeExit&&) = delete;

    private:
        XrInstance& instance;
    };

    // A sentinel object that takes a reference to an XrSession, and destroys it at scope exit if it's not XR_NULL_HANDLE.
    class CleanupSessionOnScopeExit
    {
    public:
        explicit CleanupSessionOnScopeExit(XrSession& sess) : session(sess)
        {
        }
        ~CleanupSessionOnScopeExit();

        // Destroy the session if it's not XR_NULL_HANDLE
        void Destroy();

        // Can't copy, move, or assign - it's just a scope guard.
        CleanupSessionOnScopeExit(CleanupSessionOnScopeExit const&) = delete;
        CleanupSessionOnScopeExit(CleanupSessionOnScopeExit&&) = delete;
        CleanupSessionOnScopeExit& operator=(CleanupSessionOnScopeExit const&) = delete;
        CleanupSessionOnScopeExit& operator=(CleanupSessionOnScopeExit&&) = delete;

    private:
        XrSession& session;
    };

    // CreateColorSwapchain
    //
    // Creates a swapchain for the given session and graphics plugin.
    // If the widthHeight parameter has 0 for width or height, the given width or height is
    // chosen by the implementation. The resulting width and height are written to widthHeight.
    //
    XrResult CreateColorSwapchain(XrSession session, IGraphicsPlugin* graphicsPlugin, XrSwapchain* swapchain, XrExtent2Di* widthHeight,
                                  uint32_t arraySize = 1, bool cubemap = false, XrSwapchainCreateInfo* createInfoReturn = nullptr);

    // CreateDepthSwapchain
    //
    // Creates a depth swapchain for the given session and graphics plugin.
    //
    XrResult CreateDepthSwapchain(XrSession session, IGraphicsPlugin* graphicsPlugin, XrSwapchain* swapchain, XrExtent2Di* widthHeight,
                                  uint32_t arraySize = 1);

    // Executes xrAcquireSwapchainImage, xrWaitSwapchainImage, xrReleaseSwapchainImage, with no drawing.
    // The contents of the swapchain images have no predictable content as a result of this.
    // Returns any XrResult that xrAcquireSwapchainImage, xrWaitSwapchainImage, or xrReleaseSwapchainImage may return.
    XrResult CycleToNextSwapchainImage(XrSwapchain* swapchainArray, size_t count, XrDuration timeoutNs);

    // CreateActionSet
    //
    // Creates an action set and some actions, suitable for certain kinds of basic testing.
    //
    XrResult CreateActionSet(XrInstance instance, XrActionSet* actionSet, std::vector<XrAction>* actionVector,
                             const XrPath* subactionPathArray = nullptr, size_t subactionPathArraySize = 0);

    // Proxy type used to provide a unique identity for XR_NULL_HANDLE, for comparisons, etc.
    // Implicitly convertible to XR_NULL_HANDLE in all the places you want.
    //
    // Typically just use the instance XR_NULL_HANDLE_CPP
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
    constexpr NullHandleType XR_NULL_HANDLE_CPP{};

    std::ostream& operator<<(std::ostream& os, NullHandleType const& /*unused*/);

    template <typename HandleType, typename Destroyer>
    class ScopedHandle;
    /// Used by ScopedHandle to allow it to be set "directly" by functions taking a pointer to a handle.
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
        ScopedHandleResetProxy(ScopedHandleResetProxy&& other) : parent_(other.parent_)
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

    template <typename HandleType, typename Destroyer>
    class ScopedHandle
    {
    public:
        /// Default (empty) constructor
        ScopedHandle() = default;

        /// Empty constructor when we need a destroyer instance.
        ScopedHandle(Destroyer d) : h_(XR_NULL_HANDLE), d_(d)
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
        ScopedHandle(ScopedHandle&& other)
        {
            std::swap(h_, other.h_);
        }

        /// Move-assignable
        ScopedHandle& operator=(ScopedHandle&& other)
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

    template <typename HandleType, typename Destroyer>
    inline bool operator==(ScopedHandle<HandleType, Destroyer> const& handle, NullHandleType const&)
    {
        return handle.get() == XR_NULL_HANDLE_CPP;
    }
    template <typename HandleType, typename Destroyer>
    inline bool operator==(NullHandleType const&, ScopedHandle<HandleType, Destroyer> const& handle)
    {
        return handle.get() == XR_NULL_HANDLE_CPP;
    }

    template <typename HandleType, typename Destroyer>
    inline bool operator!=(ScopedHandle<HandleType, Destroyer> const& handle, NullHandleType const&)
    {
        return handle.get() != XR_NULL_HANDLE_CPP;
    }
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

    // InstanceCHECK
    //
    // Defines a type similar to std::unique_ptr for XrInstance which uses CHECK() on destruction to verify that the
    // destroy function succeeded.
    // (Unlike std::unique_ptr, you can call resetAndGetAddress() to assign this directly.)
    // The primary purpose of this is to auto-destroy the handle upon scope exit.
    //
    // Example usage:
    //     // While this is easier to set than unique_ptr, it's still subject to this is an unsolved C++ problem:
    //     //    http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1132r3.html
    //     //    https://devblogs.microsoft.com/oldnewthing/20190429-00/?p=102456,
    //     InstanceCHECK instanceCHECK;
    //     xrCreateInstance(instanceCHECK.resetAndGetAddress(), ...);
    //
    struct InstanceDeleteCHECK
    {
        typedef XrInstance pointer;
        void operator()(XrInstance i);
    };
    typedef ScopedHandle<XrInstance, InstanceDeleteCHECK> InstanceCHECK;

    // InstanceREQUIRE
    //
    // This is similar to InstanceCHECK except that it uses REQUIRE on the result of xrDestroyInstance.
    //
    struct InstanceDeleteREQUIRE
    {
        typedef XrInstance pointer;
        void operator()(XrInstance i);
    };
    typedef ScopedHandle<XrInstance, InstanceDeleteREQUIRE> InstanceREQUIRE;

    // SessionCHECK
    //
    // Defines a type similar to std::unique_ptr for XrSession which uses CHECK() on destruction to verify that the
    // destroy function succeeded.
    // (Unlike std::unique_ptr, you can call resetAndGetAddress() to assign this directly.)
    // The primary purpose of this is to auto-destroy the handle upon scope exit.
    //
    // Example usage:
    //     // See InstanceCHECK for caveats
    //     XrSession session = ...;
    //     SessionCHECK sessionCHECK(session);
    //
    struct SessionDeleteCHECK
    {
        typedef XrSession pointer;
        void operator()(XrSession s);
    };
    typedef ScopedHandle<XrSession, SessionDeleteCHECK> SessionCHECK;

    // SessionREQUIRE
    //
    // This is similar to SessionCHECK except that it uses REQUIRE on the result of xrDestroySession.
    //
    struct SessionDeleteREQUIRE
    {
        typedef XrSession pointer;
        void operator()(XrSession s);
    };
    typedef ScopedHandle<XrSession, SessionDeleteREQUIRE> SessionREQUIRE;

    // SpaceCHECK
    //
    // Defines a type similar to std::unique_ptr for XrSpace which uses CHECK() on destruction to verify that the
    // destroy function succeeded.
    // (Unlike std::unique_ptr, you can call resetAndGetAddress() to assign this directly.)
    // The primary purpose of this is to auto-destroy the handle upon scope exit.
    //
    // Example usage:
    //     // See InstanceCHECK for caveats
    //     XrSpace space = ...;
    //     SpaceCHECK spaceCHECK(space);
    //
    struct SpaceDeleteCHECK
    {
        typedef XrSpace pointer;
        void operator()(XrSpace s);
    };
    typedef ScopedHandle<XrSpace, SpaceDeleteCHECK> SpaceCHECK;

    // SpaceREQUIRE
    //
    // This is similar to SpaceCHECK except that it uses REQUIRE on the result of xrDestroySpace.
    //
    struct SpaceDeleteREQUIRE
    {
        typedef XrSpace pointer;
        void operator()(XrSpace s);
    };
    typedef ScopedHandle<XrSpace, SpaceDeleteREQUIRE> SpaceREQUIRE;

    // SwapchainCHECK
    //
    // Defines a type similar to std::unique_ptr for XrSwapchain which uses CHECK() on destruction to verify that the
    // destroy function succeeded.
    // (Unlike std::unique_ptr, you can call resetAndGetAddress() to assign this directly.)
    // The primary purpose of this is to auto-destroy the handle upon scope exit.
    //
    // Example usage:
    //     // See InstanceCHECK for caveats
    //     XrSwachain swapchain = ...;
    //     SwapchainCHECK swapchainCHECK(swapchain);
    //
    struct SwapchainDeleteCHECK
    {
        typedef XrSwapchain pointer;
        void operator()(XrSwapchain s);
    };
    typedef ScopedHandle<XrSwapchain, SwapchainDeleteCHECK> SwapchainCHECK;

    // SpaceREQUIRE
    //
    // This is similar to SwapchainCHECK except that it uses REQUIRE on the result of xrDestroySwapchain.
    //
    struct SwapchainDeleteREQUIRE
    {
        typedef XrSwapchain pointer;
        void operator()(XrSwapchain s);
    };
    typedef ScopedHandle<XrSwapchain, SwapchainDeleteREQUIRE> SwapchainREQUIRE;

    // GetUnrecognizableExtension
    //
    // Returns an extension struct pointer suitable for use as a struct next parameter.
    // The returns extension is one that is not defined by the OpenXR spec and serves the
    // purpose of intentionally being unrecognizable. The returned struct pointer is read-only
    // and suitable for use multiple times simultaneously, including in separate threads.
    //
    const void* GetUnrecognizableExtension();

    // InsertUnrecognizableExtension
    // Inserts an unrecognizable extension into an existing struct's next chain.
    //
    // Example usage:
    //    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    //    InsertUnrecognizableExtension(&createInfo);
    //    [...]
    //    result = xrCreateInstance(&createInfo, instance);
    //
    template <typename Struct>
    void InsertUnrecognizableExtension(Struct* inStructure)
    {
        // We have a bit of declspec and casting here because there are two types of
        // next pointers, oonst and non-const.
        auto nextSaved = inStructure->next;  // This is const or non-const void*
        inStructure->next = (decltype(nextSaved))GetUnrecognizableExtension();
        reinterpret_cast<Struct*>(const_cast<void*>(inStructure->next))->next = nextSaved;
    }

    // RemoveUnrecognizableExtension
    //
    template <typename Struct>
    void RemoveUnrecognizableExtension(Struct* inStructure)
    {
        const void* ext = GetUnrecognizableExtension();

        // We assume that a present unrecognized extension is always inStructure->next,
        // as that's currently the only way we ever insert it.
        if (inStructure->next == ext) {
            inStructure->next = reinterpret_cast<Struct*>(const_cast<void*>(inStructure->next))->next;
        }
    }

    // InsertUnrecognizableExtensionArray
    // Array version of InsertUnrecognizableExtension.
    //
    // Example usage:
    //    std::vector<XrViewConfigurationView> vcvArray(20, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    //    InsertUnrecognizableExtensionArray(vcvArray.data(), vcvArray.size());
    //    [...]
    //
    template <typename Struct>
    void InsertUnrecognizableExtensionArray(Struct* inStructure, size_t arraySize)
    {
        for (size_t i = 0; i < arraySize; ++i) {
            InsertUnrecognizableExtension(inStructure + i);
        }
    }

    // Example usage:
    //     XrDuration timeout = 10_xrSeconds;
    inline constexpr XrDuration operator"" _xrSeconds(unsigned long long value)
    {
        return (value * 1000 * 1000 * 1000);  // Convert seconds to XrDuration nanoseconds.
    }

    // Example usage:
    //     XrDuration timeout = 10_xrMilliseconds;
    inline constexpr XrDuration operator"" _xrMilliseconds(unsigned long long value)
    {
        return (value * 1000 * 1000);  // Convert milliseconds to XrDuration nanoseconds.
    }

    // Example usage:
    //     XrDuration timeout = 10_xrMicroseconds;
    inline constexpr XrDuration operator"" _xrMicroseconds(unsigned long long value)
    {
        return (value * 1000);  // Convert microseconds to XrDuration nanoseconds.
    }

    // Example usage:
    //     XrDuration timeout = 10_xrNanoseconds;
    inline constexpr XrDuration operator"" _xrNanoseconds(unsigned long long value)
    {
        return value;  // XrDuration is already in nanoseconds
    }

    // Because std::chrono::chrono_literals is not available with C++11.
    // Remove this when upgrading to C++14
    constexpr std::chrono::milliseconds operator"" _ms(unsigned long long ms)
    {
        return std::chrono::milliseconds(ms);
    }
    constexpr std::chrono::microseconds operator"" _us(unsigned long long us)
    {
        return std::chrono::microseconds(us);
    }
    constexpr std::chrono::nanoseconds operator"" _ns(unsigned long long ns)
    {
        return std::chrono::nanoseconds(ns);
    }
    constexpr std::chrono::seconds operator"" _sec(unsigned long long s)
    {
        return std::chrono::seconds(s);
    }

    // Stopwatch
    //
    // Implements a single-run stopwatch using std::chrono.
    //
    class Stopwatch
    {
    public:
        Stopwatch(bool start = false);

        // Restarts the stopwatch, resetting the elapsed time to zero.
        void Restart();

        // Reserved for being able to start without resetting the elapsed time to zero.
        // void Start();

        // Stops the stopwatch, freezing the end time.
        void Stop();

        // Returns true if the stopwatch is running.
        bool IsStarted() const;

        // Can be called whether the stopwatch is started or stopped.
        std::chrono::nanoseconds Elapsed() const;

    private:
        std::chrono::time_point<std::chrono::system_clock> startTime;
        std::chrono::time_point<std::chrono::system_clock> endTime;
        bool running;
    };

    // CountdownTimer
    //
    // Implements a countdown timer.
    //
    class CountdownTimer
    {
    public:
        CountdownTimer() : stopwatch(), timeoutDuration()
        {
        }

        CountdownTimer(std::chrono::nanoseconds timeout) : stopwatch(), timeoutDuration(timeout)
        {
            stopwatch.Restart();
        }

        void Restart(std::chrono::nanoseconds timeout)
        {
            timeoutDuration = timeout;
            stopwatch.Restart();
        }

        bool IsStarted() const
        {
            return stopwatch.IsStarted();
        }

        bool IsTimeUp() const
        {
            return (stopwatch.Elapsed() >= timeoutDuration);
        }

    private:
        Stopwatch stopwatch;
        std::chrono::nanoseconds timeoutDuration;
    };

    // CreateBasicInstance
    //
    // Creates an XrInstance suitable for enabling testing of additional functionality.
    //
    // Example usage:
    //    XrInstance instance;
    //    XrResult result = CreateBasicInstance(&instance);
    //    if(XR_SUCCEEDED(result))
    //        xrDestroyInstance(instance);
    //
    XrResult CreateBasicInstance(XrInstance* instance, bool permitDebugMessenger = true,
                                 std::vector<const char*> additionalEnabledExtensions = std::vector<const char*>());

    // AutoBasicInstance
    //
    // Similar to CreateBasicInstance but manages handle lifetime, including destroying
    // the handle if a test exception occurs. Do not call xrDestroyInstance on this, as it
    // will handle that itself.
    //
    // Example usage:
    //    void Test() {
    //        AutoBasicInstance instance;
    //        REQUIRE(instance != XR_NULL_HANDLE_CPP);
    //
    //        xrSomeFunction(instance, ...);
    //    }
    //
    //    void Test2() {
    //        AutoBasicInstance instance(AutoBasicInstance::createSystemId);
    //        REQUIRE(instance != XR_NULL_HANDLE_CPP);
    //        [...]
    //    }
    //
    struct AutoBasicInstance
    {
    public:
        enum OptionFlags
        {
            none = 0x00,
            createSystemId = 0x01,
            skipDebugMessenger = 0x02,
        };

        // Create a new XrInstance.
        AutoBasicInstance(const std::vector<const char*>& additionalEnabledExtensions, int optionFlags = 0);

        // Take over ownership of a supplied XrInstance.
        // AutoBasicInstance(XrInstance instance, int optionFlags = 0);
        // Create a new XrInstance or take ownership of an existing instance handle.
        AutoBasicInstance(int optionFlags = 0, XrInstance instance_ = XR_NULL_HANDLE);

        ~AutoBasicInstance();

        operator XrInstance() const
        {
            return instance;
        }
        XrInstance GetInstance() const
        {
            return instance;
        }

        bool operator==(NullHandleType const& /*unused*/) const;
        bool operator!=(NullHandleType const& /*unused*/) const;

        bool IsValidHandle() const
        {
            return instance != XR_NULL_HANDLE;
        }

    private:
        void Initialize(int optionFlags, XrInstance instance_,
                        const std::vector<const char*>& additionalEnabledExtensions = std::vector<const char*>());

    public:
        XrInstance instance{XR_NULL_HANDLE_CPP};
        XrResult instanceCreateResult{XR_SUCCESS};
        XrDebugUtilsMessengerEXT debugMessenger{XR_NULL_HANDLE_CPP};
        XrSystemId systemId{XR_NULL_SYSTEM_ID};
    };

    std::ostream& operator<<(std::ostream& os, AutoBasicInstance const& inst);

    // CreateBasicSession
    //
    // Creates an XrSession suitable for enabling testing of additional functionality.
    // If enableGraphicsSystem is false then no graphics system is specified with the
    // call to xrCreateSession. This is useful for testing headless operation and runtime behavior
    // requirements.
    //
    // Example usage:
    //    XrSession session;
    //    XrResult result = CreateBasicSession(instance, &session);
    //    if(XR_SUCCEEDED(result))
    //        xrDestroySession(session);
    //
    XrResult CreateBasicSession(XrInstance instance, XrSystemId* systemId, XrSession* session, bool enableGraphicsSystem = true);

    // AutoBasicSession
    //
    // Similar to CreateBasicSession but manages handle lifetime, including destroying
    // the handle if a test exception occurs. Do not call xrDestroySesion on this, as it
    // will handle that itself.
    //
    // The enumerated types containers (e.g. swapchain formats) are auto-populated upon successful
    // creation of their precursors (e.g. instance, session, systemId).
    //
    // To do: Make a base struct named SessionData (see above) and have AutoBasicSession inherit
    // from it or own it. That way the SessionData can be passed around to testing subfunctions.
    //
    // Example usage:
    //    void Test() {
    //        AutoBasicSession session(AutoBasicSession::beginSession, XR_NULL_HANDLE);
    //        REQUIRE(session != XR_NULL_HANDLE_CPP);
    //
    //        xrSomeFunction(session, ...);
    //    }
    //
    struct AutoBasicSession
    {
    public:
        enum OptionFlags
        {
            none = 0x00,
            createInstance = 0x01,
            createSession = 0x02,
            beginSession = 0x04,
            createSwapchains = 0x08,
            createActions = 0x10,
            createSpaces = 0x20,
            skipGraphics = 0x40
        };

        // If instance is valid then we inherit it instead of create one ourselves.
        AutoBasicSession(int optionFlags = 0, XrInstance instance = XR_NULL_HANDLE);

        // Calls Shutdown if not shut down already.
        ~AutoBasicSession();

        // If instance is valid then we inherit it instead of create one ourselves.
        void Init(int optionFlags, XrInstance instance = XR_NULL_HANDLE);

        // Restores the class instance to a pre-initialized state.
        void Shutdown();

        XrInstance GetInstance() const
        {
            return instance;
        }

        XrSession GetSession() const
        {
            return session;
        }

        XrSystemId GetSystemId() const
        {
            return systemId;
        }

        XrSessionState GetSessionState() const
        {
            return sessionState;
        }

        operator XrSession() const
        {
            return session;
        }

        std::vector<XrEnvironmentBlendMode> SupportedEnvironmentBlendModes() const
        {
            return environmentBlendModeVector;
        }

        bool operator==(NullHandleType const& /*unused*/) const;
        bool operator!=(NullHandleType const& /*unused*/) const;

        bool IsValidHandle() const
        {
            return session != XR_NULL_HANDLE;
        }

    public:
        int optionFlags{0};  // Enum OptionFlags

        XrInstance instance{XR_NULL_HANDLE};
        bool instanceOwned{false};  // True if we created it and not the caller of us.

        XrSystemId systemId{XR_NULL_SYSTEM_ID};

        XrSession session{XR_NULL_HANDLE};
        XrResult sessionCreateResult{XR_SUCCESS};
        XrSessionState sessionState{XR_SESSION_STATE_UNKNOWN};

        std::array<XrPath, 2> handSubactionArray;  // "/user/hand/left", "/user/hand/right"

        // Optional created types.
        std::vector<XrSwapchain> swapchainVector;  // May be empty if not enabled.
        XrExtent2Di swapchainExtent;               // Dimensions of swapchains.
        XrActionSet actionSet;                     // May be null if not enabled.
        std::vector<XrAction> actionVector;        // May be empty if not enabled.
        std::vector<XrSpace> spaceVector;          // May be empty if not enabled.

        // Enumerated types.
        std::vector<int64_t> swapchainFormatVector;
        std::vector<XrReferenceSpaceType> spaceTypeVector;
        std::vector<XrViewConfigurationType> viewConfigurationTypeVector;
        std::vector<XrViewConfigurationView> viewConfigurationViewVector;
        std::vector<XrEnvironmentBlendMode> environmentBlendModeVector;
    };

    std::ostream& operator<<(std::ostream& os, AutoBasicSession const& sess);

    bool WaitUntilPredicateWithTimeout(std::function<bool()> predicate, const std::chrono::nanoseconds timeout,
                                       const std::chrono::nanoseconds delay);

    // Identifies conformance-related information about individual OpenXR functions.
    struct FunctionInfo
    {
        PFN_xrVoidFunction functionPtr;
        bool nullInstanceOk;
        const char* requiredExtension;
        std::vector<XrResult> validResults;

        FunctionInfo(PFN_xrVoidFunction functionPtr = nullptr, bool nullInstanceOk = false, const char* requiredExtension = nullptr,
                     std::vector<XrResult> validResults = std::vector<XrResult>())
            : functionPtr(functionPtr)
            , nullInstanceOk(nullInstanceOk)
            , requiredExtension(requiredExtension)
            , validResults(std::move(validResults))
        {
        }
    };

    // Accessor for the FunctionInfoMap singleton.
    typedef std::unordered_map<std::string, FunctionInfo> FunctionInfoMap;
    const FunctionInfoMap& GetFunctionInfoMap();

    // Returns true if the extension name is in the list (case-insensitive) of extensions that are
    // enabled by default for instance creation (GlobalData::Options::enabledInstanceExtensionNames).
    bool IsInstanceExtensionEnabled(const char* extensionName);

    // Returns true if the extension of this number is in the list of extensions that are
    // enabled by default for instance creation (GlobalData::Options::enabledInstanceExtensionNames).
    bool IsInstanceExtensionEnabled(uint64_t extensionNumber);

    // Returns true if the interaction profile is in the list oc interaction profiles that are
    // enabled by default for conformance testing (GlobalDat::Options::enabledInteractionProfiles).
    bool IsInteractionProfileEnabled(const char* interactionProfile);

    // Returns true if the extension function (case-sensitive) belongs to an extension that
    // is enabled as per IsInstanceExtensionEnabled. Returns false if the function is unknown.
    bool IsExtensionFunctionEnabled(const char* functionName);

    // Returns true if the enum is valid, either being in the core of the spec or enabled via
    // an extension (using IsInstanceExtensionEnabled), the max value is never valid.
    bool IsViewConfigurationTypeEnumValid(XrViewConfigurationType viewType);

    // Returns only the major/minor version of the runtime, not also the patch version.
    bool GetRuntimeMajorMinorVersion(XrVersion& version);

    // Builds upon AutoBasicSession to run frame looping.
    // A typical use case is to use this with a created AutoBasicSession to start running a
    // frame loop until some XrSessionState is reached. Upon that time the test may choose to
    // start submitting frames itself as part of some subsystem exercise.
    //
    // FrameIterator creates no resources of its own. It's a utility function that entirely uses
    // resources created by AutoBasicSession. It does change the state of the application and
    // the runtime, however.
    //
    // Due to limitations in the OpenXR API (no ability to query session state), this class must
    // be used before any events are polled from the runtime, or at least before any session-state
    // change events are received. Or else the user of the class must pass in the starting point
    // session state to the FrameIterator constructor.
    //
    // Example usage:
    //    // Get a session started.
    //    AutoBasicSession session(AutoBasicSession::createInstance | AutoBasicSession::createSession |
    //                         AutoBasicSession::beginSession | AutoBasicSession::createSwapchains | AutoBasicSession::createSpaces);
    //
    //    // Get frames iterating to the point of app focused state. This will draw frames along the way.
    //    FrameIterator frameIterator(&session);
    //    FrameIterator::RunResult runResult = frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED, timeoutMicroseconds);
    //    REQUIRE(runResult == FrameIterator::RunResult::Success);
    //
    //    // Let's have the FrameIterator draw one more frame itself.
    //    runResult = frameIterator.SubmitFrame();
    //    REQUIRE(runResult == FrameIterator::RunResult::Success);
    //
    //    // Now let's draw a frame ourselves.
    //    runResult = frameIterator.PrepareSubmitFrame();
    //    REQUIRE(runResult == FrameIterator::RunResult::Success);
    //
    //    const XrCompositionLayerBaseHeader* headerPtrArray[1] = {
    //        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection)};
    //    frameIterator.frameEndInfo.layerCount = 1;
    //    frameIterator.frameEndInfo.layers = headerPtrArray;
    //
    //    XrResult result = xrEndFrame(session.GetSession(), &frameIterator.frameEndInfo);
    //    CHECK(result == XR_SUCCESS);

    class FrameIterator
    {
    public:
        FrameIterator(AutoBasicSession* autoBasicSession_ = nullptr);
        ~FrameIterator() = default;

        // Must not be called after calling any other member function.
        void SetAutoBasicSession(AutoBasicSession* autoBasicSession_);

        XrSessionState GetCurrentSessionState() const;

        enum class TickResult
        {
            SessionStateUnchanged,  // This is a successful result.
            SessionStateChanged,    // This is a successful result.
            Error
        };

        TickResult PollEvent();

        enum class RunResult
        {
            Success,
            Timeout,
            Error
        };

        // Calls xrWaitFrame, xrLocateViews, xrBeginFrame. In doing so it sets up viewVector.
        // This is a building block function used by PrepareSubmitFrame or possibly an external
        // user wanting more custom control.
        RunResult WaitAndBeginFrame();

        // Calls xrAcquireSwapchainImage, xrWaitSwapchainImage, xrReleaseSwapchainImage on each
        // of the swapchains, in preparation for a call to EndFrame with the swapchains. Does not
        // draw anything to the images.
        // This is a building block function used by PrepareSubmitFrame or possibly an external
        // user wanting more custom control.
        RunResult CycleToNextSwapchainImage();

        // Sets up XrFrameEndInfo and XrCompositionLayerProjection, in preparation for a call to
        // xrEndFrame. However, this leaves the frameEndInfo.layerCount and frameEndInfo.layers
        // variables zeroed, with the expectation that the caller will set them appropriately and
        // then call xrEndFrame.
        // This is a building block function used by PrepareSubmitFrame or possibly an external
        // user wanting more custom control.
        RunResult PrepareFrameEndInfo();

        // This function calls WaitAndBeginFrame(), DrawSwapchains(), PrepareFrameEndInfo() and
        // any error checking along the way. No need to call these three functions if you are
        // calling this function. This itself is a higher level building block function for
        // the SubmitFrame function.
        RunResult PrepareSubmitFrame();

        // This calls PrepareSubmitFrame() and then calls xrEndFrame with a default set of layers.
        // If you are calling RunToSessionState then you don't want to call this function, as it
        // will do so internally until it gets to the specified state. And if you want to have
        // control over the layers being sent then you would not use this function but instead
        // use the PrepareSubmitFrame and call xrEndFrame yourself. See SubmitFrame source for
        // an example of this.
        RunResult SubmitFrame();

        // Runs until the given XrSessionState is achieved or timesout before so.
        // targetSessionState may be any XrSessionState, but some session states may require
        // special handling in order to get to, such as XR_SESSION_STATE_LOSS_PENDING.
        // Will repeatedly call SubmitFrame if necessary to get to the desired state.
        RunResult RunToSessionState(XrSessionState targetSessionState, std::chrono::nanoseconds timeout);

    protected:
        AutoBasicSession* autoBasicSession;
        XrSessionState sessionState;
        CountdownTimer countdownTimer;

    public:
        XrFrameState frameState;                                             // xrWaitFrame from WaitAndBeginFrame fills this in.
        std::vector<XrView> viewVector;                                      // xrLocateViews from WaitAndBeginFrame fills this in.
        XrFrameEndInfo frameEndInfo;                                         // PrepareFrameEndInfo sets this up.
        std::vector<XrCompositionLayerProjectionView> projectionViewVector;  // PrepareFrameEndInfo sets this up.
        XrCompositionLayerProjection compositionLayerProjection;             // PrepareFrameEndInfo sets this up.
    };

    /*!
 * Overwrites all members of an OpenXR tagged/chainable struct with "bad" data.
 * 
 * Leaves @p s.type and @p s.next intact, while allowing the conformance layer to verify that structures are actually overwritten, rather than just left at an acceptable zero-initialized state.
 */
    template <typename StructType>
    static inline void PoisonStructContents(StructType& s)
    {
        auto type = s.type;
        auto next = s.next;
        std::memset(&s, 1, sizeof(s));
        s.type = type;
        s.next = next;
    }
}  // namespace Conformance
