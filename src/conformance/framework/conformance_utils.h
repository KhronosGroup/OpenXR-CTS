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

#include "utilities/event_reader.h"
#include "utilities/types_and_constants.h"

#include <openxr/openxr.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Conformance
{
    using namespace std::chrono_literals;

    // Forward declarations
    struct IGraphicsPlugin;

    /// PathToString
    ///
    /// Returns a string for a given XrPath if it exists, else "<unknown XrPath %u>"
    ///
    /// Example usage:
    /// ```
    /// std::string pathString = PathToString(instance, path);
    /// ```
    ///
    std::string PathToString(XrInstance instance, XrPath path);

    /// ValidateResultAllowed
    ///
    /// Returns true if the given function (e.g. "xrPollEvent") may return the given result (e.g. XR_ERROR_PATH_INVALID).
    ///
    /// NOTE: Most usages of this function are unnecessary as the Conformance Layer (mandatory for conformance) already
    /// checks this for every call.
    ///
    /// Example usage:
    /// ```
    /// XrResult result = xrPollEvent(instance, &eventData);
    /// REQUIRE(ValidateResultAllowed("xrPollEvent", result));
    /// ```
    ///
    bool ValidateResultAllowed(const char* functionName, XrResult result);

    /// ValidateStructType
    ///
    /// Validates that a struct has a 'type' of the given expected type.
    ///
    /// Example usage:
    /// ```
    /// XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
    /// CHECK(ValidateStructType(&systemGetInfo, XR_TYPE_SYSTEM_GET_INFO));
    /// ```
    ///
    template <typename Struct>
    bool ValidateStructType(const Struct* s, XrStructureType expectedType)
    {
        return (reinterpret_cast<const XrBaseOutStructure*>(s)->type == expectedType);
    }

    /// ValidateStructArrayType
    ///
    /// Validates that an array of some struct has a 'type' of the given expected type.
    ///
    /// Example usage:
    /// ```
    /// std::array<XrSystemGetInfo, 4> systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
    /// CHECK(ValidateStructArrayType(systemGetInfo.data(), 4, XR_TYPE_SYSTEM_GET_INFO));
    /// ```
    ///
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

    /// ValidateStructArrayType
    ///
    /// Validates that a vector of some struct has a 'type' of the given expected type.
    ///
    /// Example usage:
    /// ```
    /// std::vector<XrSystemGetInfo> systemGetInfoVector(4, {XR_TYPE_SYSTEM_GET_INFO});
    /// CHECK(ValidateStructVectorType(systemGetInfoVector, XR_TYPE_SYSTEM_GET_INFO));
    /// ```
    ///
    template <typename StructVector>
    bool ValidateStructVectorType(const StructVector& sv, XrStructureType expectedType)
    {
        return ValidateStructArrayType(sv.data(), sv.size(), expectedType);
    }

    /// A scope-guard object that takes a reference to an XrInstance, and destroys it at scope exit if it's not XR_NULL_HANDLE.
    class CleanupInstanceOnScopeExit
    {
    public:
        explicit CleanupInstanceOnScopeExit(XrInstance& inst) : instance(inst)
        {
        }
        ~CleanupInstanceOnScopeExit();

        /// Destroy the instance if it's not XR_NULL_HANDLE
        void Destroy();

        // Can't copy, move, or assign - it's just a scope guard.
        CleanupInstanceOnScopeExit(CleanupInstanceOnScopeExit const&) = delete;
        CleanupInstanceOnScopeExit(CleanupInstanceOnScopeExit&&) = delete;
        CleanupInstanceOnScopeExit& operator=(CleanupInstanceOnScopeExit const&) = delete;
        CleanupInstanceOnScopeExit& operator=(CleanupInstanceOnScopeExit&&) = delete;

    private:
        XrInstance& instance;
    };

    /// A scope-guard object that takes a reference to an XrSession, and destroys it at scope exit if it's not XR_NULL_HANDLE.
    class CleanupSessionOnScopeExit
    {
    public:
        explicit CleanupSessionOnScopeExit(XrSession& sess) : session(sess)
        {
        }
        ~CleanupSessionOnScopeExit();

        /// Destroy the session if it's not XR_NULL_HANDLE
        void Destroy();

        // Can't copy, move, or assign - it's just a scope guard.
        CleanupSessionOnScopeExit(CleanupSessionOnScopeExit const&) = delete;
        CleanupSessionOnScopeExit(CleanupSessionOnScopeExit&&) = delete;
        CleanupSessionOnScopeExit& operator=(CleanupSessionOnScopeExit const&) = delete;
        CleanupSessionOnScopeExit& operator=(CleanupSessionOnScopeExit&&) = delete;

    private:
        XrSession& session;
    };

    /// Creates a swapchain for the given session and graphics plugin.
    /// If the widthHeight parameter has 0 for width or height, the given width or height is
    /// chosen by the implementation. The resulting width and height are written to widthHeight.
    XrResult CreateColorSwapchain(XrSession session, IGraphicsPlugin* graphicsPlugin, XrSwapchain* swapchain, XrExtent2Di* widthHeight,
                                  uint32_t arraySize = 1, bool cubemap = false, XrSwapchainCreateInfo* createInfoReturn = nullptr);

    /// Creates a depth swapchain for the given session and graphics plugin.
    XrResult CreateDepthSwapchain(XrSession session, IGraphicsPlugin* graphicsPlugin, XrSwapchain* swapchain, XrExtent2Di* widthHeight,
                                  uint32_t arraySize = 1);

    /// Creates a motion vector swapchain for the given session and graphics plugin.
    XrResult CreateMotionVectorSwapchain(XrSession session, IGraphicsPlugin* graphicsPlugin, XrSwapchain* swapchain,
                                         XrExtent2Di* widthHeight, uint32_t arraySize = 1);

    /// Executes xrAcquireSwapchainImage, xrWaitSwapchainImage, xrReleaseSwapchainImage, with no drawing.
    ///
    /// The contents of the swapchain images have no predictable content as a result of this.
    /// Returns any XrResult that xrAcquireSwapchainImage, xrWaitSwapchainImage, or xrReleaseSwapchainImage may return.
    XrResult CycleToNextSwapchainImage(XrSwapchain* swapchainArray, size_t count, XrDuration timeoutNs);

    /// Creates an action set and some actions, suitable for certain kinds of basic testing.
    XrResult CreateActionSet(XrInstance instance, XrActionSet* actionSet, std::vector<XrAction>* actionVector,
                             const XrPath* subactionPathArray = nullptr, size_t subactionPathArraySize = 0);

    /// @}

    namespace deleters
    {

        struct InstanceDeleteCHECK
        {
            typedef XrInstance pointer;
            void operator()(XrInstance i) const;
        };
        struct InstanceDeleteREQUIRE
        {
            typedef XrInstance pointer;
            void operator()(XrInstance i) const;
        };
        struct InstanceDelete
        {
            typedef XrInstance pointer;
            void operator()(XrInstance i) const;
        };
        struct SessionDeleteCHECK
        {
            typedef XrSession pointer;
            void operator()(XrSession s) const;
        };
        struct SessionDeleteREQUIRE
        {
            typedef XrSession pointer;
            void operator()(XrSession s) const;
        };
        struct SessionDelete
        {
            typedef XrSession pointer;
            void operator()(XrSession s) const;
        };
        struct SpaceDeleteCHECK
        {
            typedef XrSpace pointer;
            void operator()(XrSpace s) const;
        };
        struct SpaceDeleteREQUIRE
        {
            typedef XrSpace pointer;
            void operator()(XrSpace s) const;
        };
        struct SwapchainDeleteCHECK
        {
            typedef XrSwapchain pointer;
            void operator()(XrSwapchain s) const;
        };
        struct SwapchainDeleteREQUIRE
        {
            typedef XrSwapchain pointer;
            void operator()(XrSwapchain s) const;
        };
        struct SwapchainDelete
        {
            typedef XrSwapchain pointer;
            void operator()(XrSwapchain s) const;
        };
    }  // namespace deleters

    /// Defines a type similar to std::unique_ptr for XrInstance which uses CHECK() on destruction to verify that the
    /// destroy function succeeded.
    /// (Unlike std::unique_ptr, this copes with 32-bit builds where the handles are not pointers but uint64_t typedefs.)
    /// The primary purpose of this is to auto-destroy the handle upon scope exit.
    ///
    /// Example usage:
    /// ```
    ///     XrInstance instanceRaw{XR_NULL_HANDLE_CPP};
    ///     xrCreateInstance(&instanceRaw, ...);
    ///     InstanceCHECK instanceCHECK{instanceRaw};
    /// ```
    using InstanceCHECK = ScopedHandle<XrInstance, deleters::InstanceDeleteCHECK>;

    /// This is similar to InstanceCHECK except that it uses REQUIRE on the result of xrDestroyInstance.
    ///
    using InstanceREQUIRE = ScopedHandle<XrInstance, deleters::InstanceDeleteREQUIRE>;

    /// This is similar to InstanceCHECK except that it ignores the result of xrDestroyInstance.
    ///
    using InstanceScoped = ScopedHandle<XrInstance, deleters::InstanceDelete>;

    /// Defines a type similar to std::unique_ptr for XrSession which uses CHECK() on destruction to verify that the
    /// destroy function succeeded.
    /// (Unlike std::unique_ptr, this copes with 32-bit builds where the handles are not pointers but uint64_t typedefs.)
    /// The primary purpose of this is to auto-destroy the handle upon scope exit.
    ///
    /// See @ref InstanceCHECK for caveats.
    ///
    /// Example usage:
    /// ```
    /// XrSession session = ...;
    /// SessionCHECK sessionCHECK(session);
    /// ```
    using SessionCHECK = ScopedHandle<XrSession, deleters::SessionDeleteCHECK>;

    /// This is similar to SessionCHECK except that it uses REQUIRE on the result of xrDestroySession.
    ///
    using SessionREQUIRE = ScopedHandle<XrSession, deleters::SessionDeleteREQUIRE>;

    /// This is similar to SessionCHECK except that it ignores the result of xrDestroySession.
    ///
    using SessionScoped = ScopedHandle<XrSession, deleters::SessionDelete>;

    /// Defines a type similar to std::unique_ptr for XrSpace which uses CHECK() on destruction to verify that the
    /// destroy function succeeded.
    /// (Unlike std::unique_ptr, this copes with 32-bit builds where the handles are not pointers but uint64_t typedefs.)
    /// The primary purpose of this is to auto-destroy the handle upon scope exit.
    ///
    /// See @ref InstanceCHECK for caveats.
    ///
    /// Example usage:
    ///
    /// ```
    /// XrSpace space = ...;
    /// SpaceCHECK spaceCHECK(space);
    /// ```
    using SpaceCHECK = ScopedHandle<XrSpace, deleters::SpaceDeleteCHECK>;

    /// This is similar to SpaceCHECK except that it uses REQUIRE on the result of xrDestroySpace.
    using SpaceREQUIRE = ScopedHandle<XrSpace, deleters::SpaceDeleteREQUIRE>;

    /// Defines a type similar to std::unique_ptr for XrSwapchain which uses CHECK() on destruction to verify that the
    /// destroy function succeeded.
    /// (Unlike std::unique_ptr, this copes with 32-bit builds where the handles are not pointers but uint64_t typedefs.)
    /// The primary purpose of this is to auto-destroy the handle upon scope exit.
    ///
    /// See @ref InstanceCHECK for caveats.
    ///
    /// Example usage:
    /// ```
    /// XrSwachain swapchain = ...;
    /// SwapchainCHECK swapchainCHECK(swapchain);
    /// ```
    using SwapchainCHECK = ScopedHandle<XrSwapchain, deleters::SwapchainDeleteCHECK>;

    /// SwapchainREQUIRE
    ///
    /// This is similar to SwapchainCHECK except that it uses REQUIRE on the result of xrDestroySwapchain.
    ///
    using SwapchainREQUIRE = ScopedHandle<XrSwapchain, deleters::SwapchainDeleteREQUIRE>;

    /// SwapchainScoped
    ///
    /// Like SwapchainREQUIRE but with no checking of the return value.
    using SwapchainScoped = ScopedHandle<XrSwapchain, deleters::SwapchainDelete>;

    /// Returns an extension struct pointer suitable for use as a struct next parameter.
    /// The returns extension is one that is not defined by the OpenXR spec and serves the
    /// purpose of intentionally being unrecognizable. The returned struct pointer is read-only
    /// and suitable for use multiple times simultaneously, including in separate threads.
    const void* GetUnrecognizableExtension();

    /// Inserts an unrecognizable extension into an existing struct's next chain.
    ///
    /// Example usage:
    /// ```
    ///    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    ///    InsertUnrecognizableExtension(&createInfo);
    ///    [...]
    ///    result = xrCreateInstance(&createInfo, instance);
    /// ```
    template <typename Struct>
    void InsertUnrecognizableExtension(Struct* inStructure)
    {
        // We have a bit of declspec and casting here because there are two types of
        // next pointers, const and non-const.
        auto nextSaved = inStructure->next;  // This is const or non-const void*
        inStructure->next = (decltype(nextSaved))GetUnrecognizableExtension();
        reinterpret_cast<Struct*>(const_cast<void*>(inStructure->next))->next = nextSaved;
    }

    /// Undo @ref InsertUnrecognizableExtension
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

    /// Array version of InsertUnrecognizableExtension.
    ///
    /// Example usage:
    /// ```
    ///    std::vector<XrViewConfigurationView> vcvArray(20, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    ///    InsertUnrecognizableExtensionArray(vcvArray.data(), vcvArray.size());
    ///    [...]
    /// ```
    ///
    template <typename Struct>
    void InsertUnrecognizableExtensionArray(Struct* inStructure, size_t arraySize)
    {
        for (size_t i = 0; i < arraySize; ++i) {
            InsertUnrecognizableExtension(inStructure + i);
        }
    }

    /// Implements a single-run stopwatch using std::chrono.
    class Stopwatch
    {
    public:
        Stopwatch(bool start = false);

        /// Restarts the stopwatch, resetting the elapsed time to zero.
        void Restart();

        /// Reserved for being able to start without resetting the elapsed time to zero.
        /// void Start();

        /// Stops the stopwatch, freezing the end time.
        void Stop();

        /// Returns true if the stopwatch is running.
        bool IsStarted() const;

        /// Can be called whether the stopwatch is started or stopped.
        std::chrono::nanoseconds Elapsed() const;

    private:
        std::chrono::time_point<std::chrono::system_clock> startTime;
        std::chrono::time_point<std::chrono::system_clock> endTime;
        bool running{};
    };

    /// Implements a countdown timer.
    class CountdownTimer
    {
    public:
        explicit CountdownTimer(std::chrono::nanoseconds timeout) : stopwatch(), timeoutDuration(timeout)
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

    /// Creates an XrInstance suitable for enabling testing of additional functionality.
    ///
    /// Example usage:
    /// ```
    /// XrInstance instance;
    /// XrResult result = CreateBasicInstance(&instance);
    /// if(XR_SUCCEEDED(result))
    ///     xrDestroyInstance(instance);
    /// ```
    XrResult CreateBasicInstance(XrInstance* instance, bool permitDebugMessenger = true,
                                 const std::vector<const char*>& additionalEnabledExtensions = std::vector<const char*>());

    /// Similar to CreateBasicInstance but manages handle lifetime, including destroying
    /// the handle if a test exception occurs. Do not call xrDestroyInstance on this, as it
    /// will handle that itself.
    ///
    /// Example usage:
    /// ```
    /// void Test() {
    ///     AutoBasicInstance instance;
    ///     REQUIRE(instance != XR_NULL_HANDLE_CPP);
    ///
    ///     xrSomeFunction(instance, ...);
    /// }
    ///
    /// void Test2() {
    ///     AutoBasicInstance instance(AutoBasicInstance::createSystemId);
    ///     REQUIRE(instance != XR_NULL_HANDLE_CPP);
    ///     [...]
    /// }
    /// ```
    struct AutoBasicInstance
    {
    public:
        enum OptionFlags
        {
            none = 0x00,
            createSystemId = 0x01,
            skipDebugMessenger = 0x02,
        };

        /// Create a new XrInstance.
        AutoBasicInstance(const std::vector<const char*>& additionalEnabledExtensions, int optionFlags = 0);

        /// Take over ownership of a supplied XrInstance.
        /// AutoBasicInstance(XrInstance instance, int optionFlags = 0);
        /// Create a new XrInstance or take ownership of an existing instance handle.
        AutoBasicInstance(int optionFlags = 0, XrInstance instance_ = XR_NULL_HANDLE);

        ~AutoBasicInstance() noexcept;

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

    /// Output operator for the `XrInstance` handle in a @ref AutoBasicInstance
    /// @relates AutoBasicInstance
    std::ostream& operator<<(std::ostream& os, AutoBasicInstance const& inst);

    /// Finds an XrSystemId suitable for testing of additional functionality.
    XrResult FindBasicSystem(XrInstance instance, XrSystemId* systemId);

    /// Creates an XrSession suitable for enabling testing of additional functionality.
    /// If enableGraphicsSystem is false then no graphics system is specified with the
    /// call to xrCreateSession. This is useful for testing headless operation and runtime behavior
    /// requirements.
    ///
    /// Example usage:
    /// ```
    /// XrSession session;
    /// XrResult result = CreateBasicSession(instance, &session);
    /// if(XR_SUCCEEDED(result))
    ///     xrDestroySession(session);
    /// ```
    ///
    XrResult CreateBasicSession(XrInstance instance, XrSystemId* systemId, XrSession* session, bool enableGraphicsSystem = true);

    /// Similar to CreateBasicSession but manages handle lifetime, including destroying
    /// the handle if a test exception occurs. Do not call xrDestroySesion on this, as it
    /// will handle that itself.
    ///
    /// The enumerated types containers (e.g. swapchain formats) are auto-populated upon successful
    /// creation of their precursors (e.g. instance, session, systemId).
    ///
    /// To do: Make a base struct named SessionData (see above) and have AutoBasicSession inherit
    /// from it or own it. That way the SessionData can be passed around to testing subfunctions.
    ///
    /// Example usage:
    /// ```
    /// void Test() {
    ///     AutoBasicSession session(AutoBasicSession::beginSession, XR_NULL_HANDLE);
    ///     REQUIRE(session != XR_NULL_HANDLE_CPP);
    ///     xrSomeFunction(session, ...);
    /// }
    /// ```
    ///
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

        /// If instance is valid then we inherit it instead of create one ourselves.
        AutoBasicSession(int optionFlags = 0, XrInstance instance = XR_NULL_HANDLE);

        /// Calls Shutdown if not shut down already.
        ~AutoBasicSession();

        /// If instance is valid then we inherit it instead of create one ourselves.
        void Init(int optionFlags, XrInstance instance = XR_NULL_HANDLE);

        /// Begin the session.
        void BeginSession();

        /// Restores the class instance to a pre-initialized state.
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

        const std::vector<XrEnvironmentBlendMode>& SupportedEnvironmentBlendModes() const noexcept
        {
            return environmentBlendModeVector;
        }

        EventQueue& GetEventQueue() const
        {
            return *m_eventQueue;
        }

        bool operator==(NullHandleType const& /*unused*/) const;
        bool operator!=(NullHandleType const& /*unused*/) const;

        bool IsValidHandle() const
        {
            return session != XR_NULL_HANDLE;
        }

    public:
        int optionFlags{0};  //< Enum OptionFlags

        XrInstance instance{XR_NULL_HANDLE};
        InstanceScoped instanceOwned;

        XrSystemId systemId{XR_NULL_SYSTEM_ID};

        XrSession session{XR_NULL_HANDLE};
        XrResult sessionCreateResult{XR_SUCCESS};
        XrSessionState sessionState{XR_SESSION_STATE_UNKNOWN};
        std::unique_ptr<EventQueue> m_eventQueue;
        std::unique_ptr<EventReader> m_privateEventReader;

        std::array<XrPath, 2> handSubactionArray;  // "/user/hand/left", "/user/hand/right"

        // Optional created types.
        std::vector<XrSwapchain> swapchainVector;  // May be empty if not enabled.
        XrExtent2Di swapchainExtent;               /// Dimensions of swapchains.
        XrActionSet actionSet;                     /// May be null if not enabled.
        std::vector<XrAction> actionVector;        /// May be empty if not enabled.
        std::vector<XrSpace> spaceVector;          /// May be empty if not enabled.

        // Enumerated types.
        std::vector<int64_t> swapchainFormatVector;
        std::vector<XrReferenceSpaceType> spaceTypeVector;
        std::vector<XrViewConfigurationType> viewConfigurationTypeVector;
        std::vector<XrViewConfigurationView> viewConfigurationViewVector;
        std::vector<XrEnvironmentBlendMode> environmentBlendModeVector;
    };

    /// Output operator for the `XrSession` handle in a @ref AutoBasicSession
    /// @relates AutoBasicSession
    std::ostream& operator<<(std::ostream& os, AutoBasicSession const& sess);

    /// Calls your @p predicate repeatedly, pausing @p delay in between, until either it returns `true` or @p timeout has elapsed.
    ///
    /// @note This does not inherently submit frames and is thus likely to cause problems if a session is running unless your predicate submits a frame!
    /// It is intended for use outside of a frame loop.
    bool WaitUntilPredicateWithTimeout(const std::function<bool()>& predicate, const std::chrono::nanoseconds timeout,
                                       const std::chrono::nanoseconds delay);

    /// Identifies conformance-related information about individual OpenXR functions.
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

    typedef std::unordered_map<std::string, FunctionInfo> FunctionInfoMap;

    /// Accessor for the FunctionInfoMap singleton.
    const FunctionInfoMap& GetFunctionInfoMap();

    /// Returns true if the extension name is in the list (case-insensitive) of extensions that are
    /// enabled by default for instance creation (GlobalData::Options::enabledInstanceExtensionNames).
    bool IsInstanceExtensionEnabled(const char* extensionName);

    /// Returns true if the extension of this number is in the list of extensions that are
    /// enabled by default for instance creation (GlobalData::Options::enabledInstanceExtensionNames).
    bool IsInstanceExtensionEnabled(uint64_t extensionNumber);

    /// Returns true if the interaction profile is in the list oc interaction profiles that are
    /// enabled by default for conformance testing (GlobalDat::Options::enabledInteractionProfiles).
    bool IsInteractionProfileEnabled(const char* interactionProfile);

    /// Returns true if the extension function (case-sensitive) belongs to an extension that
    /// is enabled as per IsInstanceExtensionEnabled. Returns false if the function is unknown.
    bool IsExtensionFunctionEnabled(const char* functionName);

    /// Returns true if the enum is valid, either being in the core of the spec or enabled via
    /// an extension (using IsInstanceExtensionEnabled), the max value is never valid.
    bool IsViewConfigurationTypeEnumValid(XrViewConfigurationType viewType);

    /// Returns only the major/minor version of the runtime, not also the patch version.
    bool GetRuntimeMajorMinorVersion(XrVersion& version);

    /// Builds upon AutoBasicSession to run frame looping.
    /// A typical use case is to use this with a created AutoBasicSession to start running a
    /// frame loop until some XrSessionState is reached. Upon that time the test may choose to
    /// start submitting frames itself as part of some subsystem exercise.
    ///
    /// FrameIterator creates no resources of its own. It's a utility function that entirely uses
    /// resources created by AutoBasicSession. It does change the state of the application and
    /// the runtime, however.
    ///
    /// Due to limitations in the OpenXR API (no ability to query session state), this class must
    /// be used before any events are polled from the runtime, or at least before any session-state
    /// change events are received. Or else the user of the class must pass in the starting point
    /// session state to the FrameIterator constructor.
    ///
    /// Example usage:
    /// ```
    ///    // Get a session started.
    ///    AutoBasicSession session(AutoBasicSession::createInstance | AutoBasicSession::createSession |
    ///                         AutoBasicSession::beginSession | AutoBasicSession::createSwapchains | AutoBasicSession::createSpaces);
    ///
    ///    // Get frames iterating to the point of app focused state. This will draw frames along the way.
    ///    FrameIterator frameIterator(&session);
    ///    frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);
    ///
    ///    // Let's have the FrameIterator draw one more frame itself.
    ///    FrameIterator::RunResult runResult = frameIterator.SubmitFrame();
    ///    REQUIRE(runResult == FrameIterator::RunResult::Success);
    ///
    ///    // Now let's draw a frame ourselves.
    ///    runResult = frameIterator.PrepareSubmitFrame();
    ///    REQUIRE(runResult == FrameIterator::RunResult::Success);
    ///
    ///    const XrCompositionLayerBaseHeader* headerPtrArray[1] = {
    ///        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&frameIterator.compositionLayerProjection)};
    ///    frameIterator.frameEndInfo.layerCount = 1;
    ///    frameIterator.frameEndInfo.layers = headerPtrArray;
    ///
    ///    XrResult result = xrEndFrame(session.GetSession(), &frameIterator.frameEndInfo);
    ///    CHECK(result == XR_SUCCESS);
    /// ```
    class FrameIterator
    {
    public:
        explicit FrameIterator(AutoBasicSession* autoBasicSession_ = nullptr);
        ~FrameIterator() = default;

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

        /// Calls xrWaitFrame, xrLocateViews, xrBeginFrame. In doing so it sets up viewVector.
        /// This is a building block function used by PrepareSubmitFrame or possibly an external
        /// user wanting more custom control.
        RunResult WaitAndBeginFrame();

        /// Calls xrAcquireSwapchainImage, xrWaitSwapchainImage, xrReleaseSwapchainImage on each
        /// of the swapchains, in preparation for a call to EndFrame with the swapchains. Does not
        /// draw anything to the images.
        /// This is a building block function used by PrepareSubmitFrame or possibly an external
        /// user wanting more custom control.
        RunResult CycleToNextSwapchainImage();

        /// Sets up XrFrameEndInfo and XrCompositionLayerProjection, in preparation for a call to
        /// xrEndFrame. However, this leaves the frameEndInfo.layerCount and frameEndInfo.layers
        /// variables zeroed, with the expectation that the caller will set them appropriately and
        /// then call xrEndFrame.
        /// This is a building block function used by PrepareSubmitFrame or possibly an external
        /// user wanting more custom control.
        RunResult PrepareFrameEndInfo();

        /// This function calls WaitAndBeginFrame(), DrawSwapchains(), PrepareFrameEndInfo() and
        /// any error checking along the way. No need to call these three functions if you are
        /// calling this function. This itself is a higher level building block function for
        /// the SubmitFrame function.
        RunResult PrepareSubmitFrame();

        /// This calls PrepareSubmitFrame() and then calls xrEndFrame with a default set of layers.
        /// If you are calling RunToSessionState then you don't want to call this function, as it
        /// will do so internally until it gets to the specified state. And if you want to have
        /// control over the layers being sent then you would not use this function but instead
        /// use the PrepareSubmitFrame and call xrEndFrame yourself. See SubmitFrame source for
        /// an example of this.
        RunResult SubmitFrame();

        /// Runs until the given XrSessionState is achieved or times out before so.
        /// targetSessionState may be any XrSessionState, but some session states may require
        /// special handling in order to get to, such as XR_SESSION_STATE_LOSS_PENDING.
        /// Will repeatedly call SubmitFrame if necessary to get to the desired state.
        /// Will fail test if targetSessionState is not reached.
        void RunToSessionState(XrSessionState targetSessionState);

    protected:
        AutoBasicSession* const autoBasicSession;
        XrSessionState sessionState;

    public:
        XrFrameState frameState;                                             //< xrWaitFrame from WaitAndBeginFrame fills this in.
        std::vector<XrView> viewVector;                                      //< xrLocateViews from WaitAndBeginFrame fills this in.
        XrFrameEndInfo frameEndInfo;                                         //< PrepareFrameEndInfo sets this up.
        std::vector<XrCompositionLayerProjectionView> projectionViewVector;  //< PrepareFrameEndInfo sets this up.
        XrCompositionLayerProjection compositionLayerProjection;             //< PrepareFrameEndInfo sets this up.
    };

    /// Overwrites all members of an OpenXR tagged/chainable struct with "bad" data.
    ///
    /// Leaves @p s.type and @p s.next intact, while allowing the conformance layer to verify that
    /// structures are actually overwritten, rather than just left at an acceptable zero-initialized state.
    template <typename StructType>
    static inline void PoisonStructContents(StructType& s)
    {
        auto type = s.type;
        auto next = s.next;
        std::memset(&s, 1, sizeof(s));
        s.type = type;
        s.next = next;
    }

    /// Make a test title given a short test name, a subtest index, and the number of subtests.
    std::string SubtestTitle(const char* testName, size_t subtestIdx, size_t subtestCount);

    /// Make a test title given a short test name, a subtest index, and the array of subtests.
    template <typename T, std::size_t Size>
    inline std::string SubtestTitle(const char* testName, size_t subtestIdx, const T (&subtestArray)[Size])
    {
        (void)subtestArray;
        return SubtestTitle(testName, subtestIdx, Size);
    }

    /// Make pixel subrects based on normalized subrects and pixel dimensions
    inline XrRect2Di CropImage(int32_t width, int32_t height, XrRect2Df crop)
    {
        return {
            {int32_t(crop.offset.x * width), int32_t(crop.offset.y * height)},
            {int32_t(crop.extent.width * width), int32_t(crop.extent.height * height)},
        };
    }
}  // namespace Conformance
