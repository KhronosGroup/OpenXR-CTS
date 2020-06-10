// Copyright (c) 2019-2020 The Khronos Group Inc.
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

#include "graphics_plugin.h"

#ifdef XR_USE_GRAPHICS_API_OPENGL

#include "swapchain_parameters.h"

#include "xr_dependencies.h"

#include <GL/gl.h>
#include "graphics_plugin_opengl_loader.h"

#include "conformance_framework.h"
#include <catch2/catch.hpp>

#include <openxr/openxr_platform.h>
#include <openxr/openxr.h>
#include <thread>

#ifndef XR_USE_PLATFORM_WIN32
#include "gfxwrapper_opengl.h"
#endif  // XR_USE_PLATFORM_WIN32

#if defined(XR_USE_PLATFORM_WIN32)
LRESULT CALLBACK windowsMessageCallback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT:
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
#endif

namespace Conformance
{

    struct OpenGLGraphicsPlugin : public IGraphicsPlugin
    {
        OpenGLGraphicsPlugin(const std::shared_ptr<IPlatformPlugin>& /*unused*/);
        ~OpenGLGraphicsPlugin() override;

        OpenGLGraphicsPlugin(const OpenGLGraphicsPlugin&) = delete;
        OpenGLGraphicsPlugin& operator=(const OpenGLGraphicsPlugin&) = delete;
        OpenGLGraphicsPlugin(OpenGLGraphicsPlugin&&) = delete;
        OpenGLGraphicsPlugin& operator=(OpenGLGraphicsPlugin&&) = delete;

        bool Initialize() override;

        bool IsInitialized() const override;

        void Shutdown() override;

        std::string DescribeGraphics() const override;

        std::vector<std::string> GetInstanceExtensions() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

        // TODO: not implemented yet, not called for automatic conformance tests
        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, int64_t imageFormat, uint32_t arraySlice,
                           const RGBAImage& image) override;

        std::string GetImageFormatName(int64_t imageFormat) const override;

        bool IsImageFormatKnown(int64_t imageFormat) const override;

        bool GetSwapchainCreateTestParameters(XrInstance instance, XrSession session, XrSystemId systemId, int64_t imageFormat,
                                              SwapchainCreateTestParameters* swapchainTestParameters) override;

        bool ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                     uint32_t* imageCount) const override;
        bool ValidateSwapchainImageState(XrSwapchain swapchain, uint32_t index, int64_t imageFormat) const override;

        int64_t SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        // Format required by RGBAImage type. TODO: Mandate this type in the spec?
        int64_t GetRGBA8UnormFormat() const override;

        std::shared_ptr<SwapchainImageStructs> AllocateSwapchainImageStructs(size_t size,
                                                                             const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                             int64_t colorSwapchainFormat) override;

        // TODO: not implemented yet, not called for automatic conformance tests, working code exists in hello_xr!
        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        int64_t colorSwapchainFormat, const std::vector<Cube>& cubes) override;

    protected:
        struct OpenGLSwapchainImageStructs : public IGraphicsPlugin::SwapchainImageStructs
        {
            std::vector<XrSwapchainImageOpenGLKHR> imageVector;
        };

    private:
        bool initialized;

        void deleteGLContext();

        XrVersion OpenGLVersionOfContext = 0;

        bool deviceInitialized{false};
#if defined(XR_USE_PLATFORM_WIN32)
        XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR, nullptr, nullptr, nullptr};
        // Windows related:
        void WindowMainLoop();
        HINSTANCE hInstance;
        HWND hWindow;
        HDC hDC = 0;
        HGLRC hGLRC = 0;
        bool keepWindowOpen = true;

        std::thread windowMainLoop;
#else  // XR_USE_PLATFORM_WIN32

        ksGpuWindow window{};
#endif

#if defined(XR_USE_PLATFORM_XLIB)
        XrGraphicsBindingOpenGLXlibKHR graphicsBinding = {
            XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR, nullptr, nullptr, 0, nullptr, 0, nullptr};

#endif  // XR_USE_PLATFORM_XLIB
    };

    OpenGLGraphicsPlugin::OpenGLGraphicsPlugin(const std::shared_ptr<IPlatformPlugin>& /*unused*/) : initialized(false)
    {
    }

    OpenGLGraphicsPlugin::~OpenGLGraphicsPlugin()
    {
        ShutdownDevice();
        Shutdown();
    }

    bool OpenGLGraphicsPlugin::Initialize()
    {
        if (initialized) {
            return false;
        }

#if defined(XR_USE_PLATFORM_WIN32)
        hInstance = static_cast<HINSTANCE>(GetModuleHandleW(nullptr));
        if (!hInstance) {
            return false;
        }

        LPCSTR className = "ConformanceTestOpenGL";

        WNDCLASSEX wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = windowsMessageCallback;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = NULL;
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = NULL;
        wcex.lpszClassName = className;
        wcex.hIconSm = NULL;
        RegisterClassEx(&wcex);

        hWindow = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, className, "ConformanceTest OpenGL", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                 CW_USEDEFAULT, 640, 480, NULL, NULL, hInstance, 0);

        if (!hWindow) {
            return false;
        }

        ShowWindow(hWindow, SW_SHOWDEFAULT);
        UpdateWindow(hWindow);

        windowMainLoop = std::thread(&OpenGLGraphicsPlugin::WindowMainLoop, this);
#endif  // XR_USE_PLATFORM_WIN32

        initialized = true;
        return initialized;
    }

    bool OpenGLGraphicsPlugin::IsInitialized() const
    {
        return initialized;
    }

    void OpenGLGraphicsPlugin::Shutdown()
    {
        if (initialized) {
#if defined(XR_USE_PLATFORM_WIN32)
            keepWindowOpen = false;
            DestroyWindow(hWindow);
            windowMainLoop.join();
#endif  // XR_USE_PLATFORM_WIN32

#ifdef XR_USE_PLATFORM_XLIB

#endif  // XR_USE_PLATFORM_XLIB
            initialized = false;
        }
    }

    std::string OpenGLGraphicsPlugin::DescribeGraphics() const
    {
        return std::string("OpenGL");
    }

    std::vector<std::string> OpenGLGraphicsPlugin::GetInstanceExtensions() const
    {
        return {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
    }

    const XrBaseInStructure* OpenGLGraphicsPlugin::GetGraphicsBinding() const
    {
        if (deviceInitialized) {
            return reinterpret_cast<const XrBaseInStructure*>(&graphicsBinding);
        }
        return nullptr;
    }

    void OpenGLGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* /*swapchainImage*/, int64_t /*imageFormat*/,
                                             uint32_t /*arraySlice*/, const RGBAImage& /*image*/)
    {
        IGRAPHICSPLUGIN_UNIMPLEMENTED_METHOD();
    }

    void OpenGLGraphicsPlugin::deleteGLContext()
    {
#if defined(XR_USE_PLATFORM_WIN32)
        if (0 != hGLRC) {
            wglMakeCurrent(hDC, 0);
            wglDeleteContext(hGLRC);
            hGLRC = 0;
            graphicsBinding.hGLRC = hGLRC;
        }
#else   // XR_USE_PLATFORM_WIN32
        if (deviceInitialized) {
            ksGpuWindow_Destroy(&window);
        }
#endif  // XR_USE_PLATFORM_WIN32

        deviceInitialized = false;
    }

#if defined(XR_USE_PLATFORM_WIN32)
    void OpenGLGraphicsPlugin::WindowMainLoop()
    {
        MSG msg;
        while (keepWindowOpen) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    break;
                }
            }
        }
    }
#endif  // XR_USE_PLATFORM_WIN32

    bool OpenGLGraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                                                uint32_t deviceCreationFlags)
    {
        XrGraphicsRequirementsOpenGLKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR, nullptr, XR_MAKE_VERSION(3, 2, 0),
                                                             XR_MAKE_VERSION(4, 6, 0)};

        // optional check to get the graphics requirements:
        if (checkGraphicsRequirements) {

            auto xrGetOpenGLGraphicsRequirementsKHR =
                GetInstanceExtensionFunction<PFN_xrGetOpenGLGraphicsRequirementsKHR>(instance, "xrGetOpenGLGraphicsRequirementsKHR");

            XrResult result = xrGetOpenGLGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements);
            CHECK(ValidateResultAllowed("xrGetOpenGLGraphicsRequirementsKHR", result));
            if (XR_FAILED(result)) {
                // Log result?
                return false;
            }
        }

        // In contrast to DX, OpenGL on Windows needs a window to render:
        if (deviceInitialized == true) {
            // a context exists, this function has been called before!
            if (OpenGLVersionOfContext >= graphicsRequirements.minApiVersionSupported) {
                // no test for max version as using a higher (compatible) version is allowed!
                return true;
            }

            // delete the context to make a new one:
            deleteGLContext();
        }

#if defined(XR_USE_PLATFORM_WIN32)
        // GL context:
        hDC = GetDC(hWindow);
        GLuint pixelFormat;

        PIXELFORMATDESCRIPTOR pixelFormatDesc;
        memset(&pixelFormatDesc, 0, sizeof(PIXELFORMATDESCRIPTOR));

        pixelFormatDesc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pixelFormatDesc.nVersion = 1;
        pixelFormatDesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | deviceCreationFlags;
        pixelFormatDesc.iPixelType = PFD_TYPE_RGBA;
        pixelFormatDesc.cColorBits = 32;
        pixelFormatDesc.cDepthBits = 24;
        pixelFormatDesc.cStencilBits = 8;

        pixelFormat = ChoosePixelFormat(hDC, &pixelFormatDesc);
        SetPixelFormat(hDC, pixelFormat, &pixelFormatDesc);

        hGLRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hGLRC);

        graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
        graphicsBinding.next = nullptr;
        graphicsBinding.hDC = hDC;
        graphicsBinding.hGLRC = hGLRC;

#else   // XR_USE_PLATFORM_WIN32
        (void)deviceCreationFlags;  // To silence wanrings/errors.

        ksDriverInstance driverInstance{};
        ksGpuQueueInfo queueInfo{};
        ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
        ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
        ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
        if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat, depthFormat, sampleCount, 640, 480, false)) {
            throw std::runtime_error("Unable to create GL context");
        }
#endif  // XR_USE_PLATFORM_WIN32

#ifdef XR_USE_PLATFORM_XLIB
        REQUIRE(window.context.xDisplay != nullptr);
        graphicsBinding.xDisplay = window.context.xDisplay;
        graphicsBinding.visualid = window.context.visualid;
        graphicsBinding.glxFBConfig = window.context.glxFBConfig;
        graphicsBinding.glxDrawable = window.context.glxDrawable;
        graphicsBinding.glxContext = window.context.glxContext;
#endif

        GLenum error = glGetError();
        CHECK(error == GL_NO_ERROR);

        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        error = glGetError();
        if (error != GL_NO_ERROR) {
            // Query for the GL version based on ints was added in OpenGL 3.1
            // this error means we would have to use the old way and parse a string (with implementation defined content!)
            // ( const GLubyte* versionString = glGetString(GL_VERSION); )

            // for now, the conformance tests require at least 3.1...
            deleteGLContext();
            return false;
        }

        OpenGLVersionOfContext = XR_MAKE_VERSION(major, minor, 0);
        if (OpenGLVersionOfContext < graphicsRequirements.minApiVersionSupported) {
            // OpenGL version of the conformance tests is lower than what the runtime requests -> can not be tested

            deleteGLContext();
            return false;
        }

        deviceInitialized = true;
        return true;
    }

    void OpenGLGraphicsPlugin::ShutdownDevice()
    {
        deleteGLContext();
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGL(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<OpenGLGraphicsPlugin>(std::move(platformPlugin));
    }

    // clang-format off
    // Note: mapping of OpenXR usage flags to OpenGL
    //
    // XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT: can be bound to a framebuffer as color
    // XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT: can be bound to a framebuffer as depth (or stencil-only GL_STENCIL_INDEX8)
    // XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT: image load/store and core since 4.2. List of supported formats is in https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
    // XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT & XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT: must be compatible format with glCopyTexImage* calls
    // XR_SWAPCHAIN_USAGE_SAMPLED_BIT: can be sampled in a shader
    // XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT: all GL formats are typed, but some can be reinterpreted with a different view. OpenGL 4.2 / 4.3 with MSAA. Only for color formats and compressed ones (list with compatible textures: https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_view.txt )
    //   Note: no GL formats are "mutableFormats" in the sense of SwapchainCreateTestParameters as this is intended for TYPELESS, however, some are "supportsMutableFormat"

    #define XRC_ALL_CREATE_FLAGS \
    { \
        0, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT | XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT \
    }

    // the app might request any combination of flags
#define XRC_COLOR_UA_COPY_SAMPLED_MUTABLE_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    }
    #define XRC_COLOR_UA_SAMPLED_MUTABLE_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    }
    #define XRC_COLOR_COPY_SAMPLED_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    }
    #define XRC_COLOR_COPY_SAMPLED_MUTABLE_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    }
    #define XRC_COLOR_SAMPLED_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    }
    #define XRC_DEPTH_COPY_SAMPLED_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    }
    #define XRC_DEPTH_SAMPLED_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, \
        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    }

    #define XRC_COMPRESSED_SAMPLED_MUTABLE_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    }
    #define XRC_COMPRESSED_SAMPLED_USAGE_FLAGS \
    {                     \
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    }

#define ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, true, true, false, FORMAT, XRC_COLOR_UA_COPY_SAMPLED_MUTABLE_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT(X) ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT2(X, #X)

#define ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, true, true, false, FORMAT, XRC_COLOR_UA_SAMPLED_MUTABLE_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(X) ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT2(X, #X)

    #define ADD_GL_COLOR_COPY_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, false, true, false, FORMAT, XRC_COLOR_COPY_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_COLOR_COPY_SAMPLED_FORMAT(X) ADD_GL_COLOR_COPY_SAMPLED_FORMAT2(X, #X)
    
    #define ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, true, true, false, FORMAT, XRC_COLOR_COPY_SAMPLED_MUTABLE_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT(X) ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT2(X, #X)

    #define ADD_GL_COLOR_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, false, true, false, FORMAT, XRC_COLOR_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_COLOR_SAMPLED_FORMAT(X) ADD_GL_COLOR_SAMPLED_FORMAT2(X, #X)

#define ADD_GL_DEPTH_COPY_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, false, false, false, FORMAT, XRC_DEPTH_COPY_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_DEPTH_COPY_SAMPLED_FORMAT(X) ADD_GL_DEPTH_COPY_SAMPLED_FORMAT2(X, #X)

#define ADD_GL_DEPTH_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, false, false, false, FORMAT, XRC_DEPTH_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_DEPTH_SAMPLED_FORMAT(X) ADD_GL_DEPTH_SAMPLED_FORMAT2(X, #X)

#define ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, true, true, true, FORMAT, XRC_COMPRESSED_SAMPLED_MUTABLE_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(X) ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT2(X, #X)

#define ADD_GL_COMPRESSED_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
    {                                                                              \
        {FORMAT},                                                                  \
        {                                                                          \
            NAME, false, false, true, true, FORMAT, XRC_COMPRESSED_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
            {                                                                      \
            }                                                                      \
        }                                                                          \
    }
#define ADD_GL_COMPRESSED_SAMPLED_FORMAT(X) ADD_GL_COMPRESSED_SAMPLED_FORMAT2(X, #X)

    // Only texture formats which are in OpenGL core and which are either color or depth renderable or
    // of a specific compressed format are listed below. Runtimes can support additional formats, but those
    // will not get tested.
    typedef std::map<int64_t, SwapchainCreateTestParameters> SwapchainTestMap;
    SwapchainTestMap openGLSwapchainTestMap{
        ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT(GL_RGBA8),
        ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT(GL_RGBA16),
        ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT(GL_RGB10_A2),

        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R8),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R16),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG8),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG16), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGB10_A2UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R16F), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG16F), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA16F), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R32F), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG32F), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA32F), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R11F_G11F_B10F), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R8I), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R8UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R16I), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R16UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R32I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R32UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG8I), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG8UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG16I), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG16UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG32I), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG32UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA8I), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA8UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA16I), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA16UI), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA32I), 
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA32UI),

        ADD_GL_COLOR_COPY_SAMPLED_FORMAT(GL_RGBA4),
        ADD_GL_COLOR_COPY_SAMPLED_FORMAT(GL_RGB5_A1),

        ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT(GL_SRGB8_ALPHA8),

        ADD_GL_COLOR_SAMPLED_FORMAT(GL_RGB565),

        ADD_GL_DEPTH_COPY_SAMPLED_FORMAT(GL_DEPTH_COMPONENT16),
        ADD_GL_DEPTH_COPY_SAMPLED_FORMAT(GL_DEPTH_COMPONENT24),

        ADD_GL_DEPTH_SAMPLED_FORMAT(GL_DEPTH_COMPONENT32F),
        ADD_GL_DEPTH_SAMPLED_FORMAT(GL_DEPTH24_STENCIL8),
        ADD_GL_DEPTH_SAMPLED_FORMAT(GL_DEPTH32F_STENCIL8),
        ADD_GL_DEPTH_SAMPLED_FORMAT(GL_STENCIL_INDEX8),

        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RED_RGTC1),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_SIGNED_RED_RGTC1),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RG_RGTC2),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_SIGNED_RG_RGTC2),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RGBA_BPTC_UNORM),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT),

        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_RGB8_ETC2),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SRGB8_ETC2),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_RGBA8_ETC2_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_R11_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SIGNED_R11_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_RG11_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SIGNED_RG11_EAC)
    };
#undef XRC_ALL_CREATE_FLAGS
#undef XRC_COLOR_UA_COPY_SAMPLED_MUTABLE_USAGE_FLAGS
#undef XRC_COLOR_UA_SAMPLED_MUTABLE_USAGE_FLAGS
#undef XRC_COLOR_COPY_SAMPLED_USAGE_FLAGS
#undef XRC_COLOR_COPY_SAMPLED_MUTABLE_USAGE_FLAGS
#undef XRC_COLOR_SAMPLED_USAGE_FLAGS
#undef XRC_DEPTH_COPY_SAMPLED_USAGE_FLAGS
#undef XRC_DEPTH_SAMPLED_USAGE_FLAGS
#undef XRC_COMPRESSED_SAMPLED_MUTABLE_USAGE_FLAGS
#undef XRC_COMPRESSED_SAMPLED_USAGE_FLAGS
#undef ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT2
#undef ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT
#undef ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT2
#undef ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT
#undef ADD_GL_COLOR_COPY_SAMPLED_FORMAT2
#undef ADD_GL_COLOR_COPY_SAMPLED_FORMAT
#undef ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT2
#undef ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT
#undef ADD_GL_COLOR_SAMPLED_FORMAT2
#undef ADD_GL_COLOR_SAMPLED_FORMAT
#undef ADD_GL_DEPTH_COPY_SAMPLED_FORMAT2
#undef ADD_GL_DEPTH_COPY_SAMPLED_FORMAT
#undef ADD_GL_DEPTH_SAMPLED_FORMAT2
#undef ADD_GL_DEPTH_SAMPLED_FORMAT
#undef ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT2
#undef ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT
#undef ADD_GL_COMPRESSED_SAMPLED_FORMAT2
#undef ADD_GL_COMPRESSED_SAMPLED_FORMAT
    // clang-format on

    std::string OpenGLGraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        SwapchainTestMap::const_iterator it = openGLSwapchainTestMap.find(imageFormat);

        if (it != openGLSwapchainTestMap.end()) {
            return it->second.imageFormatName;
        }

        return std::string("unknown");
    }

    bool OpenGLGraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        SwapchainTestMap::const_iterator it = openGLSwapchainTestMap.find(imageFormat);

        return (it != openGLSwapchainTestMap.end());
    }

    bool OpenGLGraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                                int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        // Swapchain image format support by the runtime is specified by the xrEnumerateSwapchainFormats function.
        // Runtimes should support R8G8B8A8 and R8G8B8A8 sRGB formats if possible.

        SwapchainTestMap::iterator it = openGLSwapchainTestMap.find(imageFormat);

        // Verify that the image format is known. If it's not known then this test needs to be
        // updated to recognize new OpenGL formats.
        CAPTURE(imageFormat);
        CHECK_MSG(it != openGLSwapchainTestMap.end(), "Unknown OpenGL image format.");
        if (it == openGLSwapchainTestMap.end()) {
            return false;
        }

        // We may now proceed with creating swapchains with the format.
        SwapchainCreateTestParameters& tp = it->second;
        tp.arrayCountVector = {1, 2};
        if (!tp.compressedFormat) {
            tp.mipCountVector = {1, 2};
        }
        else {
            tp.mipCountVector = {1};
        }

        *swapchainTestParameters = tp;
        return true;
    }

    bool OpenGLGraphicsPlugin::ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                                       uint32_t* imageCount) const
    {
        *imageCount = 0;  // Zero until set below upon success.

        std::vector<XrSwapchainImageOpenGLKHR> swapchainImageVector;
        uint32_t countOutput;

        XrResult result = xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr);
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput > 0);

        swapchainImageVector.resize(countOutput, XrSwapchainImageOpenGLKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, nullptr});

        // Exercise XR_ERROR_SIZE_INSUFFICIENT
        if (countOutput >= 2) {  // Need at least two in order to exercise XR_ERROR_SIZE_INSUFFICIENT
            result = xrEnumerateSwapchainImages(swapchain, 1, &countOutput,
                                                reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
            CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
            CHECK(result == XR_ERROR_SIZE_INSUFFICIENT);
            CHECK(countOutput == swapchainImageVector.size());
            // Contents of swapchainImageVector is undefined, so nothing to validate about the output.
        }

        countOutput = (uint32_t)swapchainImageVector.size();  // Restore countOutput if it was (mistakenly) modified.
        swapchainImageVector.clear();                         // Who knows what the runtime may have mistakely written into our vector.
        swapchainImageVector.resize(countOutput, XrSwapchainImageOpenGLKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, nullptr});
        result = xrEnumerateSwapchainImages(swapchain, countOutput, &countOutput,
                                            reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput == swapchainImageVector.size());
        REQUIRE(ValidateStructVectorType(swapchainImageVector, XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR));

        for (const XrSwapchainImageOpenGLKHR& image : swapchainImageVector) {
            CHECK(glGetError() == GL_NO_ERROR);

            CHECK(glIsTexture(image.image));
            CHECK(glGetError() == GL_NO_ERROR);

            CHECK(imageFormat == tp->expectedCreatedImageFormat);
        }

        *imageCount = countOutput;
        return true;
    }

    bool OpenGLGraphicsPlugin::ValidateSwapchainImageState(XrSwapchain /*swapchain*/, uint32_t /*index*/, int64_t /*imageFormat*/) const
    {
        // No resource state in OpenGL
        return true;
    }

    int64_t OpenGLGraphicsPlugin::SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const
    {
        // List of supported color swapchain formats.
        const std::array<GLenum, 5> f{GL_RGBA8, GL_SRGB8_ALPHA8, GL_RGBA16, GL_RGBA16F, GL_RGBA32F};

        const int64_t* formatArrayEnd = imageFormatArray + count;
        auto it = std::find_first_of(imageFormatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return imageFormatArray[0];
        }

        return *it;
    }

    int64_t OpenGLGraphicsPlugin::SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const
    {
        // List of supported depth swapchain formats.
        const std::array<GLenum, 5> f{GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F,
                                      GL_DEPTH_COMPONENT16};

        const int64_t* formatArrayEnd = imageFormatArray + count;
        auto it = std::find_first_of(imageFormatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return imageFormatArray[0];
        }

        return *it;
    }

    int64_t OpenGLGraphicsPlugin::GetRGBA8UnormFormat() const
    {
        return GL_RGBA8;
    }

    std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
    OpenGLGraphicsPlugin::AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& /*swapchainCreateInfo*/)
    {
        auto derivedResult = std::make_shared<OpenGLSwapchainImageStructs>();

        derivedResult->imageVector.resize(size, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, nullptr});

        for (XrSwapchainImageOpenGLKHR& image : derivedResult->imageVector) {
            derivedResult->imagePtrVector.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
        }

        // Cast our derived type to the caller-expected type.
        std::shared_ptr<SwapchainImageStructs> result =
            std::static_pointer_cast<SwapchainImageStructs, OpenGLSwapchainImageStructs>(derivedResult);

        return result;
    }

    void OpenGLGraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* /*colorSwapchainImage*/, uint32_t /*imageArrayIndex*/,
                                               int64_t /*colorSwapchainFormat*/)
    {
        IGRAPHICSPLUGIN_UNIMPLEMENTED_METHOD();
    }

    void OpenGLGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& /*layerView*/,
                                          const XrSwapchainImageBaseHeader* /*colorSwapchainImage*/, int64_t /*colorSwapchainFormat*/,
                                          const std::vector<Cube>& /*cubes*/)
    {
        IGRAPHICSPLUGIN_UNIMPLEMENTED_METHOD();
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_OPENGL
