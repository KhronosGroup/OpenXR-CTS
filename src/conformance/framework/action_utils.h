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

#pragma once

#include "RGBAImage.h"
#include "composition_utils.h"
#include "input_testinputdevice.h"
#include "utilities/event_reader.h"

#include <openxr/openxr.h>

#include <chrono>
#include <functional>
#include <iosfwd>
#include <memory>
#include <mutex>

namespace Conformance
{

    extern const std::chrono::nanoseconds kActionWaitDelay;

    /// Manages showing a quad with help text.
    struct ActionLayerManager : public ITestMessageDisplay
    {
        ActionLayerManager(CompositionHelper& compositionHelper);

        /// Access the contained @ref EventReader
        EventReader& GetEventReader() noexcept
        {
            return m_eventReader;
        }

        /// Access the contained @ref RenderLoop
        RenderLoop& GetRenderLoop() noexcept
        {
            return m_renderLoop;
        }

        /// Wait until your callback returns true, while displaying a text message on the display.
        ///
        /// This helper:
        ///
        /// - DOES submit frames
        /// - DOES NOT call `xrSyncActions`
        /// - DOES NOT poll events through this object's EventReader (though the CompositionHelper will poll events)
        bool WaitWithMessage(const char* waitMessage, std::function<bool()> frameCallback);

        /// Submit frames until focus is available, based on waiting for the session state event,
        /// in case focus was lost at some point.
        ///
        /// This helper:
        ///
        /// - DOES submit frames (wraps a call to @ref WaitWithMessage)
        /// - DOES NOT call `xrSyncActions`
        /// - DOES call `xrPollEvent`
        /// - DOES poll events through this object's EventReader
        void WaitForSessionFocusWithMessage();

        /// Waits until xrLocateSpace reports that position/orientation valid flags match @p expectLocatability.
        ///
        /// This helper:
        ///
        /// - DOES submit frames (wraps a call to @ref WaitWithMessage)
        /// - DOES NOT call `xrSyncActions` - you must call it beforehand at least once with the right action set(s) to make
        ///   your action space active!
        /// - DOES NOT poll events through this object's EventReader (though the CompositionHelper will poll events)
        ///
        /// @param hand Hand name for message
        ///
        bool WaitForLocatability(const std::string& hand, XrSpace space, XrSpace localSpace, XrSpaceLocation* location,
                                 bool expectLocatability);

        /// Sync actions until focus is available, observed by xrSyncActions returning XR_SUCCESS instead of XR_SESSION_NOT_FOCUSED,
        /// in case focus was lost at some point.
        ///
        /// This helper:
        ///
        /// - DOES submit frames (wraps a call to @ref WaitWithMessage)
        /// - DOES call `xrSyncActions` - if you do not want to sync actions, see @ref WaitForSessionFocusWithMessage
        /// - DOES NOT poll events through this object's EventReader (though the CompositionHelper will poll events)
        void SyncActionsUntilFocusWithMessage(const XrActionsSyncInfo& syncInfo);

        /// "Sleep", but keep the render loop going on this thread
        ///
        /// This helper:
        ///
        /// - DOES submit frames
        /// - DOES NOT call `xrSyncActions`
        /// - DOES NOT poll events through this object's EventReader (though the CompositionHelper will poll events)
        template <class Rep, class Period>
        void Sleep_For(const std::chrono::duration<Rep, Period>& sleep_duration)
        {
            const auto startTime = std::chrono::system_clock::now();
            while (std::chrono::system_clock::now() - startTime < sleep_duration) {
                m_renderLoop.IterateFrame();
            }
        }

        /// Call `xrEndFrame` via the @ref CompositionHelper, then let it poll for events to decide whether to stop.
        ///
        /// If there was a call to @ref DisplayMessage, a layer for the message will be submitted.
        bool EndFrame(const XrFrameState& frameState);

        /// Calls `xrWaitFrame`, `xrBeginFrame`, and `xrEndFrame`, delegating to the owned @ref RenderLoop
        void IterateFrame() override;

        /// Display a message on the console and in the immersive environment.
        ///
        /// Prepares a static swapchain with the message for use the next time @ref EndFrame is called,
        /// directly or indirectly, through this helper object.
        /// (Does not directly submit frames!)
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
