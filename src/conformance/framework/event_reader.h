// Copyright (c) 2019-2022, The Khronos Group Inc.
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
#include <mutex>
#include <openxr/openxr.h>

namespace Conformance
{
    /**
     * @defgroup cts_eventreader Event Reader/Queue
     * @ingroup cts_framework
     */
    /// @{

    /// Buffered collection of all events read. Only accessible through an @ref EventReader.
    class EventQueue
    {
    public:
        explicit EventQueue(XrInstance instance);

    private:
        friend class EventReader;  // ;-)

        void ReadEvents() const;

        const XrInstance m_instance;
        mutable std::mutex m_mutex;
        mutable std::vector<XrEventDataBuffer> m_events;
    };

    /// Reads all events added to the @ref EventQueue after this object was created.
    /// Separate EventReaders from the same @ref EventQueue will not impact each other.
    /// This allows different parts of the tests to read events without impacting each other (event multiplexing).
    class EventReader
    {
    public:
        explicit EventReader(const EventQueue& eventQueue);

        bool TryReadNext(XrEventDataBuffer& dataBuffer);

        bool TryReadUntilEvent(XrEventDataBuffer& dataBuffer, XrStructureType eventType);

        void ReadUntilEmpty();

    private:
        const EventQueue& m_eventQueue;
        size_t m_nextEventIndex;
    };
    /// @}

}  // namespace Conformance
