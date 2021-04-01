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

#include <array>
#include <thread>
#include <numeric>
#include "utils.h"
#include "report.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "composition_utils.h"
#include <catch2/catch.hpp>
#include <openxr/openxr.h>
#include <xr_linear.h>

using namespace Conformance;

namespace
{
    const XrVector3f Up{0, 1, 0};

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
        constexpr XrColor4f Purple = {1, 0, 1, 1};
        constexpr XrColor4f Yellow = {1, 1, 0, 1};
        constexpr XrColor4f Orange = {1, 0.65f, 0, 1};
        constexpr XrColor4f White = {1, 1, 1, 1};
        constexpr XrColor4f Transparent = {0, 0, 0, 0};

        // Avoid including red which is a "failure color".
        constexpr std::array<XrColor4f, 4> UniqueColors{Green, Blue, Yellow, Orange};
    }  // namespace Colors

    namespace Math
    {
        // Do a linear conversion of a number from one range to another range.
        // e.g. 5 in range [0-8] projected into range (-.6 to 0.6) is 0.15.
        float LinearMap(int i, int sourceMin, int sourceMax, float targetMin, float targetMax)
        {
            float percent = (i - sourceMin) / (float)sourceMax;
            return targetMin + ((targetMax - targetMin) * percent);
        }

        constexpr float DegToRad(float degree)
        {
            return degree / 180 * MATH_PI;
        }
    }  // namespace Math

    namespace Quat
    {
        constexpr XrQuaternionf Identity{0, 0, 0, 1};

        XrQuaternionf FromAxisAngle(XrVector3f axis, float radians)
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
                    exampleSwapchain = compositionHelper.CreateStaticSwapchainImage(RGBAImage::Load(exampleImage), true /* sRGB */);
                }
                else {
                    RGBAImage image(256, 256);
                    image.PutText(XrRect2Di{{0, image.height / 2}, {image.width, image.height}}, "Example Not Available", 64, {1, 0, 0, 1});
                    exampleSwapchain = compositionHelper.CreateStaticSwapchainImage(image);
                }

                // Create a quad to the right of the help text.
                m_exampleQuad = compositionHelper.CreateQuadLayer(exampleSwapchain, m_viewSpace, 1.25f, {Quat::Identity, {0.5f, 0, -1.5f}});
                XrQuaternionf_CreateFromAxisAngle(&m_exampleQuad->pose.orientation, &Up, -15 * MATH_PI / 180);
            }

            // Set up the quad layer for showing the help text to the left of the example image.
            m_descriptionQuad = compositionHelper.CreateQuadLayer(
                m_compositionHelper.CreateStaticSwapchainImage(CreateTextImage(768, 768, descriptionText, 48)), m_viewSpace, 0.75f,
                {Quat::Identity, {-0.5f, 0, -1.5f}});
            m_descriptionQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            XrQuaternionf_CreateFromAxisAngle(&m_descriptionQuad->pose.orientation, &Up, 15 * MATH_PI / 180);

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

        XrActionSet m_actionSet;
        XrAction m_select;
        XrAction m_menu;

        XrSpace m_viewSpace;
        XrSwapchain m_sceneActionsSwapchain;
        XrSwapchain m_helpActionsSwapchain;
        XrCompositionLayerQuad* m_actionsQuad;
        XrCompositionLayerQuad* m_descriptionQuad;
        XrCompositionLayerQuad* m_exampleQuad;
        std::vector<XrCompositionLayerBaseHeader*> m_sceneLayers;
    };
}  // namespace

namespace Conformance
{
    // Purpose: Verify behavior of quad visibility and occlusion with the expectation that:
    // 1. Quads render with painters algo.
    // 2. Quads which are facing away are not visible.
    TEST_CASE("Quad Occlusion", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Quad Occlusion");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "quad_occlusion.png",
            "This test includes a blue and green quad at Z=-2 with opposite rotations on Y axis forming X. The green quad should be"
            " fully visible due to painter's algorithm. A red quad is facing away and should not be visible.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        const XrSwapchain redSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Red);

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        // Each quad is rotated on Y axis by 45 degrees to form an X.
        // Green is added second so it should draw over the blue quad.
        const XrQuaternionf blueRot = Quat::FromAxisAngle({0, 1, 0}, Math::DegToRad(-45));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(blueSwapchain, viewSpace, 1.0f, XrPosef{blueRot, {0, 0, -2}}));
        const XrQuaternionf greenRot = Quat::FromAxisAngle({0, 1, 0}, Math::DegToRad(45));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(greenSwapchain, viewSpace, 1.0f, XrPosef{greenRot, {0, 0, -2}}));
        // Red quad is rotated away from the viewer and should not be visible.
        const XrQuaternionf redRot = Quat::FromAxisAngle({0, 1, 0}, Math::DegToRad(180));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(redSwapchain, viewSpace, 1.0f, XrPosef{redRot, {0, 0, -1}}));

        RenderLoop(compositionHelper.GetSession(),
                   [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); })
            .Loop();
    }

    // Purpose: Verify order of transforms by exercising the two ways poses can be specified:
    // 1. A pose offset when creating the space
    // 2. A pose offset when adding the layer
    // If the poses are applied in an incorrect order, the quads will not rendener in the correct place or orientation.
    TEST_CASE("Quad Poses", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Quad Poses");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "quad_poses.png",
            "Render pairs of quads using similar poses to validate order of operations. The blue/green quads apply a"
            " rotation around the Z axis on an XrSpace and then translate the quad out on the Z axis through the quad"
            " layer's pose. The purple/yellow quads apply the same translation on the XrSpace and the rotation on the"
            " quad layer's pose.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        const XrSwapchain orangeSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Orange);
        const XrSwapchain yellowSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Yellow);

        constexpr int RotationCount = 2;
        constexpr float MaxRotationDegrees = 30;
        // For each rotation there are a pair of quads.
        static_assert(RotationCount * 2 <= XR_MIN_COMPOSITION_LAYERS_SUPPORTED, "Too many layers");

        for (int i = 0; i < RotationCount; i++) {
            const float radians =
                Math::LinearMap(i, 0, RotationCount - 1, Math::DegToRad(-MaxRotationDegrees), Math::DegToRad(MaxRotationDegrees));

            const XrPosef pose1 = XrPosef{Quat::FromAxisAngle({0, 1, 0}, radians), {0, 0, 0}};
            const XrPosef pose2 = XrPosef{Quat::Identity, {0, 0, -1}};

            const XrSpace viewSpacePose1 = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, pose1);
            const XrSpace viewSpacePose2 = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, pose2);

            auto quad1 = compositionHelper.CreateQuadLayer((i % 2) == 0 ? blueSwapchain : greenSwapchain, viewSpacePose1, 0.25f, pose2);
            interactiveLayerManager.AddLayer(quad1);

            auto quad2 = compositionHelper.CreateQuadLayer((i % 2) == 0 ? orangeSwapchain : yellowSwapchain, viewSpacePose2, 0.25f, pose1);
            interactiveLayerManager.AddLayer(quad2);
        }

        RenderLoop(compositionHelper.GetSession(),
                   [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); })
            .Loop();
    }

    // Purpose: Validates alpha blending (both premultiplied and unpremultiplied).
    TEST_CASE("Source Alpha Blending", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Source Alpha Blending");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "source_alpha_blending.png",
                                                        "All three squares should have an identical blue-green gradient.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        constexpr float QuadZ = -3;  // How far away quads are placed.

        // Creates image with correctly combined green and blue gradient (this is the the source of truth).
        {
            Conformance::RGBAImage blueGradientOverGreen(256, 256);
            for (int y = 0; y < 256; y++) {
                const float t = y / 255.0f;
                const XrColor4f dst = Colors::Green;
                const XrColor4f src{0, 0, t, t};
                const XrColor4f blended{dst.r * (1 - src.a) + src.r, dst.g * (1 - src.a) + src.g, dst.b * (1 - src.a) + src.b, 0};
                blueGradientOverGreen.DrawRect(0, y, blueGradientOverGreen.width, 1, blended);
            }

            const XrSwapchain answerSwapchain = compositionHelper.CreateStaticSwapchainImage(blueGradientOverGreen);
            XrCompositionLayerQuad *truthQuad =
                compositionHelper.CreateQuadLayer( answerSwapchain, viewSpace, 1.0f, XrPosef { Quat::Identity, {0, 0, QuadZ} } ));
            truthQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            interactiveLayerManager.AddLayer( truthQuad );
        }

        auto createGradientTest = [&](bool premultiplied, float x, float y) {
            // A solid green quad layer will be composited under a blue gradient.
            {
                const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::GreenZeroAlpha);
                XrCompositionLayerQuad* greenQuad =
                    compositionHelper.CreateQuadLayer( greenSwapchain, viewSpace, 1.0f, XrPosef { Quat::Identity, {x, y, QuadZ} } );
                greenQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
                interactiveLayerManager.AddLayer(greenQuad);
            }

            // Create gradient of blue lines from 0.0 to 1.0.
            {
                Conformance::RGBAImage blueGradient(256, 256);
                for (int row = 0; row < blueGradient.height; row++) {
                    XrColor4f color{0, 0, 1, row / (float)blueGradient.height};
                    if (premultiplied) {
                        color = XrColor4f{color.r * color.a, color.g * color.a, color.b * color.a, color.a};
                    }

                    blueGradient.DrawRect(0, row, blueGradient.width, 1, color);
                }

                const XrSwapchain gradientSwapchain = compositionHelper.CreateStaticSwapchainImage(blueGradient);
                XrCompositionLayerQuad* gradientQuad =
                    compositionHelper.CreateQuadLayer(gradientSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {x, y, QuadZ}});

                gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                if (!premultiplied) {
                    gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
                }

                interactiveLayerManager.AddLayer(gradientQuad);
            }
        };

        createGradientTest(true, -1.02f, 0);  // Test premultiplied (left of center "answer")
        createGradientTest(false, 1.02f, 0);  // Test unpremultiplied (right of center "answer")

        RenderLoop(compositionHelper.GetSession(),
                   [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); })
            .Loop();
    }

    // Purpose: Validate eye visibility flags.
    TEST_CASE("Eye Visibility", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Eye Visibility");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "eye_visibility.png",
                                                        "A green quad is shown in the left eye and a blue quad is shown in the right eye.");
        compositionHelper.GetInteractionManager().AttachActionSets();

        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        XrCompositionLayerQuad* quad1 =
            compositionHelper.CreateQuadLayer(greenSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {-1, 0, -2}});
        quad1->eyeVisibility = XR_EYE_VISIBILITY_LEFT;
        interactiveLayerManager.AddLayer(quad1);

        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        XrCompositionLayerQuad* quad2 =
            compositionHelper.CreateQuadLayer(blueSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {1, 0, -2}});
        quad2->eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
        interactiveLayerManager.AddLayer(quad2);

        RenderLoop(compositionHelper.GetSession(),
                   [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); })
            .Loop();
    }

    TEST_CASE("Subimage Tests", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Subimage Tests");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "subimage.png",
            "Creates a 4x2 grid of quad layers testing subImage array index and imageRect. Red should not be visible except minor bleed in.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{Quat::Identity, {0, 0, -1}});

        constexpr float QuadZ = -4;  // How far away quads are placed.
        constexpr int ImageColCount = 4;
        constexpr int ImageArrayCount = 2;
        constexpr int ImageWidth = 1024;
        constexpr int ImageHeight = ImageWidth / ImageColCount;
        constexpr int RedZoneBorderSize = 16;
        constexpr int CellWidth = (ImageWidth / ImageColCount);
        constexpr int CellHeight = CellWidth;

        // Create an array swapchain
        auto swapchainCreateInfo =
            compositionHelper.DefaultColorSwapchainCreateInfo(ImageWidth, ImageHeight, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT);
        swapchainCreateInfo.format = GetGlobalData().graphicsPlugin->GetRGBA8Format(false /* sRGB */);
        swapchainCreateInfo.arraySize = ImageArrayCount;
        const XrSwapchain swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

        // Render a grid of numbers (1,2,3,4) in slice 0 and (5,6,7,8) in slice 1 of the swapchain
        // Create a quad layer referencing each number cell.
        compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) {
            int number = 1;
            for (int arraySlice = 0; arraySlice < ImageArrayCount; arraySlice++) {
                Conformance::RGBAImage numberGridImage(ImageWidth, ImageHeight);

                // All unused areas are red (should not be seen).
                numberGridImage.DrawRect(0, 0, numberGridImage.width, numberGridImage.height, Colors::Red);

                for (int x = 0; x < ImageColCount; x++) {
                    const auto& color = Colors::UniqueColors[number % Colors::UniqueColors.size()];
                    const XrRect2Di numberRect{{x * CellWidth + RedZoneBorderSize, RedZoneBorderSize},
                                               {CellWidth - RedZoneBorderSize * 2, CellHeight - RedZoneBorderSize * 2}};
                    numberGridImage.DrawRect(numberRect.offset.x, numberRect.offset.y, numberRect.extent.width, numberRect.extent.height,
                                             Colors::Transparent);
                    numberGridImage.PutText(numberRect, std::to_string(number).c_str(), CellHeight, color);
                    numberGridImage.DrawRectBorder(numberRect.offset.x, numberRect.offset.y, numberRect.extent.width,
                                                   numberRect.extent.height, 4, color);
                    number++;

                    const float quadX = Math::LinearMap(x, 0, ImageColCount - 1, -2.0f, 2.0f);
                    const float quadY = Math::LinearMap(arraySlice, 0, ImageArrayCount - 1, 0.75f, -0.75f);
                    XrCompositionLayerQuad* const quad =
                        compositionHelper.CreateQuadLayer(swapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {quadX, quadY, QuadZ}});
                    quad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    quad->subImage.imageArrayIndex = arraySlice;
                    quad->subImage.imageRect = numberRect;
                    quad->size.height = 1.0f;  // Height needs to be corrected since the imageRect is customized.
                    interactiveLayerManager.AddLayer(quad);
                }

                GetGlobalData().graphicsPlugin->CopyRGBAImage(swapchainImage, format, arraySlice, numberGridImage);
            }
        });

        RenderLoop(compositionHelper.GetSession(),
                   [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); })
            .Loop();
    }

    TEST_CASE("Projection Array Swapchain", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Projection Array Swapchain");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "projection_array.png",
            "Uses a single texture array for a projection layer (each view is a different slice and each slice has a unique color).");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace localSpace =
            compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosef{Quat::Identity, {0, 0, 0}});

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        // Because a single swapchain is being used for all views (each view is a slice of the texture array), the maximum dimensions must be used
        // since the dimensions of all slices are the same.
        const auto maxWidth = std::max_element(viewProperties.begin(), viewProperties.end(),
                                               [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                   return l.recommendedImageRectWidth < r.recommendedImageRectWidth;
                                               })
                                  ->recommendedImageRectWidth;
        const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                    return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                })
                                   ->recommendedImageRectHeight;

        // Create swapchain with array type.
        auto swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(maxWidth, maxHeight);
        swapchainCreateInfo.arraySize = (uint32_t)viewProperties.size() * 3;
        const XrSwapchain swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

        // Set up the projection layer
        XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
        for (uint32_t j = 0; j < projLayer->viewCount; j++) {
            // Use non-contiguous array indices to ferret out any assumptions that implementations are making
            // about array indices. In particular 0 != left and 1 != right, but this should test for other
            // assumptions too.
            uint32_t arrayIndex = swapchainCreateInfo.arraySize - (j * 2 + 1);
            const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = compositionHelper.MakeDefaultSubImage(swapchain, arrayIndex);
        }

        const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2}), Cube::Make({0, 1, -2})};

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each slice of the array swapchain using the projection layer view fov and pose.
                compositionHelper.AcquireWaitReleaseImage(
                    swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) {
                        for (uint32_t slice = 0; slice < (uint32_t)views.size(); slice++) {
                            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage,
                                                                            projLayer->views[slice].subImage.imageArrayIndex, format);

                            const_cast<XrFovf&>(projLayer->views[slice].fov) = views[slice].fov;
                            const_cast<XrPosef&>(projLayer->views[slice].pose) = views[slice].pose;
                            GetGlobalData().graphicsPlugin->RenderView(projLayer->views[slice], swapchainImage, format, cubes);
                        }
                    });

                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(compositionHelper.GetSession(), updateLayers).Loop();
    }

    TEST_CASE("Projection Wide Swapchain", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Projection Wide Swapchain");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_wide.png",
                                                        "Uses a single wide texture for a projection layer.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace localSpace =
            compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosef{Quat::Identity, {0, 0, 0}});

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        const auto totalWidth =
            std::accumulate(viewProperties.begin(), viewProperties.end(), 0,
                            [](uint32_t l, const XrViewConfigurationView& r) { return l + r.recommendedImageRectWidth; });
        // Because a single swapchain is being used for all views the maximum height must be used.
        const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                    return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                })
                                   ->recommendedImageRectHeight;

        // Create wide swapchain.
        const XrSwapchain swapchain =
            compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(totalWidth, maxHeight));

        XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
        int x = 0;
        for (uint32_t j = 0; j < projLayer->viewCount; j++) {
            XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);
            subImage.imageRect.offset = {x, 0};
            subImage.imageRect.extent = {(int32_t)viewProperties[j].recommendedImageRectWidth,
                                         (int32_t)viewProperties[j].recommendedImageRectHeight};
            const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = subImage;
            x += subImage.imageRect.extent.width;  // Each view is to the left of the previous view.
        }

        const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2}), Cube::Make({0, 1, -2})};

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                compositionHelper.AcquireWaitReleaseImage(
                    swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, format);
                        for (size_t view = 0; view < views.size(); view++) {
                            const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                            const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                            GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage, format, cubes);
                        }
                    });

                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(compositionHelper.GetSession(), updateLayers).Loop();
    }

    TEST_CASE("Projection Separate Swapchains", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Projection Separate Swapchains");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_separate.png",
                                                        "Uses separate textures for each projection layer view.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        SimpleProjectionLayerHelper simpleProjectionLayerHelper(compositionHelper);

        auto updateLayers = [&](const XrFrameState& frameState) {
            simpleProjectionLayerHelper.UpdateProjectionLayer(frameState);
            std::vector<XrCompositionLayerBaseHeader*> layers{simpleProjectionLayerHelper.GetProjectionLayer()};
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(compositionHelper.GetSession(), updateLayers).Loop();
    }

    TEST_CASE("Quad Hands", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Quad Hands");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "quad_hands.png",
                                                        "10x10cm Quads labeled \'L\' and \'R\' should appear 10cm along the grip "
                                                        "positive Z in front of the center of 10cm cubes rendered at the controller "
                                                        "grip poses. "
                                                        "The quads should face you and be upright when the controllers are in "
                                                        "a thumbs-up pointing-into-screen pose. "
                                                        "Check that the quads are properly backface-culled, "
                                                        "that \'R\' is always rendered atop \'L\', "
                                                        "and both are atop the cubes when visible.");

        const std::vector<XrPath> subactionPaths{StringToPath(compositionHelper.GetInstance(), "/user/hand/left"),
                                                 StringToPath(compositionHelper.GetInstance(), "/user/hand/right")};

        XrActionSet actionSet;
        XrAction gripPoseAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "quad_hands");
            strcpy(actionSetInfo.localizedActionSetName, "Quad Hands");
            XRC_CHECK_THROW_XRCMD(xrCreateActionSet(compositionHelper.GetInstance(), &actionSetInfo, &actionSet));

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.actionName, "grip_pose");
            strcpy(actionInfo.localizedActionName, "Grip pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));
        }

        compositionHelper.GetInteractionManager().AddActionSet(actionSet);
        XrPath simpleInteractionProfile = StringToPath(compositionHelper.GetInstance(), "/interaction_profiles/khr/simple_controller");
        compositionHelper.GetInteractionManager().AddActionBindings(
            simpleInteractionProfile,
            {{
                {gripPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/left/input/grip/pose")},
                {gripPoseAction, StringToPath(compositionHelper.GetInstance(), "/user/hand/right/input/grip/pose")},
            }});

        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        SimpleProjectionLayerHelper simpleProjectionLayerHelper(compositionHelper);

        // Spaces attached to the hand (subaction).
        std::vector<XrSpace> gripSpaces;

        // Create XrSpaces for each grip pose
        for (XrPath subactionPath : subactionPaths) {
            XrSpace space;
            XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            spaceCreateInfo.action = gripPoseAction;
            spaceCreateInfo.subactionPath = subactionPath;
            spaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, 0}};
            XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(compositionHelper.GetSession(), &spaceCreateInfo, &space));
            gripSpaces.push_back(std::move(space));
        }

        // Create 10x10cm L and R quads
        XrCompositionLayerQuad* const leftQuadLayer =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(64, 64, "L", 48)), gripSpaces[0],
                                              0.1f, {Quat::Identity, {0, 0, 0.1f}});

        XrCompositionLayerQuad* const rightQuadLayer =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(64, 64, "R", 48)), gripSpaces[1],
                                              0.1f, {Quat::Identity, {0, 0, 0.1f}});

        interactiveLayerManager.AddLayer(leftQuadLayer);
        interactiveLayerManager.AddLayer(rightQuadLayer);

        const XrVector3f cubeSize{0.1f, 0.1f, 0.1f};
        auto updateLayers = [&](const XrFrameState& frameState) {
            std::vector<Cube> cubes;
            for (const auto& space : gripSpaces) {
                XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
                if (XR_SUCCEEDED(
                        xrLocateSpace(space, simpleProjectionLayerHelper.GetLocalSpace(), frameState.predictedDisplayTime, &location))) {
                    if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
                        (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
                        cubes.emplace_back(Cube{location.pose, cubeSize});
                    }
                }
            }
            simpleProjectionLayerHelper.UpdateProjectionLayer(frameState, cubes);
            std::vector<XrCompositionLayerBaseHeader*> layers{simpleProjectionLayerHelper.GetProjectionLayer()};
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(compositionHelper.GetSession(), updateLayers).Loop();
    }

    TEST_CASE("Projection Mutable Field-of-View", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Projection Mutable Field-of-View");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_mutable.png",
                                                        "Uses mutable field-of-views for each projection layer view.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace localSpace =
            compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosef{Quat::Identity, {0, 0, 0}});

        if (!compositionHelper.GetViewConfigurationProperties().fovMutable) {
            return;
        }

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        const auto totalWidth =
            std::accumulate(viewProperties.begin(), viewProperties.end(), 0,
                            [](uint32_t l, const XrViewConfigurationView& r) { return l + r.recommendedImageRectWidth; });
        // Because a single swapchain is being used for all views the maximum height must be used.
        const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                    return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                })
                                   ->recommendedImageRectHeight;

        // Create wide swapchain.
        const XrSwapchain swapchain =
            compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(totalWidth, maxHeight));

        XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
        int x = 0;
        for (uint32_t j = 0; j < projLayer->viewCount; j++) {
            XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);
            subImage.imageRect.offset = {x, 0};
            subImage.imageRect.extent = {(int32_t)viewProperties[j].recommendedImageRectWidth,
                                         (int32_t)viewProperties[j].recommendedImageRectHeight};
            const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = subImage;
            x += subImage.imageRect.extent.width;  // Each view is to the left of the previous view.
        }

        const std::vector<Cube> cubes = {Cube::Make({-.2f, -.2f, -2}), Cube::Make({.2f, -.2f, -2}), Cube::Make({0, .1f, -2})};

        const XrVector3f Forward{0, 0, 1};
        XrQuaternionf roll180;
        XrQuaternionf_CreateFromAxisAngle(&roll180, &Forward, MATH_PI);

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                compositionHelper.AcquireWaitReleaseImage(
                    swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage, uint64_t format) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, format);
                        for (size_t view = 0; view < views.size(); view++) {
                            // Copy over the provided FOV and pose but use 40% of the suggested FOV.
                            const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                            const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                            const_cast<float&>(projLayer->views[view].fov.angleUp) *= 0.4f;
                            const_cast<float&>(projLayer->views[view].fov.angleDown) *= 0.4f;
                            const_cast<float&>(projLayer->views[view].fov.angleLeft) *= 0.4f;
                            const_cast<float&>(projLayer->views[view].fov.angleRight) *= 0.4f;

                            // Render using a 180 degree roll on Z which effectively creates a flip on both the X and Y axis.
                            XrCompositionLayerProjectionView rolled = projLayer->views[view];
                            XrQuaternionf_Multiply(&rolled.pose.orientation, &roll180, &views[view].pose.orientation);
                            GetGlobalData().graphicsPlugin->RenderView(rolled, swapchainImage, format, cubes);

                            // After rendering, report a flipped FOV on X and Y without the 180 degree roll, which has the same
                            // effect. This switcheroo is necessary since rendering with flipped FOV will result in an inverted
                            // winding causing normally hidden triangles to be visible and visible triangles to be hidden.
                            const_cast<float&>(projLayer->views[view].fov.angleUp) = -projLayer->views[view].fov.angleUp;
                            const_cast<float&>(projLayer->views[view].fov.angleDown) = -projLayer->views[view].fov.angleDown;
                            const_cast<float&>(projLayer->views[view].fov.angleLeft) = -projLayer->views[view].fov.angleLeft;
                            const_cast<float&>(projLayer->views[view].fov.angleRight) = -projLayer->views[view].fov.angleRight;
                        }
                    });

                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(compositionHelper.GetSession(), updateLayers).Loop();
    }
}  // namespace Conformance
