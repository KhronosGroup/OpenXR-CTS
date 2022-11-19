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

#include <chrono>
#include <mutex>

#include "composition_utils.h"
#include "event_reader.h"
#include "input_testinputdevice.h"

namespace Conformance
{

    extern const std::chrono::nanoseconds kActionWaitDelay;

    // Manages showing a quad with help text.
    struct ActionLayerManager : public ITestMessageDisplay
    {
        ActionLayerManager(CompositionHelper& compositionHelper);

        EventReader& GetEventReader()
        {
            return m_eventReader;
        }

        RenderLoop& GetRenderLoop()
        {
            return m_renderLoop;
        }

        bool WaitWithMessage(const char* waitMessage, std::function<bool()> frameCallback);

        void WaitForSessionFocusWithMessage();

        bool WaitForLocatability(const std::string& hand, XrSpace space, XrSpace localSpace, XrSpaceLocation* location,
                                 bool expectLocatability);

        // Sync until focus is available, in case focus was lost at some point.
        void SyncActionsUntilFocusWithMessage(const XrActionsSyncInfo& syncInfo);

        // "Sleep", but keep the render loop going on this thread
        template <class Rep, class Period>
        void Sleep_For(const std::chrono::duration<Rep, Period>& sleep_duration)
        {
            const auto startTime = std::chrono::system_clock::now();
            while (std::chrono::system_clock::now() - startTime < sleep_duration) {
                m_renderLoop.IterateFrame();
            }
        }

        bool EndFrame(const XrFrameState& frameState);
        void IterateFrame() override;
        void DisplayMessage(const std::string& message) override;

    private:
        std::mutex m_mutex;

        CompositionHelper& m_compositionHelper;
        const XrSpace m_viewSpace;
        EventReader m_eventReader;
        RenderLoop m_renderLoop;

        std::string m_lastMessage;
        std::unique_ptr<RGBAImage> m_displayMessageImage;

        struct MessageQuad : public XrCompositionLayerQuad
        {
            MessageQuad(CompositionHelper& compositionHelper, std::unique_ptr<RGBAImage> image, XrSpace compositionSpace);
            ~MessageQuad();

        private:
            CompositionHelper& m_compositionHelper;
        };

        std::unique_ptr<MessageQuad> m_messageQuad;
    };
}  // namespace Conformance
