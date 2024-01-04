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

#ifdef XR_USE_GRAPHICS_API_OPENGL

#include "conformance_framework.h"
#include "conformance_utils.h"
#include "graphics_plugin.h"
#include "matchers.h"
#include "utilities/utils.h"
#include "common/xr_dependencies.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <string>
#include <cstring>

namespace Conformance
{

    TEST_CASE("XR_KHR_opengl_enable", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionEnabled(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME)) {
            SKIP(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME " not enabled");
        }

        AutoBasicInstance instance;

        XrSystemId systemId{XR_NULL_SYSTEM_ID};
        REQUIRE(XR_SUCCESS == FindBasicSystem(instance.GetInstance(), &systemId));

        // Create the graphics plugin we'll need to exercise session create functionality below.
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin;

        if (!globalData.options.graphicsPlugin.empty()) {
            REQUIRE_NOTHROW(graphicsPlugin = Conformance::CreateGraphicsPlugin(globalData.options.graphicsPlugin.c_str(),
                                                                               globalData.GetPlatformPlugin()));
            REQUIRE(graphicsPlugin->Initialize());
        }

        // We'll use this XrSession and XrSessionCreateInfo for testing below.
        XrSession session = XR_NULL_HANDLE_CPP;

        XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
        sessionCreateInfo.systemId = systemId;

        CleanupSessionOnScopeExit cleanup(session);

        SECTION("No graphics binding")
        {
            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            sessionCreateInfo.next = nullptr;
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

#if defined(XR_USE_PLATFORM_WIN32)
        // tests related to the graphics binding are OS specific
        SECTION("NULL context: both are NULL")
        {
            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            XrGraphicsBindingOpenGLWin32KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphicsPlugin->GetGraphicsBinding());
            graphicsBinding.hDC = nullptr;
            graphicsBinding.hGLRC = nullptr;
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

        SECTION("NULL context: DC is NULL")
        {
            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            XrGraphicsBindingOpenGLWin32KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphicsPlugin->GetGraphicsBinding());
            graphicsBinding.hDC = nullptr;
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

        SECTION("NULL context: GLRC is NULL")
        {
            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            XrGraphicsBindingOpenGLWin32KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphicsPlugin->GetGraphicsBinding());
            graphicsBinding.hGLRC = nullptr;
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

        SECTION("Valid session after bad session")
        {
            // Pass invalid binding the first time
            {
                REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
                XrGraphicsBindingOpenGLWin32KHR graphicsBinding =
                    *reinterpret_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphicsPlugin->GetGraphicsBinding());
                graphicsBinding.hDC = nullptr;
                sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
                CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
                cleanup.Destroy();
                graphicsPlugin->ShutdownDevice();
            }

            // Using the same instance pass valid binding the second time
            {
                REQUIRE(XR_SUCCESS == FindBasicSystem(instance.GetInstance(), &systemId));
                sessionCreateInfo.systemId = systemId;

                REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
                XrGraphicsBindingOpenGLWin32KHR graphicsBinding =
                    *reinterpret_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphicsPlugin->GetGraphicsBinding());
                sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
                CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);
                cleanup.Destroy();
                graphicsPlugin->ShutdownDevice();
            }
        }

        // This test dies in the tear-down wglMakeCurrent in ksGpuContext_Destroy, turn it off for now.
#if 0
        SECTION("Context for runtime is not current")
        {
            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            sessionCreateInfo.next = graphicsPlugin->GetGraphicsBinding();

            // Exercise presence of unrecognized extensions, which the runtime should ignore.
            InsertUnrecognizableExtension(&sessionCreateInfo);

            GetGlobalData().graphicsPlugin->MakeCurrent(true);

            // The currently set graphics context does not have to be the one the runtime should
            // use. Here the context is unset, but the application might also use multiple contexts
            // and have one of the other ones bound.
            HDC dcAtFunctionCall = nullptr;
            HGLRC gldcAtFunctionCall = nullptr;
            wglMakeCurrent(dcAtFunctionCall, gldcAtFunctionCall);

            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);

            // The runtime probably sets the context provided by the application to set up API interop.
            // However, it should "clean up" afterwards by making the context current which had been
            // current when the xrCreateSession was called.
            HDC currentDC = wglGetCurrentDC();
            CHECK(currentDC == dcAtFunctionCall);
            HGLRC currentGLRC = wglGetCurrentContext();
            CHECK(currentGLRC == gldcAtFunctionCall);

            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }
#endif  // 0

        SECTION("Multiple session with same device")
        {
            auto createSwapchains = [](std::shared_ptr<IGraphicsPlugin> graphicsPlugin, XrSession session) {
                for (int i = 0; i < 3; ++i) {
                    XrSwapchain swapchain;
                    XrExtent2Di widthHeight{0, 0};  // 0,0 means Use defaults.
                    XrResult result = CreateColorSwapchain(session, graphicsPlugin.get(), &swapchain, &widthHeight);
                    CHECK_THAT(result, In<XrResult>({XR_SUCCESS, XR_ERROR_LIMIT_REACHED}));

                    if (XR_SUCCEEDED(result)) {
                        CHECK_RESULT_UNQUALIFIED_SUCCESS(xrDestroySwapchain(swapchain));
                    }
                }
            };

            auto xrGetOpenGLGraphicsRequirementsKHR =
                GetInstanceExtensionFunction<PFN_xrGetOpenGLGraphicsRequirementsKHR>(instance, "xrGetOpenGLGraphicsRequirementsKHR");

            XrGraphicsRequirementsOpenGLKHR referenceGraphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
            REQUIRE(xrGetOpenGLGraphicsRequirementsKHR(instance, systemId, &referenceGraphicsRequirements) == XR_SUCCESS);

            REQUIRE(graphicsPlugin->InitializeDevice(instance, systemId, true));
            XrGraphicsBindingOpenGLWin32KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphicsPlugin->GetGraphicsBinding());
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            for (int i = 0; i < 3; ++i) {
                REQUIRE(XR_SUCCESS == FindBasicSystem(instance.GetInstance(), &systemId));
                sessionCreateInfo.systemId = systemId;

                XrGraphicsRequirementsOpenGLKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
                REQUIRE(xrGetOpenGLGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements) == XR_SUCCESS);

                // We expect that the graphics requirements don't change...
                REQUIRE(referenceGraphicsRequirements.maxApiVersionSupported == graphicsRequirements.maxApiVersionSupported);
                REQUIRE(referenceGraphicsRequirements.minApiVersionSupported == graphicsRequirements.minApiVersionSupported);

                CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_SUCCESS);
                createSwapchains(graphicsPlugin, session);
                CHECK(xrDestroySession(session) == XR_SUCCESS);
                session = XR_NULL_HANDLE;
            }
            graphicsPlugin->ShutdownDevice();
        }
#endif  // XR_USE_PLATFORM_WIN32
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_OPENGL
