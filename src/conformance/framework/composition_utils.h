// Copyright (c) 2019-2021, The Khronos Group Inc.
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

#include <list>
#include <vector>
#include <map>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>
#include <openxr/openxr.h>
#include "graphics_plugin.h"
#include "event_reader.h"
#include "conformance_utils.h"
#include "conformance_framework.h"

namespace Conformance
{
    RGBAImage CreateTextImage(int width, int height, const char* text, int fontHeight);

    XrPath StringToPath(XrInstance instance, const char* pathStr);

    using UpdateLayers = std::function<void(const XrFrameState&)>;
    using EndFrame = std::function<bool(const XrFrameState&)>;  // Return false to stop the loop.

    class RenderLoop
    {
    public:
        RenderLoop(XrSession session, EndFrame endFrame) : m_session(session), m_endFrame(endFrame)
        {
        }

        bool IterateFrame();
        void Loop();

        XrTime GetLastPredictedDisplayTime() const;

    private:
        XrSession m_session;
        EndFrame m_endFrame;
        std::atomic<XrTime> m_lastPredictedDisplayTime;
    };

    struct InteractionManager
    {
        InteractionManager(XrInstance instance, XrSession session);

        void AddActionBindings(XrPath interactionProfile, std::vector<XrActionSuggestedBinding> bindings);

        void AddActionSet(XrActionSet actionSet);
        void AttachActionSets();
        void SyncActions(XrPath subactionPath);

    private:
        XrInstance m_instance;
        XrSession m_session;
        std::map<XrPath, std::vector<XrActionSuggestedBinding>> m_bindings;
        std::vector<XrActionSet> m_actionSets;
    };

    struct CompositionHelper
    {
        CompositionHelper(const char* testName);
        ~CompositionHelper();

        InteractionManager& GetInteractionManager();

        XrInstance GetInstance() const;
        XrSession GetSession() const;

        std::vector<XrViewConfigurationView> EnumerateConfigurationViews();

        XrViewConfigurationProperties GetViewConfigurationProperties();

        void BeginSession();

        std::tuple<XrViewState, std::vector<XrView>> LocateViews(XrSpace space, int64_t displayTime);

        bool PollEvents();

        EventQueue& GetEventQueue() const;

        void EndFrame(XrTime predictedDisplayTime, std::vector<XrCompositionLayerBaseHeader*> layers);

        void AcquireWaitReleaseImage(XrSwapchain swapchain, std::function<void(const XrSwapchainImageBaseHeader*, uint64_t)> doUpdate);

        XrSpace CreateReferenceSpace(XrReferenceSpaceType type, XrPosef pose = XrPosefCPP());

        XrSwapchainCreateInfo DefaultColorSwapchainCreateInfo(uint32_t width, uint32_t height, XrSwapchainCreateFlags createFlags = 0,
                                                              int64_t format = -1);

        XrSwapchain CreateSwapchain(const XrSwapchainCreateInfo& createInfo);

        void DestroySwapchain(XrSwapchain swapchain);

        XrSwapchain CreateStaticSwapchainSolidColor(const XrColor4f& color);

        XrSwapchain CreateStaticSwapchainImage(const RGBAImage& rgbaImage);

        XrSwapchainSubImage MakeDefaultSubImage(XrSwapchain swapchain, uint32_t imageArrayIndex = 0);

        XrCompositionLayerQuad* CreateQuadLayer(XrSwapchain swapchain, XrSpace space, float width, XrPosef pose = XrPosefCPP());

        XrCompositionLayerProjection* CreateProjectionLayer(XrSpace space);

    private:
        std::mutex m_mutex;

        InstanceREQUIRE m_instance;
        XrSession m_session;
        XrSystemId m_systemId;

        std::unique_ptr<EventQueue> m_eventQueue;
        std::unique_ptr<EventReader> m_privateEventReader;

        std::unique_ptr<InteractionManager> m_interactionManager;

        uint64_t m_defaultColorFormat;
        XrViewConfigurationType m_primaryViewType;
        uint32_t m_projectionViewCount{0};

        std::list<XrCompositionLayerProjection> m_projections;
        std::list<std::vector<XrCompositionLayerProjectionView>> m_projectionViews;
        std::list<XrCompositionLayerQuad> m_quads;

        std::map<XrSwapchain, XrSwapchainCreateInfo> m_createdSwapchains;
        std::map<XrSwapchain, std::shared_ptr<Conformance::IGraphicsPlugin::SwapchainImageStructs>> m_swapchainImages;
        std::vector<XrSpace> m_spaces;

        // For the menu overlays:
        XrSpace m_viewSpace{XR_NULL_HANDLE};

        XrCompositionLayerQuad m_testNameQuad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    };

    // Helper class to provide simple world-locked projection layer of some cubes. Each view of the projection is a separate swapchain.
    class SimpleProjectionLayerHelper
    {
    public:
        SimpleProjectionLayerHelper(CompositionHelper& compositionHelper);
        XrCompositionLayerBaseHeader* GetProjectionLayer() const;
        void UpdateProjectionLayer(const XrFrameState& frameState,
                                   const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}),
                                                                    Cube::Make({0, -1, -2}), Cube::Make({0, 1, -2})});
        XrSpace GetLocalSpace() const
        {
            return m_localSpace;
        }

    private:
        CompositionHelper& m_compositionHelper;
        XrSpace m_localSpace;
        XrCompositionLayerProjection* m_projLayer;
        std::vector<XrSwapchain> m_swapchains;
    };
}  // namespace Conformance
