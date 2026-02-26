// Copyright (c) 2019-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "ext_render_model.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "controller_animation_handler.h"
#include "graphics_plugin.h"
#include "cts_tinygltf.h"
#include "gltf_helpers.h"
#include "two_call_struct_tests.h"
#include "two_call_struct_metadata.h"
#include "utilities/uuid_utils.h"

#include <openxr/openxr.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <tuple>

namespace Conformance
{

    void deleters::RenderModelEXTDelete::operator()(XrRenderModelEXT s) const
    {

        if (s != XR_NULL_HANDLE) {
            xrDestroyRenderModelEXT_(s);
        }
    }

    void deleters::RenderModelAssetEXTDelete::operator()(XrRenderModelAssetEXT s) const
    {

        if (s != XR_NULL_HANDLE) {
            xrDestroyRenderModelAssetEXT_(s);
        }
    }
    namespace
    {
        std::tuple<RenderModelEXTScoped, std::vector<XrRenderModelAssetNodePropertiesEXT>, std::shared_ptr<const tinygltf::Model>>
        testRenderModelWithCreateInfo(const DispatchTable_EXT_render_model& ext, XrSession session,
                                      const XrRenderModelCreateInfoEXT& createInfo)
        {
            RenderModelEXTScoped rm(ext.xrDestroyRenderModelEXT_);
            {
                INFO("Creating render model");
                XrRenderModelEXT rmRaw;
                REQUIRE(XR_SUCCESS == ext.xrCreateRenderModelEXT_(session, &createInfo, &rmRaw));
                rm.adopt(rmRaw);
            }
            XrRenderModelPropertiesEXT properties{XR_TYPE_RENDER_MODEL_PROPERTIES_EXT};
            {
                INFO("Getting render model properties with a null getInfo");
                REQUIRE(XR_SUCCESS == ext.xrGetRenderModelPropertiesEXT_(rm.get(), nullptr, &properties));
            }
            {
                // The properties of an slink:XrRenderModelEXT handle are immutable and must:
                // not change for the lifetime of the handle.

                // The runtime must: set pname:cacheId to a valid UUID value and subsequent calls
                // to flink:xrGetRenderModelPropertiesEXT with the same slink:XrRenderModelEXT and
                // slink:XrRenderModelPropertiesGetInfoEXT values must: return the same values for
                // pname:animatableNodeCount and pname:cacheId.

                INFO("Populating properties2 using a non-null getInfo");
                XrRenderModelPropertiesEXT properties2{XR_TYPE_RENDER_MODEL_PROPERTIES_EXT};

                const XrRenderModelPropertiesGetInfoEXT getInfo{XR_TYPE_RENDER_MODEL_PROPERTIES_GET_INFO_EXT};
                REQUIRE(XR_SUCCESS == ext.xrGetRenderModelPropertiesEXT_(rm.get(), &getInfo, &properties2));

                INFO("Expect it to match the properties from the previous call");
                REQUIRE(properties.animatableNodeCount == properties2.animatableNodeCount);
                REQUIRE(properties.cacheId == properties2.cacheId);
                {

                    INFO("Populating properties3 using a non-null getInfo");
                    XrRenderModelPropertiesEXT properties3{XR_TYPE_RENDER_MODEL_PROPERTIES_EXT};
                    REQUIRE(XR_SUCCESS == ext.xrGetRenderModelPropertiesEXT_(rm.get(), &getInfo, &properties3));

                    INFO("Expect it to match the properties from the previous call");
                    REQUIRE(properties2.animatableNodeCount == properties3.animatableNodeCount);
                    REQUIRE(properties2.cacheId == properties3.cacheId);
                }
            }

            RenderModelEXTScoped rm2(ext.xrDestroyRenderModelEXT_);
            {
                INFO("Creating a second render model with the same create info");
                XrRenderModelEXT rmRaw;
                REQUIRE(XR_SUCCESS == ext.xrCreateRenderModelEXT_(session, &createInfo, &rmRaw));
                rm2.adopt(rmRaw);
            }

            {
                INFO("Populating properties2 using the second render model handle");
                XrRenderModelPropertiesEXT properties4{XR_TYPE_RENDER_MODEL_PROPERTIES_EXT};
                REQUIRE(XR_SUCCESS == ext.xrGetRenderModelPropertiesEXT_(rm2.get(), nullptr, &properties4));
                REQUIRE(properties.animatableNodeCount == properties4.animatableNodeCount);
                REQUIRE(properties.cacheId == properties4.cacheId);
            }

            std::vector<XrRenderModelAssetNodePropertiesEXT> nodeProperties;

            std::shared_ptr<const tinygltf::Model> model;
            {
                RenderModelAssetEXTScoped rma(ext.xrDestroyRenderModelAssetEXT_);
                Stopwatch assetLoadTime;
                {
                    INFO("Creating render model asset - this part may be slow");
                    XrRenderModelAssetEXT rmaRaw;

                    XrRenderModelAssetCreateInfoEXT assetCreateInfo{XR_TYPE_RENDER_MODEL_ASSET_CREATE_INFO_EXT};
                    assetCreateInfo.cacheId = properties.cacheId;
                    assetLoadTime.Restart();
                    REQUIRE(XR_SUCCESS == ext.xrCreateRenderModelAssetEXT_(session, &assetCreateInfo, &rmaRaw));
                    assetLoadTime.Stop();
                    rma.adopt(rmaRaw);
                }

                // Checks for error behavior in getting asset properties
                if (properties.animatableNodeCount != 0) {
                    INFO("Asset properties with zero node name space");

                    XrRenderModelAssetPropertiesEXT assetProperties{XR_TYPE_RENDER_MODEL_ASSET_PROPERTIES_EXT};
                    CHECK(XR_ERROR_VALIDATION_FAILURE == ext.xrGetRenderModelAssetPropertiesEXT_(rma.get(), nullptr, &assetProperties));
                }

                if (properties.animatableNodeCount > 2) {
                    INFO("Asset properties with insufficient node name space");
                    nodeProperties.resize(2);

                    XrRenderModelAssetPropertiesEXT assetProperties{XR_TYPE_RENDER_MODEL_ASSET_PROPERTIES_EXT};
                    assetProperties.nodeProperties = nodeProperties.data();
                    assetProperties.nodePropertyCount = (uint32_t)nodeProperties.size();
                    CHECK(XR_ERROR_VALIDATION_FAILURE == ext.xrGetRenderModelAssetPropertiesEXT_(rma.get(), nullptr, &assetProperties));
                }
                {
                    INFO("Asset properties with too much node name space");
                    nodeProperties.resize(properties.animatableNodeCount + 1);

                    XrRenderModelAssetPropertiesEXT assetProperties{XR_TYPE_RENDER_MODEL_ASSET_PROPERTIES_EXT};
                    assetProperties.nodeProperties = nodeProperties.data();
                    assetProperties.nodePropertyCount = (uint32_t)nodeProperties.size();
                    CHECK(XR_ERROR_VALIDATION_FAILURE == ext.xrGetRenderModelAssetPropertiesEXT_(rma.get(), nullptr, &assetProperties));
                }

                // Get the real properties for real
                {
                    nodeProperties.resize(properties.animatableNodeCount);
                    XrRenderModelAssetPropertiesEXT assetProperties{XR_TYPE_RENDER_MODEL_ASSET_PROPERTIES_EXT};
                    assetProperties.nodeProperties = nodeProperties.data();
                    assetProperties.nodePropertyCount = (uint32_t)nodeProperties.size();

                    INFO("Get the real asset properties - this part should be fast");
                    Stopwatch assetPropertiesTime(true);
                    REQUIRE(XR_SUCCESS == ext.xrGetRenderModelAssetPropertiesEXT_(rma.get(), nullptr, &assetProperties));
                    assetPropertiesTime.Stop();

                    // we add 1ms of margin to the "slow" step to get the limit for a "fast" step
                    // TODO do we need to add this specific detail to the spec explicitly?
                    CHECK(assetLoadTime.Elapsed() + std::chrono::milliseconds(1) > assetPropertiesTime.Elapsed());
                }

                std::vector<uint8_t> buffer;
                {
                    INFO("Get the asset buffer - this part should be fast.");
                    XrRenderModelAssetDataEXT assetData{XR_TYPE_RENDER_MODEL_ASSET_DATA_EXT};

                    {
                        INFO("Two-call idiom: first call, get buffer size");
                        Stopwatch assetBufferTime(true);
                        REQUIRE(XR_SUCCESS == ext.xrGetRenderModelAssetDataEXT_(rma.get(), nullptr, &assetData));
                        assetBufferTime.Stop();
                        INFO("Verify that this 'fast step' is no slower than the by-design 'slow step' (plus 1ms for margin)");
                        CHECK(assetLoadTime.Elapsed() + std::chrono::milliseconds(1) > assetBufferTime.Elapsed());
                    }

                    REQUIRE(assetData.bufferCountOutput != 0);
                    buffer.resize(assetData.bufferCountOutput);
                    assetData.buffer = buffer.data();
                    assetData.bufferCapacityInput = (uint32_t)buffer.size();

                    {
                        INFO("Two-call idiom: second call, copy data");
                        Stopwatch assetBufferTime(true);
                        REQUIRE(XR_SUCCESS == ext.xrGetRenderModelAssetDataEXT_(rma.get(), nullptr, &assetData));
                        assetBufferTime.Stop();
                        {
                            INFO("Size must not have changed");
                            REQUIRE(assetData.bufferCountOutput == buffer.size());
                        }
                        INFO("Verify that this 'fast step' is no slower than the by-design 'slow step' (plus 1ms for margin)");
                        CHECK(assetLoadTime.Elapsed() + std::chrono::milliseconds(1) > assetBufferTime.Elapsed());
                    }
                }

                {
                    REQUIRE_NOTHROW(model = LoadGLTF(buffer));
                    REQUIRE(model != nullptr);
                    if (createInfo.gltfExtensionCount == 0) {
                        INFO("No glTF extensions were reported as supported, so none may be required in the asset.");
                        CHECK(model->extensionsRequired.empty());
                    }
                    else {
                        std::vector<std::string> gltfExtensions;
                        gltfExtensions.reserve(createInfo.gltfExtensionCount);
                        for (uint32_t i = 0; i < createInfo.gltfExtensionCount; ++i) {
                            gltfExtensions.emplace_back(createInfo.gltfExtensions[i]);
                        }

                        INFO("All required extensions from the model must be in the list the app supplied");
                        CHECK_THAT(gltfExtensions, Catch::Matchers::Contains(model->extensionsRequired));
                    }

                    std::vector<std::string> nodeNames;
                    nodeNames.reserve(nodeProperties.size());
                    std::transform(nodeProperties.begin(), nodeProperties.end(), std::back_inserter(nodeNames),
                                   [](const XrRenderModelAssetNodePropertiesEXT& nodeProp) { return std::string{nodeProp.uniqueName}; });
                    {
                        // The runtime must: return node names in pname:properties member
                        // slink:XrRenderModelAssetPropertiesEXT::pname:properties that are unique within
                        // the corresponding glTF asset.
                        INFO("Names of animatable nodes must be unique in the list");
                        // converting to a set will remove duplicates
                        std::set<std::string> uniqueNodeNames(nodeNames.begin(), nodeNames.end());
                        CHECK(uniqueNodeNames.size() == nodeNames.size());
                        {
                            INFO("None of the specified animatable node names should be the empty string");
                            CHECK(uniqueNodeNames.count("") == 0);
                        }
                    }
                    {

                        // The string returned in pname:uniqueName must: be the name of exactly one
                        // node in the glTF asset.
                        INFO("Names of animatable nodes must be unique in the model");
                        for (const auto& nodeName : nodeNames) {
                            if (!nodeName.empty()) {
                                CAPTURE(nodeName);
                                CHECK(1 == std::count_if(model->nodes.begin(), model->nodes.end(),
                                                         [&](const tinygltf::Node& node) { return node.name == nodeName; }));
                            }
                        }
                    }

                    // TODO: For a valid slink:XrRenderModelPropertiesEXT::pname:cacheId, the runtime
                    // must: return the same glTF asset data, even between different sessions, if
                    // the cache ID is returned from both sessions.

                    // TODO: Further, the runtime must: return the same glTF binary data for any
                    // slink:XrRenderModelAssetEXT handles created using the same slink:XrUuidEXT
                    // slink:XrRenderModelPropertiesEXT::pname:cacheId.
                }
                // TODO write buffer to disk for offline validation of asset against glTF spec

                // Do automated struct-two-call tests on xrGetRenderModelAssetDataEXT
                // true means we are saying that empty is an error
                CheckTwoCallStructConformance(getTwoCallStructData<XrRenderModelAssetDataEXT>(), {XR_TYPE_RENDER_MODEL_ASSET_DATA_EXT},
                                              "xrGetRenderModelAssetDataEXT", true, [&](XrRenderModelAssetDataEXT* assetData) {
                                                  return ext.xrGetRenderModelAssetDataEXT_(rma.get(), nullptr, assetData);
                                              });
            }
            return std::make_tuple(std::move(rm), std::move(nodeProperties), std::move(model));
        }
    }  // namespace

    RenderModelData testRenderModel(const DispatchTable_EXT_render_model& ext, XrSession session, XrRenderModelIdEXT rmid,
                                    bool testNoExtensions, const span<const char*> extensions)
    {
        XrRenderModelCreateInfoEXT ci{XR_TYPE_RENDER_MODEL_CREATE_INFO_EXT};
        ci.renderModelId = rmid;
        if (testNoExtensions) {
            INFO("Testing this render model ID with no glTF extensions");
            // discarding return value because we don't care about this
            testRenderModelWithCreateInfo(ext, session, ci);
        }
        ci.gltfExtensions = extensions.data();
        ci.gltfExtensionCount = (uint32_t)extensions.size();
        RenderModelEXTScoped rm{ext.xrDestroyRenderModelEXT_};
        std::vector<XrRenderModelAssetNodePropertiesEXT> nodeProperties;
        std::shared_ptr<const tinygltf::Model> tinygltfModel;
        {
            INFO("Testing this render model ID with these extensions");
            CAPTURE(extensions);

            std::tie(rm, nodeProperties, tinygltfModel) = testRenderModelWithCreateInfo(ext, session, ci);
        }

        // Load the model into the graphics plugin
        auto& graphicsPlugin = *GetGlobalData().graphicsPlugin;
        GLTFModelHandle gltfModel = graphicsPlugin.LoadGLTF(tinygltfModel);
        GLTFModelInstanceHandle modelInstance = graphicsPlugin.CreateGLTFModelInstance(gltfModel);
        RenderModelData ret{
            std::move(rm),
            gltfModel,
            modelInstance,
        };

        // Create animation handler
        ret.animationHandler = RenderModelAnimationHandler(graphicsPlugin.GetPbrModel(gltfModel), nodeProperties);
        return ret;
    }

}  // namespace Conformance
