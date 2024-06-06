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
#include "utilities/colors.h"
#include "common/xr_linear.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "graphics_plugin.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <vector>

namespace Conformance
{
    class EventQueue;
    class EventReader;
    class ISwapchainImageData;

    RGBAImage CreateTextImage(int32_t width, int32_t height, const char* text, int32_t fontHeight, WordWrap wordWrap = WordWrap::Enabled);

    XrPath StringToPath(XrInstance instance, const std::string& pathStr);

    using UpdateLayers = std::function<void(const XrFrameState&)>;
    using EndFrame = std::function<bool(const XrFrameState&)>;  // Return false to stop the loop.

    /// Minimal wrapper for the OpenXR render loop.
    class RenderLoop
    {
    public:
        RenderLoop(XrSession session, EndFrame endFrame)
            : m_session(session), m_endFrame(std::move(endFrame)), m_lastPredictedDisplayTime(0)
        {
        }

        /// Call `xrWaitFrame`, `xrBeginFrame`, `xrEndFrame`.
        /// Returns whatever your @ref EndFrame function returned
        bool IterateFrame();

        /// Call @ref IterateFrame repeatedly until your @ref EndFrame returns false,
        /// checking that no exceptions are thrown
        void Loop();

        XrTime GetLastPredictedDisplayTime() const;

    private:
        XrSession m_session;
        EndFrame m_endFrame;
        std::atomic<XrTime> m_lastPredictedDisplayTime;
    };

    /// Helper to simplify action-related code in tests that are not specifically testing action code.
    struct InteractionManager
    {
        InteractionManager(XrInstance instance, XrSession session);

        void AddActionBindings(XrPath interactionProfile, std::vector<XrActionSuggestedBinding> bindings);

        void AddActionSet(XrActionSet actionSet);
        void AttachActionSets(std::vector<XrPath>* assertInteractionProfilePath = nullptr);
        void SyncActions(XrPath subactionPath);
        void SyncActions(const std::initializer_list<XrPath>& subactionPaths);

    private:
        XrInstance m_instance;
        XrSession m_session;
        std::map<XrPath, std::vector<XrActionSuggestedBinding>> m_bindings;
        // Some tests require control of the binding order
        std::vector<XrPath> m_bindingsOrder;
        std::vector<XrActionSet> m_actionSets;
    };

    /// A helper for basic frame loop and rendering operations, wrapping an instance, session, and @ref InteractionManager.
    ///
    /// Displays the usual title box.
    struct CompositionHelper
    {
        /// Constructor
        ///
        /// Note that "testName" is the title that will be shown on the device: it is limited in size and often cannot show the entire actual test name.
        CompositionHelper(const char* testName, const std::vector<const char*>& additionalEnabledExtensions = std::vector<const char*>());

        /// Constructor for when you already have an instance, and maybe know your view config type you want to use.
        CompositionHelper(const char* testName, XrInstance instance, XrViewConfigurationType viewConfigType = (XrViewConfigurationType)0,
                          bool skipOnUnsupportedViewType = false);

        ~CompositionHelper();

        InteractionManager& GetInteractionManager();

        /// Access the instance handle owned by this object.
        ///
        /// @note Do not destroy the handle returned from this method through OpenXR. It is cleaned up on object destruction.
        XrInstance GetInstance() const;

        /// Access the system ID used to create the session in this object.
        XrSystemId GetSystemId() const;

        /// Access the session handle owned by this object.
        ///
        /// @note Do not destroy the handle returned from this method through OpenXR. It is cleaned up on object destruction.
        XrSession GetSession() const;

        std::vector<XrViewConfigurationView> EnumerateConfigurationViews();

        XrViewConfigurationProperties GetViewConfigurationProperties();

        void BeginSession();

        /// Locate views relative to @p space at time @p displayTime
        ///
        /// @return a tuple of the view state and a vector of views.
        ///
        /// @param space Base space - does *not* need to have been created/tracked using this object.
        /// @param displayTime the predicted display time of the next frame
        std::tuple<XrViewState, std::vector<XrView>> LocateViews(XrSpace space, XrTime displayTime);

        /// Check for OpenXR events and handle them.
        ///
        /// @return false if an unexpected session state transition means the test should exit early
        bool PollEvents();

        EventQueue& GetEventQueue() const;

        /// Call xrEndFrame submitting the given layers.
        void EndFrame(XrTime predictedDisplayTime, std::vector<XrCompositionLayerBaseHeader*> layers);

        /// Create a handle for a reference space of type @p type owned by this class.
        ///
        /// The only reason you would use this is to allow this object to perform cleanup of the space for you.
        /// It is not used elsewhere in the class.
        ///
        /// @note Do not destroy the handle returned from this method through OpenXR. It is cleaned up on object destruction.
        XrSpace CreateReferenceSpace(XrReferenceSpaceType type, XrPosef pose = XrPosefCPP());

        /// Return the XrSwapchainCreateInfo for a basic color swapchain of given width and height, with optional arguments.
        ///
        /// Usage flags are `XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT`
        ///
        /// Some RGBA format will be chosen if @p format is not specified.
        XrSwapchainCreateInfo DefaultColorSwapchainCreateInfo(uint32_t width, uint32_t height, XrSwapchainCreateFlags createFlags = 0,
                                                              int64_t format = -1);

        /// Return the XrSwapchainCreateInfo for a basic depth swapchain of given width and height, with optional arguments.
        ///
        /// Usage flags are `XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT`
        ///
        /// Some depth format will be chosen if @p format is not specified.
        XrSwapchainCreateInfo DefaultDepthSwapchainCreateInfo(uint32_t width, uint32_t height, XrSwapchainCreateFlags createFlags = 0,
                                                              int64_t format = -1);

        /// Create a swapchain image handled by this class.
        ///
        /// @note Do not destroy this directly using OpenXR functions: use @ref DestroySwapchain instead.
        ///
        /// Specializations of this function:
        ///
        /// - @ref CreateStaticSwapchainSolidColor
        /// - @ref CreateStaticSwapchainImage
        XrSwapchain CreateSwapchain(const XrSwapchainCreateInfo& createInfo);

        /// Create a swapchain image handled by this class as well as a depth swapchain.
        ///
        /// @note Do not destroy this directly using OpenXR functions: use @ref DestroySwapchain instead.
        ///
        std::pair<XrSwapchain, XrSwapchain> CreateSwapchainWithDepth(const XrSwapchainCreateInfo& createInfo,
                                                                     const XrSwapchainCreateInfo& depthCreateInfo);

        /// Destroy a swapchain image created using @ref CreateSwapchain()
        ///
        /// @param swapchain A swapchain created with @ref CreateSwapchain or a specialization of it.
        void DestroySwapchain(XrSwapchain swapchain);

        /// Perform an xrAcquireSwapchainImage, xrWaitSwapchainImage, xrReleaseSwapchainImage sequence,
        /// calling your update functor between Wait and Release.
        ///
        /// Also does Acquire, Wait, Release on corresponding depth image managed by the graphics plugin.
        ///
        /// @throws on timeout or other error
        ///
        /// @param swapchain A swapchain created with @ref CreateSwapchain or a specialization of it.
        /// @param doUpdate A functor to call between Wait and Release that will be passed the swapchain image as a base header pointer.
        void AcquireWaitReleaseImage(XrSwapchain swapchain, const std::function<void(const XrSwapchainImageBaseHeader*)>& doUpdate);

        /// Create and return a static swapchain that has had a solid color texture copied to it: specialization of @ref CreateSwapchain
        ///
        /// Color is interpreted in a *linear* color space (and thus converted before upload), not SRGB/gamma.
        ///
        /// @note Do not destroy this directly using OpenXR functions: use @ref DestroySwapchain instead.
        XrSwapchain CreateStaticSwapchainSolidColor(const XrColor4f& color);

        /// Create and return a static swapchain that has had an RGBAImage copied to it: specialization of @ref CreateSwapchain
        ///
        /// @note Do not destroy this directly using OpenXR functions: use @ref DestroySwapchain instead.
        XrSwapchain CreateStaticSwapchainImage(const RGBAImage& rgbaImage);

        /// For a swapchain created using @ref CreateSwapchain or one of its specialized versions, return a `XrSwapchainSubImage` structure
        /// populated with the full sub-image as default (start at 0, 0, full width and height) and the provided
        /// optional @p imageArrayIndex
        ///
        /// @param swapchain A swapchain created with @ref CreateSwapchain or a specialization of it.
        /// @param imageArrayIndex The index/slice in the image array to use, if applicable.
        XrSwapchainSubImage MakeDefaultSubImage(XrSwapchain swapchain, uint32_t imageArrayIndex = 0);

        /// Create a quad layer structure owned by this object, displaying @p swapchain with @p width
        /// attached to the provided @p space with optional @p pose
        ///
        /// @param swapchain A swapchain created with @ref CreateSwapchain or a specialization of it.
        /// @param space The space to attach the quad layer to.
        /// @param width The width for the quad layer, goes directly to XrCompositionLayerQuad::size.width
        /// @param pose The pose of the quad in @p space
        XrCompositionLayerQuad* CreateQuadLayer(XrSwapchain swapchain, XrSpace space, float width, XrPosef pose = XrPosefCPP());

        /// Create a projection layer structure (with projection view) owned by this object, attached to the provided @p space.
        ///
        /// Typically used with @ref MakeDefaultSubImage to finish populating the structure.
        XrCompositionLayerProjection* CreateProjectionLayer(XrSpace space);

        /// Return the session state from the most recent session state changed event
        XrSessionState GetSessionState() const
        {
            return m_sessionState;
        }

    private:
        void SharedInit(const char* testName, bool skipOnUnsupportedViewType = false);
        std::mutex m_mutex;

        XrInstance m_instance;
        InstanceREQUIRE m_instanceOwned;
        XrSession m_session;
        XrSystemId m_systemId;

        std::unique_ptr<EventQueue> m_eventQueue;
        std::unique_ptr<EventReader> m_privateEventReader;

        std::unique_ptr<InteractionManager> m_interactionManager;

        int64_t m_defaultColorFormat;
        int64_t m_defaultDepthFormat;
        XrViewConfigurationType m_primaryViewType;
        uint32_t m_projectionViewCount{0};
        XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;

        std::list<XrCompositionLayerProjection> m_projections;
        std::list<std::vector<XrCompositionLayerProjectionView>> m_projectionViews;
        std::list<XrCompositionLayerQuad> m_quads;

        std::map<XrSwapchain, XrSwapchainCreateInfo> m_createdSwapchains;
        std::map<XrSwapchain, ISwapchainImageData*> m_swapchainImages;
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
            /// and a background color of choice.
            /// Must call IGraphicsPlugin::RenderView with @p projectionView , @p swapchainImage ,
            /// and the geometry to draw.
            /// Projection view pose/fov fields are preset to match the corresponding view fields.
            /// Views are located relative to GetLocalSpace()
            virtual void RenderView(const BaseProjectionLayerHelper& projectionLayerHelper, uint32_t viewIndex,
                                    const XrViewState& viewState, const XrView& view, XrCompositionLayerProjectionView& projectionView,
                                    const XrSwapchainImageBaseHeader* swapchainImage) = 0;
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

    /// Helper class to provide simple world-locked projection layer of some cubes. Each view of the projection is a separate swapchain.
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
                            const XrSwapchainImageBaseHeader* swapchainImage) override
            {
                GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                GetGlobalData().graphicsPlugin->RenderView(projectionView, swapchainImage, RenderParams{}.Draw(m_cubes));
            }

        private:
            const std::vector<Cube>& m_cubes;
        };
    };

    const XrVector3f UpVector{0, 1, 0};

    enum class LayerMode
    {
        Scene,
        Help,
        Complete
    };

    namespace Math
    {
        /// Do a linear conversion of a number from one range to another range.
        /// e.g. 5 in range [0-8] projected into range (-.6 to 0.6) is 0.15.
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

    /// Appends composition layers for interacting with interactive composition tests.
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
            m_localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosef{{0, 0, 0, 1}, {0, 0, 0}});

            // description quad to the left, example image quad to the right, counter-rotated 15 degrees towards the viewer
            m_descriptionQuadSpace = compositionHelper.CreateReferenceSpace(
                XR_REFERENCE_SPACE_TYPE_VIEW, {Quat::FromAxisAngle(UpVector, 15 * MATH_PI / 180), {-0.5f, 0, -1.5f}});
            m_exampleQuadSpace = compositionHelper.CreateReferenceSpace(
                XR_REFERENCE_SPACE_TYPE_VIEW, {Quat::FromAxisAngle(UpVector, -15 * MATH_PI / 180), {0.5f, 0, -1.5f}});

            Configure(exampleImage, descriptionText);
        }

        void Configure(const char* exampleImage, const char* descriptionText)
        {
            if (m_descriptionQuad != nullptr && m_descriptionQuad->subImage.swapchain != XR_NULL_HANDLE) {
                m_compositionHelper.DestroySwapchain(m_descriptionQuad->subImage.swapchain);
            }
            if (m_exampleQuad != nullptr && m_exampleQuad->subImage.swapchain != XR_NULL_HANDLE) {
                m_compositionHelper.DestroySwapchain(m_exampleQuad->subImage.swapchain);
            }

            // Load example screenshot if available and set up the quad layer for it.
            {
                XrSwapchain exampleSwapchain;
                if (exampleImage) {
                    exampleSwapchain = m_compositionHelper.CreateStaticSwapchainImage(RGBAImage::Load(exampleImage));
                }
                else {
                    RGBAImage image(256, 256);
                    image.PutText(XrRect2Di{{0, image.height / 2}, {image.width, image.height}}, "Example Not Available", 64, {1, 0, 0, 1});
                    exampleSwapchain = m_compositionHelper.CreateStaticSwapchainImage(image);
                }

                // Create a quad to the right of the help text.
                m_exampleQuad = m_compositionHelper.CreateQuadLayer(exampleSwapchain, m_exampleQuadSpace, 1.25f);
            }

            constexpr uint32_t width = 768;
            constexpr uint32_t descriptionHeight = width;
            constexpr uint32_t fontHeight = 48;
            constexpr uint32_t actionsHeight = 128;

            // Set up the quad layer for showing the help text to the left of the example image.
            m_descriptionQuad = m_compositionHelper.CreateQuadLayer(
                m_compositionHelper.CreateStaticSwapchainImage(CreateTextImage(width, descriptionHeight, descriptionText, fontHeight)),
                m_descriptionQuadSpace, 0.75f);
            m_descriptionQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

            m_sceneActionsSwapchain = m_compositionHelper.CreateStaticSwapchainImage(
                CreateTextImage(width, actionsHeight, "Press Select to PASS. Press Menu for description", fontHeight));
            m_helpActionsSwapchain =
                m_compositionHelper.CreateStaticSwapchainImage(CreateTextImage(width, actionsHeight, "Press select to FAIL", fontHeight));

            // Set up the quad layer and swapchain for showing what actions the user can take in the Scene/Help mode.
            m_actionsQuad =
                m_compositionHelper.CreateQuadLayer(m_sceneActionsSwapchain, m_viewSpace, 0.75f, {Quat::Identity, {0, -0.4f, -1}});
            m_actionsQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        }

        template <typename T>
        void AddLayer(T* layer)
        {
            m_sceneLayers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(layer));
        }

        template <typename T>
        void AddBackgroundLayer(T* layer)
        {
            m_backgroundLayers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(layer));
        }

        bool EndFrame(const XrFrameState& frameState, std::vector<XrCompositionLayerBaseHeader*> layers = {})
        {
            bool keepRunning = AppendLayers(layers, frameState.predictedDisplayTime);
            keepRunning &= m_compositionHelper.PollEvents();
            m_compositionHelper.EndFrame(frameState.predictedDisplayTime, std::move(layers));
            return keepRunning;
        }

    private:
        bool AppendLayers(std::vector<XrCompositionLayerBaseHeader*>& layers, XrTime predictedDisplayTime)
        {
            LayerMode layerMode = GetLayerMode();
            LayerMode lastLayerMode = m_lastLayerMode;
            m_lastLayerMode = layerMode;

            // Add layer(s) based on the interaction mode.
            switch (layerMode) {
            case LayerMode::Scene:
                for (auto& backgroundLayer : m_backgroundLayers) {
                    layers.push_back(backgroundLayer);
                }

                m_actionsQuad->subImage = m_compositionHelper.MakeDefaultSubImage(m_sceneActionsSwapchain);
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(m_actionsQuad));

                for (auto& sceneLayer : m_sceneLayers) {
                    layers.push_back(sceneLayer);
                }
                break;

            case LayerMode::Help:
                if (lastLayerMode != LayerMode::Help) {
                    // convert a quad's reference space to local space when the help menu is opened
                    // this avoids view-locking the help, allowing the user to read it more naturally
                    auto placeQuad = [&](XrCompositionLayerQuad* quad, XrSpace quadSpace) {
                        XrSpaceLocation quadInLocalSpace{XR_TYPE_SPACE_LOCATION};
                        XRC_CHECK_THROW_XRCMD(xrLocateSpace(quadSpace, m_localSpace, predictedDisplayTime, &quadInLocalSpace));
                        if (quadInLocalSpace.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT &&
                            quadInLocalSpace.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
                            quad->space = m_localSpace;
                            quad->pose = quadInLocalSpace.pose;
                        }
                        else {
                            // xrLocateSpace didn't return a valid pose, fall back to view space
                            quad->space = quadSpace;
                            quad->pose = XrPosefCPP{};
                        }
                    };
                    placeQuad(m_descriptionQuad, m_descriptionQuadSpace);
                    placeQuad(m_exampleQuad, m_exampleQuadSpace);
                }

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
        XrSpace m_localSpace;
        XrSwapchain m_sceneActionsSwapchain;
        XrSwapchain m_helpActionsSwapchain;
        LayerMode m_lastLayerMode{LayerMode::Scene};
        XrCompositionLayerQuad* m_actionsQuad;
        XrCompositionLayerQuad* m_descriptionQuad{};
        XrSpace m_descriptionQuadSpace;
        XrCompositionLayerQuad* m_exampleQuad{};
        XrSpace m_exampleQuadSpace;
        std::vector<XrCompositionLayerBaseHeader*> m_sceneLayers;
        std::vector<XrCompositionLayerBaseHeader*> m_backgroundLayers;
    };
}  // namespace Conformance
