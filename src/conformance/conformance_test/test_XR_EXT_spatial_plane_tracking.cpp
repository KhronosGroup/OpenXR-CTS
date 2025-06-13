// Copyright (c) 2019-2025, The Khronos Group Inc.
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

#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <functional>
#include <unordered_set>

#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "report.h"
#include "spatial_conformance_utils.h"
#include "spatial_test_runner.h"
#include "utilities/colors.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"
#include "utilities/xr_math_operators.h"

#define _USE_MATH_DEFINES
#include <cmath>

using namespace Conformance;
using Catch::Matchers::VectorContains;

namespace Conformance
{
    constexpr XrColor4f Purple = {0.62f, 0.125f, 0.93f, 1.f};

    namespace
    {

        TEST_CASE("XR_EXT_spatial_plane_tracking", "[XR_EXT_spatial_plane_tracking][XR_EXT_spatial_entity]")
        {
            XrSpatialCapabilityConfigurationPlaneTrackingEXT planeConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_PLANE_TRACKING_EXT,
                                                                         nullptr, XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT};

            TestSpatialConformance(XR_EXT_SPATIAL_PLANE_TRACKING_EXTENSION_NAME, XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT,
                                   {XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT, XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT}, planeConfig,
                                   XR_SPATIAL_COMPONENT_TYPE_BOUNDED_3D_EXT);
        }

        struct SpatialPlaneTrackingTestRunner : public SpatialTestRunner
        {
            SpatialPlaneTrackingTestRunner(const std::vector<XrSpatialComponentTypeEXT>& enabledComponents)
                : mEnabledComponents(enabledComponents)
            {
                mPlaneConfig = {
                    XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_PLANE_TRACKING_EXT,
                    nullptr,
                    XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT,
                    static_cast<uint32_t>(mEnabledComponents.size()),
                    mEnabledComponents.data(),
                };

                bounded2dList = {
                    XR_TYPE_SPATIAL_COMPONENT_BOUNDED_2D_LIST_EXT,
                };
                planeAlignmentList = {
                    XR_TYPE_SPATIAL_COMPONENT_PLANE_ALIGNMENT_LIST_EXT,
                };
                semanticLabelList = {
                    XR_TYPE_SPATIAL_COMPONENT_PLANE_SEMANTIC_LABEL_LIST_EXT,
                };
                polygon2dList = {
                    XR_TYPE_SPATIAL_COMPONENT_POLYGON_2D_LIST_EXT,
                };
            }

            std::vector<const char*> getRequiredExtensions() override
            {
                return {
                    XR_EXT_FUTURE_EXTENSION_NAME,
                    XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME,
                    XR_EXT_SPATIAL_PLANE_TRACKING_EXTENSION_NAME,
                };
            }

            void onCreateSpatialContext(std::vector<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>& capabilityConfigs) override
            {
                capabilityConfigs.push_back(reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&mPlaneConfig));
            }

            std::vector<XrSpatialComponentTypeEXT> getQueryComponents() override
            {
                return mEnabledComponents;
            }

            std::vector<XrBaseOutStructure*> getComponentDataListStructPtrs(uint32_t entityCount) override
            {
                std::vector<XrBaseOutStructure*> listStructPtrs;

                if (VectorContains(XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT).match(mEnabledComponents)) {
                    bounded2Ds.resize(entityCount);
                    bounded2dList.boundCount = static_cast<uint32_t>(bounded2Ds.size());
                    bounded2dList.bounds = bounded2Ds.data();
                    listStructPtrs.push_back(reinterpret_cast<XrBaseOutStructure*>(&bounded2dList));
                }

                if (VectorContains(XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT).match(mEnabledComponents)) {
                    planeAlignments.resize(entityCount);
                    planeAlignmentList.planeAlignmentCount = static_cast<uint32_t>(planeAlignments.size());
                    planeAlignmentList.planeAlignments = planeAlignments.data();
                    listStructPtrs.push_back(reinterpret_cast<XrBaseOutStructure*>(&planeAlignmentList));
                }

                if (VectorContains(XR_SPATIAL_COMPONENT_TYPE_PLANE_SEMANTIC_LABEL_EXT).match(mEnabledComponents)) {
                    planeSemanticLabels.resize(entityCount);
                    semanticLabelList.semanticLabelCount = static_cast<uint32_t>(planeSemanticLabels.size());
                    semanticLabelList.semanticLabels = planeSemanticLabels.data();
                    listStructPtrs.push_back(reinterpret_cast<XrBaseOutStructure*>(&semanticLabelList));
                }

                if (VectorContains(XR_SPATIAL_COMPONENT_TYPE_POLYGON_2D_EXT).match(mEnabledComponents)) {
                    polygon2Ds.resize(entityCount);
                    polygon2dList.polygonCount = static_cast<uint32_t>(polygon2Ds.size());
                    polygon2dList.polygons = polygon2Ds.data();
                    listStructPtrs.push_back(reinterpret_cast<XrBaseOutStructure*>(&polygon2dList));
                }

                return listStructPtrs;
            }

            void render() override
            {
                renderedCubes.clear();
                renderedMeshes.clear();

                for (uint32_t i = 0; i < static_cast<uint32_t>(bounded2Ds.size()); ++i) {
                    if (static_cast<uint32_t>(planeSemanticLabels.size()) > i) {
                        renderBounded2D(bounded2Ds[i], getColor(planeSemanticLabels[i]));
                    }
                    else if (static_cast<uint32_t>(planeAlignments.size()) > i) {
                        renderBounded2D(bounded2Ds[i], getColor(planeAlignments[i]));
                    }
                    else {
                        renderBounded2D(bounded2Ds[i]);
                    }
                }
            }

            XrColor4f getColor(XrSpatialPlaneAlignmentEXT planeAlignment)
            {
                switch (planeAlignment) {
                case XR_SPATIAL_PLANE_ALIGNMENT_HORIZONTAL_UPWARD_EXT:
                    return Colors::Yellow;
                case XR_SPATIAL_PLANE_ALIGNMENT_HORIZONTAL_DOWNWARD_EXT:
                    return Colors::Orange;
                case XR_SPATIAL_PLANE_ALIGNMENT_VERTICAL_EXT:
                    return Colors::Magenta;
                case XR_SPATIAL_PLANE_ALIGNMENT_ARBITRARY_EXT:
                    return Purple;
                default:
                    return XrColor4f{1, 1, 1, 1};
                }
            }

            XrColor4f getColor(XrSpatialPlaneSemanticLabelEXT semanticLabel)
            {
                switch (semanticLabel) {
                case XR_SPATIAL_PLANE_SEMANTIC_LABEL_FLOOR_EXT:
                    return Colors::Yellow;
                case XR_SPATIAL_PLANE_SEMANTIC_LABEL_WALL_EXT:
                    return Colors::Magenta;
                case XR_SPATIAL_PLANE_SEMANTIC_LABEL_CEILING_EXT:
                    return Colors::Orange;
                case XR_SPATIAL_PLANE_SEMANTIC_LABEL_TABLE_EXT:
                    return Purple;
                case XR_SPATIAL_PLANE_SEMANTIC_LABEL_UNCATEGORIZED_EXT:
                default:
                    return XrColor4f{1, 1, 1, 1};
                }
            }

            XrSpatialCapabilityConfigurationPlaneTrackingEXT mPlaneConfig;
            const std::vector<XrSpatialComponentTypeEXT> mEnabledComponents;

            XrSpatialComponentBounded2DListEXT bounded2dList;
            XrSpatialComponentPlaneAlignmentListEXT planeAlignmentList;
            XrSpatialComponentPlaneSemanticLabelListEXT semanticLabelList;
            XrSpatialComponentPolygon2DListEXT polygon2dList;

            std::vector<XrSpatialBounded2DDataEXT> bounded2Ds;
            std::vector<XrSpatialPlaneAlignmentEXT> planeAlignments;
            std::vector<XrSpatialPlaneSemanticLabelEXT> planeSemanticLabels;
            std::vector<XrSpatialPolygon2DDataEXT> polygon2Ds;
        };

        TEST_CASE("XR_EXT_spatial_plane_tracking-plane-alignment",
                  "[XR_EXT_spatial_plane_tracking][XR_EXT_spatial_entity][scenario]["
                  "interactive][no_auto]")
        {
            SpatialPlaneTrackingTestRunner({
                                               XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
                                               XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT,
                                           })
                .RunTest(XR_EXT_SPATIAL_PLANE_TRACKING_EXTENSION_NAME,
                         "All planes should be rendered, colored based on the alignment enum. "
                         "Horiz Up: Yellow, Horiz Down: Orange, Vertical: Magenta, Arbit: "
                         "Purple. Blue Axis (Z) must be the plane's normal. "
                         "Press the select button on either controller to pass the test.");
        }

        TEST_CASE("XR_EXT_spatial_plane_tracking-semantic-label",
                  "[XR_EXT_spatial_plane_tracking][XR_EXT_spatial_entity][scenario]["
                  "interactive][no_auto]")
        {
            SpatialPlaneTrackingTestRunner({
                                               XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
                                               XR_SPATIAL_COMPONENT_TYPE_PLANE_SEMANTIC_LABEL_EXT,
                                           })
                .RunTest(XR_EXT_SPATIAL_PLANE_TRACKING_EXTENSION_NAME,
                         "All planes should be rendered, colored based on the semantic label "
                         "enum. "
                         "Floor: Yellow, Wall: Magenta, Ceiling: Orange, Table: Purple, "
                         "Others: "
                         "White. Blue Axis (Z) must be the plane's normal. "
                         "Press the select button on either controller to pass the test.");
        }

        struct SpatialPlaneTrackingUpdateSnapshotTestRunner : public SpatialPlaneTrackingTestRunner
        {
            static constexpr size_t kMaxEntityHandles = 5;

            SpatialPlaneTrackingUpdateSnapshotTestRunner(const std::vector<XrSpatialComponentTypeEXT>& enabledComponents)
                : SpatialPlaneTrackingTestRunner(enabledComponents)
            {
            }

            void postCreateSpatialContextCompletion(const XrFrameState&) override
            {
                xrCreateSpatialEntityFromIdEXT =
                    GetInstanceExtensionFunction<PFN_xrCreateSpatialEntityFromIdEXT>(instance, "xrCreateSpatialEntityFromIdEXT");
                xrCreateSpatialUpdateSnapshotEXT =
                    GetInstanceExtensionFunction<PFN_xrCreateSpatialUpdateSnapshotEXT>(instance, "xrCreateSpatialUpdateSnapshotEXT");
                xrDestroySpatialSnapshotEXT =
                    GetInstanceExtensionFunction<PFN_xrDestroySpatialSnapshotEXT>(instance, "xrDestroySpatialSnapshotEXT");
            }

            void postCreateDiscoverySnapshotCompletion(XrSpatialSnapshotEXT snapshot) override
            {
                SpatialPlaneTrackingTestRunner::postCreateDiscoverySnapshotCompletion(snapshot);

                // Create handles for kMaxEntityHandles number of unique entities. For
                // these entities, we create an "update snapshot" every frame to
                // render them.
                if (entityHandles.size() < kMaxEntityHandles) {
                    for (uint32_t i = 0; i < static_cast<uint32_t>(entityIds.size()) && entityHandles.size() < kMaxEntityHandles; ++i) {
                        if (idsForEntityHandles.find(entityIds[i]) == idsForEntityHandles.end()) {
                            XrSpatialEntityFromIdCreateInfoEXT entityCreateInfo{
                                XR_TYPE_SPATIAL_ENTITY_FROM_ID_CREATE_INFO_EXT,
                                nullptr,
                                entityIds[i],
                            };
                            XrSpatialEntityEXT spatialEntity = XR_NULL_HANDLE;
                            XRC_CHECK_THROW_XRCMD(xrCreateSpatialEntityFromIdEXT(spatialContext, &entityCreateInfo, &spatialEntity));
                            entityHandles.push_back(spatialEntity);
                            idsForEntityHandles.insert(entityIds[i]);
                        }
                    }
                }

                // Stop discovery once we have the requisite number of entity handles.
                if (entityHandles.size() == kMaxEntityHandles) {
                    state = DiscoveryState::StopDiscovery;
                }
            }

            void render() override
            {
            }

            void onUpdate(const XrFrameState& frameState) override
            {
                if (entityHandles.size() > 0) {
                    XrSpatialSnapshotEXT updateSnapshot;
                    XrSpatialUpdateSnapshotCreateInfoEXT updateSnapshotCreateInfo{
                        XR_TYPE_SPATIAL_UPDATE_SNAPSHOT_CREATE_INFO_EXT,
                        nullptr,
                        static_cast<uint32_t>(entityHandles.size()),
                        entityHandles.data(),
                        0,
                        nullptr,
                        localSpace,
                        frameState.predictedDisplayTime,
                    };

                    XRC_CHECK_THROW_XRCMD(xrCreateSpatialUpdateSnapshotEXT(spatialContext, &updateSnapshotCreateInfo, &updateSnapshot));

                    querySnapshot(updateSnapshot);
                    SpatialPlaneTrackingTestRunner::render();
                    XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(updateSnapshot));
                }
            }

            std::vector<XrSpatialEntityEXT> entityHandles;
            std::unordered_set<XrSpatialEntityIdEXT> idsForEntityHandles;

            PFN_xrCreateSpatialEntityFromIdEXT xrCreateSpatialEntityFromIdEXT;
            PFN_xrCreateSpatialUpdateSnapshotEXT xrCreateSpatialUpdateSnapshotEXT;
            PFN_xrDestroySpatialSnapshotEXT xrDestroySpatialSnapshotEXT;
        };

        TEST_CASE("XR_EXT_spatial_plane_tracking-update-snapshot",
                  "[XR_EXT_spatial_plane_tracking][XR_EXT_spatial_entity][scenario]["
                  "interactive][no_auto]")
        {
            SpatialPlaneTrackingUpdateSnapshotTestRunner({
                                                             XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
                                                             XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT,
                                                         })
                .RunTest(XR_EXT_SPATIAL_PLANE_TRACKING_EXTENSION_NAME,
                         "Entity handles are created for the first 5 discovered planes "
                         "are rendered as per their \"update snapshot\", created every frame, "
                         "and colored based on the alignment enum. "
                         "Horiz Up: Yellow, Horiz Down: Orange, Vertical: Magenta, Arbit: "
                         "Purple. "
                         "Press the select button on either controller to pass the test.");
        }

    }  // namespace
}  // namespace Conformance
