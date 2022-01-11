// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#include "utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "matchers.h"
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

namespace Conformance
{

    TEST_CASE("xrCreateSession", "")
    {
        GlobalData& globalData = GetGlobalData();

        // XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);

        AutoBasicInstance instance;

        XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO, nullptr, globalData.options.formFactorValue};
        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrGetSystem(instance, &systemGetInfo, &systemId));

        // Create the graphics plugin we'll need to exercise session create functionality below.
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin;  // CreateGraphicsPlugin may throw a C++ exception.

        if (globalData.IsGraphicsPluginRequired()) {
            // Should have quit earlier.
            assert(!globalData.options.graphicsPlugin.empty());
        }
        if (!globalData.options.graphicsPlugin.empty()) {
            REQUIRE_NOTHROW(graphicsPlugin = Conformance::CreateGraphicsPlugin(globalData.options.graphicsPlugin.c_str(),
                                                                               globalData.GetPlatformPlugin()));
            REQUIRE(graphicsPlugin->Initialize());
        }

        // We'll use this XrSession and XrSessionCreateInfo for testing below.
        XrSession session = XR_NULL_HANDLE_CPP;
        XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO, nullptr, 0, systemId};
        CleanupSessionOnScopeExit cleanup(session);

        SECTION("Missing graphics binding implies XR_ERROR_GRAPHICS_DEVICE_INVALID")
        {
            sessionCreateInfo.next = nullptr;
            XrResult result;
            CAPTURE(result = xrCreateSession(instance, &sessionCreateInfo, &session));
            if (globalData.IsGraphicsPluginRequired()) {
                CHECK(result == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            }
            else {
                INFO("A headless extension is enabled - valid to omit graphics binding struct");

                CHECK(result == XR_SUCCESS);
                cleanup.Destroy();
            }
        }

        if (graphicsPlugin) {
            SECTION("XR_ERROR_VALIDATION_FAILURE if we skip the graphics requirements call")
            {
                // Happens if the application tries to create the session but hasn't queried the graphics requirements (e.g.
                // xrGetD3D12GraphicsRequirementsKHR). This spec states that applications must call this, but
                // how we enforce it in conformance testing is problematic because a specific return code isn't specified.
                graphicsPlugin->InitializeDevice(instance, systemId, false /* checkGraphicsRequirements */);
                sessionCreateInfo.next = graphicsPlugin->GetGraphicsBinding();
                XrResult sessionResult = xrCreateSession(instance, &sessionCreateInfo, &session);
                CHECK_THAT(sessionResult, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING}));
                if (sessionResult == XR_ERROR_VALIDATION_FAILURE) {
                    WARN("Runtime should prefer XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING over XR_ERROR_VALIDATION_FAILURE");
                }

                cleanup.Destroy();
                graphicsPlugin->ShutdownDevice();
            }
        }

        SECTION("XR_ERROR_SYSTEM_INVALID on XR_NULL_SYSTEM_ID")
        {
            sessionCreateInfo.systemId = XR_NULL_SYSTEM_ID;
            REQUIRE(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_SYSTEM_INVALID);
        }

        SECTION("XR_ERROR_SYSTEM_INVALID on an arbitrary, presumably invalid system ID")
        {
            sessionCreateInfo.systemId = globalData.invalidSystemId;
            REQUIRE(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_SYSTEM_INVALID);
        }

        SECTION("XR_SUCCESS in repetition")
        {
            for (int i = 0; i < 20; ++i) {
                CAPTURE(i);
                AutoBasicSession sessionTemp(
                    ((i % 4) < 2) ? AutoBasicSession::OptionFlags::beginSession : AutoBasicSession::OptionFlags::createSession, instance);
            }

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                SECTION("Null handle")
                {
                    CHECK(xrCreateSession(XR_NULL_HANDLE_CPP, &sessionCreateInfo, &session) == XR_ERROR_HANDLE_INVALID);
                }
                SECTION("Non-null but presumably invalid handle")
                {
                    CHECK(xrCreateSession(globalData.invalidInstance, &sessionCreateInfo, &session) == XR_ERROR_HANDLE_INVALID);
                }
            }
        }
    }
}  // namespace Conformance
