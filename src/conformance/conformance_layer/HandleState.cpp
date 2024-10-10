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

#include "HandleState.h"

#include "Common.h"

#include <algorithm>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <stddef.h>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    struct HandleStateKeyHash
    {
        std::size_t operator()(const HandleStateKey& k) const
        {
            // Combine hashes of both handle value and object type enum.
            return std::hash<IntHandle>()(k.first) ^ std::hash<int>()((int)k.second);
        }
    };

    std::unordered_map<HandleStateKey, std::unique_ptr<HandleState>, HandleStateKeyHash> g_handleStates;
    std::mutex g_handleStatesMutex;
}  // namespace

void RegisterHandleState(std::unique_ptr<HandleState> handleState)
{
    std::unique_lock<std::mutex> lock(g_handleStatesMutex);
    HandleStateKey mapKey(handleState->handle, handleState->type);
    auto it = g_handleStates.insert(std::pair<HandleStateKey, std::unique_ptr<HandleState>>(mapKey, std::move(handleState)));
    if (!it.second) {
        throw HandleException(std::string("Encountered duplicate ") + to_string(mapKey.second) + " handle with value " +
                              std::to_string(mapKey.first));
    }
}

void UnregisterHandleStateInternal(std::unique_lock<std::mutex>& lockProof, HandleStateKey key)
{
    auto it = g_handleStates.find(key);
    if (it == g_handleStates.end()) {
        throw HandleException(std::string("Encountered unknown ") + to_string(key.second) + " handle with value " +
                              std::to_string(key.first));
    }

    // Unregister children from map (recursively)
    {
        std::unique_lock<std::recursive_mutex> lock(it->second->childrenMutex);
        while (!it->second->children.empty()) {
            // Unregistering the child will cause it to be removed from the list of children.
            HandleState* const frontChild = it->second->children.front();
            UnregisterHandleStateInternal(lockProof, HandleStateKey(frontChild->handle, frontChild->type));
        }
    }

    if (it->second->parent != nullptr) {  // XrInstance has no parent
        // Remove self from parent's list of children
        std::unique_lock<std::recursive_mutex> lock(it->second->parent->childrenMutex);
        std::vector<HandleState*>& siblings = it->second->parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), it->second.get()), siblings.end());
    }

    // Finally remove self from map.
    g_handleStates.erase(it);
}

void UnregisterHandleState(HandleStateKey key)
{
    std::unique_lock<std::mutex> lock(g_handleStatesMutex);
    UnregisterHandleStateInternal(lock, key);
}

HandleState* GetHandleState(HandleStateKey key)
{
    std::unique_lock<std::mutex> lock(g_handleStatesMutex);
    auto it = g_handleStates.find(key);
    if (it == g_handleStates.end()) {
        throw HandleNotFoundException(std::string("Encountered unknown ") + to_string(key.second) + " handle with value " +
                                      std::to_string(key.first));
    }
    return it->second.get();
}
