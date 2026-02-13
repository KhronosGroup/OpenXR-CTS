// Copyright (c) 2019-2026 The Khronos Group Inc.
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

//
// These data structures and functions are used by both the generated and customized code.
//
#pragma once

#include <openxr/openxr.h>

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

struct EnabledVersions
{
    EnabledVersions(const XrInstanceCreateInfo* createInfo) noexcept
        : apiVersion(createInfo->applicationInfo.apiVersion)
        // Note that this assumes that 1.1 requires 1.0 conformance, which isn't technically strictly required,
        // but in practice it is true.
        , version_1_x_compatible(XR_VERSION_MAJOR(apiVersion) == 1)
        // Similarly this assumes that 1.2 requires 1.1 conformance. 1.2 does not yet exist.
        , version_1_1_compatible(version_1_x_compatible && XR_VERSION_MINOR(apiVersion) >= 1)
    {
    }
    XrVersion apiVersion;
    bool version_1_x_compatible{false};
    bool version_1_1_compatible{false};
};

/// Base class for "custom" handle state that differs between handle types
struct ICustomHandleState
{
    ICustomHandleState() = default;
    virtual ~ICustomHandleState() = default;

    ICustomHandleState(const ICustomHandleState&) = delete;
    ICustomHandleState& operator=(ICustomHandleState&) = delete;
};

using IntHandle = uint64_t;   // A common type for all handles so a single map can be used.
struct ConformanceHooksBase;  // forward-declare

/// Common state kept around for all XR handles.
struct HandleState
{
    HandleState(IntHandle handle_, XrObjectType type, HandleState* parent, std::shared_ptr<ConformanceHooksBase> conformanceHooks)
        : handle(handle_), type(type), conformanceHooks(std::move(conformanceHooks)), parent(parent)
    {
    }

    /// "fork-exec" for handles, basically. Called from generated ConformanceHooksBase implementations
    ///
    /// @note Locks and unlocks the child list mutex in this object
    std::unique_ptr<HandleState> CloneForChild(IntHandle handle_, XrObjectType childType)
    {
        // Note that the cloned HandleState will start with a null customState and no children.
        auto childState = std::unique_ptr<HandleState>(new HandleState(handle_, childType, this /* parent */, conformanceHooks));

        {
            std::unique_lock<std::recursive_mutex> lock(childrenMutex);
            children.push_back(childState.get());
        }

        return childState;
    }

    /// Assign a custom state object to this handle.
    ///
    /// @note Locks and unlocks the custom state mutex in this object
    void SetCustomState(std::unique_ptr<ICustomHandleState>&& newCustomState)
    {
        std::unique_lock<std::mutex> lock(customStateMutex);
        customState = std::move(newCustomState);
    }

    /// Access the custom state object in this handle.
    ///
    /// @note Locks and unlocks the custom state mutex in this object
    ICustomHandleState* GetCustomState() const
    {
        std::unique_lock<std::mutex> lock(customStateMutex);
        return customState.get();
    }

    const IntHandle handle;

    const XrObjectType type;

    const std::shared_ptr<ConformanceHooksBase> conformanceHooks;

    HandleState* const parent;

    /// Non-owning pointers to handle state of child handles.
    mutable std::recursive_mutex childrenMutex;
    std::vector<HandleState*> children;

private:
    /// Additional data stored by the hand-coded validations.
    mutable std::mutex customStateMutex;
    std::unique_ptr<ICustomHandleState> customState;
};

/// Handle exception type: Inherit from std::runtime_error so it can be caught in the ABI boundary.
struct HandleException : public std::runtime_error
{
    HandleException(const std::string& message) : std::runtime_error(message)
    {
    }
};

struct HandleNotFoundException : public HandleException
{
    HandleNotFoundException(const std::string& message) : HandleException(message)
    {
    }
};

/// Global handle state map key: The value of a handle as an int, and its XrObjectType enumerant.
using HandleStateKey = std::pair<IntHandle, XrObjectType>;

/// Destroy a handle state object owned by the global handle state map.
/// Also destroys state for all child handles (recursively)
///
/// @note Locks and unlocks the mutex for the global handle state map, as well
/// as a child-list mutex in the parent handle and every child handle.
void UnregisterHandleState(HandleState* handleState);

/// Transfer ownership of a handle state object to the global handle state map.
/// Usually called directly with the return value of @ref HandleState::CloneForChild
///
/// @note Locks and unlocks the mutex for the global handle state map
void RegisterHandleState(std::unique_ptr<HandleState> handleState);

/// Combines @ref HandleState::CloneForChild, and @ref RegisterHandleState
/// since they are frequently used together in this single configuration.
void CreateAndRegisterHandleState(HandleState* parentHandleState, HandleStateKey handleKey);

/// Retrieve common handle state based on a handle and object type enum.
/// Throws if not found.
///
/// @note Locks and unlocks the mutex for the global handle state map
HandleState* GetHandleState(HandleStateKey key);
