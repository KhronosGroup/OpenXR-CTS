// Copyright (c) 2019-2025 The Khronos Group Inc.
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
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <chrono>
#include <functional>
#include <unordered_set>

#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "report.h"
#include "spatial_conformance_utils.h"
#include "utilities/stringification.h"
#include "utilities/throw_helpers.h"

using namespace Conformance;
using Catch::Matchers::VectorContains;

namespace Conformance
{
    namespace
    {

        using namespace std::literals::chrono_literals;
        const std::chrono::nanoseconds kTimeout = 5s;
        const std::chrono::nanoseconds kInterval = 16ms;

#define PERSISTENCE_FUNCTION_POINTERS(_)            \
    _(xrEnumerateSpatialPersistenceScopesEXT)       \
    _(xrCreateSpatialPersistenceContextAsyncEXT)    \
    _(xrCreateSpatialPersistenceContextCompleteEXT) \
    _(xrDestroySpatialPersistenceContextEXT)

#define FUTURE_FUNCTION_POINTERS(_) _(xrPollFutureEXT)

#define VALIDATE_FUNCTION_NOT_SUPPORTED(name) ValidateInstanceExtensionFunctionNotSupported(instance, #name);
#define LOAD_FUNCTION_POINTER(name) PFN_##name name = GetInstanceExtensionFunction<PFN_##name>(instance, #name);

        TEST_CASE("XR_EXT_spatial_persistence", "[XR_EXT_spatial_persistence][XR_EXT_spatial_entity]")
        {
            GlobalData& globalData = GetGlobalData();
            if (!globalData.IsInstanceExtensionSupported(XR_EXT_SPATIAL_PERSISTENCE_EXTENSION_NAME)) {
                SKIP(XR_EXT_SPATIAL_PERSISTENCE_EXTENSION_NAME << " not supported");
            }

            SECTION("Extension not enabled")
            {
                if (!globalData.IsInstanceExtensionEnabled(XR_EXT_SPATIAL_PERSISTENCE_EXTENSION_NAME)) {
                    AutoBasicInstance instance;

                    PERSISTENCE_FUNCTION_POINTERS(VALIDATE_FUNCTION_NOT_SUPPORTED);
                }
                else {
                    WARN(XR_EXT_SPATIAL_PERSISTENCE_EXTENSION_NAME << " force-enabled, cannot test behavior when extension is enabled.");
                }
            }

            SECTION("Extension enabled")
            {
                AutoBasicInstance instance(
                    {XR_EXT_FUTURE_EXTENSION_NAME, XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME, XR_EXT_SPATIAL_PERSISTENCE_EXTENSION_NAME},
                    AutoBasicInstance::createSystemId);

                PERSISTENCE_FUNCTION_POINTERS(LOAD_FUNCTION_POINTER)
                FUTURE_FUNCTION_POINTERS(LOAD_FUNCTION_POINTER)

                uint32_t persistenceScopeCountOutput = 0;
                XRC_CHECK_THROW_XRCMD(
                    xrEnumerateSpatialPersistenceScopesEXT(instance, instance.systemId, 0, &persistenceScopeCountOutput, nullptr));
                if (persistenceScopeCountOutput == 0) {
                    SKIP(XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME << ": No persistence scope supported. Skipping tests.");
                }

                std::vector<XrSpatialPersistenceScopeEXT> persistenceScopes(persistenceScopeCountOutput);
                XRC_CHECK_THROW_XRCMD(xrEnumerateSpatialPersistenceScopesEXT(instance, instance.systemId, persistenceScopeCountOutput,
                                                                             &persistenceScopeCountOutput, persistenceScopes.data()));

                SECTION(
                    "Create and destroy spatial persistence context : "
                    "XR_SPATIAL_PERSISTENCE_SCOPE_SYSTEM_MANAGED_EXT")
                {
                    if (!VectorContains(XR_SPATIAL_PERSISTENCE_SCOPE_SYSTEM_MANAGED_EXT).match(persistenceScopes)) {
                        SKIP(
                            "XR_SPATIAL_PERSISTENCE_SCOPE_SYSTEM_MANAGED_EXT not supported. "
                            "Skipping tests.");
                    }
                    AutoBasicSession session(AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                                 AutoBasicSession::createSwapchains | AutoBasicSession::OptionFlags::createSpaces,
                                             instance);

                    XrSpatialPersistenceContextCreateInfoEXT createInfo{XR_TYPE_SPATIAL_PERSISTENCE_CONTEXT_CREATE_INFO_EXT};
                    createInfo.scope = XR_SPATIAL_PERSISTENCE_SCOPE_SYSTEM_MANAGED_EXT;

                    XrFutureEXT future;
                    REQUIRE(XR_SUCCESS == xrCreateSpatialPersistenceContextAsyncEXT(session, &createInfo, &future));
                    REQUIRE(WaitUntilPredicateWithTimeout(
                        [&]() {
                            XrFuturePollInfoEXT pollInfo{XR_TYPE_FUTURE_POLL_INFO_EXT};
                            pollInfo.future = future;

                            XrFuturePollResultEXT pollResult{XR_TYPE_FUTURE_POLL_RESULT_EXT};
                            XrResult result = xrPollFutureEXT(instance, &pollInfo, &pollResult);
                            return (result == XR_SUCCESS) && pollResult.state == XR_FUTURE_STATE_READY_EXT;
                        },
                        kTimeout, kInterval));

                    XrCreateSpatialPersistenceContextCompletionEXT completion{XR_TYPE_CREATE_SPATIAL_PERSISTENCE_CONTEXT_COMPLETION_EXT};
                    REQUIRE(XR_SUCCESS == xrCreateSpatialPersistenceContextCompleteEXT(session, future, &completion));
                    REQUIRE(XR_SUCCESS == completion.futureResult);
                    if (completion.createResult != XR_SPATIAL_PERSISTENCE_CONTEXT_RESULT_SUCCESS_EXT) {
                        SKIP("Skipping test, spatial persistence context creation failed: " << XrEnumStr(completion.createResult));
                    }

                    REQUIRE_FALSE(XR_NULL_HANDLE == completion.persistenceContext);

                    REQUIRE(XR_SUCCESS == xrDestroySpatialPersistenceContextEXT(completion.persistenceContext));
                }
            }
        }

    }  // namespace
}  // namespace Conformance
