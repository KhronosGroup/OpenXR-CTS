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

#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "graphics_plugin.h"
#include "mesh_projection_layer.h"
#include "two_call_struct_metadata.h"
#include "two_call_struct_tests.h"
#include "type_utils.h"
#include "matchers.h"
#include "utilities/Geometry.h"
#include "utilities/types_and_constants.h"

#include <nonstd/type.hpp>

#include <earcut.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <openxr/openxr.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace Conformance
{
    constexpr XrColor4f BrightRed = {1.0, 0.0, 0.0, 1.0f};

    static inline void checkIndices(const XrVisibilityMaskKHR& visibilityMask)
    {
        for (uint32_t i = 0; i < visibilityMask.indexCountOutput; ++i) {
            CHECK(visibilityMask.indices[i] < visibilityMask.vertexCountOutput);  // Index should be valid.
        }
    }

    static inline bool isCounterClockwise(const XrVector2f& a, const XrVector2f& b, const XrVector2f& c)
    {
        return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y) >= 0;
    }

    static inline XrVector2f getVertexAtIndexNumber(const XrVisibilityMaskKHR& visibilityMask, uint32_t i)
    {
        return visibilityMask.vertices[visibilityMask.indices[i % visibilityMask.indexCountOutput]];
    }

    static inline uint32_t viewCountForConfiguration(XrViewConfigurationType viewConfigurationType)
    {

        switch (viewConfigurationType) {
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
            return 1;

        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
            return 2;

        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO:
            return 4;
        default:
            FAIL("Unknown view configuration type, update function");
        }
        return 0;
    }

    static bool HasVisibilityMask(XrSession session, PFN_xrGetVisibilityMaskKHR xrGetVisibilityMaskKHR_,
                                  XrViewConfigurationType viewConfigurationType)
    {

        int num_empty = 0;
        for (XrVisibilityMaskTypeKHR maskType :
             {XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR,
              XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR}) {

            XrVisibilityMaskKHR visibilityMask{XR_TYPE_VISIBILITY_MASK_KHR};
            REQUIRE_RESULT_SUCCEEDED(xrGetVisibilityMaskKHR_(session, viewConfigurationType, 0, maskType, &visibilityMask));
            if (visibilityMask.vertexCountOutput == 0) {
                num_empty++;
            }
        }
        INFO("Either all mask types return vertices, or none return vertices");
        REQUIRE_THAT(num_empty, In<int>({0, 3}));
        return num_empty == 0;
    }

    TEST_CASE("XR_KHR_visibility_mask", "[XR_KHR_visibility_mask]")
    {
        // successcodes="XR_SUCCESS,XR_SESSION_LOSS_PENDING"
        // errorcodes="XR_ERROR_HANDLE_INVALID,XR_ERROR_INSTANCE_LOST,XR_ERROR_RUNTIME_FAILURE,XR_ERROR_VALIDATION_FAILURE,
        //             XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED,XR_ERROR_SIZE_INSUFFICIENT,XR_ERROR_SESSION_LOST,XR_ERROR_FUNCTION_UNSUPPORTED"
        //
        // XrResult xrGetVisibilityMaskKHR(XrSession session, XrViewConfigurationType viewConfigurationType,
        //              uint32_t viewIndex, XrVisibilityMaskTypeKHR visibilityMaskType, XrVisibilityMaskKHR* visibilityMask);

        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME)) {
            SKIP(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME " not supported");
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Test run not using graphics plugin");
        }

        AutoBasicInstance instance({XR_KHR_VISIBILITY_MASK_EXTENSION_NAME});
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

        // Verify that we can acquire the function.
        auto xrGetVisibilityMaskKHR_ =
            GetInstanceExtensionFunction<PFN_xrGetVisibilityMaskKHR>(session.GetInstance(), "xrGetVisibilityMaskKHR");
        REQUIRE(xrGetVisibilityMaskKHR_ != nullptr);

        // We need to exercise whatever view configuration type is active (currently mono, stereo, quad),
        // and retrieve masks for 1, 2, or 4 views respectively, depending on the view configuration type.
        // We need to exercise each of the mask visibility types hidden, visible, line.
        // We need to exercise the two call idiom (call once to get required capacities).

        const XrViewConfigurationType viewConfigurationType = globalData.options.viewConfigurationValue;

        // First, make sure that either all mask types get an output, or none of them do.
        // SKip the rest of the test if there is no mask.
        bool hasMask = HasVisibilityMask(session, xrGetVisibilityMaskKHR_, viewConfigurationType);
        if (!hasMask) {
            WARN("Could not evaluate conformance of two-call idiom behavior because no visibility mask available");
            SKIP("No vertices returned, so no visibility mask available in this system.");
        }

        uint32_t viewCount = viewCountForConfiguration(viewConfigurationType);
        const auto twoCallData = getTwoCallStructData<XrVisibilityMaskKHR>();
        XrVisibilityMaskTypeKHR maskType =
            GENERATE(XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR,
                     XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR);

        for (uint32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
            CAPTURE(maskType);
            CAPTURE(viewIndex);

            XrVisibilityMaskKHR visibilityMask{XR_TYPE_VISIBILITY_MASK_KHR};
            CheckTwoCallStructConformance(twoCallData, visibilityMask, "xrGetVisibilityMaskKHR", true /* error if empty */,
                                          [&](XrVisibilityMaskKHR* mask) {
                                              return xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, mask);
                                          });

            // First call once to get the required vertex and index counts.
            REQUIRE_RESULT_SUCCEEDED(xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, &visibilityMask));

            // Runtime may return 0 vertices/indices if no view mask is available.
            if (visibilityMask.indexCountOutput == 0) {
                continue;
            }
            SECTION("Retrieve the full mask")
            {
                std::vector<XrVector2f> vertexVector(visibilityMask.vertexCountOutput);
                std::vector<uint32_t> indexVector(visibilityMask.indexCountOutput);
                visibilityMask.vertexCapacityInput = (uint32_t)vertexVector.size();
                visibilityMask.vertices = vertexVector.data();
                visibilityMask.indexCapacityInput = (uint32_t)indexVector.size();
                visibilityMask.indices = indexVector.data();

                // Call to get the full data. Expect success.
                REQUIRE_RESULT_SUCCEEDED(xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, &visibilityMask));

                // Do some output validation
                //! @todo move this to the conformance layer?
                checkIndices(visibilityMask);
                if (maskType == XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR ||
                    maskType == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR) {
                    REQUIRE((visibilityMask.indexCountOutput % 3) == 0);
                    for (uint32_t i = 0; i < visibilityMask.indexCountOutput; i += 3) {
                        // a, b, c should format a counter-clockwise triangle
                        CAPTURE(i);
                        auto a = getVertexAtIndexNumber(visibilityMask, i);
                        auto b = getVertexAtIndexNumber(visibilityMask, i + 1);
                        auto c = getVertexAtIndexNumber(visibilityMask, i + 2);
                        CAPTURE(a);
                        CAPTURE(b);
                        CAPTURE(c);
                        CHECK(isCounterClockwise(a, b, c));
                    }
                }
                else if (maskType == XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR) {
                    CHECK(visibilityMask.indexCountOutput == visibilityMask.vertexCountOutput);

                    for (uint32_t i = 0; i < visibilityMask.indexCountOutput; ++i) {
                        // The line is counter-clockwise (around the origin)
                        CAPTURE(i);
                        XrVector2f origin{0, 0};
                        XrVector2f a = getVertexAtIndexNumber(visibilityMask, i);
                        // With the last point implicitly connecting to the first point.
                        XrVector2f b = getVertexAtIndexNumber(visibilityMask, i + 1);
                        CAPTURE(a);
                        CAPTURE(b);
                        CHECK(isCounterClockwise(origin, a, b));
                    }
                }
                else {
                    FAIL("Unexpected value");
                }
            }

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // Exercise XR_ERROR_HANDLE_INVALID
                REQUIRE(XR_ERROR_HANDLE_INVALID ==
                        xrGetVisibilityMaskKHR_(XR_NULL_HANDLE_CPP, viewConfigurationType, viewIndex, maskType, &visibilityMask));

                REQUIRE(XR_ERROR_HANDLE_INVALID ==
                        xrGetVisibilityMaskKHR_(globalData.invalidSession, viewConfigurationType, viewIndex, maskType, &visibilityMask));
            }
        }
    }

    /**
     *
     * @param visibilityMask
     * @param indexVector The index vector used in @p visibilityMask
     */
    static inline void makeMaskFromLines(XrVisibilityMaskKHR& visibilityMask, std::vector<uint32_t>& indexVector)
    {

        // Create array
        using Point = std::array<float, 2>;
        std::vector<Point> polygon;
        polygon.reserve(visibilityMask.indexCountOutput);

        for (uint32_t i = 0; i < visibilityMask.indexCountOutput; ++i) {
            XrVector2f vertex = getVertexAtIndexNumber(visibilityMask, i);

            polygon.push_back({vertex.x, vertex.y});
        }

        // earcut wants a vector of polygons, subsequent polygons are holes
        std::vector<std::vector<Point>> polygons{polygon};

        // Run tessellation
        // Returns array of indices that refer to the vertices of the input polygon.
        // Three subsequent indices form a triangle. Output triangles are clockwise.
        indexVector = mapbox::earcut<uint32_t>(polygons);

        visibilityMask.indices = indexVector.data();
        visibilityMask.indexCountOutput = visibilityMask.indexCapacityInput = (uint32_t)indexVector.size();

        // earcut doesn't seem to output consistent winding order triangles,
        // so we fix them up here
        for (uint32_t i = 0; i < visibilityMask.indexCountOutput; i += 3) {
            // a, b, c should format a count-clockwise triangle
            auto a = getVertexAtIndexNumber(visibilityMask, i);
            auto b = getVertexAtIndexNumber(visibilityMask, i + 1);
            auto c = getVertexAtIndexNumber(visibilityMask, i + 2);
            if (!isCounterClockwise(a, b, c)) {
                std::swap(visibilityMask.indices[i + 1], visibilityMask.indices[i + 2]);
            }
        }
    }

    static std::tuple<MeshHandle, XrColor4f> MakeMaskMesh(XrSession session, PFN_xrGetVisibilityMaskKHR xrGetVisibilityMaskKHR_,
                                                          XrViewConfigurationType viewConfigurationType, uint32_t viewIndex,
                                                          XrVisibilityMaskTypeKHR maskType)
    {
        MeshHandle mesh;
        bool meshCoversHiddenArea = maskType == XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR;

        XrColor4f normalBgColor = GetGlobalData().GetClearColorForBackground();

        XrColor4f color = meshCoversHiddenArea ? BrightRed : normalBgColor;
        XrColor4f bgColor = meshCoversHiddenArea ? normalBgColor : BrightRed;

        XrVisibilityMaskKHR visibilityMask{XR_TYPE_VISIBILITY_MASK_KHR};

        // First call once to get the required vertex and index counts.
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, &visibilityMask));

        // Runtime may return 0 vertices/indices if no view mask is available.
        if (visibilityMask.indexCountOutput == 0) {
            // no mask?
            FAIL("Got zero indices, expected to get the mask");
            return {mesh, bgColor};
        }
        std::vector<XrVector2f> vertexVector(visibilityMask.vertexCountOutput);
        std::vector<uint32_t> indexVector(visibilityMask.indexCountOutput);
        visibilityMask.vertexCapacityInput = (uint32_t)vertexVector.size();
        visibilityMask.vertices = vertexVector.data();
        visibilityMask.indexCapacityInput = (uint32_t)indexVector.size();
        visibilityMask.indices = indexVector.data();

        // Call to get the full data. Expect success.
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, &visibilityMask));

        if (maskType == XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR) {
            makeMaskFromLines(visibilityMask, indexVector);
        }

        // Copy mesh indices, changing the winding order as well
        std::vector<uint16_t> meshIndices = {};
        meshIndices.reserve(visibilityMask.indexCountOutput);
        REQUIRE((visibilityMask.indexCountOutput % 3) == 0);
        for (uint32_t i = 0; i < visibilityMask.indexCountOutput; i += 3) {
            meshIndices.push_back(static_cast<uint16_t>(visibilityMask.indices[i]));
            meshIndices.push_back(static_cast<uint16_t>(visibilityMask.indices[i + 2]));
            meshIndices.push_back(static_cast<uint16_t>(visibilityMask.indices[i + 1]));
        }

        std::vector<Geometry::Vertex> meshVertices;
        meshVertices.reserve(visibilityMask.vertexCountOutput);
        for (uint32_t i = 0; i < visibilityMask.vertexCountOutput; i++) {
            XrVector2f vertex = visibilityMask.vertices[i];
            meshVertices.push_back({{vertex.x, vertex.y, -1.0}, {color.r, color.g, color.b}});
        }

        mesh = GetGlobalData().graphicsPlugin->MakeSimpleMesh(meshIndices, meshVertices);
        return {mesh, bgColor};
    }

    TEST_CASE("XR_KHR_visibility_mask-interactive", "[XR_KHR_visibility_mask][composition][interactive]")
    {
        // successcodes="XR_SUCCESS,XR_SESSION_LOSS_PENDING"
        // errorcodes="XR_ERROR_HANDLE_INVALID,XR_ERROR_INSTANCE_LOST,XR_ERROR_RUNTIME_FAILURE,XR_ERROR_VALIDATION_FAILURE,
        //             XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED,XR_ERROR_SIZE_INSUFFICIENT,XR_ERROR_SESSION_LOST,XR_ERROR_FUNCTION_UNSUPPORTED"
        //
        // XrResult xrGetVisibilityMaskKHR(XrSession session, XrViewConfigurationType viewConfigurationType,
        //              uint32_t viewIndex, XrVisibilityMaskTypeKHR visibilityMaskType, XrVisibilityMaskKHR* visibilityMask);

        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Test run not using graphics plugin");
        }

        if (!globalData.IsInstanceExtensionSupported(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME)) {
            SKIP(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper("Visibility Mask", {XR_KHR_VISIBILITY_MASK_EXTENSION_NAME});

        // Verify that we can acquire the function.
        auto xrGetVisibilityMaskKHR_ =
            GetInstanceExtensionFunction<PFN_xrGetVisibilityMaskKHR>(compositionHelper.GetInstance(), "xrGetVisibilityMaskKHR");
        REQUIRE(xrGetVisibilityMaskKHR_ != nullptr);

        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "visibility_mask_with_red.png",
            "The image shows a sample of what is actually rendered per eye. "
            "However, you should not see any red geometry when looking into the device, as red is rendered only where the visibility mask indicates "
            "content should not be rendered as it is likely not visible. "
            "If you see more than just a trace of red around the edges in normal usage (away from this instruction screen), fail the test.");
        compositionHelper.GetInteractionManager().AttachActionSets();

        MeshProjectionLayerHelper meshProjectionLayerHelper(compositionHelper);

        compositionHelper.BeginSession();

        // We need to exercise whatever view configuration type is active (currently mono, stereo, quad),
        // and retrieve masks for 1, 2, or 4 views respectively, depending on the view configuration type.
        // We need to exercise each of the mask visibility types hidden, visible, line.
        // We need to exercise the two call idiom (call once to get required capacities).

        const XrViewConfigurationType viewConfigurationType = globalData.options.viewConfigurationValue;

        // First, make sure that either all mask types get an output, or none of them do.
        // SKip the rest of the test if there is no mask.
        bool hasMask = HasVisibilityMask(compositionHelper.GetSession(), xrGetVisibilityMaskKHR_, viewConfigurationType);
        if (!hasMask) {
            SKIP("No vertices returned, so no visibility mask available in this system.");
        }

        std::vector<XrColor4f> bgColors{meshProjectionLayerHelper.GetViewCount(), Colors::Green};  // should be overwritten before render

        // const auto maskTypes = {XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR,
        // XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR};
        // for (XrVisibilityMaskTypeKHR maskType : maskTypes) {
        XrVisibilityMaskTypeKHR maskType =
            GENERATE(XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR,
                     XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR);
        {
            CAPTURE(maskType);
            std::vector<MeshHandle> meshes;
            const uint32_t nViews = meshProjectionLayerHelper.GetViewCount();
            meshes.reserve(nViews);
            for (uint32_t viewIndex = 0; viewIndex < nViews; ++viewIndex) {
                CAPTURE(viewIndex);
                CAPTURE(viewConfigurationType);
                auto meshAndBackground =
                    MakeMaskMesh(compositionHelper.GetSession(), xrGetVisibilityMaskKHR_, viewConfigurationType, viewIndex, maskType);
                INFO("Checking that we could successfully create the mesh");
                REQUIRE(std::get<MeshHandle>(meshAndBackground) != MeshHandle{});
                meshes.emplace_back(std::get<MeshHandle>(meshAndBackground));
                bgColors[viewIndex] = std::get<XrColor4f>(meshAndBackground);
            }

            meshProjectionLayerHelper.SetMeshes(std::move(meshes));
            meshProjectionLayerHelper.SetBgColors(std::move(bgColors));

            if (!meshProjectionLayerHelper.HasMeshes()) {
                WARN("Missing a mesh for this type.");
            }
            else {
                auto updateLayers = [&](const XrFrameState& frameState) {
                    std::vector<XrCompositionLayerBaseHeader*> layers;
                    if (XrCompositionLayerBaseHeader* projLayer = meshProjectionLayerHelper.TryGetUpdatedProjectionLayer(frameState)) {
                        layers.push_back(projLayer);
                    }
                    return interactiveLayerManager.EndFrame(frameState, layers);
                };

                RenderLoop(compositionHelper.GetSession(), updateLayers).Loop();
            }
        }
    }
}  // namespace Conformance
