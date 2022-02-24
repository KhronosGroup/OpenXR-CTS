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

#include "event_reader.h"
#include "conformance_framework.h"

EventQueue::EventQueue(XrInstance instance) : m_instance(instance)
{
}

void EventQueue::ReadEvents() const
{
    XrResult pollRes;
    XrEventDataBuffer eventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
    while ((pollRes = xrPollEvent(m_instance, &eventDataBuffer)) == XR_SUCCESS) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_events.push_back(eventDataBuffer);
        eventDataBuffer.type = XR_TYPE_EVENT_DATA_BUFFER;
        eventDataBuffer.next = nullptr;
    }

    XRC_CHECK_THROW_XRRESULT(pollRes, "xrPollEvent");
}

EventReader::EventReader(const EventQueue& eventQueue) : m_eventQueue(eventQueue), m_nextEventIndex(eventQueue.m_events.size())
{
}

bool EventReader::TryReadNext(XrEventDataBuffer& dataBuffer)
{
    m_eventQueue.ReadEvents();

    std::unique_lock<std::mutex> lock(m_eventQueue.m_mutex);
    if (m_nextEventIndex >= m_eventQueue.m_events.size()) {
        return false;
    }

    dataBuffer = m_eventQueue.m_events[m_nextEventIndex++];
    return true;
}

bool EventReader::TryReadUntilEvent(XrEventDataBuffer& dataBuffer, XrStructureType eventType)
{
    while (TryReadNext(dataBuffer)) {
        if (dataBuffer.type == eventType) {
            return true;
        }
    }

    return false;
}

void EventReader::ReadUntilEmpty()
{
    m_eventQueue.ReadEvents();

    std::unique_lock<std::mutex> lock(m_eventQueue.m_mutex);
    m_nextEventIndex = m_eventQueue.m_events.size();
}
