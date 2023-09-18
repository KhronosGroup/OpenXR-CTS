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

#include "composition_utils.h"

#include "conformance_framework.h"
#include "swapchain_image_data.h"
#include "utilities/event_reader.h"
#include "utilities/throw_helpers.h"
#include "utilities/xrduration_literals.h"

#include <openxr/openxr.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <ratio>
#include <utility>

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

    bool RenderLoop::IterateFrame()
    {
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XRC_CHECK_THROW_XRCMD(xrWaitFrame(m_session, &waitInfo, &frameState));

        m_lastPredictedDisplayTime.store(frameState.predictedDisplayTime);

        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        XRC_CHECK_THROW_XRCMD(xrBeginFrame(m_session, &beginInfo));
        return m_endFrame(frameState);
    }

    void RenderLoop::Loop()
    {
        CHECK_NOTHROW([&]() {
            while (IterateFrame()) {
            }
        }());
    }

    XrTime RenderLoop::GetLastPredictedDisplayTime() const
    {
        return m_lastPredictedDisplayTime.load();
    }

    InteractionManager::InteractionManager(XrInstance instance, XrSession session) : m_instance(instance), m_session(session)
    {
    }

    void InteractionManager::AddActionBindings(XrPath interactionProfile, std::vector<XrActionSuggestedBinding> bindings)
    {
        m_bindings.insert({interactionProfile, {}});
        auto& vec = m_bindings.at(interactionProfile);
        vec.insert(vec.end(), bindings.begin(), bindings.end());

        // Keep track of the order interaction profiles were used. Apps do not need to do this, but some conformance tests need it
        if (std::find(m_bindingsOrder.begin(), m_bindingsOrder.end(), interactionProfile) == m_bindingsOrder.end()) {
            m_bindingsOrder.push_back(interactionProfile);
        }
    }

    void InteractionManager::AddActionSet(XrActionSet actionSet)
    {
        m_actionSets.push_back(actionSet);
    }

    void InteractionManager::AttachActionSets(std::vector<XrPath>* assertInteractionProfilePathOrder /* = nullptr*/)
    {
        // Some tests rely on controlling the order of suggestInteractionProfile, this is a validity check of that ordering
        if (assertInteractionProfilePathOrder) {
            REQUIRE((*assertInteractionProfilePathOrder).size() == m_bindingsOrder.size());
            REQUIRE(std::equal(m_bindingsOrder.begin(), m_bindingsOrder.end(), (*assertInteractionProfilePathOrder).begin()));
        }

        for (const auto& interactionProfile : m_bindingsOrder) {
            const auto& bindings = m_bindings.at(interactionProfile);
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = interactionProfile;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            XRC_CHECK_THROW_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        if (!m_actionSets.empty()) {
            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.actionSets = m_actionSets.data();
            attachInfo.countActionSets = (uint32_t)m_actionSets.size();
            XRC_CHECK_THROW_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
        }
    }

    void InteractionManager::SyncActions(XrPath subactionPath)
    {
        std::vector<XrActiveActionSet> activeActionSet;
        for (auto& actionSet : m_actionSets) {
            XrActiveActionSet activeSet{actionSet, subactionPath};
            activeActionSet.emplace_back(activeSet);
        }

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = uint32_t(m_actionSets.size());
        syncInfo.activeActionSets = activeActionSet.data();
        XRC_CHECK_THROW_XRCMD(xrSyncActions(m_session, &syncInfo));
    }

    CompositionHelper::CompositionHelper(const char* testName, const std::vector<const char*>& additionalEnabledExtensions)
    {
        m_primaryViewType = GetGlobalData().GetOptions().viewConfigurationValue;

        XrInstance instanceRaw{XR_NULL_HANDLE_CPP};
        XRC_CHECK_THROW_XRCMD(CreateBasicInstance(&instanceRaw, true, additionalEnabledExtensions));
        m_instance.adopt(instanceRaw);

        m_eventQueue = std::unique_ptr<EventQueue>(new EventQueue(m_instance.get()));
        m_privateEventReader = std::unique_ptr<EventReader>(new EventReader(*m_eventQueue));

        XRC_CHECK_THROW_XRCMD(CreateBasicSession(m_instance.get(), &m_systemId, &m_session));

        XRC_CHECK_THROW_XRCMD(
            xrEnumerateViewConfigurationViews(m_instance.get(), m_systemId, m_primaryViewType, 0, &m_projectionViewCount, nullptr));

        m_interactionManager = std::make_unique<InteractionManager>(m_instance.get(), m_session);

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

        if (GetGlobalData().IsUsingGraphicsPlugin()) {
            m_defaultColorFormat =
                GetGlobalData().graphicsPlugin->SelectColorSwapchainFormat(swapchainFormats.data(), swapchainFormats.size());
        }
        else {
            m_defaultColorFormat = static_cast<uint64_t>(-1);
        }

        m_viewSpace = CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{{0, 0, 0, 1}, {0, 0, 0}});

        {
            constexpr int TitleFontHeightPixels = 32;
            RGBAImage image = CreateTextImage(512, 44, testName, TitleFontHeightPixels);

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
        if (globalData.IsUsingGraphicsPlugin()) {
            auto graphicsPlugin = globalData.GetGraphicsPlugin();
            if (graphicsPlugin->IsInitialized()) {
                graphicsPlugin->ShutdownDevice();
            }
        }
    }

    InteractionManager& CompositionHelper::GetInteractionManager()
    {
        return *m_interactionManager;
    }

    XrInstance CompositionHelper::GetInstance() const
    {
        return m_instance.get();
    }

    XrSystemId CompositionHelper::GetSystemId() const
    {
        return m_systemId;
    }

    XrSession CompositionHelper::GetSession() const
    {
        return m_session;
    }

    std::vector<XrViewConfigurationView> CompositionHelper::EnumerateConfigurationViews()
    {
        std::vector<XrViewConfigurationView> views;

        uint32_t countOutput;
        XRC_CHECK_THROW_XRCMD(xrEnumerateViewConfigurationViews(m_instance.get(), m_systemId, m_primaryViewType, 0, &countOutput, nullptr));
        if (countOutput != 0) {
            views.resize(countOutput, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
            XRC_CHECK_THROW_XRCMD(xrEnumerateViewConfigurationViews(m_instance.get(), m_systemId, m_primaryViewType, (uint32_t)views.size(),
                                                                    &countOutput, views.data()));
        }

        return views;
    }

    XrViewConfigurationProperties CompositionHelper::GetViewConfigurationProperties()
    {
        XrViewConfigurationProperties properties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
        XRC_CHECK_THROW_XRCMD(xrGetViewConfigurationProperties(m_instance.get(), m_systemId, m_primaryViewType, &properties));
        return properties;
    }

    void CompositionHelper::BeginSession()
    {
        bool result = WaitUntilPredicateWithTimeout(
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
            15s, 10ms);
        XRC_CHECK_THROW_MSG(result, "Failed to reach session ready state");

        XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
        beginInfo.primaryViewConfigurationType = m_primaryViewType;
        XRC_CHECK_THROW_XRCMD(xrBeginSession(m_session, &beginInfo));
    }

    std::tuple<XrViewState, std::vector<XrView>> CompositionHelper::LocateViews(XrSpace space, XrTime displayTime)
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
                                                    const std::function<void(const XrSwapchainImageBaseHeader*)>& doUpdate)
    {
        uint32_t colorImageIndex;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        XRC_CHECK_THROW_XRCMD(xrAcquireSwapchainImage(swapchain, &acquireInfo, &colorImageIndex));

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = 500_xrMilliseconds;  // Call can block waiting for image to become available for writing.
        XRC_CHECK_THROW_XRCMD(xrWaitSwapchainImage(swapchain, &waitInfo));
        m_swapchainImages[swapchain]->AcquireAndWaitDepthSwapchainImage(colorImageIndex);

        std::unique_lock<std::mutex> lock(m_mutex);
        const XrSwapchainImageBaseHeader* image = m_swapchainImages[swapchain]->GetGenericColorImage(colorImageIndex);
        lock.unlock();

        doUpdate(image);

        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        XRC_CHECK_THROW_XRCMD(xrReleaseSwapchainImage(swapchain, &releaseInfo));
        m_swapchainImages[swapchain]->ReleaseDepthSwapchainImage();
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
        if (!GetGlobalData().IsUsingGraphicsPlugin()) {
            return XR_NULL_HANDLE;
        }

        XrSwapchain swapchain;
        XRC_CHECK_THROW_XRCMD(xrCreateSwapchain(m_session, &createInfo, &swapchain));

        std::lock_guard<std::mutex> lock(m_mutex);

        // Cache the swapchain create info and image structs.
        m_createdSwapchains.insert({swapchain, createInfo});

        // Cache the swapchain image structs.
        uint32_t imageCount;
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));

        ISwapchainImageData* swapchainImages = GetGlobalData().graphicsPlugin->AllocateSwapchainImageData(imageCount, createInfo);
        XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, swapchainImages->GetColorImageArray()));
        m_swapchainImages[swapchain] = swapchainImages;

        return swapchain;
    }

    void CompositionHelper::DestroySwapchain(XrSwapchain swapchain)
    {
        // Drop all associated resources.
        m_swapchainImages[swapchain]->Reset();

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
        if (!GetGlobalData().IsUsingGraphicsPlugin()) {
            return XR_NULL_HANDLE;
        }

        // The swapchain format must be R8G8B8A8 UNORM to match the RGBAImage format.
        const int64_t format = GetGlobalData().graphicsPlugin->GetSRGBA8Format();
        auto swapchainCreateInfo =
            DefaultColorSwapchainCreateInfo(rgbaImage.width, rgbaImage.height, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, format);
        swapchainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        const XrSwapchain swapchain = CreateSwapchain(swapchainCreateInfo);

        RGBAImage srgbImage = rgbaImage;
        if (!rgbaImage.isSrgb)
            srgbImage.ConvertToSRGB();
        AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
            GetGlobalData().graphicsPlugin->CopyRGBAImage(swapchainImage, 0, srgbImage);
        });

        return swapchain;
    }

    XrSwapchainSubImage CompositionHelper::MakeDefaultSubImage(XrSwapchain swapchain, uint32_t imageArrayIndex /*= 0*/)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        XrSwapchainSubImage subImage;
        subImage.swapchain = swapchain;
        if (GetGlobalData().IsUsingGraphicsPlugin()) {
            // Look up the swapchain creation details to get default width/height.
            auto swapchainInfoIt = m_createdSwapchains.find(swapchain);
            XRC_CHECK_THROW_MSG(swapchainInfoIt != m_createdSwapchains.end(), "Not a tracked swapchain");
            subImage.imageRect = {{0, 0}, {(int32_t)swapchainInfoIt->second.width, (int32_t)swapchainInfoIt->second.height}};
        }
        subImage.imageArrayIndex = imageArrayIndex;
        return subImage;
    }

    XrCompositionLayerQuad* CompositionHelper::CreateQuadLayer(XrSwapchain swapchain, XrSpace space, float width,
                                                               XrPosef pose /*= XrPosefCPP()*/)
    {
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        quad.pose = pose;
        quad.space = space;
        quad.subImage = MakeDefaultSubImage(swapchain);
        quad.size = {width, width * quad.subImage.imageRect.extent.height / quad.subImage.imageRect.extent.width};

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

    BaseProjectionLayerHelper::BaseProjectionLayerHelper(CompositionHelper& compositionHelper, XrReferenceSpaceType spaceType)
        : m_compositionHelper(compositionHelper), m_localSpace(compositionHelper.CreateReferenceSpace(spaceType, XrPosefCPP{}))
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

    // out of line to provide key function
    BaseProjectionLayerHelper::ViewRenderer::~ViewRenderer() = default;

    XrCompositionLayerBaseHeader* BaseProjectionLayerHelper::TryGetUpdatedProjectionLayer(const XrFrameState& frameState,
                                                                                          ViewRenderer& renderer)
    {
        auto viewData = m_compositionHelper.LocateViews(m_localSpace, frameState.predictedDisplayTime);
        const auto& viewState = std::get<XrViewState>(viewData);

        if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT && viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
            const auto& views = std::get<std::vector<XrView>>(viewData);

            // Render into each view swapchain using the recommended view fov and pose.
            for (uint32_t viewIndex = 0; viewIndex < GetViewCount(); viewIndex++) {
                m_compositionHelper.AcquireWaitReleaseImage(m_swapchains[viewIndex], [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                    auto& projectionView = const_cast<XrCompositionLayerProjectionView&>(m_projLayer->views[viewIndex]);
                    auto& view = views[viewIndex];
                    projectionView.fov = view.fov;
                    projectionView.pose = view.pose;
                    renderer.RenderView(*this, viewIndex, viewState, view, projectionView, swapchainImage);
                });
            }

            return reinterpret_cast<XrCompositionLayerBaseHeader*>(m_projLayer);
        }
        // Cannot use the projection layer because the swapchains it uses may not have ever been acquired and released.
        return nullptr;
    }

}  // namespace Conformance
