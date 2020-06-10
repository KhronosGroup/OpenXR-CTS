// Copyright (c) 2019-2020 The Khronos Group Inc.
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

#include "composition_utils.h"
#include "utils.h"
#include "report.h"
#include "conformance_framework.h"
#include <fstream>
#include <array>
#include <string>
#include <cstring>
#include <thread>
#include <condition_variable>
#include <catch2/catch.hpp>
#include <xr_linear.h>

using namespace std::chrono_literals;

namespace Conformance
{
    RGBAImage CreateTextImage(int width, int height, const char* text, int fontHeight)
    {
        constexpr int FontPaddingPixels = 4;
        constexpr int BorderPixels = 2;
        constexpr int InsetPixels = BorderPixels + FontPaddingPixels;

        RGBAImage image(width, height);
        image.DrawRect(0, 0, image.width, image.height, {0, 0, 0, 0.5f});
        image.DrawRectBorder(0, 0, image.width, image.height, BorderPixels, {1, 1, 1, 1});
        image.PutText(XrRect2Di{{InsetPixels, InsetPixels}, {image.width - InsetPixels * 2, image.height - InsetPixels * 2}}, text,
                      fontHeight, {1, 1, 1, 1});
        return image;
    }

    XrPath StringToPath(XrInstance instance, const char* pathStr)
    {
        XrPath path;
        XRC_CHECK_THROW_XRCMD(xrStringToPath(instance, pathStr, &path));
        return path;
    }

    using UpdateLayers = std::function<void(const XrFrameState&)>;
    using EndFrame = std::function<bool(const XrFrameState&)>;

    RenderLoop::RenderLoop(XrSession session, EndFrame endFrame) : m_session(session), m_endFrame(endFrame)
    {
        m_thread = std::thread([&] {
            CHECK_NOTHROW([&]() {
                while (m_running) {
                    XrFrameState frameState{XR_TYPE_FRAME_STATE};
                    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
                    XRC_CHECK_THROW_XRCMD(xrWaitFrame(m_session, &waitInfo, &frameState));

                    m_lastPredictedDisplayTime.store(frameState.predictedDisplayTime);

                    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
                    XRC_CHECK_THROW_XRCMD(xrBeginFrame(m_session, &beginInfo));

                    if (!m_endFrame(frameState)) {
                        break;
                    }
                }
            }());
        });
    }

    XrTime RenderLoop::GetLastPredictedDisplayTime() const
    {
        return m_lastPredictedDisplayTime.load();
    }

    void RenderLoop::WaitForEnd()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    RenderLoop::~RenderLoop()
    {
        m_running = false;
        WaitForEnd();
    }

    InteractionManager::InteractionManager(XrInstance instance, XrSession session) : m_instance(instance), m_session(session)
    {
    }

    void InteractionManager::AddActionBindings(XrPath interactionProfile, std::vector<XrActionSuggestedBinding> bindings)
    {
        m_bindings.insert({interactionProfile, {}});
        auto& vec = m_bindings.at(interactionProfile);
        vec.insert(vec.end(), bindings.begin(), bindings.end());
    }

    void InteractionManager::AddActionSet(XrActionSet actionSet)
    {
        m_actionSets.push_back(actionSet);
    }

    void InteractionManager::AttachActionSets()
    {
        for (const auto& pair : m_bindings) {
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = pair.first;
            suggestedBindings.suggestedBindings = pair.second.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)pair.second.size();
            XRC_CHECK_THROW_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        if (!m_actionSets.empty()) {
            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.actionSets = m_actionSets.data();
            attachInfo.countActionSets = (uint32_t)m_actionSets.size();
            XRC_CHECK_THROW_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
        }
    }

    CompositionHelper::CompositionHelper(const char* testName)
    {
        m_primaryViewType = GetGlobalData().GetOptions().viewConfigurationValue;

        XRC_CHECK_THROW_XRCMD(CreateBasicInstance(&m_instance));

        m_eventQueue = std::unique_ptr<EventQueue>(new EventQueue(m_instance));
        m_privateEventReader = std::unique_ptr<EventReader>(new EventReader(*m_eventQueue));

        XRC_CHECK_THROW_XRCMD(CreateBasicSession(m_instance, &m_systemId, &m_session));

        XRC_CHECK_THROW_XRCMD(
            xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_primaryViewType, 0, &m_projectionViewCount, nullptr));

        m_interactionManager = std::make_unique<InteractionManager>(m_instance, m_session);

        std::vector<int64_t> swapchainFormats;
        {
            uint32_t countOutput;
            XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &countOutput, nullptr));
            if (countOutput != 0) {
                swapchainFormats.resize(countOutput);
                XRC_CHECK_THROW_XRCMD(
                    xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(), &countOutput, swapchainFormats.data()));
            }
        }

        m_defaultColorFormat = GetGlobalData().graphicsPlugin->SelectColorSwapchainFormat(swapchainFormats.data(), swapchainFormats.size());

        m_viewSpace = CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{{0, 0, 0, 1}, {0, 0, 0}});

        {
            constexpr int TitleFontHeightPixels = 32;
            constexpr int TitleFontPaddingPixels = 2;
            constexpr int TitleBorderPixels = 2;
            constexpr int InsetPixels = TitleBorderPixels + TitleFontPaddingPixels;

            RGBAImage image(512, TitleFontHeightPixels + InsetPixels * 2);
            image.DrawRect(0, 0, image.width, image.height, {0.5f, 0.5f, 0.5f, 0.5f});
            image.DrawRectBorder(0, 0, image.width, image.height, TitleBorderPixels, {0.5f, 0.5f, 0.5f, 1});
            image.PutText(XrRect2Di{{InsetPixels, InsetPixels}, {image.width - InsetPixels * 2, image.height - InsetPixels * 2}}, testName,
                          TitleFontHeightPixels, {1, 1, 1, 1});

            m_testNameQuad.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            m_testNameQuad.size.width = 0.75f;
            m_testNameQuad.size.height = m_testNameQuad.size.width * image.height / image.width;
            m_testNameQuad.pose = XrPosef{{0, 0, 0, 1}, {0, 0.4f, -1}};
            m_testNameQuad.space = m_viewSpace;
            m_testNameQuad.subImage = MakeDefaultSubImage(CreateStaticSwapchainImage(image));
        }
    }

    CompositionHelper::~CompositionHelper()
    {
        for (XrSpace space : m_spaces) {
            XRC_CHECK_THROW_XRCMD(xrDestroySpace(space));
        }

        for (auto swapchain : m_createdSwapchains) {
            XRC_CHECK_THROW_XRCMD(xrDestroySwapchain(swapchain.first));
        }

        xrDestroySession(m_session);

        GlobalData& globalData = GetGlobalData();
        auto graphicsPlugin = globalData.GetGraphicsPlugin();
        if (graphicsPlugin->IsInitialized()) {
            graphicsPlugin->ShutdownDevice();
        }

        xrDestroyInstance(m_instance);
    }

    InteractionManager& CompositionHelper::GetInteractionManager()
    {
        return *m_interactionManager;
    }

    XrInstance CompositionHelper::GetInstance() const
    {
        return m_instance;
    }

    XrSession CompositionHelper::GetSession() const
    {
        return m_session;
    }

    std::vector<XrViewConfigurationView> CompositionHelper::EnumerateConfigurationViews()
    {
        std::vector<XrViewConfigurationView> views;

        uint32_t countOutput;
        XRC_CHECK_THROW_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_primaryViewType, 0, &countOutput, nullptr));
        if (countOutput != 0) {
            views.resize(countOutput, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
            XRC_CHECK_THROW_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_primaryViewType, (uint32_t)views.size(),
                                                                    &countOutput, views.data()));
        }

        return views;
    }

    XrViewConfigurationProperties CompositionHelper::GetViewConfigurationProperties()
    {
        XrViewConfigurationProperties properties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
        XRC_CHECK_THROW_XRCMD(xrGetViewConfigurationProperties(m_instance, m_systemId, m_primaryViewType, &properties));
        return properties;
    }

    void CompositionHelper::BeginSession()
    {
        REQUIRE_MSG(WaitUntilPredicateWithTimeout(
                        [&] {
                            XrEventDataBuffer eventData;
                            while (m_privateEventReader->TryReadUntilEvent(eventData, XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)) {
                                auto sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                                if (sessionStateChanged->session == m_session && sessionStateChanged->state == XR_SESSION_STATE_READY) {
                                    return true;
                                }
                            }

                            return false;
                        },
                        15s, 10ms),
                    "Failed to reach session ready state");

        XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
        beginInfo.primaryViewConfigurationType = m_primaryViewType;
        XRC_CHECK_THROW_XRCMD(xrBeginSession(m_session, &beginInfo));
    }

    std::tuple<XrViewState, std::vector<XrView>> CompositionHelper::LocateViews(XrSpace space, int64_t displayTime)
    {
        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.displayTime = displayTime;
        viewLocateInfo.space = space;
        viewLocateInfo.viewConfigurationType = m_primaryViewType;
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        std::vector<XrView> views(m_projectionViewCount, {XR_TYPE_VIEW});
        uint32_t viewCount = m_projectionViewCount;
        XRC_CHECK_THROW_XRCMD(xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCount, &viewCount, views.data()));

        return std::make_tuple(viewState, std::move(views));
    }

    void CompositionHelper::EndFrame(XrTime predictedDisplayTime, std::vector<XrCompositionLayerBaseHeader*> layers)
    {
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&m_testNameQuad));

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.environmentBlendMode = GetGlobalData().GetOptions().environmentBlendModeValue;
        frameEndInfo.displayTime = predictedDisplayTime;
        frameEndInfo.layerCount = (uint32_t)layers.size();
        frameEndInfo.layers = layers.data();
        XRC_CHECK_THROW_XRCMD(xrEndFrame(m_session, &frameEndInfo));
    }

    EventQueue& CompositionHelper::GetEventQueue() const
    {
        return *m_eventQueue;
    }

    bool CompositionHelper::PollEvents()
    {
        XrEventDataBuffer eventBuffer;
        while (m_privateEventReader->TryReadNext(eventBuffer)) {
            if (eventBuffer.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                auto sessionState = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventBuffer);

                // The composition frame loop should always be running, otherwise something unexpected happened (perhaps a conformance bug
                // or the runtime wants to move the session to IDLE which the user shouldn't have requested during conformance).
                if (sessionState->state != XR_SESSION_STATE_READY && sessionState->state != XR_SESSION_STATE_SYNCHRONIZED &&
                    sessionState->state != XR_SESSION_STATE_VISIBLE && sessionState->state != XR_SESSION_STATE_FOCUSED) {
                    FAIL("Unexpected transition to session state " << sessionState->state);
                    return false;  // Stop running.
                }
            }
        }

        return true;
    }

    void CompositionHelper::AcquireWaitReleaseImage(XrSwapchain swapchain,
                                                    std::function<void(const XrSwapchainImageBaseHeader*, uint64_t)> doUpdate)
    {
        uint32_t imageIndex;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        XRC_CHECK_THROW_XRCMD(xrAcquireSwapchainImage(swapchain, &acquireInfo, &imageIndex));

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        XRC_CHECK_THROW_XRCMD(xrWaitSwapchainImage(swapchain, &waitInfo));

        std::unique_lock<std::mutex> lock(m_mutex);
        const XrSwapchainImageBaseHeader* image = m_swapchainImages[swapchain]->imagePtrVector[imageIndex];
        const uint64_t format = m_createdSwapchains[swapchain].format;
        lock.unlock();

        doUpdate(image, format);

        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        XRC_CHECK_THROW_XRCMD(xrReleaseSwapchainImage(swapchain, &releaseInfo));
    }

    XrSpace CompositionHelper::CreateReferenceSpace(XrReferenceSpaceType type, XrPosef pose /*= XrPosefCPP()*/)
    {
        XrSpace space;
        XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        createInfo.poseInReferenceSpace = pose;
        createInfo.referenceSpaceType = type;
        XRC_CHECK_THROW_XRCMD(xrCreateReferenceSpace(m_session, &createInfo, &space));

        std::lock_guard<std::mutex> lock(m_mutex);
        m_spaces.push_back(space);
        return space;
    }

    XrSwapchainCreateInfo CompositionHelper::DefaultColorSwapchainCreateInfo(uint32_t width, uint32_t height,
                                                                             XrSwapchainCreateFlags createFlags /*= 0*/,
                                                                             int64_t format /*= -1*/)
    {
        if (format == -1) {  // Is -1 a safe "uninitialized" value?
            format = m_defaultColorFormat;
        }

        XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        createInfo.arraySize = 1;
        createInfo.format = format;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.mipCount = 1;
        createInfo.faceCount = 1;
        createInfo.sampleCount = 1;
        createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.createFlags = createFlags;
        return createInfo;
    }

    XrSwapchain CompositionHelper::CreateSwapchain(const XrSwapchainCreateInfo& createInfo)
    {
        XrSwapchain swapchain;
        XRC_CHECK_THROW_XRCMD(xrCreateSwapchain(m_session, &createInfo, &swapchain));

        std::lock_guard<std::mutex> lock(m_mutex);

        // Cache the swapchain create info and image structs.
        m_createdSwapchains.insert({swapchain, createInfo});

        // Cache the swapchain image structs.
        uint32_t imageCount;
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
        std::shared_ptr<Conformance::IGraphicsPlugin::SwapchainImageStructs> swapchainImages =
            GetGlobalData().graphicsPlugin->AllocateSwapchainImageStructs(imageCount, createInfo);
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, swapchainImages->imagePtrVector[0]));
        m_swapchainImages[swapchain] = swapchainImages;

        return swapchain;
    }

    void CompositionHelper::DestroySwapchain(XrSwapchain swapchain)
    {
        XRC_CHECK_THROW_XRCMD(xrDestroySwapchain(swapchain));

        std::lock_guard<std::mutex> lock(m_mutex);
        XRC_CHECK_THROW(1 == m_createdSwapchains.erase(swapchain));
        XRC_CHECK_THROW(1 == m_swapchainImages.erase(swapchain));
    }

    XrSwapchain CompositionHelper::CreateStaticSwapchainSolidColor(const XrColor4f& color)
    {
        // Avoid using a 1x1 image here since runtimes may do special processing near texture edges.
        RGBAImage image(256, 256);
        image.DrawRect(0, 0, 256, 256, color);

        return CreateStaticSwapchainImage(image);
    }

    XrSwapchain CompositionHelper::CreateStaticSwapchainImage(const RGBAImage& rgbaImage)
    {
        // The swapchain format must be R8G8B8A8 UNORM to match the RGBAImage format.
        const int64_t format = GetGlobalData().graphicsPlugin->GetRGBA8UnormFormat();
        auto swapchainCreateInfo =
            DefaultColorSwapchainCreateInfo(rgbaImage.width, rgbaImage.height, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, format);

        const XrSwapchain swapchain = CreateSwapchain(swapchainCreateInfo);

        AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) {
            GetGlobalData().graphicsPlugin->CopyRGBAImage(swapchainImage, format, 0, rgbaImage);
        });

        return swapchain;
    }

    XrSwapchainSubImage CompositionHelper::MakeDefaultSubImage(XrSwapchain swapchain, uint32_t imageArrayIndex /*= 0*/)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Look up the swapchain creation details to get default width/height.
        auto swapchainInfoIt = m_createdSwapchains.find(swapchain);
        XRC_CHECK_THROW_MSG(swapchainInfoIt != m_createdSwapchains.end(), "Not a tracked swapchain");

        XrSwapchainSubImage subImage;
        subImage.swapchain = swapchain;
        subImage.imageRect = {{0, 0}, {(int32_t)swapchainInfoIt->second.width, (int32_t)swapchainInfoIt->second.height}};
        subImage.imageArrayIndex = imageArrayIndex;
        return subImage;
    }

    XrCompositionLayerQuad* CompositionHelper::CreateQuadLayer(XrSwapchain swapchain, XrSpace space, XrPosef pose /*= XrPosefCPP()*/)
    {
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        quad.size = {1, 1};
        quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        quad.pose = pose;
        quad.space = space;
        quad.subImage = MakeDefaultSubImage(swapchain);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_quads.push_back(quad);
        return &m_quads.back();
    }

    XrCompositionLayerProjection* CompositionHelper::CreateProjectionLayer(XrSpace space)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Allocate projection views and store.
        assert(m_projectionViewCount > 0);
        XrCompositionLayerProjectionView init{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        // Make sure the pose is valid
        init.pose.orientation.w = 1.0f;
        std::vector<XrCompositionLayerProjectionView> projViews(m_projectionViewCount, init);
        m_projectionViews.push_back(std::move(projViews));

        // Allocate projection and store.
        XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        projection.space = space;
        projection.viewCount = (uint32_t)m_projectionViews.back().size();
        projection.views = m_projectionViews.back().data();
        m_projections.push_back(projection);

        return &m_projections.back();
    }

    SimpleProjectionLayerHelper::SimpleProjectionLayerHelper(CompositionHelper& compositionHelper)
        : m_compositionHelper(compositionHelper)
        , m_localSpace(compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosefCPP{}))
    {
        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        m_projLayer = compositionHelper.CreateProjectionLayer(m_localSpace);
        for (uint32_t j = 0; j < m_projLayer->viewCount; j++) {
            const XrSwapchain swapchain = compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(
                viewProperties[j].recommendedImageRectWidth, viewProperties[j].recommendedImageRectHeight));
            const_cast<XrSwapchainSubImage&>(m_projLayer->views[j].subImage) = compositionHelper.MakeDefaultSubImage(swapchain, 0);
            m_swapchains.push_back(swapchain);
        }
    }

    XrCompositionLayerBaseHeader* SimpleProjectionLayerHelper::GetProjectionLayer() const
    {
        return reinterpret_cast<XrCompositionLayerBaseHeader*>(m_projLayer);
    }

    void SimpleProjectionLayerHelper::UpdateProjectionLayer(const XrFrameState& frameState)
    {
        const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2}), Cube::Make({0, 1, -2})};

        auto viewData = m_compositionHelper.LocateViews(m_localSpace, frameState.predictedDisplayTime);
        const auto& viewState = std::get<XrViewState>(viewData);

        std::vector<XrCompositionLayerBaseHeader*> layers;
        if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT && viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
            const auto& views = std::get<std::vector<XrView>>(viewData);

            // Render into each view swapchain using the recommended view fov and pose.
            for (size_t view = 0; view < views.size(); view++) {
                m_compositionHelper.AcquireWaitReleaseImage(
                    m_swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, format);

                        const_cast<XrFovf&>(m_projLayer->views[view].fov) = views[view].fov;
                        const_cast<XrPosef&>(m_projLayer->views[view].pose) = views[view].pose;
                        GetGlobalData().graphicsPlugin->RenderView(m_projLayer->views[view], swapchainImage, format, cubes);
                    });
            }
        }
    }
}  // namespace Conformance
