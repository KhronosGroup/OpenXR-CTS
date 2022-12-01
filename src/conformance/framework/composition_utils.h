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

#include <list>
#include <utility>
#include <vector>
#include <map>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>
#include <openxr/openxr.h>
#include <xr_linear.h>
#include "graphics_plugin.h"
#include "event_reader.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "throw_helpers.h"

namespace Conformance
{
    RGBAImage CreateTextImage(int width, int height, const char* text, int fontHeight);

    XrPath StringToPath(XrInstance instance, const char* pathStr);

    using UpdateLayers = std::function<void(const XrFrameState&)>;
    using EndFrame = std::function<bool(const XrFrameState&)>;  // Return false to stop the loop.

    class RenderLoop
    {
    public:
        RenderLoop(XrSession session, EndFrame endFrame)
            : m_session(session), m_endFrame(std::move(endFrame)), m_lastPredictedDisplayTime(0)
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
        void AttachActionSets(std::vector<XrPath>* assertInteractionProfilePath = nullptr);
        void SyncActions(XrPath subactionPath);

    private:
        XrInstance m_instance;
        XrSession m_session;
        std::map<XrPath, std::vector<XrActionSuggestedBinding>> m_bindings;
        // Some tests require control of the binding order
        std::vector<XrPath> m_bindingsOrder;
        std::vector<XrActionSet> m_actionSets;
    };

    struct CompositionHelper
    {
        CompositionHelper(const char* testName, const std::vector<const char*>& additionalEnabledExtensions = std::vector<const char*>());
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

        void AcquireWaitReleaseImage(XrSwapchain swapchain, std::function<void(const XrSwapchainImageBaseHeader*, int64_t)> doUpdate);

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

    /// Helper class to provide projection layer rendering. Each view of the projection is a separate swapchain.
    /// Typically wrapped by another utility providing an implementation of @ref BaseProjectionLayerHelper::ViewRenderer
    class BaseProjectionLayerHelper
    {
    public:
        BaseProjectionLayerHelper(CompositionHelper& compositionHelper, XrReferenceSpaceType spaceType);

        class ViewRenderer
        {
        public:
            virtual ~ViewRenderer();
            /// Usually must call  @ref IGraphicsPlugin::ClearImageSlice with @p swapchainImage , array index 0,
            /// @p format , and a background color of choice.
            /// Must call IGraphicsPlugin::RenderView with @p projectionView , @p swapchainImage , @p format ,
            /// and the geometry to draw.
            /// Projection view pose/fov fields are preset to match the corresponding view fields.
            /// Views are located relative to GetLocalSpace()
            virtual void RenderView(const BaseProjectionLayerHelper& projectionLayerHelper, uint32_t viewIndex,
                                    const XrViewState& viewState, const XrView& view, XrCompositionLayerProjectionView& projectionView,
                                    const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) = 0;
        };

        /// Gets view state/location, then for each view, calls your ViewRenderer from within
        /// CompositionHelper::AcquireWaitReleaseImage after clearing the image slice for you.
        XrCompositionLayerBaseHeader* TryGetUpdatedProjectionLayer(const XrFrameState& frameState, ViewRenderer& renderer);

        XrSpace GetLocalSpace() const
        {
            return m_localSpace;
        }

        uint32_t GetViewCount() const
        {
            return m_projLayer->viewCount;
        }

    private:
        CompositionHelper& m_compositionHelper;
        XrSpace m_localSpace;
        XrCompositionLayerProjection* m_projLayer;
        std::vector<XrSwapchain> m_swapchains;
    };

    // Helper class to provide simple world-locked projection layer of some cubes. Each view of the projection is a separate swapchain.
    class SimpleProjectionLayerHelper
    {
    public:
        SimpleProjectionLayerHelper(CompositionHelper& compositionHelper) : m_baseHelper(compositionHelper, XR_REFERENCE_SPACE_TYPE_LOCAL)
        {
        }

        XrCompositionLayerBaseHeader* TryGetUpdatedProjectionLayer(const XrFrameState& frameState,
                                                                   const std::vector<Cube>& cubes = {
                                                                       Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}),
                                                                       Cube::Make({0, -1, -2}), Cube::Make({0, 1, -2})})
        {
            ViewRenderer renderer(cubes);
            return m_baseHelper.TryGetUpdatedProjectionLayer(frameState, renderer);
        }

        XrSpace GetLocalSpace() const
        {
            return m_baseHelper.GetLocalSpace();
        }

    private:
        BaseProjectionLayerHelper m_baseHelper;
        class ViewRenderer : public BaseProjectionLayerHelper::ViewRenderer
        {
        public:
            ViewRenderer(const std::vector<Cube>& cubes) : m_cubes(cubes)
            {
            }

            ~ViewRenderer() override = default;
            void RenderView(const BaseProjectionLayerHelper& /* projectionLayerHelper */, uint32_t /* viewIndex */,
                            const XrViewState& /* viewState */, const XrView& /* view */, XrCompositionLayerProjectionView& projectionView,
                            const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) override
            {
                GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, format);
                GetGlobalData().graphicsPlugin->RenderView(projectionView, swapchainImage, format, RenderParams{}.Draw(m_cubes));
            }

        private:
            const std::vector<Cube>& m_cubes;
        };
    };
}  // namespace Conformance

namespace
{
    using namespace Conformance;

    const XrVector3f UpVector{0, 1, 0};

    enum class LayerMode
    {
        Scene,
        Help,
        Complete
    };

    namespace Colors
    {
        constexpr XrColor4f Red = {1, 0, 0, 1};
        constexpr XrColor4f Green = {0, 1, 0, 1};
        constexpr XrColor4f GreenZeroAlpha = {0, 1, 0, 0};
        constexpr XrColor4f Blue = {0, 0, 1, 1};
        constexpr XrColor4f Yellow = {1, 1, 0, 1};
        constexpr XrColor4f Orange = {1, 0.65f, 0, 1};
        constexpr XrColor4f Transparent = {0, 0, 0, 0};

        // Avoid including red which is a "failure color".
        constexpr std::array<XrColor4f, 4> UniqueColors{Green, Blue, Yellow, Orange};
    }  // namespace Colors

    namespace Math
    {
        // Do a linear conversion of a number from one range to another range.
        // e.g. 5 in range [0-8] projected into range (-.6 to 0.6) is 0.15.
        static inline float LinearMap(int i, int sourceMin, int sourceMax, float targetMin, float targetMax)
        {
            float percent = (i - sourceMin) / (float)sourceMax;
            return targetMin + ((targetMax - targetMin) * percent);
        }

        static inline constexpr float DegToRad(float degree)
        {
            return degree / 180 * MATH_PI;
        }
    }  // namespace Math

    namespace Quat
    {
        constexpr XrQuaternionf Identity{0, 0, 0, 1};

        static inline XrQuaternionf FromAxisAngle(XrVector3f axis, float radians)
        {
            XrQuaternionf rowQuat;
            XrQuaternionf_CreateFromAxisAngle(&rowQuat, &axis, radians);
            return rowQuat;
        }
    }  // namespace Quat

    // Appends composition layers for interacting with interactive composition tests.
    struct InteractiveLayerManager
    {
        InteractiveLayerManager(CompositionHelper& compositionHelper, const char* exampleImage, const char* descriptionText)
            : m_compositionHelper(compositionHelper)
        {
            // Set up the input system for toggling between modes and passing/failing.
            {
                XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                strcpy(actionSetInfo.actionSetName, "interaction_test");
                strcpy(actionSetInfo.localizedActionSetName, "Interaction Test");
                XRC_CHECK_THROW_XRCMD(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetInfo, &m_actionSet));

                compositionHelper.GetInteractionManager().AddActionSet(m_actionSet);

                XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy(actionInfo.actionName, "interaction_manager_select");
                strcpy(actionInfo.localizedActionName, "Interaction Manager Select");
                XRC_CHECK_THROW_XRCMD(xrCreateAction(m_actionSet, &actionInfo, &m_select));

                strcpy(actionInfo.actionName, "interaction_manager_menu");
                strcpy(actionInfo.localizedActionName, "Interaction Manager Menu");
                XRC_CHECK_THROW_XRCMD(xrCreateAction(m_actionSet, &actionInfo, &m_menu));

                XrPath simpleInteractionProfile =
                    StringToPath(compositionHelper.GetInstance(), "/interaction_profiles/khr/simple_controller");
                compositionHelper.GetInteractionManager().AddActionBindings(
                    simpleInteractionProfile,
                    {{
                        {m_select, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/select/click")},
                        {m_select, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/select/click")},
                        {m_menu, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/menu/click")},
                        {m_menu, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/menu/click")},
                    }});
            }

            m_viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{{0, 0, 0, 1}, {0, 0, 0}});

            // Load example screenshot if available and set up the quad layer for it.
            {
                XrSwapchain exampleSwapchain;
                if (exampleImage) {
                    exampleSwapchain = compositionHelper.CreateStaticSwapchainImage(RGBAImage::Load(exampleImage));
                }
                else {
                    RGBAImage image(256, 256);
                    image.PutText(XrRect2Di{{0, image.height / 2}, {image.width, image.height}}, "Example Not Available", 64, {1, 0, 0, 1});
                    exampleSwapchain = compositionHelper.CreateStaticSwapchainImage(image);
                }

                // Create a quad to the right of the help text.
                m_exampleQuad = compositionHelper.CreateQuadLayer(exampleSwapchain, m_viewSpace, 1.25f, {Quat::Identity, {0.5f, 0, -1.5f}});
                XrQuaternionf_CreateFromAxisAngle(&m_exampleQuad->pose.orientation, &UpVector, -15 * MATH_PI / 180);
            }

            // Set up the quad layer for showing the help text to the left of the example image.
            m_descriptionQuad = compositionHelper.CreateQuadLayer(
                m_compositionHelper.CreateStaticSwapchainImage(CreateTextImage(768, 768, descriptionText, 48)), m_viewSpace, 0.75f,
                {Quat::Identity, {-0.5f, 0, -1.5f}});
            m_descriptionQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            XrQuaternionf_CreateFromAxisAngle(&m_descriptionQuad->pose.orientation, &UpVector, 15 * MATH_PI / 180);

            constexpr uint32_t actionsWidth = 768, actionsHeight = 128;
            m_sceneActionsSwapchain = compositionHelper.CreateStaticSwapchainImage(
                CreateTextImage(actionsWidth, actionsHeight, "Press Select to PASS. Press Menu for description", 48));
            m_helpActionsSwapchain =
                compositionHelper.CreateStaticSwapchainImage(CreateTextImage(actionsWidth, actionsHeight, "Press select to FAIL", 48));

            // Set up the quad layer and swapchain for showing what actions the user can take in the Scene/Help mode.
            m_actionsQuad =
                compositionHelper.CreateQuadLayer(m_sceneActionsSwapchain, m_viewSpace, 0.75f, {Quat::Identity, {0, -0.4f, -1}});
            m_actionsQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        }

        template <typename T>
        void AddLayer(T* layer)
        {
            m_sceneLayers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(layer));
        }

        bool EndFrame(const XrFrameState& frameState, std::vector<XrCompositionLayerBaseHeader*> layers = {})
        {
            bool keepRunning = AppendLayers(layers);
            keepRunning &= m_compositionHelper.PollEvents();
            m_compositionHelper.EndFrame(frameState.predictedDisplayTime, std::move(layers));
            return keepRunning;
        }

    private:
        bool AppendLayers(std::vector<XrCompositionLayerBaseHeader*>& layers)
        {
            // Add layer(s) based on the interaction mode.
            switch (GetLayerMode()) {
            case LayerMode::Scene:
                m_actionsQuad->subImage = m_compositionHelper.MakeDefaultSubImage(m_sceneActionsSwapchain);
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(m_actionsQuad));

                for (auto& sceneLayer : m_sceneLayers) {
                    layers.push_back(sceneLayer);
                }
                break;

            case LayerMode::Help:
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(m_descriptionQuad));
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(m_exampleQuad));

                m_actionsQuad->subImage = m_compositionHelper.MakeDefaultSubImage(m_helpActionsSwapchain);
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(m_actionsQuad));
                break;

            case LayerMode::Complete:
                return false;  // Interactive test is complete.
            }

            return true;
        }

        LayerMode GetLayerMode()
        {
            m_compositionHelper.GetInteractionManager().SyncActions(XR_NULL_PATH);

            XrActionStateBoolean actionState{XR_TYPE_ACTION_STATE_BOOLEAN};
            XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};

            LayerMode mode = LayerMode::Scene;
            getInfo.action = m_menu;
            XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(m_compositionHelper.GetSession(), &getInfo, &actionState));
            if (actionState.currentState) {
                mode = LayerMode::Help;
            }

            getInfo.action = m_select;
            XRC_CHECK_THROW_XRCMD(xrGetActionStateBoolean(m_compositionHelper.GetSession(), &getInfo, &actionState));
            if (actionState.changedSinceLastSync && actionState.currentState) {
                if (mode != LayerMode::Scene) {
                    // Select on the non-Scene modes (help description/preview image) means FAIL and move to the next.
                    FAIL("User failed the interactive test");
                }

                // Select on scene means PASS and move to next
                mode = LayerMode::Complete;
            }

            return mode;
        }

        CompositionHelper& m_compositionHelper;

        XrActionSet m_actionSet{XR_NULL_HANDLE};
        XrAction m_select{XR_NULL_HANDLE};
        XrAction m_menu{XR_NULL_HANDLE};

        XrSpace m_viewSpace;
        XrSwapchain m_sceneActionsSwapchain;
        XrSwapchain m_helpActionsSwapchain;
        XrCompositionLayerQuad* m_actionsQuad;
        XrCompositionLayerQuad* m_descriptionQuad;
        XrCompositionLayerQuad* m_exampleQuad;
        std::vector<XrCompositionLayerBaseHeader*> m_sceneLayers;
    };
}  // namespace
