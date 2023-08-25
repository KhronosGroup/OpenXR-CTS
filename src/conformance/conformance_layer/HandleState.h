// Copyright (c) 2019-2023, The Khronos Group Inc.
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

struct ICustomHandleState
{
    virtual ~ICustomHandleState() = default;
};

using IntHandle = uint64_t;   // A common type for all handles so a single map can be used.
struct ConformanceHooksBase;  // forward-declare

// Common state kept around for all XR handles.
struct HandleState
{
    HandleState(IntHandle handle_, XrObjectType type, HandleState* parent, std::shared_ptr<ConformanceHooksBase> conformanceHooks)
        : handle(handle_), type(type), conformanceHooks(std::move(conformanceHooks)), parent(parent)
    {
    }

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

    std::vector<HandleState*> children;

    // Additional data stored by the hand-coded validations.
    std::unique_ptr<ICustomHandleState> customState;
};

// Inherit from std::runtime_error so it can be caught in the ABI boundary.
struct HandleException : public std::runtime_error
{
    HandleException(const std::string& message) : std::runtime_error(message)
    {
    }
};

using HandleStateKey = std::pair<IntHandle, XrObjectType>;

void UnregisterHandleStateInternal(std::unique_lock<std::mutex>& lockProof, HandleStateKey key);
void UnregisterHandleState(HandleStateKey key);
void RegisterHandleState(std::unique_ptr<HandleState> handleState);

HandleState* GetHandleState(HandleStateKey key);
