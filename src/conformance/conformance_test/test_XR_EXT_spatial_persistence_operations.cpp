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
#include <chrono>
#include <functional>
#include <thread>
#include <unordered_set>

#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "report.h"
#include "spatial_conformance_utils.h"
#include "utilities/stringification.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"
#include "utilities/uuid_utils.h"

using namespace Conformance;
using Catch::Matchers::VectorContains;

namespace Conformance
{
    namespace
    {

        using namespace std::literals::chrono_literals;
        static const std::chrono::nanoseconds kTimeout = 5s;
        static const std::chrono::nanoseconds kInterval = 16ms;

#define STRINGIFY(x) #x
#define VALIDATE_FUNCTION_NOT_SUPPORTED(name, ext) ValidateInstanceExtensionFunctionNotSupported(instance, "xr" STRINGIFY(name));
#define LOAD_FUNCTION_POINTER(name, ext)                                                                \
    PFN_xr##name xr##name = GetInstanceExtensionFunction<PFN_xr##name>(instance, "xr" STRINGIFY(name)); \
    (void)xr##name;

        static void RequireWaitUntilFutureComplete(XrInstance instance, XrFutureEXT future, std::chrono::nanoseconds timeout = kTimeout,
                                                   std::chrono::nanoseconds interval = kInterval)
        {
            XR_LIST_FUNCTIONS_XR_EXT_future(LOAD_FUNCTION_POINTER);
            REQUIRE(WaitUntilPredicateWithTimeout(
                [&]() {
                    XrFuturePollInfoEXT pollInfo{XR_TYPE_FUTURE_POLL_INFO_EXT};
                    pollInfo.future = future;

                    XrFuturePollResultEXT pollResult{XR_TYPE_FUTURE_POLL_RESULT_EXT};
                    XrResult result = xrPollFutureEXT(instance, &pollInfo, &pollResult);
                    return (result == XR_SUCCESS) && pollResult.state == XR_FUTURE_STATE_READY_EXT;
                },
                timeout, interval));
        }

        static void RequireWaitUntilRefSpaceLocatable(AutoBasicSession& session, FrameIterator& frameIterator)
        {
            const auto isRefSpaceLocatable = [&]() {
                // Needed for predictedDisplayTime
                frameIterator.SubmitFrame();

                XrSpaceLocation refSpaceLocation = {XR_TYPE_SPACE_LOCATION};
                const XrResult result = xrLocateSpace(session.spaceVector[0], session.spaceVector[1],
                                                      frameIterator.frameState.predictedDisplayTime, &refSpaceLocation);
                return result == XR_SUCCESS && ((refSpaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0) &&
                       ((refSpaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0);
            };

            REQUIRE(WaitUntilPredicateWithTimeout(isRefSpaceLocatable, kTimeout, kInterval));
        }

        static XrSpatialSnapshotEXT createDiscoverySnapshot(XrInstance instance, AutoBasicSession& session, FrameIterator& frameIterator,
                                                            XrSpatialContextEXT spatialContext,
                                                            const XrSpatialDiscoverySnapshotCreateInfoEXT& createInfo)
        {
            PFN_xrCreateSpatialDiscoverySnapshotAsyncEXT xrCreateSpatialDiscoverySnapshotAsyncEXT =
                GetInstanceExtensionFunction<PFN_xrCreateSpatialDiscoverySnapshotAsyncEXT>(instance,
                                                                                           "xrCreateSpatialDiscoverySnapshotAsyncEXT");
            PFN_xrCreateSpatialDiscoverySnapshotCompleteEXT xrCreateSpatialDiscoverySnapshotCompleteEXT =
                GetInstanceExtensionFunction<PFN_xrCreateSpatialDiscoverySnapshotCompleteEXT>(
                    instance, "xrCreateSpatialDiscoverySnapshotCompleteEXT");

            XrFutureEXT future{};
            XRC_CHECK_THROW_XRCMD(xrCreateSpatialDiscoverySnapshotAsyncEXT(spatialContext, &createInfo, &future));
            RequireWaitUntilFutureComplete(instance, future);

            frameIterator.SubmitFrame();

            XrCreateSpatialDiscoverySnapshotCompletionInfoEXT createSnapshotCompletionInfo{
                XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_INFO_EXT};
            createSnapshotCompletionInfo.baseSpace = session.spaceVector[0];
            createSnapshotCompletionInfo.time = frameIterator.frameState.predictedDisplayTime;
            createSnapshotCompletionInfo.future = future;
            XrCreateSpatialDiscoverySnapshotCompletionEXT discoverySnapshotCompletion{
                XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_EXT};
            REQUIRE(XR_SUCCESS == xrCreateSpatialDiscoverySnapshotCompleteEXT(spatialContext, &createSnapshotCompletionInfo,
                                                                              &discoverySnapshotCompletion));
            REQUIRE(XR_SUCCESS == discoverySnapshotCompletion.futureResult);
            return discoverySnapshotCompletion.snapshot;
        }

        static void querySnapshot(XrInstance instance, XrSpatialSnapshotEXT snapshot,
                                  const std::vector<XrSpatialComponentTypeEXT>& enabledComponents,
                                  std::vector<XrSpatialEntityIdEXT>& entityIds, std::vector<XrSpatialEntityTrackingStateEXT>& entityStates,
                                  std::vector<XrSpatialPersistenceDataEXT>& persistData)
        {
            PFN_xrQuerySpatialComponentDataEXT xrQuerySpatialComponentDataEXT =
                GetInstanceExtensionFunction<PFN_xrQuerySpatialComponentDataEXT>(instance, "xrQuerySpatialComponentDataEXT");
            entityIds.clear();
            entityStates.clear();
            persistData.clear();

            XrSpatialComponentDataQueryConditionEXT queryCond{XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT};
            queryCond.componentTypeCount = static_cast<uint32_t>(enabledComponents.size());
            queryCond.componentTypes = enabledComponents.data();

            XrSpatialComponentDataQueryResultEXT queryResult{XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT};

            REQUIRE(XR_SUCCESS == xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult));

            entityIds.resize(queryResult.entityIdCountOutput);
            entityStates.resize(queryResult.entityIdCountOutput);
            queryResult.entityIdCapacityInput = static_cast<uint32_t>(entityIds.size());
            queryResult.entityIds = entityIds.data();
            queryResult.entityStateCapacityInput = static_cast<uint32_t>(entityStates.size());
            queryResult.entityStates = entityStates.data();

            XrSpatialComponentPersistenceListEXT persistenceList{XR_TYPE_SPATIAL_COMPONENT_PERSISTENCE_LIST_EXT};
            const bool queryPersistenceComponent = VectorContains(XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT).match(enabledComponents);
            if (queryPersistenceComponent) {
                persistData.resize(queryResult.entityIdCountOutput);
                persistenceList.persistDataCount = queryResult.entityIdCountOutput;
                persistenceList.persistData = persistData.data();

                queryResult.next = &persistenceList;
            }

            if (queryResult.entityIdCountOutput > 0) {
                XRC_CHECK_THROW_XRCMD(xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult));
            }
        }

        TEST_CASE(
            "XR_EXT_spatial_persistence_operations",
            "[XR_EXT_spatial_persistence_operations][XR_EXT_spatial_persistence][XR_EXT_spatial_entity][scenario][interactive][no_auto]")
        {
            const std::vector<const char*> requiredExtensions = {
                XR_EXT_FUTURE_EXTENSION_NAME, XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME, XR_EXT_SPATIAL_ANCHOR_EXTENSION_NAME,
                XR_EXT_SPATIAL_PERSISTENCE_EXTENSION_NAME, XR_EXT_SPATIAL_PERSISTENCE_OPERATIONS_EXTENSION_NAME};

            GlobalData& globalData = GetGlobalData();
            for (const char* extension : requiredExtensions) {
                if (!globalData.IsInstanceExtensionSupported(extension)) {
                    SKIP(extension << " not supported");
                }
            }

            SECTION("Extension not enabled")
            {
                if (!globalData.IsInstanceExtensionEnabled(XR_EXT_SPATIAL_PERSISTENCE_OPERATIONS_EXTENSION_NAME)) {
                    AutoBasicInstance instance;

                    XR_LIST_FUNCTIONS_XR_EXT_spatial_persistence_operations(VALIDATE_FUNCTION_NOT_SUPPORTED);
                }
                else {
                    WARN(XR_EXT_SPATIAL_PERSISTENCE_OPERATIONS_EXTENSION_NAME
                         << " force-enabled, cannot test behavior when extension is enabled.");
                }
            }

            SECTION("Extension enabled")
            {
                // Create and destroy spatial persistence context :
                // XR_SPATIAL_PERSISTENCE_SCOPE_LOCAL_ANCHORS_EXT
                {
                    AutoBasicInstance instance(requiredExtensions, AutoBasicInstance::createSystemId);

                    XR_LIST_FUNCTIONS_XR_EXT_spatial_persistence(LOAD_FUNCTION_POINTER);
                    XR_LIST_FUNCTIONS_XR_EXT_spatial_persistence_operations(LOAD_FUNCTION_POINTER);
                    XR_LIST_FUNCTIONS_XR_EXT_spatial_entity(LOAD_FUNCTION_POINTER);
                    XR_LIST_FUNCTIONS_XR_EXT_spatial_anchor(LOAD_FUNCTION_POINTER);

                    uint32_t persistenceScopeCountOutput = 0;
                    XRC_CHECK_THROW_XRCMD(
                        xrEnumerateSpatialPersistenceScopesEXT(instance, instance.systemId, 0, &persistenceScopeCountOutput, nullptr));
                    if (persistenceScopeCountOutput == 0) {
                        SKIP(XR_EXT_SPATIAL_PERSISTENCE_OPERATIONS_EXTENSION_NAME << ": No persistence scope supported. Skipping tests.");
                    }

                    std::vector<XrSpatialPersistenceScopeEXT> persistenceScopes(persistenceScopeCountOutput);
                    XRC_CHECK_THROW_XRCMD(xrEnumerateSpatialPersistenceScopesEXT(instance, instance.systemId, persistenceScopeCountOutput,
                                                                                 &persistenceScopeCountOutput, persistenceScopes.data()));

                    if (!VectorContains(XR_SPATIAL_PERSISTENCE_SCOPE_LOCAL_ANCHORS_EXT).match(persistenceScopes)) {
                        SKIP(
                            "XR_SPATIAL_PERSISTENCE_SCOPE_LOCAL_ANCHORS_EXT not supported. "
                            "Skipping tests.");
                    }

                    AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                                 AutoBasicSession::createSwapchains | AutoBasicSession::OptionFlags::createSpaces,
                                             instance);
                    FrameIterator frameIterator(&session);
                    frameIterator.RunToSessionState(XR_SESSION_STATE_VISIBLE);

                    XrSpatialPersistenceContextCreateInfoEXT createInfo{XR_TYPE_SPATIAL_PERSISTENCE_CONTEXT_CREATE_INFO_EXT};
                    createInfo.scope = XR_SPATIAL_PERSISTENCE_SCOPE_LOCAL_ANCHORS_EXT;

                    XrFutureEXT future{};
                    REQUIRE(XR_SUCCESS == xrCreateSpatialPersistenceContextAsyncEXT(session, &createInfo, &future));
                    RequireWaitUntilFutureComplete(instance, future);

                    XrCreateSpatialPersistenceContextCompletionEXT persistenceContextCreateCompletion{
                        XR_TYPE_CREATE_SPATIAL_PERSISTENCE_CONTEXT_COMPLETION_EXT};
                    REQUIRE(XR_SUCCESS ==
                            xrCreateSpatialPersistenceContextCompleteEXT(session, future, &persistenceContextCreateCompletion));
                    REQUIRE(XR_SUCCESS == persistenceContextCreateCompletion.futureResult);
                    if (persistenceContextCreateCompletion.createResult != XR_SPATIAL_PERSISTENCE_CONTEXT_RESULT_SUCCESS_EXT) {
                        SKIP("Skipping test, spatial persistence context creation failed: "
                             << XrEnumStr(persistenceContextCreateCompletion.createResult));
                    }

                    REQUIRE_FALSE(XR_NULL_HANDLE == persistenceContextCreateCompletion.persistenceContext);

                    XrSpatialPersistenceContextEXT persistenceContext = persistenceContextCreateCompletion.persistenceContext;

                    // Anchor capability & component support
                    {
                        // If a runtime enumerates
                        // XR_SPATIAL_PERSISTENCE_SCOPE_LOCAL_ANCHORS_EXT in
                        // xrEnumerateSpatialPersistenceScopesEXT, the runtime must also
                        // enumerate XR_SPATIAL_CAPABILITY_ANCHOR_EXT in
                        // xrEnumerateSpatialCapabilitiesEXT and
                        // XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT in
                        // xrEnumerateSpatialCapabilityComponentTypesEXT for
                        // XR_SPATIAL_CAPABILITY_ANCHOR_EXT.
                        REQUIRE(IsSpatialCapabilitySupported(instance, instance.systemId, XR_SPATIAL_CAPABILITY_ANCHOR_EXT));
                        REQUIRE(IsSpatialComponentSupported(instance, instance.systemId, XR_SPATIAL_CAPABILITY_ANCHOR_EXT,
                                                            XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT));
                    }

                    std::array<XrSpatialComponentTypeEXT, 2> enabledComponents = {{
                        XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT,
                        XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT,
                    }};
                    XrSpatialCapabilityConfigurationAnchorEXT anchorConfig{XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ANCHOR_EXT};
                    anchorConfig.capability = XR_SPATIAL_CAPABILITY_ANCHOR_EXT;
                    anchorConfig.enabledComponentCount = static_cast<uint32_t>(enabledComponents.size());
                    anchorConfig.enabledComponents = enabledComponents.data();

                    std::array<XrSpatialCapabilityConfigurationBaseHeaderEXT*, 1> capabilityConfigs = {{
                        reinterpret_cast<XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&anchorConfig),
                    }};

                    XrSpatialContextCreateInfoEXT contextCreateInfo{XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT, nullptr,
                                                                    static_cast<uint32_t>(capabilityConfigs.size()),
                                                                    capabilityConfigs.data()};

                    // Persistence component without persistence context config is
                    // invalid
                    {
                        // If the application is including
                        // XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT in the enabled component
                        // list, it must also include XrSpatialContextPersistenceConfigEXT in
                        // the next chain of XrSpatialContextCreateInfoEXT otherwise the
                        // runtime must return
                        // XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT from
                        // xrCreateSpatialContextAsyncEXT.
                        REQUIRE(XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT ==
                                xrCreateSpatialContextAsyncEXT(session, &contextCreateInfo, &future));
                    }

                    // Create spatial context with persistence context
                    {
                        XrSpatialContextPersistenceConfigEXT persistenceConfig{XR_TYPE_SPATIAL_CONTEXT_PERSISTENCE_CONFIG_EXT};
                        persistenceConfig.persistenceContextCount = 1;
                        persistenceConfig.persistenceContexts = &persistenceContext;

                        contextCreateInfo.next = &persistenceConfig;
                        XRC_CHECK_THROW_XRCMD(xrCreateSpatialContextAsyncEXT(session, &contextCreateInfo, &future));

                        RequireWaitUntilFutureComplete(instance, future);

                        XrCreateSpatialContextCompletionEXT spatialContextCreateCompletion{XR_TYPE_CREATE_SPATIAL_CONTEXT_COMPLETION_EXT};
                        XRC_CHECK_THROW_XRCMD(xrCreateSpatialContextCompleteEXT(session, future, &spatialContextCreateCompletion));
                        XRC_CHECK_THROW_XRCMD(spatialContextCreateCompletion.futureResult);

                        XrSpatialContextEXT spatialContext = spatialContextCreateCompletion.spatialContext;

                        const XrPosef anchorCreatePose = {Quat::Identity, {0, 0, -0.5f}};
                        XrSpatialEntityIdEXT anchorId = XR_NULL_SPATIAL_ENTITY_ID_EXT;
                        XrSpatialEntityEXT anchor = XR_NULL_HANDLE;
                        XrUuid persistUuid;
                        const std::vector<XrSpatialEntityTrackingStateEXT> validTrackingStates{
                            XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT,
                            XR_SPATIAL_ENTITY_TRACKING_STATE_PAUSED_EXT,
                        };

                        std::vector<XrSpatialEntityIdEXT> entityIds;
                        std::vector<XrSpatialEntityTrackingStateEXT> entityStates;
                        std::vector<XrSpatialPersistenceDataEXT> persistData;

                        // Create anchor"
                        {
                            RequireWaitUntilRefSpaceLocatable(session, frameIterator);
                            XrSpatialAnchorCreateInfoEXT anchorCreateInfo = {XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_EXT};
                            anchorCreateInfo.baseSpace = session.spaceVector[0];
                            anchorCreateInfo.time = frameIterator.frameState.predictedDisplayTime;
                            anchorCreateInfo.pose = anchorCreatePose;

                            XRC_CHECK_THROW_XRCMD(xrCreateSpatialAnchorEXT(spatialContext, &anchorCreateInfo, &anchorId, &anchor));

                            // Newly created anchor must not have persist component
                            {
                                XrSpatialDiscoverySnapshotCreateInfoEXT discoverySnapshotCreateInfo{
                                    XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT,
                                };
                                XrSpatialSnapshotEXT discoverySnapshot =
                                    createDiscoverySnapshot(instance, session, frameIterator, spatialContext, discoverySnapshotCreateInfo);

                                querySnapshot(instance, discoverySnapshot, {XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT}, entityIds, entityStates,
                                              persistData);
                                // Newly created anchor (having anchor component) must be present in
                                // discovery snapshot
                                REQUIRE_THAT(entityIds, VectorContains(anchorId));
                                querySnapshot(instance, discoverySnapshot, {XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT}, entityIds,
                                              entityStates, persistData);
                                // Newly created anchor must not have persist component
                                REQUIRE(entityIds.end() == std::find(entityIds.begin(), entityIds.end(), anchorId));

                                XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(discoverySnapshot));
                            }

                            // Persisting invalid entity must fail
                            {
                                XrSpatialEntityPersistInfoEXT persistInfo{XR_TYPE_SPATIAL_ENTITY_PERSIST_INFO_EXT};
                                persistInfo.spatialContext = spatialContext;
                                persistInfo.spatialEntityId = XR_NULL_SPATIAL_ENTITY_ID_EXT;
                                REQUIRE(XR_ERROR_SPATIAL_ENTITY_ID_INVALID_EXT ==
                                        xrPersistSpatialEntityAsyncEXT(persistenceContext, &persistInfo, &future));

                                // ensure random entity is not null and not anchor id, [begin, end)
                                persistInfo.spatialEntityId = GetGlobalData().GetRandEngine().RandUint64(1, anchorId);
                                REQUIRE(XR_ERROR_SPATIAL_ENTITY_ID_INVALID_EXT ==
                                        xrPersistSpatialEntityAsyncEXT(persistenceContext, &persistInfo, &future));
                            }

                            const auto persistAnchorWithRetries = [&](uint32_t retries = 5) {
                                XrPersistSpatialEntityCompletionEXT persistCompletion{XR_TYPE_PERSIST_SPATIAL_ENTITY_COMPLETION_EXT};
                                uint32_t attempt = 0;
                                while ((attempt++) <= retries) {
                                    XrSpatialEntityPersistInfoEXT persistInfo{XR_TYPE_SPATIAL_ENTITY_PERSIST_INFO_EXT};
                                    persistInfo.spatialContext = spatialContext;
                                    persistInfo.spatialEntityId = anchorId;
                                    XRC_CHECK_THROW_XRCMD(xrPersistSpatialEntityAsyncEXT(persistenceContext, &persistInfo, &future));
                                    RequireWaitUntilFutureComplete(instance, future);

                                    XRC_CHECK_THROW_XRCMD(
                                        xrPersistSpatialEntityCompleteEXT(persistenceContext, future, &persistCompletion));
                                    REQUIRE(XR_SUCCESS == persistCompletion.futureResult);
                                    if (persistCompletion.persistResult != XR_SPATIAL_PERSISTENCE_CONTEXT_RESULT_SUCCESS_EXT) {
                                        ReportF("Persist attempt [%d/%d] failed with error %s.", attempt, retries + 1,
                                                XrEnumStr(persistCompletion.persistResult));
                                        std::this_thread::sleep_for(1s);
                                    }
                                    else {
                                        break;
                                    }
                                }

                                REQUIRE(XR_SPATIAL_PERSISTENCE_CONTEXT_RESULT_SUCCESS_EXT == persistCompletion.persistResult);
                                persistUuid = persistCompletion.persistUuid;
                            };

                            // Persist anchor
                            {
                                persistAnchorWithRetries();

                                // Persisted anchor must have persist component
                                {
                                    XrSpatialDiscoverySnapshotCreateInfoEXT discoverySnapshotCreateInfo{
                                        XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT};
                                    XrSpatialSnapshotEXT discoverySnapshot = createDiscoverySnapshot(
                                        instance, session, frameIterator, spatialContext, discoverySnapshotCreateInfo);

                                    querySnapshot(instance, discoverySnapshot, {XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT}, entityIds,
                                                  entityStates, persistData);

                                    const auto itr = std::find(entityIds.begin(), entityIds.end(), anchorId);
                                    REQUIRE(entityIds.end() != itr);

                                    const uint32_t anchorEntityIndex = static_cast<uint32_t>(itr - entityIds.begin());
                                    REQUIRE(persistUuid == persistData[anchorEntityIndex].persistUuid);
                                    REQUIRE(XR_SPATIAL_PERSISTENCE_STATE_LOADED_EXT == persistData[anchorEntityIndex].persistState);

                                    XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(discoverySnapshot));
                                }

                                // Persist UUID filter for discovery snapshot
                                {
                                    const std::array<XrUuid, 2> persistUuids = {{
                                        persistUuid,
                                        {{0}},
                                    }};
                                    XrSpatialDiscoveryPersistenceUuidFilterEXT persistFilter{
                                        XR_TYPE_SPATIAL_DISCOVERY_PERSISTENCE_UUID_FILTER_EXT};
                                    persistFilter.persistedUuidCount = static_cast<uint32_t>(persistUuids.size());
                                    persistFilter.persistedUuids = persistUuids.data();
                                    XrSpatialDiscoverySnapshotCreateInfoEXT discoverySnapshotCreateInfo{
                                        XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT};
                                    discoverySnapshotCreateInfo.next = &persistFilter;
                                    XrSpatialSnapshotEXT discoverySnapshot = createDiscoverySnapshot(
                                        instance, session, frameIterator, spatialContext, discoverySnapshotCreateInfo);

                                    querySnapshot(instance, discoverySnapshot, {XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT}, entityIds,
                                                  entityStates, persistData);

                                    // Snapshot must have exactly one entry for each uuid in the
                                    // filter
                                    REQUIRE(persistUuids.size() == entityIds.size());

                                    auto itr = std::find(entityIds.begin(), entityIds.end(), anchorId);
                                    REQUIRE(entityIds.end() != itr);

                                    const uint32_t anchorEntityIndex = static_cast<uint32_t>(itr - entityIds.begin());
                                    REQUIRE(persistUuid == persistData[anchorEntityIndex].persistUuid);
                                    REQUIRE(XR_SPATIAL_PERSISTENCE_STATE_LOADED_EXT == persistData[anchorEntityIndex].persistState);

                                    // Entity id of invalid uuid must be null
                                    itr = std::find(entityIds.begin(), entityIds.end(), XR_NULL_SPATIAL_ENTITY_ID_EXT);
                                    REQUIRE(entityIds.end() != itr);

                                    const uint32_t nullEntityIndex = static_cast<uint32_t>(itr - entityIds.begin());
                                    // Tracking state of invalid uuid must be STOPPED
                                    REQUIRE(XR_SPATIAL_ENTITY_TRACKING_STATE_STOPPED_EXT == entityStates[nullEntityIndex]);
                                    REQUIRE(persistUuids[1] == persistData[nullEntityIndex].persistUuid);
                                    // Persist state of invalid uuid must be NOT FOUND
                                    REQUIRE(XR_SPATIAL_PERSISTENCE_STATE_NOT_FOUND_EXT == persistData[nullEntityIndex].persistState);

                                    XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(discoverySnapshot));
                                }

                                // Unpersist anchor
                                {
                                    XrSpatialEntityUnpersistInfoEXT unpersistInfo{XR_TYPE_SPATIAL_ENTITY_UNPERSIST_INFO_EXT};
                                    unpersistInfo.persistUuid = persistUuid;
                                    XRC_CHECK_THROW_XRCMD(xrUnpersistSpatialEntityAsyncEXT(persistenceContext, &unpersistInfo, &future));

                                    RequireWaitUntilFutureComplete(instance, future);

                                    XrUnpersistSpatialEntityCompletionEXT unpersistCompletion{
                                        XR_TYPE_UNPERSIST_SPATIAL_ENTITY_COMPLETION_EXT,
                                    };
                                    XRC_CHECK_THROW_XRCMD(
                                        xrUnpersistSpatialEntityCompleteEXT(persistenceContext, future, &unpersistCompletion));
                                    REQUIRE(XR_SUCCESS == unpersistCompletion.futureResult);
                                    REQUIRE(XR_SPATIAL_PERSISTENCE_CONTEXT_RESULT_SUCCESS_EXT == unpersistCompletion.unpersistResult);

                                    // Unpersisted anchor must not have persist component
                                    {
                                        XrSpatialDiscoverySnapshotCreateInfoEXT discoverySnapshotCreateInfo{
                                            XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT};
                                        XrSpatialSnapshotEXT discoverySnapshot = createDiscoverySnapshot(
                                            instance, session, frameIterator, spatialContext, discoverySnapshotCreateInfo);

                                        querySnapshot(instance, discoverySnapshot, {XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT}, entityIds,
                                                      entityStates, persistData);
                                        REQUIRE_THAT(entityIds, VectorContains(anchorId));
                                        querySnapshot(instance, discoverySnapshot, {XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT}, entityIds,
                                                      entityStates, persistData);
                                        REQUIRE(entityIds.end() == std::find(entityIds.begin(), entityIds.end(), anchorId));

                                        XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(discoverySnapshot));
                                    }

                                    // Unpersisted uuid state must be "not found"
                                    {
                                        const std::array<XrUuid, 1> persistUuids = {{
                                            persistUuid,
                                        }};
                                        XrSpatialDiscoveryPersistenceUuidFilterEXT persistFilter{
                                            XR_TYPE_SPATIAL_DISCOVERY_PERSISTENCE_UUID_FILTER_EXT};
                                        persistFilter.persistedUuidCount = static_cast<uint32_t>(persistUuids.size());
                                        persistFilter.persistedUuids = persistUuids.data();
                                        XrSpatialDiscoverySnapshotCreateInfoEXT discoverySnapshotCreateInfo{
                                            XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT};
                                        discoverySnapshotCreateInfo.next = &persistFilter;
                                        XrSpatialSnapshotEXT discoverySnapshot = createDiscoverySnapshot(
                                            instance, session, frameIterator, spatialContext, discoverySnapshotCreateInfo);

                                        querySnapshot(instance, discoverySnapshot, {XR_SPATIAL_COMPONENT_TYPE_PERSISTENCE_EXT}, entityIds,
                                                      entityStates, persistData);

                                        REQUIRE(persistUuids.size() == entityIds.size());

                                        auto itr = std::find(entityIds.begin(), entityIds.end(), XR_NULL_SPATIAL_ENTITY_ID_EXT);
                                        REQUIRE(entityIds.end() != itr);

                                        const uint32_t nullEntityIndex = static_cast<uint32_t>(itr - entityIds.begin());
                                        REQUIRE(XR_SPATIAL_ENTITY_TRACKING_STATE_STOPPED_EXT == entityStates[nullEntityIndex]);
                                        REQUIRE(persistUuids[0] == persistData[nullEntityIndex].persistUuid);
                                        REQUIRE(XR_SPATIAL_PERSISTENCE_STATE_NOT_FOUND_EXT == persistData[nullEntityIndex].persistState);

                                        XRC_CHECK_THROW_XRCMD(xrDestroySpatialSnapshotEXT(discoverySnapshot));
                                    }
                                }
                            }

                            XRC_CHECK_THROW_XRCMD(xrDestroySpatialEntityEXT(anchor));
                        }

                        XRC_CHECK_THROW_XRCMD(xrDestroySpatialContextEXT(spatialContext));
                    }

                    REQUIRE(XR_SUCCESS == xrDestroySpatialPersistenceContextEXT(persistenceContext));
                }
            }
        }

    }  // namespace
}  // namespace Conformance
