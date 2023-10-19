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
#pragma once

#include <queue>
#include <stdint.h>

namespace Conformance
{
    /// Tracks some kind of owned resource and the corresponding fence value at which it can be released.
    template <typename OwnedResource>
    class DestructionQueue
    {
    public:
        /// Push some thing you can de-allocate following a fence value.
        ///
        /// Move your ownership into this method, and the container will release it at some future @ref ReleaseForFenceValue call
        ///
        /// @param fenceValue the fence value you signaled after finishing use of the resources
        /// @param resource a resource owner
        void PushResource(uint64_t fenceValue, OwnedResource&& resource)
        {
            resourcesAwaitingDestruction.emplace(fenceValue, std::move(resource));
        }

        // /// Push more than one thing to de-allocate after a fence value.
        // ///
        // /// @param fenceValue the fence value you signaled after finishing use of the resources
        // /// @param begin the begin iterator
        // /// @param end the past-the-end iterator
        // ///
        // /// The iterator arguments match the conventional usage of std::begin/std::end, std::vector::begin/std::vector::end, etc.
        // template <typename It>
        // void PushResources(uint64_t fenceValue, It begin, It end)
        // {
        //     for (; begin != end; ++begin) {
        //         PushResource(fenceValue, std::move(*begin));
        //     }
        // }

        /// Push more than one thing to de-allocate after a fence value.
        ///
        /// @param fenceValue the fence value you signaled after finishing use of the resources
        /// @param resources the resources in a vector you move in
        void PushResources(uint64_t fenceValue, std::vector<OwnedResource>&& resources)
        {
            for (auto res : resources) {
                PushResource(fenceValue, std::move(res));
            }
        }

        /// Release all resources associated with a fence value less than or equal to the parameter.
        ///
        /// @param completedFenceValue the completed fence value from the fence.
        void ReleaseForFenceValue(uint64_t completedFenceValue)
        {
            while (!resourcesAwaitingDestruction.empty() && resourcesAwaitingDestruction.top().fenceValue <= completedFenceValue) {
                resourcesAwaitingDestruction.pop();
            }
        }

    private:
        struct QueueEntry
        {
            uint64_t fenceValue;
            OwnedResource resource;

            QueueEntry(uint64_t fenceValue, OwnedResource res) : fenceValue(fenceValue), resource(std::move(res))
            {
            }
        };

        struct QueueLater
        {
            /// true if lhs will be ready later than rhs
            bool operator()(const QueueEntry& lhs, const QueueEntry& rhs) const noexcept
            {
                return lhs.fenceValue > rhs.fenceValue;
            }
        };

        // Priority queue is a "max queue" that uses std::less by default: the comparison functor returns true if LHS is "lower priority" (later in array, etc) than RHS
        // todo: consider wrapping deque here instead of vector
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueLater /* std::greater<QueueEntry> */> resourcesAwaitingDestruction;
    };
}  // namespace Conformance
