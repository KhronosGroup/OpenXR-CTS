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

#include <vector>
#include <stdint.h>

namespace Conformance
{
    /// Wraps a vector to keep track of collections of things referenced by a type-safe handle.
    /// The handle consists of the index in the vector combined with a "generation number" which is
    /// incremented every time the container is cleared.
    ///
    /// Used with things like @ref MeshHandle, inside the graphics plugin implementations
    template <typename T, typename HandleType>
    class VectorWithGenerationCountedHandles
    {
    public:
        template <typename... Args>
        HandleType emplace_back(Args&&... args)
        {
            auto index = m_data.size();
            m_data.emplace_back(std::forward<Args&&>(args)...);
            return HandleType{index | (static_cast<uint64_t>(m_generationNumber) << kGenerationShift)};
        }

        T& operator[](HandleType h)
        {
            return m_data[checkHandleAndGetIndex(h)];
        }

        const T& operator[](HandleType h) const
        {
            return m_data[checkHandleAndGetIndex(h)];
        }

        void clear()
        {
            m_generationNumber++;
            m_data.clear();
        }

    private:
        uint32_t checkHandleAndGetIndex(HandleType h) const
        {

            if (h == HandleType{}) {
                throw std::logic_error("Internal CTS error: Trying to use a null graphics handle!");
            }
            auto generation = static_cast<uint32_t>(h.get() >> kGenerationShift);
            if (generation != m_generationNumber) {
                throw std::logic_error(
                    "Internal CTS error: Trying to use a graphics handle left over from before a Shutdown() or ShutdownDevice() call!");
            }
            auto index = static_cast<uint32_t>(h.get());
            return index;
        }
        static constexpr size_t kGenerationShift = 32;
        std::vector<T> m_data;
        uint32_t m_generationNumber{1};
    };

}  // namespace Conformance
