// Copyright (c) 2019-2025 The Khronos Group Inc.
// Copyright (c) 2019 Collabora, Ltd.
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

#pragma once

#include <openxr/openxr.h>

#include <array>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <chrono>

#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/stringification.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"

namespace Conformance
{

    /// Checks if the given spatial capability is supported.
    bool IsSpatialCapabilitySupported(XrInstance instance, XrSystemId systemId, XrSpatialCapabilityEXT capability);

    /// Checks if the given spatial component is supported for the given capability.
    bool IsSpatialComponentSupported(XrInstance instance, XrSystemId systemId, XrSpatialCapabilityEXT capability,
                                     XrSpatialComponentTypeEXT component);

    /// Checks if the given spatial capability feature is supported for the given capability.
    bool IsSpatialCapabilityFeatureSupported(XrInstance instance, XrSystemId systemId, XrSpatialCapabilityEXT capability,
                                             XrSpatialCapabilityFeatureEXT feature);

    /// Runs spatial context and discovery snapshot tests for the given capability,
    /// its expected list of guaranteed components, it configuration struct and
    /// a component type that is not in the gguaranteedComponents list to check for
    /// the XR_ERROR_SPATIAL_COMPONENT_NOT_ENABLED_EXT case.
    template <typename T>
    void TestSpatialConformance(const char* capabilityExtensionName, XrSpatialCapabilityEXT capability,
                                const std::vector<XrSpatialComponentTypeEXT>& guaranteedComponents, const T& capabilityConfig,
                                XrSpatialComponentTypeEXT nonEnabledComponent)
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(capabilityExtensionName)) {
            SKIP(capabilityExtensionName << " not supported");
        }

        SECTION("Extension not enabled")
        {
            if (!globalData.IsInstanceExtensionEnabled(capabilityExtensionName)) {
                AutoBasicInstance instance({XR_EXT_FUTURE_EXTENSION_NAME, XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME},
                                           AutoBasicInstance::createSystemId);

                // Capability must not be enumerated if its extension is not enabled.
                REQUIRE_FALSE(IsSpatialCapabilitySupported(instance, instance.systemId, capability));
            }
            else {
                WARN(capabilityExtensionName << " force-enabled, cannot test behavior when extension is enabled.");
            }
        }

        SECTION("Extension enabled")
        {
            AutoBasicInstance instance({XR_EXT_FUTURE_EXTENSION_NAME, XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME, capabilityExtensionName},
                                       AutoBasicInstance::createSystemId);

#define FUNCTION_POINTERS(_)                       \
    _(xrCreateSpatialContextAsyncEXT)              \
    _(xrCreateSpatialContextCompleteEXT)           \
    _(xrDestroySpatialContextEXT)                  \
    _(xrCreateSpatialDiscoverySnapshotAsyncEXT)    \
    _(xrCreateSpatialDiscoverySnapshotCompleteEXT) \
    _(xrDestroySpatialSnapshotEXT)                 \
    _(xrPollFutureEXT)

#define LOAD_FUNCTION_POINTER(name) PFN_##name name = GetInstanceExtensionFunction<PFN_##name>(instance, #name);

            FUNCTION_POINTERS(LOAD_FUNCTION_POINTER)

#undef LOAD_FUNCTION_POINTER
#undef FUNCTION_POINTERS

            static const std::chrono::nanoseconds kTimeout = 5s;
            static const std::chrono::nanoseconds kInterval = 16ms;

            if (!IsSpatialCapabilitySupported(instance, instance.systemId, capability)) {
                SKIP(XrEnumStr(capability) << " not enumerated as "
                                              "supported.");
            }

            SECTION("Guaranteed components")
            {
                for (XrSpatialComponentTypeEXT guaranteedComponent : guaranteedComponents) {
                    // Capability must support all of its guaranteed components.
                    REQUIRE(IsSpatialComponentSupported(instance, instance.systemId, capability, guaranteedComponent));
                }
            }

            AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession | AutoBasicSession::createSwapchains |
                                         AutoBasicSession::OptionFlags::createSpaces,
                                     instance);

            SECTION("No capabilities in context create info")
            {
                XrSpatialContextCreateInfoEXT contextCreateInfo{XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT};
                XrFutureEXT future;
                // The runtime must: return
                // XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT if
                // slink:XrSpatialContextCreateInfoEXT::capabilityConfigCount is 0.
                // A spatial context handle needs at least one capability.
                REQUIRE(XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT ==
                        xrCreateSpatialContextAsyncEXT(session, &contextCreateInfo, &future));
            }

            SECTION("No components in capability config")
            {
                T config = capabilityConfig;
                config.enabledComponentCount = 0;
                config.enabledComponents = nullptr;

                std::array<const XrSpatialCapabilityConfigurationBaseHeaderEXT*, 1> capabilityConfigs = {{
                    reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&config),
                }};

                XrSpatialContextCreateInfoEXT contextCreateInfo{XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT, nullptr,
                                                                static_cast<uint32_t>(capabilityConfigs.size()), capabilityConfigs.data()};

                XrFutureEXT future;
                // The runtime must: return
                // XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT if any
                // XrSpatialCapabilityConfigurationBaseHeaderEXT::enabledComponentCount
                // in XrSpatialContextCreateInfoEXT::capabilityConfigs is 0.
                // A capability configuration is incomplete without a list of component types
                // that need to be enabled for that capability.
                REQUIRE(XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT ==
                        xrCreateSpatialContextAsyncEXT(session, &contextCreateInfo, &future));
            }

            // TODO:
            // 1. check for XR_ERROR_SPATIAL_CAPABILITY_UNSUPPORTED_EXT if a config is
            // provided for an unsupported capability.
            // 2. check for XR_ERROR_SPATIAL_COMPONENT_UNSUPPORTED_FOR_CAPABILITY_EXT
            // if an unsupported component is provided in the capability config.

            SECTION("Duplicate configs")
            {
                T config1 = capabilityConfig;
                config1.enabledComponentCount = static_cast<uint32_t>(guaranteedComponents.size());
                config1.enabledComponents = guaranteedComponents.data();
                T config2 = config1;

                std::array<const XrSpatialCapabilityConfigurationBaseHeaderEXT*, 2> capabilityConfigs = {{
                    reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&config1),
                    reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&config2),
                }};

                XrSpatialContextCreateInfoEXT contextCreateInfo{XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT, nullptr,
                                                                static_cast<uint32_t>(capabilityConfigs.size()), capabilityConfigs.data()};

                XrFutureEXT future;
                // The runtime must: return
                // XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT if
                // XrSpatialContextCreateInfoEXT::capabilityConfigs contains
                // multiple structs with the same
                // XrSpatialCapabilityConfigurationBaseHeaderEXT::capability.
                REQUIRE(XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT ==
                        xrCreateSpatialContextAsyncEXT(session, &contextCreateInfo, &future));
            }

            SECTION("Create and destroy spatial context")
            {
                T config = capabilityConfig;
                config.enabledComponentCount = static_cast<uint32_t>(guaranteedComponents.size());
                config.enabledComponents = guaranteedComponents.data();

                std::array<const XrSpatialCapabilityConfigurationBaseHeaderEXT*, 1> capabilityConfigs = {{
                    reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&config),
                }};

                XrSpatialContextCreateInfoEXT contextCreateInfo{XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT, nullptr,
                                                                static_cast<uint32_t>(capabilityConfigs.size()), capabilityConfigs.data()};

                XrFutureEXT future;
                REQUIRE(XR_SUCCESS == xrCreateSpatialContextAsyncEXT(session, &contextCreateInfo, &future));
                REQUIRE(WaitUntilPredicateWithTimeout(
                    [&]() {
                        XrFuturePollInfoEXT pollInfo{
                            XR_TYPE_FUTURE_POLL_INFO_EXT,
                            nullptr,
                            future,
                        };
                        XrFuturePollResultEXT pollResult{
                            XR_TYPE_FUTURE_POLL_RESULT_EXT,
                        };
                        XrResult result = xrPollFutureEXT(instance, &pollInfo, &pollResult);
                        return (result == XR_SUCCESS) && pollResult.state == XR_FUTURE_STATE_READY_EXT;
                    },
                    kTimeout, kInterval));

                XrCreateSpatialContextCompletionEXT completion{XR_TYPE_CREATE_SPATIAL_CONTEXT_COMPLETION_EXT};
                REQUIRE(XR_SUCCESS == xrCreateSpatialContextCompleteEXT(session, future, &completion));
                REQUIRE(XR_SUCCESS == completion.futureResult);

                XrSpatialContextEXT spatialContext = completion.spatialContext;

                SECTION("Discovery snapshot")
                {
                    XrSpatialDiscoverySnapshotCreateInfoEXT discoverySnapshotCreateInfo{
                        XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT,
                        nullptr,
                        1,
                        &nonEnabledComponent,
                    };

                    SECTION("Component not enabled")
                    {
                        // The runtime must: return XR_ERROR_SPATIAL_COMPONENT_NOT_ENABLED_EXT if
                        // any of the XrSpatialComponentTypeEXT in
                        // XrSpatialDiscoverySnapshotCreateInfoEXT::componentTypes are not
                        // enabled for the spatial capabilities passed to
                        // XrSpatialContextCreateInfoEXT::capabilityConfigs when creating
                        // spatialContext.
                        REQUIRE(XR_ERROR_SPATIAL_COMPONENT_NOT_ENABLED_EXT ==
                                xrCreateSpatialDiscoverySnapshotAsyncEXT(spatialContext, &discoverySnapshotCreateInfo, &future));
                    }

                    SECTION("Create and destroy discovery snapshot")
                    {
                        discoverySnapshotCreateInfo.componentTypeCount = 0;
                        discoverySnapshotCreateInfo.componentTypes = nullptr;
                        REQUIRE(XR_SUCCESS ==
                                xrCreateSpatialDiscoverySnapshotAsyncEXT(spatialContext, &discoverySnapshotCreateInfo, &future));
                        REQUIRE(WaitUntilPredicateWithTimeout(
                            [&]() {
                                XrFuturePollInfoEXT pollInfo{
                                    XR_TYPE_FUTURE_POLL_INFO_EXT,
                                    nullptr,
                                    future,
                                };
                                XrFuturePollResultEXT pollResult{
                                    XR_TYPE_FUTURE_POLL_RESULT_EXT,
                                };
                                XrResult result = xrPollFutureEXT(instance, &pollInfo, &pollResult);
                                return (result == XR_SUCCESS) && pollResult.state == XR_FUTURE_STATE_READY_EXT;
                            },
                            kTimeout, kInterval));

                        FrameIterator frameIterator(&session);
                        frameIterator.RunToSessionState(XR_SESSION_STATE_VISIBLE);

                        const auto isRefSpaceLocatable = [&]() {
                            frameIterator.SubmitFrame();

                            XrSpaceLocation refSpaceLocation = {
                                XR_TYPE_SPACE_LOCATION,
                            };
                            const XrResult result = xrLocateSpace(session.spaceVector[0], session.spaceVector[1],
                                                                  frameIterator.frameState.predictedDisplayTime, &refSpaceLocation);
                            return result == XR_SUCCESS &&
                                   ((refSpaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0) &&
                                   ((refSpaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0);
                        };

                        if (WaitUntilPredicateWithTimeout(isRefSpaceLocatable, kTimeout, kInterval)) {
                            XrCreateSpatialDiscoverySnapshotCompletionInfoEXT createSnapshotCompletionInfo{
                                XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_INFO_EXT,
                                nullptr,
                                session.spaceVector[0],
                                frameIterator.frameState.predictedDisplayTime,
                                future,
                            };
                            XrCreateSpatialDiscoverySnapshotCompletionEXT discoverySnapshotCompletion{
                                XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_EXT};
                            REQUIRE(XR_SUCCESS == xrCreateSpatialDiscoverySnapshotCompleteEXT(spatialContext, &createSnapshotCompletionInfo,
                                                                                              &discoverySnapshotCompletion));
                            REQUIRE(XR_SUCCESS == discoverySnapshotCompletion.futureResult);

                            REQUIRE(XR_SUCCESS == xrDestroySpatialSnapshotEXT(discoverySnapshotCompletion.snapshot));
                        }
                        else {
                            FAIL(
                                "Could not locate ref space 0 against 1, never got to "
                                "completing "
                                "the "
                                "discovery snapshot creation.");
                        }
                    }
                }

                REQUIRE(XR_SUCCESS == xrDestroySpatialContextEXT(completion.spatialContext));
            }
        }
    }
}  // namespace Conformance
