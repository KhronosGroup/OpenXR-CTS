// Copyright (c) 2019-2026 The Khronos Group Inc.
// Copyright (c) Meta Platforms, LLC and its affiliates. All rights reserved.
//
// SPDX-License-Identifier: Apache-2.0

#include "utilities/utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "composition_utils.h"
#include <catch2/catch_test_macros.hpp>
#include "utilities/system_properties_helper.h"
#include "utilities/xr_math_operators.h"
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

using namespace Conformance;

namespace Conformance
{
    constexpr XrVector3f Up{0, 1, 0};

    static const auto SystemSupportsHandTracking =
        MakeSystemPropertiesBoolChecker(XrSystemHandTrackingPropertiesEXT{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT},
                                        &XrSystemHandTrackingPropertiesEXT::supportsHandTracking);

#define CTS_DECLARE_FUNCTION_POINTER(name, ext) PFN_xr##name xr##name##_;
#define CTS_LOAD_FUNCTION_POINTER(name, ext) xr##name##_ = GetInstanceExtensionFunction<PFN_xr##name>(instance, "xr" #name);

    // clang-format off

#define CTS_EXTENSION_DATA_FOR(ext)                             \
    struct ExtensionDataFor##ext                                \
    {                                                           \
        XR_LIST_FUNCTIONS_##ext(CTS_DECLARE_FUNCTION_POINTER)   \
                                                                \
        ExtensionDataFor##ext(XrInstance instance)              \
        {                                                       \
            XR_LIST_FUNCTIONS_##ext(CTS_LOAD_FUNCTION_POINTER)  \
        }                                                       \
    }

    // clang-format on

    CTS_EXTENSION_DATA_FOR(XR_EXT_hand_tracking);
    CTS_EXTENSION_DATA_FOR(XR_FB_hand_tracking_mesh);

    struct HandMeshData
    {
        std::array<XrPosef, XR_HAND_JOINT_COUNT_EXT> jointBindPoses;
        std::array<float, XR_HAND_JOINT_COUNT_EXT> jointRadii;
        std::array<XrHandJointEXT, XR_HAND_JOINT_COUNT_EXT> jointParents;

        std::vector<XrVector3f> vertexPositions;
        std::vector<XrVector3f> vertexNormals;
        std::vector<XrVector2f> vertexUVs;
        std::vector<XrVector4sFB> vertexBlendIndices;
        std::vector<XrVector4f> vertexBlendWeights;
        std::vector<int16_t> indices;

        bool operator==(const HandMeshData& other) const
        {
            return jointBindPoses == other.jointBindPoses && jointRadii == other.jointRadii && jointParents == other.jointParents &&
                   vertexPositions == other.vertexPositions && vertexNormals == other.vertexNormals && vertexUVs == other.vertexUVs &&
                   vertexBlendIndices == other.vertexBlendIndices && vertexBlendWeights == other.vertexBlendWeights &&
                   indices == other.indices;
        }

        static HandMeshData QueryHandMesh(PFN_xrGetHandMeshFB xrGetHandMeshFB_, XrHandTrackerEXT tracker)
        {
            HandMeshData meshData;

            XrHandTrackingMeshFB mesh{XR_TYPE_HAND_TRACKING_MESH_FB};
            REQUIRE(XR_SUCCESS == xrGetHandMeshFB_(tracker, &mesh));

            REQUIRE(mesh.jointCountOutput == XR_HAND_JOINT_COUNT_EXT);
            REQUIRE(mesh.vertexCountOutput != 0);
            REQUIRE(mesh.indexCountOutput != 0);

            mesh.jointCapacityInput = mesh.jointCountOutput;
            mesh.jointBindPoses = meshData.jointBindPoses.data();
            mesh.jointRadii = meshData.jointRadii.data();
            mesh.jointParents = meshData.jointParents.data();

            meshData.vertexPositions.resize(mesh.vertexCountOutput);
            meshData.vertexNormals.resize(mesh.vertexCountOutput);
            meshData.vertexUVs.resize(mesh.vertexCountOutput);
            meshData.vertexBlendIndices.resize(mesh.vertexCountOutput);
            meshData.vertexBlendWeights.resize(mesh.vertexCountOutput);

            mesh.vertexCapacityInput = mesh.vertexCountOutput;
            mesh.vertexPositions = meshData.vertexPositions.data();
            mesh.vertexNormals = meshData.vertexNormals.data();
            mesh.vertexUVs = meshData.vertexUVs.data();
            mesh.vertexBlendIndices = meshData.vertexBlendIndices.data();
            mesh.vertexBlendWeights = meshData.vertexBlendWeights.data();

            meshData.indices.resize(mesh.indexCountOutput);

            mesh.indexCapacityInput = mesh.indexCountOutput;
            mesh.indices = meshData.indices.data();

            REQUIRE(XR_SUCCESS == xrGetHandMeshFB_(tracker, &mesh));

            return meshData;
        }
    };

    static constexpr int LEFT_HAND = 0;
    static constexpr int RIGHT_HAND = 1;
    static constexpr int HAND_COUNT = 2;

    TEST_CASE("XR_FB_hand_tracking_mesh", "[XR_FB_hand_tracking_mesh]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME)) {
            SECTION("Not supported")
            {
                // Runtime does not support extension - it should not be possible to get function pointers.
                AutoBasicInstance instance;
                ValidateInstanceExtensionFunctionNotSupported(instance, "xrGetHandMeshFB");
            }

            // Skip all following tests.
            SKIP(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME " not supported");
        }

        SECTION("Extension not enabled")
        {
            if (!globalData.IsInstanceExtensionEnabled(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME)) {
                AutoBasicInstance instance;
                ValidateInstanceExtensionFunctionNotSupported(instance, "xrGetHandMeshFB");
            }
            else {
                WARN(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME " force-enabled, cannot test behavior when extension is disabled.");
            }
        }

        SECTION("Query hand mesh for distinct sessions")
        {
            std::vector<const char*> extensions = {XR_EXT_HAND_TRACKING_EXTENSION_NAME, XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME};
            AutoBasicInstance instance(extensions, AutoBasicInstance::createSystemId);

            XrSystemId systemId = instance.systemId;
            if (!SystemSupportsHandTracking(instance, systemId)) {
                // xrCreateHandTrackerEXT returns XR_ERROR_FEATURE_UNSUPPORTED
                // (This is tested in test_XR_EXT_hand_tracking.cpp)
                SKIP("Device does not support hand tracking");
            }

            ExtensionDataForXR_EXT_hand_tracking ext_ht(instance);
            ExtensionDataForXR_FB_hand_tracking_mesh ext_htm(instance);

            static constexpr size_t SESSION_COUNT = 2;
            HandMeshData m[SESSION_COUNT][HAND_COUNT];

            for (size_t sessionIndex = 0; sessionIndex < SESSION_COUNT; ++sessionIndex) {
                AutoBasicSession session(AutoBasicSession::beginSession, instance);

                for (size_t hand = 0; hand < HAND_COUNT; ++hand) {
                    XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                    createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                    createInfo.hand = (hand == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);

                    XrHandTrackerEXT handTracker = XR_NULL_HANDLE;
                    REQUIRE(XR_SUCCESS == ext_ht.xrCreateHandTrackerEXT_(session, &createInfo, &handTracker));

                    XrHandTrackingMeshFB mesh{XR_TYPE_HAND_TRACKING_MESH_FB};
                    REQUIRE(XR_SUCCESS == ext_htm.xrGetHandMeshFB_(handTracker, &mesh));

                    // Using XR_HAND_JOINT_SET_DEFAULT_EXT -> XR_HAND_JOINT_COUNT_EXT
                    REQUIRE(mesh.jointCountOutput == XR_HAND_JOINT_COUNT_EXT);
                    REQUIRE(mesh.vertexCountOutput != 0);
                    REQUIRE(mesh.indexCountOutput != 0);

                    m[sessionIndex][hand] = HandMeshData::QueryHandMesh(ext_htm.xrGetHandMeshFB_, handTracker);

                    REQUIRE(XR_SUCCESS == ext_ht.xrDestroyHandTrackerEXT_(handTracker));
                }
            }

            // Static mesh data (required to be the same for the lifetime of the XrInstance)
            for (size_t hand = 0; hand < HAND_COUNT; ++hand) {
                REQUIRE(m[0][hand] == m[1][hand]);
            }
        }

        SECTION("Query hand mesh for distinct data sources")
        {
            if (!globalData.IsInstanceExtensionSupported(XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME)) {
                SKIP(XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME " not supported");
            }

            std::vector<const char*> extensions = {XR_EXT_HAND_TRACKING_EXTENSION_NAME, XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME,
                                                   XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME};
            AutoBasicInstance instance(extensions, AutoBasicInstance::createSystemId);

            XrSystemId systemId = instance.systemId;
            if (!SystemSupportsHandTracking(instance, systemId)) {
                // xrCreateHandTrackerEXT returns XR_ERROR_FEATURE_UNSUPPORTED
                // (This is tested in test_XR_EXT_hand_tracking.cpp)
                SKIP("Device does not support hand tracking");
            }

            ExtensionDataForXR_EXT_hand_tracking ext_ht(instance);
            ExtensionDataForXR_FB_hand_tracking_mesh ext_htm(instance);

            static constexpr size_t DATA_SOURCE_COUNT = 2;
            HandMeshData m[DATA_SOURCE_COUNT][HAND_COUNT];

            std::array<std::array<XrHandTrackingDataSourceEXT, 1>, DATA_SOURCE_COUNT> testDataSources = {{
                std::array<XrHandTrackingDataSourceEXT, 1>{{XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT}},
                std::array<XrHandTrackingDataSourceEXT, 1>{{XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT}},
            }};

            AutoBasicSession session(AutoBasicSession::beginSession, instance);

            for (size_t dataSourcesIndex = 0; dataSourcesIndex < testDataSources.size(); ++dataSourcesIndex) {
                for (size_t hand = 0; hand < HAND_COUNT; ++hand) {
                    XrHandTrackingDataSourceInfoEXT dataSourceInfo{XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT};
                    dataSourceInfo.requestedDataSourceCount = static_cast<uint32_t>(testDataSources[dataSourcesIndex].size());
                    dataSourceInfo.requestedDataSources = testDataSources[dataSourcesIndex].data();

                    XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                    createInfo.next = &dataSourceInfo;
                    createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                    createInfo.hand = (hand == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);

                    XrHandTrackerEXT handTracker = XR_NULL_HANDLE;
                    REQUIRE(XR_SUCCESS == ext_ht.xrCreateHandTrackerEXT_(session, &createInfo, &handTracker));

                    XrHandTrackingMeshFB mesh{XR_TYPE_HAND_TRACKING_MESH_FB};
                    REQUIRE(XR_SUCCESS == ext_htm.xrGetHandMeshFB_(handTracker, &mesh));

                    // Using XR_HAND_JOINT_SET_DEFAULT_EXT -> XR_HAND_JOINT_COUNT_EXT
                    REQUIRE(mesh.jointCountOutput == XR_HAND_JOINT_COUNT_EXT);
                    REQUIRE(mesh.vertexCountOutput != 0);
                    REQUIRE(mesh.indexCountOutput != 0);

                    m[dataSourcesIndex][hand] = HandMeshData::QueryHandMesh(ext_htm.xrGetHandMeshFB_, handTracker);

                    REQUIRE(XR_SUCCESS == ext_ht.xrDestroyHandTrackerEXT_(handTracker));
                }
            }

            // Static mesh data (required to be the same for the lifetime of the XrInstance)
            for (size_t hand = 0; hand < HAND_COUNT; ++hand) {
                REQUIRE(m[0][hand] == m[1][hand]);
            }
        }
    }

    TEST_CASE("XR_FB_hand_tracking_mesh-interactive", "[XR_FB_hand_tracking_mesh][scenario][interactive][no_auto]")
    {
        const char* instructions =
            "Hand meshes are rendered for both hands. "
            "Move your hands around to see the mesh tracking. "
            "Bring index finger of both hands together to complete the validation. "
            "Prevent both hands from tracking for 20 seconds to fail.";

        static constexpr float kTipDistanceRequired = 0.01f;  // 1cm
        static constexpr std::chrono::nanoseconds kHandTrackingLostTimeout = 20s;
        static constexpr std::chrono::nanoseconds kHandTrackingGainedTime = 1s;

        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME) ||
            !globalData.IsInstanceExtensionSupported(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME)) {
            SKIP(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME " not supported");
        }

        std::vector<const char*> extensions = {XR_EXT_HAND_TRACKING_EXTENSION_NAME, XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME};

        // If hand tracking data source extension is supported, we test with it too.
        // If not, no problem - we just don't test with it.
        if (globalData.IsInstanceExtensionSupported(XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME)) {
            extensions.push_back(XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME);
        }

        CompositionHelper compositionHelper("XR_FB_hand_tracking_mesh", extensions);

        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();

        if (!SystemSupportsHandTracking(instance, compositionHelper.GetSystemId())) {
            SKIP("Device does not support hand tracking");
        }

        ExtensionDataForXR_EXT_hand_tracking ext_ht(instance);
        ExtensionDataForXR_FB_hand_tracking_mesh ext_htm(instance);

        std::array<XrHandTrackerEXT, HAND_COUNT> handTracker;
        std::array<HandMeshData, HAND_COUNT> handMeshData;

        for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
            std::array<XrHandTrackingDataSourceEXT, 2> dataSources = {
                {XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT, XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT}};
            XrHandTrackingDataSourceInfoEXT dataSourceInfo{XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT};
            dataSourceInfo.requestedDataSourceCount = static_cast<uint32_t>(dataSources.size());
            dataSourceInfo.requestedDataSources = dataSources.data();

            XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
            createInfo.next = &dataSourceInfo;
            createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
            createInfo.hand = (hand == LEFT_HAND ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);
            REQUIRE(XR_SUCCESS == ext_ht.xrCreateHandTrackerEXT_(session, &createInfo, &handTracker[hand]));
        }

        std::array<XrVector3f, HAND_COUNT> handColor = {{XrVector3f{0.8f, 0.2f, 0.2f}, XrVector3f{0.2f, 0.2f, 0.8f}}};
        std::array<GLTFModelHandle, HAND_COUNT> handMeshHandle;
        std::array<GLTFModelInstanceHandle, HAND_COUNT> handMeshInstanceHandle;

        auto processVertex = [](const HandMeshData& meshData, XrVector3f handColor, size_t vertIndex,
                                const std::array<XrPosef, XR_HAND_JOINT_COUNT_EXT>* skinPose = nullptr) -> Pbr::Vertex {
            XrVector3f position = meshData.vertexPositions[vertIndex];
            XrVector3f normal = meshData.vertexNormals[vertIndex];

            if (skinPose != nullptr) {
                const XrVector4sFB& bi = meshData.vertexBlendIndices[vertIndex];
                const XrVector4f& bw = meshData.vertexBlendWeights[vertIndex];
                const std::array<int16_t, 4> blendIndex = {{bi.x, bi.y, bi.z, bi.w}};
                const std::array<float, 4> blendWeight = {{bw.x, bw.y, bw.z, bw.w}};

                auto applyWeights = [&blendIndex, &blendWeight, &skinPose](XrVector3f unskinned) -> XrVector3f {
                    XrVector3f skinned = {0, 0, 0};
                    for (int j = 0; j < 4; ++j) {
                        float weight = blendWeight[j];
                        int16_t jointIndex = blendIndex[j];

                        XRC_CHECK_THROW(weight >= 0);
                        XRC_CHECK_THROW(jointIndex >= 0 && jointIndex < XR_HAND_JOINT_COUNT_EXT);
                        if (weight > 0) {
                            XrVector3f bindSpacePos;
                            XrPosef_TransformVector3f(&bindSpacePos, &(*skinPose)[jointIndex], &unskinned);
                            bindSpacePos = bindSpacePos * weight;
                            skinned += bindSpacePos;
                        }
                    }
                    return skinned;
                };

                position = applyWeights(position);
                normal = applyWeights(normal);
            }

            // arbitrary tangent to avoid NaNs
            XrVector3f tangent;
            {
                // pick an axis that isn't too saturated
                const XrVector3f crossAxis = normal.y < 0.7 ? XrVector3f{0.0f, 1.0f, 0.0f} : XrVector3f{0.0f, 0.0f, 1.0f};
                tangent = Vector::CrossProduct(normal, crossAxis);
                Vector::Normalize(tangent);
            }

            return Pbr::Vertex{
                position,
                normal,
                XrVector4f{tangent.x, tangent.y, tangent.z, 1.0},
                {handColor.x, handColor.y, handColor.z, 1.0f},
                meshData.vertexUVs[vertIndex],
                Pbr::RootNodeIndex,
            };
        };

        // Get static mesh data once during initialization
        for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
            handMeshData[hand] = HandMeshData::QueryHandMesh(ext_htm.xrGetHandMeshFB_, handTracker[hand]);
            auto& meshData = handMeshData[hand];

            std::vector<Pbr::Vertex> vertices;
            vertices.reserve(meshData.vertexPositions.size());
            for (size_t i = 0; i < meshData.vertexPositions.size(); ++i) {
                vertices.push_back(processVertex(meshData, handColor[hand], i));
            }

            std::vector<Pbr::Vertex> wireframe = vertices;
            for (Pbr::Vertex& wireframeVertex : wireframe) {
                // bias wireframe along normal
                wireframeVertex.Position += wireframeVertex.Normal * 0.0001f;
            }

            std::vector<uint32_t> renderIndices;
            renderIndices.reserve(meshData.indices.size());
            for (size_t i = 0; i + 2 < meshData.indices.size(); i += 3) {
                renderIndices.push_back(static_cast<uint32_t>(meshData.indices[i]));
                renderIndices.push_back(static_cast<uint32_t>(meshData.indices[i + 2]));
                renderIndices.push_back(static_cast<uint32_t>(meshData.indices[i + 1]));
            }

            std::shared_ptr<Pbr::Model> pbrModel = std::make_shared<Pbr::Model>();
            GetGlobalData().graphicsPlugin->WithGltfBuilder([&](Pbr::IGltfBuilder& gltfBuilder) {
                auto pbrMaterial = gltfBuilder.CreateFlatMaterial({1.0f, 1.0f, 1.0f, 1.0f}, 0.05f, 0.0f, {0.0f, 0.0f, 0.0f});
                pbrMaterial->Name = "XR_FB_hand_tracking_mesh-interactive pbr material";
                pbrMaterial->SetShader(Pbr::Shader::Pbr);
                pbrMaterial->SetFillMode(Pbr::FillMode::Solid);

                auto wireframeMaterial = gltfBuilder.CreateFlatMaterial({1.0f, 1.0f, 1.0f, 0.1f}, 0.05f, 0.0f, {0.0f, 0.0f, 0.0f});
                wireframeMaterial->Name = "XR_FB_hand_tracking_mesh-interactive wireframe material";
                wireframeMaterial->SetShader(Pbr::Shader::Unlit);
                wireframeMaterial->SetFillMode(Pbr::FillMode::Wireframe);

                std::set<Pbr::NodeIndex_t> nodeIndices = {Pbr::RootNodeIndex};

                Pbr::PrimitiveHandle pbrPrimitive =
                    gltfBuilder.MakePrimitive(Pbr::PrimitiveBuilder{std::move(vertices), renderIndices, nodeIndices}, pbrMaterial);
                Pbr::PrimitiveHandle wireframePrimitive = gltfBuilder.MakePrimitive(
                    Pbr::PrimitiveBuilder{std::move(wireframe), std::move(renderIndices), std::move(nodeIndices)}, wireframeMaterial);
                pbrModel->AddPrimitive(pbrPrimitive);
                pbrModel->AddPrimitive(wireframePrimitive);
            });

            handMeshHandle[hand] = GetGlobalData().graphicsPlugin->RegisterPbrModel(std::move(pbrModel));
            handMeshInstanceHandle[hand] = GetGlobalData().graphicsPlugin->CreateGLTFModelInstance(handMeshHandle[hand]);
        }

        compositionHelper.GetInteractionManager().AddDefaultActions(compositionHelper.GetInstance());

        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();

        compositionHelper.BeginSession();

        XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

        XrCompositionLayerQuad* instructionsQuad =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(1024, 512, instructions, 48)),
                                              localSpace, 1.0f, {{0, 0, 0, 1.f}, {-1.5f, 0, -0.3f}});
        instructionsQuad->pose.orientation = Quat::FromAxisAngle(Up, DegToRad(70));

        auto viewConfigurationViews = compositionHelper.EnumerateConfigurationViews();
        std::vector<XrSwapchain> swapchains;
        XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);

        for (uint32_t j = 0; j < projLayer->viewCount; j++) {
            const XrSwapchainCreateInfo& colorSwapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(
                viewConfigurationViews[j].recommendedImageRectWidth, viewConfigurationViews[j].recommendedImageRectHeight);
            swapchains.push_back(compositionHelper.CreateSwapchain(colorSwapchainCreateInfo));
            const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = compositionHelper.MakeDefaultSubImage(swapchains[j]);
        }

        Stopwatch sinceHandLastContinuouslySeen;
        // avoid brief tracking glitches resetting the timer
        Stopwatch handSeenContinuouslyFor;

        auto update = [&](const XrFrameState& frameState) {
            std::vector<GLTFModelInstanceHandle> renderedMeshes;

            std::array<std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT>, 2> jointLocations;

            bool eitherHandIsTracked = false;

            // Use pre-fetched static mesh data and only get current joint locations
            for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
                const auto& meshData = handMeshData[hand];

                // Get current hand joint locations for skinning
                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                locations.jointCount = static_cast<uint32_t>(jointLocations[hand].size());
                locations.jointLocations = jointLocations[hand].data();

                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                locateInfo.baseSpace = localSpace;
                locateInfo.time = frameState.predictedDisplayTime;

                REQUIRE(XR_SUCCESS == ext_ht.xrLocateHandJointsEXT_(handTracker[hand], &locateInfo, &locations));

                if (locations.isActive) {
                    for (const XrHandJointLocationEXT& jointLocation : jointLocations[hand]) {
                        if ((jointLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) ==
                            XR_SPACE_LOCATION_POSITION_TRACKED_BIT) {
                            eitherHandIsTracked = true;
                            break;
                        }
                    }

                    XrPosef root = jointLocations[hand][XR_HAND_JOINT_WRIST_EXT].pose;
                    XrPosef rootInv = Pose::Invert(root);

                    std::array<XrPosef, XR_HAND_JOINT_COUNT_EXT> skinPose;

                    /// Update transforms
                    for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
                        XrPosef pose = jointLocations[hand][i].pose;
                        XrPosef transform = rootInv * pose;
                        XrPosef invBindPose = Pose::Invert(meshData.jointBindPoses[i]);
                        skinPose[i] = transform * invBindPose;
                    }

                    // Convert mesh data to rendering format
                    // (Note: ideally this type of work would be done on GPU / shader)
                    std::vector<Pbr::Vertex> vertices;
                    vertices.reserve(meshData.vertexPositions.size());
                    for (size_t i = 0; i < meshData.vertexPositions.size(); ++i) {
                        vertices.push_back(processVertex(meshData, handColor[hand], i, &skinPose));
                    }

                    std::vector<Pbr::Vertex> wireframe = vertices;
                    for (Pbr::Vertex& wireframeVertex : wireframe) {
                        // bias wireframe along normal
                        wireframeVertex.Position += wireframeVertex.Normal * 0.0001f;
                    }

                    std::vector<uint32_t> renderIndices;
                    renderIndices.reserve(meshData.indices.size());
                    for (size_t i = 0; i + 2 < meshData.indices.size(); i += 3) {
                        renderIndices.push_back(static_cast<uint32_t>(meshData.indices[i]));
                        renderIndices.push_back(static_cast<uint32_t>(meshData.indices[i + 2]));
                        renderIndices.push_back(static_cast<uint32_t>(meshData.indices[i + 1]));
                    }

                    GetGlobalData().graphicsPlugin->WithGltfBuilder([&](Pbr::IGltfBuilder& gltfBuilder) {
                        std::shared_ptr<Pbr::Model> model = GetGlobalData().graphicsPlugin->GetPbrModel(handMeshHandle[hand]);
                        const std::vector<Pbr::PrimitiveHandle>& handles = model->GetPrimitiveHandles();
                        gltfBuilder.UpdatePrimitive(handles[0], renderIndices, vertices);
                        gltfBuilder.UpdatePrimitive(handles[1], renderIndices, wireframe);
                    });

                    Pbr::ModelInstance& modelInstance = GetGlobalData().graphicsPlugin->GetModelInstance(handMeshInstanceHandle[hand]);
                    modelInstance.SetModelToWorld(root, {1.0f, 1.0f, 1.0f});
                    renderedMeshes.push_back(handMeshInstanceHandle[hand]);
                }
            }

            // Note BEGIN: this block is duplicated in test_XR_EXT_hand_tracking.cpp
            auto runTimerWhile = [](Stopwatch& stopwatch, bool predicate) {
                // if the predicate is false, cancel the timer
                if (!predicate) {
                    stopwatch.Stop();
                }
                // if the predicate is true, make sure timer is running.
                else if (!stopwatch.IsStarted()) {
                    stopwatch.Restart();
                }
            };

            runTimerWhile(handSeenContinuouslyFor, eitherHandIsTracked);
            bool handContinuouslySeen =
                (handSeenContinuouslyFor.IsStarted() && handSeenContinuouslyFor.Elapsed() >= kHandTrackingGainedTime);
            runTimerWhile(sinceHandLastContinuouslySeen, !handContinuouslySeen);

            // Check if user has requested to fail or complete the test.
            {
                // Check if the user has not had tracking of either hand for at least 20 seconds
                // This may be the user deliberately failing the test or because of lack of permissions
                if (sinceHandLastContinuouslySeen.IsStarted() && sinceHandLastContinuouslySeen.Elapsed() >= kHandTrackingLostTimeout) {
                    FAIL("Test failed by user request - neither hand was tracked for longer than timeout");
                }
                XrHandJointLocationEXT& leftIndexTip = jointLocations[LEFT_HAND][XR_HAND_JOINT_INDEX_TIP_EXT];
                XrHandJointLocationEXT& rightIndexTip = jointLocations[RIGHT_HAND][XR_HAND_JOINT_INDEX_TIP_EXT];

                if ((leftIndexTip.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                    (rightIndexTip.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
                    XrVector3f distance = leftIndexTip.pose.position - rightIndexTip.pose.position;
                    float len = Vector::Length(distance);
                    // bring center of index fingers to within 1cm. Probably fine for most humans, unless
                    // they have huge fingers.
                    if (len < kTipDistanceRequired) {
                        return false;
                    }
                }
            }
            // Note END: this block is duplicated in test_XR_EXT_hand_tracking.cpp

            compositionHelper.GetInteractionManager().SyncActions(XR_NULL_PATH);
            if (compositionHelper.GetInteractionManager().GetDefaultSelectPressed(session)) {
                return false;
            }

            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);
            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                for (size_t view = 0; view < views.size(); view++) {
                    compositionHelper.AcquireWaitReleaseImage(swapchains[view], [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, compositionHelper.GetEnvironmentBlendMode());
                        const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                        const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                        GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage,
                                                                   RenderParams().Draw(renderedMeshes));
                    });
                }

                layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
            }

            layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(instructionsQuad)});

            compositionHelper.EndFrame(frameState.predictedDisplayTime, layers);

            compositionHelper.PollEvents();

            return true;
        };

        RenderLoop(session, update).Loop();

        for (auto hand : {LEFT_HAND, RIGHT_HAND}) {
            REQUIRE(XR_SUCCESS == ext_ht.xrDestroyHandTrackerEXT_(handTracker[hand]));
        }

        // Potential future test:
        //  - XrHandTrackingScaleFB validation: the runtime must scale the output of this xrLocateHandJointsEXT call according to overrideValueInput
    }
}  // namespace Conformance
