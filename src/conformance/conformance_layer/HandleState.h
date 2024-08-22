// Copyright (c) 2019-2024, The Khronos Group Inc.
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
    virtual ~ICustomHandleState() = default;
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
    std::unique_ptr<HandleState> CloneForChild(IntHandle handle_, XrObjectType childType)
    {
        // Note that the cloned HandleState will start with a null customState and no children.
        auto childState = std::unique_ptr<HandleState>(new HandleState(handle_, childType, this /* parent */, conformanceHooks));

        {
            std::unique_lock<std::mutex> lock(mutex);
            children.push_back(childState.get());
        }

        return childState;
    }

    const IntHandle handle;

    const XrObjectType type;

    const std::shared_ptr<ConformanceHooksBase> conformanceHooks;

    HandleState* const parent;

    mutable std::mutex mutex;

    /// Non-owning pointers to handle state of child handles.
    std::vector<HandleState*> children;

    /// Additional data stored by the hand-coded validations.
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

using HandleStateKey = std::pair<IntHandle, XrObjectType>;

void UnregisterHandleStateInternal(std::unique_lock<std::mutex>& lockProof, HandleStateKey key);
void UnregisterHandleState(HandleStateKey key);
void RegisterHandleState(std::unique_ptr<HandleState> handleState);

/// Retrieve common handle state based on a handle and object type enum.
/// Throws if not found.
HandleState* GetHandleState(HandleStateKey key);
