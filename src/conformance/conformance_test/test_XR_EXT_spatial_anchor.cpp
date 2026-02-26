// Copyright (c) 2019-2026 The Khronos Group Inc.
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
#include <vector>

#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "report.h"
#include "spatial_conformance_utils.h"
#include "spatial_test_runner.h"
#include "utilities/colors.h"
#include "utilities/throw_helpers.h"

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

        const std::chrono::nanoseconds kWaitTimeout = 5s;
        const std::chrono::nanoseconds kWaitInterval = 16ms;

#define CORE_FUNCTION_POINTERS(_)                  \
    _(xrCreateSpatialContextAsyncEXT)              \
    _(xrCreateSpatialContextCompleteEXT)           \
    _(xrDestroySpatialContextEXT)                  \
    _(xrCreateSpatialDiscoverySnapshotAsyncEXT)    \
    _(xrCreateSpatialDiscoverySnapshotCompleteEXT) \
    _(xrDestroySpatialSnapshotEXT)                 \
    _(xrQuerySpatialComponentDataEXT)              \
    _(xrCreateSpatialEntityFromIdEXT)              \
    _(xrDestroySpatialEntityEXT)                   \
    _(xrCreateSpatialUpdateSnapshotEXT)            \
    _(xrPollFutureEXT)

#define ANCHOR_FUNCTION_POINTERS(_) _(xrCreateSpatialAnchorEXT)

#define ALL_FUNCTION_POINTERS(_) \
    CORE_FUNCTION_POINTERS(_)    \
    ANCHOR_FUNCTION_POINTERS(_)

#define DEFINE_FUNCTION_POINTER(name) static PFN_##name name;
#define LOAD_FUNCTION_POINTER(name) name = GetInstanceExtensionFunction<PFN_##name>(instance, #name);

        XrSpatialContextEXT createSpatialContextForAnchor(XrInstance instance, XrSession session)
        {
            auto xrCreateSpatialContextAsyncEXT =
                GetInstanceExtensionFunction<PFN_xrCreateSpatialContextAsyncEXT>(instance, "xrCreateSpatialContextAsyncEXT");
            auto xrCreateSpatialContextCompleteEXT =
                GetInstanceExtensionFunction<PFN_xrCreateSpatialContextCompleteEXT>(instance, "xrCreateSpatialContextCompleteEXT");
            auto xrPollFutureEXT = GetInstanceExtensionFunction<PFN_xrPollFutureEXT>(instance, "xrPollFutureEXT");

            std::array<XrSpatialComponentTypeEXT, 1> enabledComponents = {{
                XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT,
            }};
            XrSpatialCapabilityConfigurationAnchorEXT anchorConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ANCHOR_EXT};
            anchorConfig.capability = XR_SPATIAL_CAPABILITY_ANCHOR_EXT;
            anchorConfig.enabledComponentCount = static_cast<uint32_t>(enabledComponents.size());
            anchorConfig.enabledComponents = enabledComponents.data();

            std::array<XrSpatialCapabilityConfigurationBaseHeaderEXT*, 1> capabilityConfigs = {{
                reinterpret_cast<XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&anchorConfig),
            }};

            XrSpatialContextCreateInfoEXT contextCreateInfo{XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT};
            contextCreateInfo.capabilityConfigCount = static_cast<uint32_t>(capabilityConfigs.size());
            contextCreateInfo.capabilityConfigs = capabilityConfigs.data();

            XrFutureEXT future{};
            XRC_CHECK_THROW_XRCMD(xrCreateSpatialContextAsyncEXT(session, &contextCreateInfo, &future));
            REQUIRE(WaitUntilPredicateWithTimeout(
                [&]() {
                    XrFuturePollInfoEXT pollInfo{XR_TYPE_FUTURE_POLL_INFO_EXT};
                    pollInfo.future = future;
                    XrFuturePollResultEXT pollResult{XR_TYPE_FUTURE_POLL_RESULT_EXT};

                    XrResult result = xrPollFutureEXT(instance, &pollInfo, &pollResult);
                    return (result == XR_SUCCESS) && pollResult.state == XR_FUTURE_STATE_READY_EXT;
                },
                kWaitTimeout, kWaitInterval));

            XrCreateSpatialContextCompletionEXT completion{XR_TYPE_CREATE_SPATIAL_CONTEXT_COMPLETION_EXT};
            XRC_CHECK_THROW_XRCMD(xrCreateSpatialContextCompleteEXT(session, future, &completion));
            XRC_CHECK_THROW_XRCMD(completion.futureResult);

            return completion.spatialContext;
        }

        void RequireAnchorEntityIdInSnapshot(XrInstance instance, XrSpatialSnapshotEXT snapshot, XrSpatialEntityIdEXT lookupEntityId)
        {
            auto xrQuerySpatialComponentDataEXT =
                GetInstanceExtensionFunction<PFN_xrQuerySpatialComponentDataEXT>(instance, "xrQuerySpatialComponentDataEXT");

            std::array<XrSpatialComponentTypeEXT, 1> enabledComponents = {{
                XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT,
            }};

            XrSpatialComponentDataQueryConditionEXT queryCond{XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT};
            queryCond.componentTypeCount = static_cast<uint32_t>(enabledComponents.size());
            queryCond.componentTypes = enabledComponents.data();

            XrSpatialComponentDataQueryResultEXT queryResult{XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT};

            REQUIRE(XR_SUCCESS == xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult));
            REQUIRE(queryResult.entityIdCountOutput > 0);

            std::vector<XrSpatialEntityIdEXT> entityIds(queryResult.entityIdCountOutput);
            std::vector<XrSpatialEntityTrackingStateEXT> entityStates(queryResult.entityIdCountOutput);
            queryResult.entityIdCapacityInput = static_cast<uint32_t>(entityIds.size());
            queryResult.entityIds = entityIds.data();
            queryResult.entityStateCapacityInput = static_cast<uint32_t>(entityStates.size());
            queryResult.entityStates = entityStates.data();

            REQUIRE(XR_SUCCESS == xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult));

            // Result must have the entity id we're looking for. Its tracking state and
            // rest of the data doesn't matter.
            REQUIRE_THAT(entityIds, VectorContains(lookupEntityId));
        }

        TEST_CASE("XR_EXT_spatial_anchor", "[XR_EXT_spatial_anchor][XR_EXT_spatial_entity][scenario][interactive][no_auto]")
        {
            XrSpatialCapabilityConfigurationAnchorEXT anchorConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ANCHOR_EXT};
            anchorConfig.capability = XR_SPATIAL_CAPABILITY_ANCHOR_EXT;

            TestSpatialConformance(XR_EXT_SPATIAL_ANCHOR_EXTENSION_NAME, XR_SPATIAL_CAPABILITY_ANCHOR_EXT,
                                   {XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT}, anchorConfig, XR_SPATIAL_COMPONENT_TYPE_BOUNDED_3D_EXT);

            SECTION("Create and destroy spatial anchor")
            {
                AutoBasicInstance instance(
                    {XR_EXT_FUTURE_EXTENSION_NAME, XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME, XR_EXT_SPATIAL_ANCHOR_EXTENSION_NAME},
                    AutoBasicInstance::createSystemId);

                if (!IsSpatialCapabilitySupported(instance, instance.systemId, XR_SPATIAL_CAPABILITY_ANCHOR_EXT)) {
                    SKIP(
                        "XR_SPATIAL_CAPABILITY_ANCHOR_EXT not enumerated as "
                        "supported.");
                }

                ALL_FUNCTION_POINTERS(DEFINE_FUNCTION_POINTER)
                ALL_FUNCTION_POINTERS(LOAD_FUNCTION_POINTER)
                (void)xrCreateSpatialContextAsyncEXT;
                (void)xrCreateSpatialContextCompleteEXT;
                (void)xrCreateSpatialEntityFromIdEXT;
                (void)xrQuerySpatialComponentDataEXT;

                AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                             AutoBasicSession::createSwapchains | AutoBasicSession::OptionFlags::createSpaces,
                                         instance);

                FrameIterator frameIterator(&session);
                frameIterator.RunToSessionState(XR_SESSION_STATE_VISIBLE);

                XrSpatialContextEXT spatialContext = createSpatialContextForAnchor(instance, session);
                XrSpatialEntityIdEXT anchorId{};
                XrSpatialEntityEXT anchor{};

                const auto isRefSpaceLocatable = [&]() {
                    frameIterator.SubmitFrame();

                    XrSpaceLocation refSpaceLocation = {XR_TYPE_SPACE_LOCATION};
                    const XrResult result = xrLocateSpace(session.spaceVector[0], session.spaceVector[1],
                                                          frameIterator.frameState.predictedDisplayTime, &refSpaceLocation);
                    return result == XR_SUCCESS && ((refSpaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0) &&
                           ((refSpaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0);
                };

                if (WaitUntilPredicateWithTimeout(isRefSpaceLocatable, kWaitTimeout, kWaitInterval)) {
                    const XrPosef anchorCreatePose = {{0, 0, 0, 1}, {0, 0, -0.5f}};
                    XrSpatialAnchorCreateInfoEXT createInfo = {XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_EXT};
                    createInfo.baseSpace = session.spaceVector[0];
                    createInfo.time = frameIterator.frameState.predictedDisplayTime;
                    createInfo.pose = anchorCreatePose;

                    REQUIRE(XR_SUCCESS == xrCreateSpatialAnchorEXT(spatialContext, &createInfo, &anchorId, &anchor));

                    SECTION("Anchor must be in update snapshot")
                    {
                        XrSpatialUpdateSnapshotCreateInfoEXT updateSnapshotCreateInfo{XR_TYPE_SPATIAL_UPDATE_SNAPSHOT_CREATE_INFO_EXT};
                        updateSnapshotCreateInfo.entityCount = 1;
                        updateSnapshotCreateInfo.entities = &anchor;
                        updateSnapshotCreateInfo.componentTypeCount = 0;
                        updateSnapshotCreateInfo.componentTypes = nullptr;
                        updateSnapshotCreateInfo.baseSpace = session.spaceVector[0];
                        updateSnapshotCreateInfo.time = frameIterator.frameState.predictedDisplayTime;

                        XrSpatialSnapshotEXT snapshot;
                        XRC_CHECK_THROW_XRCMD(xrCreateSpatialUpdateSnapshotEXT(spatialContext, &updateSnapshotCreateInfo, &snapshot));

                        RequireAnchorEntityIdInSnapshot(instance, snapshot, anchorId);

                        XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(snapshot));
                    }

                    SECTION("Anchor must be in discovery snapshot")
                    {
                        XrSpatialDiscoverySnapshotCreateInfoEXT discoverySnapshotCreateInfo{
                            XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT};

                        XrFutureEXT future;
                        REQUIRE(XR_SUCCESS ==
                                xrCreateSpatialDiscoverySnapshotAsyncEXT(spatialContext, &discoverySnapshotCreateInfo, &future));
                        REQUIRE(WaitUntilPredicateWithTimeout(
                            [&]() {
                                frameIterator.SubmitFrame();

                                XrFuturePollInfoEXT pollInfo{XR_TYPE_FUTURE_POLL_INFO_EXT};
                                pollInfo.future = future;

                                XrFuturePollResultEXT pollResult{XR_TYPE_FUTURE_POLL_RESULT_EXT};
                                XrResult result = xrPollFutureEXT(instance, &pollInfo, &pollResult);
                                return (result == XR_SUCCESS) && pollResult.state == XR_FUTURE_STATE_READY_EXT;
                            },
                            kWaitTimeout, kWaitInterval));

                        XrCreateSpatialDiscoverySnapshotCompletionInfoEXT createSnapshotCompletionInfo{
                            XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_INFO_EXT};
                        createSnapshotCompletionInfo.baseSpace = session.spaceVector[0];
                        createSnapshotCompletionInfo.time = frameIterator.frameState.predictedDisplayTime;
                        createSnapshotCompletionInfo.future = future;

                        XrCreateSpatialDiscoverySnapshotCompletionEXT completion{XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_EXT};
                        REQUIRE(XR_SUCCESS ==
                                xrCreateSpatialDiscoverySnapshotCompleteEXT(spatialContext, &createSnapshotCompletionInfo, &completion));
                        REQUIRE(XR_SUCCESS == completion.futureResult);

                        RequireAnchorEntityIdInSnapshot(instance, completion.snapshot, anchorId);

                        XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(completion.snapshot));
                    }

                    XRC_CHECK_THROW_XRCMD(xrDestroySpatialEntityEXT(anchor));
                }
                else {
                    FAIL(
                        "Could not locate ref space 0 against 1, never got to creating the "
                        "anchor.");
                }

                REQUIRE(XR_SUCCESS == xrDestroySpatialContextEXT(spatialContext));
            }
        }

        struct SpatialAnchorTestRunner : public SpatialTestRunner
        {
            explicit SpatialAnchorTestRunner(const std::vector<XrSpatialComponentTypeEXT>& enabledComponents)
                : mEnabledComponents(enabledComponents), mAnchorLocations(1, anchorCreatePose)
            {
                mAnchorConfig = {XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ANCHOR_EXT};
                mAnchorConfig.capability = XR_SPATIAL_CAPABILITY_ANCHOR_EXT;
                mAnchorConfig.enabledComponentCount = static_cast<uint32_t>(mEnabledComponents.size());
                mAnchorConfig.enabledComponents = mEnabledComponents.data();

                mAnchorList = {XR_TYPE_SPATIAL_COMPONENT_ANCHOR_LIST_EXT};
            }

            std::vector<const char*> getRequiredExtensions() override
            {
                return {
                    XR_EXT_FUTURE_EXTENSION_NAME,
                    XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME,
                    XR_EXT_SPATIAL_ANCHOR_EXTENSION_NAME,
                };
            }

            void onCreateSpatialContext(std::vector<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>& capabilityConfigs) override
            {
                capabilityConfigs.push_back(reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&mAnchorConfig));
            }

            void postCreateSpatialContextCompletion(const XrFrameState& frameState) override
            {
                xrCreateSpatialAnchorEXT = GetInstanceExtensionFunction<PFN_xrCreateSpatialAnchorEXT>(instance, "xrCreateSpatialAnchorEXT");
                xrCreateSpatialUpdateSnapshotEXT =
                    GetInstanceExtensionFunction<PFN_xrCreateSpatialUpdateSnapshotEXT>(instance, "xrCreateSpatialUpdateSnapshotEXT");
                xrDestroySpatialSnapshotEXT =
                    GetInstanceExtensionFunction<PFN_xrDestroySpatialSnapshotEXT>(instance, "xrDestroySpatialSnapshotEXT");

                XrSpatialAnchorCreateInfoEXT anchorCreateInfo = {XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_EXT};
                anchorCreateInfo.baseSpace = localSpace;
                anchorCreateInfo.time = frameState.predictedDisplayTime;
                anchorCreateInfo.pose = anchorCreatePose;

                XRC_CHECK_THROW_XRCMD(xrCreateSpatialAnchorEXT(spatialContext, &anchorCreateInfo, &mAnchorId, &mAnchor));

                state = DiscoveryState::StopDiscovery;
            }

            std::vector<XrSpatialComponentTypeEXT> getQueryComponents() override
            {
                return mEnabledComponents;
            }

            std::vector<XrBaseOutStructure*> getComponentDataListStructPtrs(uint32_t entityCount) override
            {
                std::vector<XrBaseOutStructure*> listStructPtrs;

                if (VectorContains(XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT).match(mEnabledComponents)) {
                    mAnchorLocations.resize(entityCount);
                    mAnchorList.locationCount = static_cast<uint32_t>(mAnchorLocations.size());
                    mAnchorList.locations = mAnchorLocations.data();
                    listStructPtrs.push_back(reinterpret_cast<XrBaseOutStructure*>(&mAnchorList));
                }

                return listStructPtrs;
            }

            void render() override
            {
                renderedCubes.clear();
                renderedMeshes.clear();

                for (uint32_t i = 0; i < mAnchorLocations.size(); ++i) {
                    renderedCubes.push_back(Cube{
                        mAnchorLocations[i],
                        {0.25f, 0.25f, 0.25f},
                        getColor(entityStates[i]),
                    });
                }
            }

            void onUpdate(const XrFrameState& frameState) override
            {
                if (mAnchor != XR_NULL_HANDLE) {
                    XrSpatialSnapshotEXT updateSnapshot;
                    XrSpatialUpdateSnapshotCreateInfoEXT updateSnapshotCreateInfo{XR_TYPE_SPATIAL_UPDATE_SNAPSHOT_CREATE_INFO_EXT};
                    updateSnapshotCreateInfo.entityCount = 1;
                    updateSnapshotCreateInfo.entities = &mAnchor;
                    updateSnapshotCreateInfo.componentTypeCount = 0;
                    updateSnapshotCreateInfo.componentTypes = nullptr;
                    updateSnapshotCreateInfo.baseSpace = localSpace;
                    updateSnapshotCreateInfo.time = frameState.predictedDisplayTime;

                    XRC_CHECK_THROW_XRCMD(xrCreateSpatialUpdateSnapshotEXT(spatialContext, &updateSnapshotCreateInfo, &updateSnapshot));

                    querySnapshot(updateSnapshot);
                    render();
                    XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(updateSnapshot));
                }
            }

            static XrColor4f getColor(XrSpatialEntityTrackingStateEXT trackingState)
            {
                switch (trackingState) {
                case XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT:
                    return Colors::Green;
                case XR_SPATIAL_ENTITY_TRACKING_STATE_PAUSED_EXT:
                    return Colors::Yellow;
                case XR_SPATIAL_ENTITY_TRACKING_STATE_STOPPED_EXT:
                    return Colors::Red;
                default:
                    return Colors::Red;
                }
            }

            const XrPosef anchorCreatePose = {{0, 0, 0, 1}, {0, 0, -0.5f}};

            XrSpatialCapabilityConfigurationAnchorEXT mAnchorConfig{};
            const std::vector<XrSpatialComponentTypeEXT> mEnabledComponents;

            XrSpatialComponentAnchorListEXT mAnchorList{};
            std::vector<XrPosef> mAnchorLocations;

            XrSpatialEntityIdEXT mAnchorId = XR_NULL_SPATIAL_ENTITY_ID_EXT;
            XrSpatialEntityEXT mAnchor = XR_NULL_HANDLE;

            PFN_xrCreateSpatialAnchorEXT xrCreateSpatialAnchorEXT{};
            PFN_xrCreateSpatialUpdateSnapshotEXT xrCreateSpatialUpdateSnapshotEXT{};
            PFN_xrDestroySpatialSnapshotEXT xrDestroySpatialSnapshotEXT{};
        };

        TEST_CASE("XR_EXT_spatial_anchor-interactive", "[XR_EXT_spatial_anchor][XR_EXT_spatial_entity][scenario][interactive][no_auto]")
        {
            SpatialAnchorTestRunner({XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT})
                .RunTest(XR_EXT_SPATIAL_ANCHOR_EXTENSION_NAME,
                         "A spatial anchor is created in front of the user & a cube is "
                         "rendered "
                         "at its pose. The cube color represents the anchor "
                         "tracking state: Green: TRACKING, Yellow: PAUSED, Red: STOPPED.\n\n"
                         "If the system supports recentering, look away from the cube "
                         "& perform the recenter. This panel should move but the anchored "
                         "cube must continue to render in the same "
                         "location as before.\n\n"
                         "After recentering, press select to complete the validation.",
                         {{0, 0, 0, 1}, {-1.f, 0, -1.0f}});
        }

    }  // namespace
}  // namespace Conformance
