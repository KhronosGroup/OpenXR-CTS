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

#if !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif  // !defined(_USE_MATH_DEFINES)
#include <cmath>

using namespace Conformance;
using Catch::Matchers::VectorContains;

namespace Conformance
{
    namespace
    {

        TEST_CASE("XR_EXT_spatial_marker_tracking-qr-code", "[XR_EXT_spatial_marker_tracking][XR_EXT_spatial_entity]")
        {
            XrSpatialCapabilityConfigurationQrCodeEXT qrCodeConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_QR_CODE_EXT, nullptr,
                                                                   XR_SPATIAL_CAPABILITY_MARKER_TRACKING_QR_CODE_EXT};

            TestSpatialConformance(XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME, XR_SPATIAL_CAPABILITY_MARKER_TRACKING_QR_CODE_EXT,
                                   {XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT, XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT}, qrCodeConfig,
                                   XR_SPATIAL_COMPONENT_TYPE_BOUNDED_3D_EXT);
        }

        TEST_CASE("XR_EXT_spatial_marker_tracking-micro-qr-code", "[XR_EXT_spatial_marker_tracking][XR_EXT_spatial_entity]")
        {
            XrSpatialCapabilityConfigurationMicroQrCodeEXT microQrCodeConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_MICRO_QR_CODE_EXT,
                                                                             nullptr,
                                                                             XR_SPATIAL_CAPABILITY_MARKER_TRACKING_MICRO_QR_CODE_EXT};

            TestSpatialConformance(XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME, XR_SPATIAL_CAPABILITY_MARKER_TRACKING_MICRO_QR_CODE_EXT,
                                   {XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT, XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT}, microQrCodeConfig,
                                   XR_SPATIAL_COMPONENT_TYPE_BOUNDED_3D_EXT);
        }

        TEST_CASE("XR_EXT_spatial_marker_tracking-aruco", "[XR_EXT_spatial_marker_tracking][XR_EXT_spatial_entity]")
        {
            XrSpatialCapabilityConfigurationArucoMarkerEXT arucoConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ARUCO_MARKER_EXT};
            arucoConfig.capability = XR_SPATIAL_CAPABILITY_MARKER_TRACKING_ARUCO_MARKER_EXT;
            arucoConfig.enabledComponentCount = 0;
            arucoConfig.enabledComponents = nullptr;
            arucoConfig.arUcoDict = XR_SPATIAL_MARKER_ARUCO_DICT_4X4_50_EXT;

            TestSpatialConformance(XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME, XR_SPATIAL_CAPABILITY_MARKER_TRACKING_ARUCO_MARKER_EXT,
                                   {XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT, XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT}, arucoConfig,
                                   XR_SPATIAL_COMPONENT_TYPE_BOUNDED_3D_EXT);
        }

        TEST_CASE("XR_EXT_spatial_marker_tracking-april", "[XR_EXT_spatial_marker_tracking][XR_EXT_spatial_entity]")
        {
            XrSpatialCapabilityConfigurationAprilTagEXT aprilTagConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_APRIL_TAG_EXT};
            aprilTagConfig.capability = XR_SPATIAL_CAPABILITY_MARKER_TRACKING_APRIL_TAG_EXT;
            aprilTagConfig.enabledComponentCount = 0;
            aprilTagConfig.enabledComponents = nullptr;
            aprilTagConfig.aprilDict = XR_SPATIAL_MARKER_APRIL_TAG_DICT_16H5_EXT;

            TestSpatialConformance(XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME, XR_SPATIAL_CAPABILITY_MARKER_TRACKING_APRIL_TAG_EXT,
                                   {XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT, XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT}, aprilTagConfig,
                                   XR_SPATIAL_COMPONENT_TYPE_BOUNDED_3D_EXT);
        }

        struct SpatialMarkerTrackingTestRunner : public SpatialTestRunner
        {
            explicit SpatialMarkerTrackingTestRunner(const XrSpatialCapabilityConfigurationBaseHeaderEXT& capabilityConfig)
                : mCapabilityConfig(capabilityConfig)
                , mEnabledComponents(capabilityConfig.enabledComponents,
                                     capabilityConfig.enabledComponents + capabilityConfig.enabledComponentCount)
            {

                bounded2dList = {XR_TYPE_SPATIAL_COMPONENT_BOUNDED_2D_LIST_EXT};
                markerList = {XR_TYPE_SPATIAL_COMPONENT_MARKER_LIST_EXT};
            }

            std::vector<const char*> getRequiredExtensions() override
            {
                return {
                    XR_EXT_FUTURE_EXTENSION_NAME,
                    XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME,
                    XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME,
                };
            }

            void onCreateSpatialContext(std::vector<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>& capabilityConfigs) override
            {
                capabilityConfigs.push_back(&mCapabilityConfig);
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

                if (VectorContains(XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT).match(mEnabledComponents)) {
                    markers.resize(entityCount);
                    markerList.markerCount = static_cast<uint32_t>(markers.size());
                    markerList.markers = markers.data();
                    listStructPtrs.push_back(reinterpret_cast<XrBaseOutStructure*>(&markerList));
                }

                return listStructPtrs;
            }

            void render() override
            {
                renderedCubes.clear();
                renderedMeshes.clear();

                for (uint32_t i = 0; i < static_cast<uint32_t>(bounded2Ds.size()); ++i) {
                    REQUIRE(markers[i].capability == mCapabilityConfig.capability);
                    if (markers[i].capability == XR_SPATIAL_CAPABILITY_MARKER_TRACKING_QR_CODE_EXT ||
                        markers[i].capability == XR_SPATIAL_CAPABILITY_MARKER_TRACKING_MICRO_QR_CODE_EXT) {
                        REQUIRE(markers[i].markerId == 0);
                    }
                    else {
                        REQUIRE(markers[i].data.bufferId == XR_NULL_SPATIAL_BUFFER_ID_EXT);
                    }

                    if (markers[i].data.bufferId != XR_NULL_SPATIAL_BUFFER_ID_EXT) {
                        renderBounded2D(bounded2Ds[i], Colors::Magenta);
                    }
                    else {
                        renderBounded2D(bounded2Ds[i], Colors::Yellow);
                    }
                }
            }

            const XrSpatialCapabilityConfigurationBaseHeaderEXT& mCapabilityConfig;
            const std::vector<XrSpatialComponentTypeEXT> mEnabledComponents;

            XrSpatialComponentBounded2DListEXT bounded2dList{};
            XrSpatialComponentMarkerListEXT markerList{};

            std::vector<XrSpatialBounded2DDataEXT> bounded2Ds;
            std::vector<XrSpatialMarkerDataEXT> markers;
        };

        TEST_CASE("XR_EXT_spatial_marker_tracking-qr-code-interactive",
                  "[XR_EXT_spatial_marker_tracking][XR_EXT_spatial_entity][scenario]["
                  "interactive][no_auto]")
        {
            const std::array<XrSpatialComponentTypeEXT, 2> enabledComponents = {
                XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
                XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT,
            };

            XrSpatialCapabilityConfigurationQrCodeEXT qrCodeConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_QR_CODE_EXT};
            qrCodeConfig.capability = XR_SPATIAL_CAPABILITY_MARKER_TRACKING_QR_CODE_EXT;
            qrCodeConfig.enabledComponentCount = static_cast<uint32_t>(enabledComponents.size());
            qrCodeConfig.enabledComponents = enabledComponents.data();

            SpatialMarkerTrackingTestRunner(*reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&qrCodeConfig))
                .RunTest(XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME,
                         "Discovered QR codes should be rendered magenta if their data is "
                         "known,"
                         "and yellow otherwise. This test only renders the QR code at initial "
                         "location of discovery, does not update the pose per frame. "
                         "Press the select button on either controller to pass the test.");
        }

        TEST_CASE("XR_EXT_spatial_marker_tracking-micro-qr-code-interactive",
                  "[XR_EXT_spatial_marker_tracking][XR_EXT_spatial_entity][scenario]["
                  "interactive][no_auto]")
        {
            const std::array<XrSpatialComponentTypeEXT, 2> enabledComponents = {
                XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
                XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT,
            };
            XrSpatialCapabilityConfigurationMicroQrCodeEXT microQrCodeConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_MICRO_QR_CODE_EXT};
            microQrCodeConfig.capability = XR_SPATIAL_CAPABILITY_MARKER_TRACKING_MICRO_QR_CODE_EXT;
            microQrCodeConfig.enabledComponentCount = static_cast<uint32_t>(enabledComponents.size());
            microQrCodeConfig.enabledComponents = enabledComponents.data();

            SpatialMarkerTrackingTestRunner(*reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&microQrCodeConfig))
                .RunTest(XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME,
                         "Discovered Micro QR codes should be rendered magenta if their data is "
                         "known,"
                         "and yellow otherwise. This test only renders the QR code at initial "
                         "location of discovery, does not update the pose per frame. "
                         "Press the select button on either controller to pass the test.");
        }

        TEST_CASE("XR_EXT_spatial_marker_tracking-aruco-interactive",
                  "[XR_EXT_spatial_marker_tracking][XR_EXT_spatial_entity][scenario]["
                  "interactive][no_auto]")
        {
            const std::array<XrSpatialComponentTypeEXT, 2> enabledComponents = {
                XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
                XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT,
            };

            XrSpatialCapabilityConfigurationArucoMarkerEXT arucoConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ARUCO_MARKER_EXT};
            arucoConfig.capability = XR_SPATIAL_CAPABILITY_MARKER_TRACKING_ARUCO_MARKER_EXT;
            arucoConfig.enabledComponentCount = static_cast<uint32_t>(enabledComponents.size());
            arucoConfig.enabledComponents = enabledComponents.data();
            arucoConfig.arUcoDict = XR_SPATIAL_MARKER_ARUCO_DICT_5X5_50_EXT;

            SpatialMarkerTrackingTestRunner(*reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&arucoConfig))
                .RunTest(XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME,
                         "Discovered Aruco Markers should be rendered yellow. "
                         "This test only renders the QR code at initial "
                         "location of discovery, does not update the pose per frame. "
                         "Press the select button on either controller to pass the test.");
        }

        TEST_CASE("XR_EXT_spatial_marker_tracking-april-tag-interactive",
                  "[XR_EXT_spatial_marker_tracking][XR_EXT_spatial_entity][scenario]["
                  "interactive][no_auto]")
        {
            const std::array<XrSpatialComponentTypeEXT, 2> enabledComponents = {
                XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
                XR_SPATIAL_COMPONENT_TYPE_MARKER_EXT,
            };
            XrSpatialCapabilityConfigurationAprilTagEXT aprilTagConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_APRIL_TAG_EXT};
            aprilTagConfig.capability = XR_SPATIAL_CAPABILITY_MARKER_TRACKING_APRIL_TAG_EXT;
            aprilTagConfig.enabledComponentCount = static_cast<uint32_t>(enabledComponents.size());
            aprilTagConfig.enabledComponents = enabledComponents.data();
            aprilTagConfig.aprilDict = XR_SPATIAL_MARKER_APRIL_TAG_DICT_36H11_EXT;

            SpatialMarkerTrackingTestRunner(*reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&aprilTagConfig))
                .RunTest(XR_EXT_SPATIAL_MARKER_TRACKING_EXTENSION_NAME,
                         "Discovered April Tags should be rendered yellow. "
                         "This test only renders the QR code at initial "
                         "location of discovery, does not update the pose per frame. "
                         "Press the select button on either controller to pass the test.");
        }

    }  // namespace
}  // namespace Conformance
