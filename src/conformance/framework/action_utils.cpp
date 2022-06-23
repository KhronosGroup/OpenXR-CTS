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

#include "action_utils.h"
#include "composition_utils.h"
#include "report.h"

using namespace std::chrono_literals;

namespace Conformance
{

// On android platforms sleeping the main thread stalls the interactive tests
#ifdef XR_USE_PLATFORM_ANDROID
    const std::chrono::nanoseconds kActionWaitDelay = 0ms;
#else
    const std::chrono::nanoseconds kActionWaitDelay = 5ms;
#endif  // XR_USE_PLATFORM_ANDROID

    ActionLayerManager::ActionLayerManager(CompositionHelper& compositionHelper)
        : m_compositionHelper(compositionHelper)
        , m_viewSpace(compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW))
        , m_eventReader(m_compositionHelper.GetEventQueue())
        , m_renderLoop(m_compositionHelper.GetSession(), [&](const XrFrameState& frameState) { return EndFrame(frameState); })
    {
    }

    void ActionLayerManager::WaitWithMessage(const char* waitMessage, std::function<bool()> frameCallback)
    {
        bool messageDisplayed = false;
        bool waitCompleted = WaitUntilPredicateWithTimeout(
            [&]() {
                m_renderLoop.IterateFrame();
                bool completed = frameCallback();
                if (!completed && !messageDisplayed) {
                    messageDisplayed = true;
                    DisplayMessage(waitMessage);
                }
                return completed;
            },
            20s, kActionWaitDelay);

        REQUIRE_MSG(waitCompleted, std::string("Time out: ") + waitMessage);
        DisplayMessage("");
    }

    void ActionLayerManager::WaitForSessionFocusWithMessage()
    {
        XrSession session = m_compositionHelper.GetSession();

        WaitWithMessage("Waiting for session focus...", [&]() {
            XrEventDataBuffer eventData;
            while (m_eventReader.TryReadNext(eventData)) {
                if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    auto sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                    if (sessionStateChanged->session == session && sessionStateChanged->state == XR_SESSION_STATE_FOCUSED) {
                        return true;
                    }
                }
            }
            return false;
        });
    }

    void ActionLayerManager::SyncActionsUntilFocusWithMessage(const XrActionsSyncInfo& syncInfo)
    {
        WaitWithMessage("Waiting for session focus...", [&] {
            XrResult res = xrSyncActions(m_compositionHelper.GetSession(), &syncInfo);
            REQUIRE_RESULT_SUCCEEDED(res);
            return XR_UNQUALIFIED_SUCCESS(res);  // XR_SUCCESS means there is focus, as opposed to XR_SESSION_NOT_FOCUSED.
        });
    }

    bool ActionLayerManager::EndFrame(const XrFrameState& frameState)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_displayMessageImage) {
            m_messageQuad = std::make_unique<MessageQuad>(m_compositionHelper, std::move(m_displayMessageImage), m_viewSpace);
        }

        std::vector<XrCompositionLayerBaseHeader*> layers;
        if (m_messageQuad != nullptr) {
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(m_messageQuad.get()));
        }
        m_compositionHelper.EndFrame(frameState.predictedDisplayTime, std::move(layers));
        return m_compositionHelper.PollEvents();
    }

    void ActionLayerManager::IterateFrame()
    {
        m_renderLoop.IterateFrame();
    }

    void ActionLayerManager::DisplayMessage(const std::string& message)
    {
        if (message == m_lastMessage) {
            return;  // No need to regenerate the swapchain.
        }

        if (!message.empty()) {
            ReportStr(("Interaction message: " + message).c_str());
        }

        constexpr int TitleFontHeightPixels = 40;
        constexpr int TitleFontPaddingPixels = 2;
        constexpr int TitleBorderPixels = 2;
        constexpr int InsetPixels = TitleBorderPixels + TitleFontPaddingPixels;

        std::lock_guard<std::mutex> lock(m_mutex);

        auto image = std::make_unique<RGBAImage>(768, (TitleFontHeightPixels + InsetPixels * 2) * 5);
        if (!message.empty()) {
            image->DrawRect(0, 0, image->width, image->height, {0.25f, 0.25f, 0.25f, 0.25f});
            image->DrawRectBorder(0, 0, image->width, image->height, TitleBorderPixels, {0.5f, 0.5f, 0.5f, 1});
            image->PutText(XrRect2Di{{InsetPixels, InsetPixels}, {image->width - InsetPixels * 2, image->height - InsetPixels * 2}},
                           message.c_str(), TitleFontHeightPixels, {1, 1, 1, 1});
        }

        m_displayMessageImage = std::move(image);
        m_lastMessage = message;
    }

    ActionLayerManager::MessageQuad::MessageQuad(CompositionHelper& compositionHelper, std::unique_ptr<RGBAImage> image,
                                                 XrSpace compositionSpace)
        : m_compositionHelper(compositionHelper)
    {
        const XrSwapchain messageSwapchain = m_compositionHelper.CreateStaticSwapchainImage(*image);

        *static_cast<XrCompositionLayerQuad*>(this) = {XR_TYPE_COMPOSITION_LAYER_QUAD};
        size.width = 1;
        size.height = size.width * image->height / image->width;
        pose = XrPosef{{0, 0, 0, 1}, {0, 0, -1.5f}};
        subImage = m_compositionHelper.MakeDefaultSubImage(messageSwapchain);
        space = compositionSpace;
    }

    ActionLayerManager::MessageQuad::~MessageQuad()
    {
        if (subImage.swapchain) {
            m_compositionHelper.DestroySwapchain(subImage.swapchain);
        }
    }

}  // namespace Conformance
