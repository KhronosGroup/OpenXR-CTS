// Copyright (c) 2019-2021, The Khronos Group Inc.
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
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

#ifdef XR_USE_GRAPHICS_API_OPENGL

#if !defined(XR_USE_PLATFORM_XLIB) && !defined(XR_USE_PLATFORM_ANDROID)
// some types are not defined when compiling for XLIB, probably easy to fix
// when working on such a system, but as Linux isn't supported yet and this
// issue is preventing the CI builds, we exclude this header for now:
#include <openxr/openxr_platform.h>
#endif  // XR_USE_PLATFORM_XLIB

namespace Conformance
{

    TEST_CASE("XR_KHR_opengl_enable", "")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionEnabled("XR_KHR_opengl_enable")) {
            return;
        }

        AutoBasicInstance instance;

        XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO, nullptr, globalData.options.formFactorValue};
        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrGetSystem(instance, &systemGetInfo, &systemId));

        // Create the graphics plugin we'll need to exercise session create functionality below.
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin;

        if (!globalData.options.graphicsPlugin.empty()) {
            REQUIRE_NOTHROW(graphicsPlugin = Conformance::CreateGraphicsPlugin(globalData.options.graphicsPlugin.c_str(),
                                                                               globalData.GetPlatformPlugin()));
            REQUIRE(graphicsPlugin->Initialize());
        }

        // We'll use this XrSession and XrSessionCreateInfo for testing below.
        XrSession session = XR_NULL_HANDLE_CPP;
        XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO, nullptr, 0, systemId};
        CleanupSessionOnScopeExit cleanup(session);

        SECTION("No graphics binding")
        {
            graphicsPlugin->InitializeDevice(instance, systemId, true);
            sessionCreateInfo.next = nullptr;
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }

#if defined(XR_USE_PLATFORM_WIN32)
        // tests related to the graphics binding are OS specific
        SECTION("NULL context: both are NULL")
        {
            graphicsPlugin->InitializeDevice(instance, systemId, true);
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
            graphicsPlugin->InitializeDevice(instance, systemId, true);
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
            graphicsPlugin->InitializeDevice(instance, systemId, true);
            XrGraphicsBindingOpenGLWin32KHR graphicsBinding =
                *reinterpret_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphicsPlugin->GetGraphicsBinding());
            graphicsBinding.hGLRC = nullptr;
            sessionCreateInfo.next = reinterpret_cast<const void*>(&graphicsBinding);
            CHECK(xrCreateSession(instance, &sessionCreateInfo, &session) == XR_ERROR_GRAPHICS_DEVICE_INVALID);
            cleanup.Destroy();
            graphicsPlugin->ShutdownDevice();
        }
        // This test dies in the tear-down wglMakeCurrent in ksGpuContext_Destroy, turn it off for now.
#if 0
        SECTION("Context for runtime is not current")
        {
            graphicsPlugin->InitializeDevice(instance, systemId, true);
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
#endif
#endif  // XR_USE_PLATFORM_WIN32
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_OPENGL
