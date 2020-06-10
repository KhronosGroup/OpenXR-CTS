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

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES

#include "swapchain_parameters.h"

#include "xr_dependencies.h"

#include <GL/gl_format.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "conformance_framework.h"
#include <catch2/catch.hpp>

#include "gfxwrapper_opengl.h"

#define OVR_LOG_TAG "OpenXR_Conformance_Suite"
#define ALOGE(...) Conformance::ReportF(__VA_ARGS__);  /// __android_log_print( ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__ )
#define ALOGV(...) ;  /// Conformance::ReportF(__VA_ARGS__);  /// __android_log_print( ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__ )

namespace Conformance
{

    struct OpenGLESGraphicsPlugin : public IGraphicsPlugin
    {
        OpenGLESGraphicsPlugin(std::shared_ptr<IPlatformPlugin>& /*unused*/);
        ~OpenGLESGraphicsPlugin() override;

        OpenGLESGraphicsPlugin(const OpenGLESGraphicsPlugin&) = delete;
        OpenGLESGraphicsPlugin& operator=(const OpenGLESGraphicsPlugin&) = delete;
        OpenGLESGraphicsPlugin(OpenGLESGraphicsPlugin&&) = delete;
        OpenGLESGraphicsPlugin& operator=(OpenGLESGraphicsPlugin&&) = delete;

        bool Initialize() override;

        bool IsInitialized() const override;

        void Shutdown() override;

        std::string DescribeGraphics() const override;

        std::vector<std::string> GetInstanceExtensions() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, int64_t imageFormat, uint32_t arraySlice,
                           const RGBAImage& image) override;

        std::string GetImageFormatName(int64_t imageFormat) const override;

        bool IsImageFormatKnown(int64_t imageFormat) const override;

        bool GetSwapchainCreateTestParameters(XrInstance instance, XrSession session, XrSystemId systemId, int64_t imageFormat,
                                              SwapchainCreateTestParameters* swapchainTestParameters) override;

        virtual bool ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                             uint32_t* imageCount) const override;
        bool ValidateSwapchainImageState(XrSwapchain swapchain, uint32_t index, int64_t imageFormat) const override;

        int64_t SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t GetRGBA8UnormFormat() const override;

        std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
        AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                             int64_t colorSwapchainFormat) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        int64_t colorSwapchainFormat, const std::vector<Cube>& cubes) override;

    protected:
        struct OpenGLESSwapchainImageStructs : public IGraphicsPlugin::SwapchainImageStructs
        {
            std::vector<XrSwapchainImageOpenGLESKHR> imageVector;
        };

    private:
        bool initialized{false};

        void deleteGLContext();

        XrVersion OpenGLESVersionOfContext = 0;

        bool deviceInitialized{false};

        ksGpuWindow window{};

        XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    };

    OpenGLESGraphicsPlugin::OpenGLESGraphicsPlugin(std::shared_ptr<IPlatformPlugin>& /*unused*/) : initialized(false)
    {
    }

    OpenGLESGraphicsPlugin::~OpenGLESGraphicsPlugin()
    {
        ShutdownDevice();
        Shutdown();
    }

    bool OpenGLESGraphicsPlugin::Initialize()
    {
        if (initialized) {
            return false;
        }

        initialized = true;
        return initialized;
    }

    bool OpenGLESGraphicsPlugin::IsInitialized() const
    {
        return initialized;
    }

    void OpenGLESGraphicsPlugin::Shutdown()
    {
        if (initialized) {
            initialized = false;
        }
    }

    std::string OpenGLESGraphicsPlugin::DescribeGraphics() const
    {
        return std::string("OpenGLES");
    }

    std::vector<std::string> OpenGLESGraphicsPlugin::GetInstanceExtensions() const
    {
        return {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME, XR_KHR_ANDROID_SURFACE_SWAPCHAIN_EXTENSION_NAME};
    }

    const XrBaseInStructure* OpenGLESGraphicsPlugin::GetGraphicsBinding() const
    {
        if (deviceInitialized) {
            return reinterpret_cast<const XrBaseInStructure*>(&graphicsBinding);
        }
        return nullptr;
    }

    void OpenGLESGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* /*swapchainImage*/, int64_t /*imageFormat*/,
                                               uint32_t /*arraySlice*/, const RGBAImage& /*image*/)
    {
        //IGRAPHICSPLUGIN_UNIMPLEMENTED_METHOD();
    }

    void OpenGLESGraphicsPlugin::deleteGLContext()
    {
        if (deviceInitialized) {
            ksGpuWindow_Destroy(&window);
        }

        deviceInitialized = false;
    }

    bool OpenGLESGraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                                                  uint32_t /*deviceCreationFlags*/)
    {
        XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR, nullptr,
                                                                  XR_MAKE_VERSION(3, 1, 0), XR_MAKE_VERSION(3, 2, 0)};

        // Get the graphics requirements.
        if (checkGraphicsRequirements) {

            auto xrGetOpenGLESGraphicsRequirementsKHR =
                GetInstanceExtensionFunction<PFN_xrGetOpenGLESGraphicsRequirementsKHR>(instance, "xrGetOpenGLESGraphicsRequirementsKHR");

            XrResult result = xrGetOpenGLESGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements);
            CHECK(ValidateResultAllowed("xrGetOpenGLESGraphicsRequirementsKHR", result));
            if (XR_FAILED(result)) {
                // Log result?
                return false;
            }
        }

        if (deviceInitialized == true) {
            // a context exists, this function has been called before!
            if (OpenGLESVersionOfContext >= graphicsRequirements.minApiVersionSupported) {
                // no test for max version as using a higher (compatible) version is allowed!
                return true;
            }

            // delete the context to make a new one:
            deleteGLContext();
        }

        ksDriverInstance driverInstance{};
        ksGpuQueueInfo queueInfo{};
        ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
        ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
        ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
        if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat, depthFormat, sampleCount, 640, 480, false)) {
            throw std::runtime_error("Unable to create GL context");
        }

        // Initialize the binding once we have a context
        {
            REQUIRE(window.display != EGL_NO_DISPLAY);
            REQUIRE(window.context.context != EGL_NO_CONTEXT);
            graphicsBinding = {};
            graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
            graphicsBinding.next = nullptr;
            graphicsBinding.display = window.display;
            graphicsBinding.config = (EGLConfig)0;
            graphicsBinding.context = window.context.context;
        }

        GLenum error = glGetError();
        CHECK(error == GL_NO_ERROR);

        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        error = glGetError();
        if (error != GL_NO_ERROR) {
            deleteGLContext();
            return false;
        }

        OpenGLESVersionOfContext = XR_MAKE_VERSION(major, minor, 0);
        if (OpenGLESVersionOfContext < graphicsRequirements.minApiVersionSupported) {
            // OpenGL version of the conformance tests is lower than what the runtime requests -> can not be tested

            deleteGLContext();
            return false;
        }

        deviceInitialized = true;
        return true;
    }

    void OpenGLESGraphicsPlugin::ShutdownDevice()
    {
        deleteGLContext();
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGLES(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<OpenGLESGraphicsPlugin>(platformPlugin);
    }

    // Shorthand constants for usage below.
    static const uint64_t XRC_COLOR_TEXTURE_USAGE = (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT);

    static const uint64_t XRC_COLOR_TEXTURE_USAGE_MUTABLE = (XRC_COLOR_TEXTURE_USAGE | XRC_COLOR_TEXTURE_USAGE_MUTABLE);

    static const uint64_t XRC_COLOR_TEXTURE_USAGE_COMPRESSED =
        (XR_SWAPCHAIN_USAGE_SAMPLED_BIT);  // Compressed textures can't be rendered to, so no XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT.

    static const uint64_t XRC_DEPTH_TEXTURE_USAGE = (XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT);

#define XRC_COLOR_CREATE_FLAGS                                                             \
    {                                                                                      \
        0, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT \
    }

#define XRC_DEPTH_CREATE_FLAGS                                                             \
    {                                                                                      \
        0, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT \
    }

// one for texture formats which are not known to the GL.h (where a more modern header would be needed):
#define ADD_GL_COLOR_FORMAT2(X, Y)                                                                                                       \
    {                                                                                                                                    \
        {X},                                                                                                                             \
        {                                                                                                                                \
            Y, false, false, true, false, X, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, \
            {                                                                                                                            \
            }                                                                                                                            \
        }                                                                                                                                \
    }
#define ADD_GL_COLOR_FORMAT(X) ADD_GL_COLOR_FORMAT2(X, #X)

#define ADD_GL_COLOR_COMPRESSED_FORMAT2(X, Y)                                                                      \
    {                                                                                                              \
        {X},                                                                                                       \
        {                                                                                                          \
            Y, false, false, true, false, X, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, \
            {                                                                                                      \
            }                                                                                                      \
        }                                                                                                          \
    }
#define ADD_GL_COLOR_COMPRESSED_FORMAT(X) ADD_GL_COLOR_COMPRESSED_FORMAT2(X, #X)

#define ADD_GL_DEPTH_FORMAT2(X, Y)                                                                       \
    {                                                                                                    \
        {X},                                                                                             \
        {                                                                                                \
            Y, false, false, false, false, X, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, \
            {                                                                                            \
            }                                                                                            \
        }                                                                                                \
    }
#define ADD_GL_DEPTH_FORMAT(X) ADD_GL_DEPTH_FORMAT2(X, #X)

    typedef std::map<int64_t, SwapchainCreateTestParameters> SwapchainTestMap;
    SwapchainTestMap openGLESSwapchainTestMap{
        //{ {type}, { name, false (typeless), color, type, flags, flagV, arrayV, sampleV, mipV} },

        //
        // 8 bits per component
        //
        ADD_GL_COLOR_FORMAT(GL_R8),            // 1-component, 8-bit unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RG8),           // 2-component, 8-bit unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGB8),          // 3-component, 8-bit unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGBA8),         // 4-component, 8-bit unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_R8_SNORM),      // 1-component, 8-bit signed normalized
        ADD_GL_COLOR_FORMAT(GL_RG8_SNORM),     // 2-component, 8-bit signed normalized
        ADD_GL_COLOR_FORMAT(GL_RGB8_SNORM),    // 3-component, 8-bit signed normalized
        ADD_GL_COLOR_FORMAT(GL_RGBA8_SNORM),   // 4-component, 8-bit signed normalized
        ADD_GL_COLOR_FORMAT(GL_R8UI),          // 1-component, 8-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RG8UI),         // 2-component, 8-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RGB8UI),        // 3-component, 8-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RGBA8UI),       // 4-component, 8-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_R8I),           // 1-component, 8-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RG8I),          // 2-component, 8-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RGB8I),         // 3-component, 8-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RGBA8I),        // 4-component, 8-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_SR8),           // 1-component, 8-bit sRGB
        ADD_GL_COLOR_FORMAT(GL_SRG8),          // 2-component, 8-bit sRGB
        ADD_GL_COLOR_FORMAT(GL_SRGB8),         // 3-component, 8-bit sRGB
        ADD_GL_COLOR_FORMAT(GL_SRGB8_ALPHA8),  // 4-component, 8-bit sRGB

        //
        // 16 bits per component
        //
        ADD_GL_COLOR_FORMAT(GL_R16),           // 1-component, 16-bit unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RG16),          // 2-component, 16-bit unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGB16),         // 3-component, 16-bit unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGBA16),        // 4-component, 16-bit unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_R16_SNORM),     // 1-component, 16-bit signed normalized
        ADD_GL_COLOR_FORMAT(GL_RG16_SNORM),    // 2-component, 16-bit signed normalized
        ADD_GL_COLOR_FORMAT(GL_RGB16_SNORM),   // 3-component, 16-bit signed normalized
        ADD_GL_COLOR_FORMAT(GL_RGBA16_SNORM),  // 4-component, 16-bit signed normalized
        ADD_GL_COLOR_FORMAT(GL_R16UI),         // 1-component, 16-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RG16UI),        // 2-component, 16-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RGB16UI),       // 3-component, 16-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RGBA16UI),      // 4-component, 16-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_R16I),          // 1-component, 16-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RG16I),         // 2-component, 16-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RGB16I),        // 3-component, 16-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RGBA16I),       // 4-component, 16-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_R16F),          // 1-component, 16-bit floating-point
        ADD_GL_COLOR_FORMAT(GL_RG16F),         // 2-component, 16-bit floating-point
        ADD_GL_COLOR_FORMAT(GL_RGB16F),        // 3-component, 16-bit floating-point
        ADD_GL_COLOR_FORMAT(GL_RGBA16F),       // 4-component, 16-bit floating-point

        //
        // 32 bits per component
        //
        ADD_GL_COLOR_FORMAT(GL_R32UI),     // 1-component, 32-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RG32UI),    // 2-component, 32-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RGB32UI),   // 3-component, 32-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_RGBA32UI),  // 4-component, 32-bit unsigned integer
        ADD_GL_COLOR_FORMAT(GL_R32I),      // 1-component, 32-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RG32I),     // 2-component, 32-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RGB32I),    // 3-component, 32-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_RGBA32I),   // 4-component, 32-bit signed integer
        ADD_GL_COLOR_FORMAT(GL_R32F),      // 1-component, 32-bit floating-point
        ADD_GL_COLOR_FORMAT(GL_RG32F),     // 2-component, 32-bit floating-point
        ADD_GL_COLOR_FORMAT(GL_RGB32F),    // 3-component, 32-bit floating-point
        ADD_GL_COLOR_FORMAT(GL_RGBA32F),   // 4-component, 32-bit floating-point

        //
        // Packed
        //
        ADD_GL_COLOR_FORMAT(GL_RGB5),            // 3-component 5:5:5,       unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGB565),          // 3-component 5:6:5,       unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGB10),           // 3-component 10:10:10,    unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGBA4),           // 4-component 4:4:4:4,     unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGB5_A1),         // 4-component 5:5:5:1,     unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGB10_A2),        // 4-component 10:10:10:2,  unsigned normalized
        ADD_GL_COLOR_FORMAT(GL_RGB10_A2UI),      // 4-component 10:10:10:2,  unsigned integer
        ADD_GL_COLOR_FORMAT(GL_R11F_G11F_B10F),  // 3-component 11:11:10,    floating-point
        ADD_GL_COLOR_FORMAT(GL_RGB9_E5),         // 3-component/exp 9:9:9/5, floating-point

        //
        // S3TC/DXT/BC
        //
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGB_S3TC_DXT1_EXT),  // line through 3D space, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(
            GL_COMPRESSED_RGBA_S3TC_DXT1_EXT),  // line through 3D space plus 1-bit alpha, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT),  // line through 3D space plus line through 1D space, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(
            GL_COMPRESSED_RGBA_S3TC_DXT3_EXT),  // line through 3D space plus 4-bit alpha, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB_S3TC_DXT1_EXT),        // line through 3D space, 4x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT),  // line through 3D space plus 1-bit alpha, 4x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(
            GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT),  // line through 3D space plus line through 1D space, 4x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT),  // line through 3D space plus 4-bit alpha, 4x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_LUMINANCE_LATC1_EXT),       // line through 1D space, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(
            GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT),  // two lines through 1D space, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT),  // line through 1D space, 4x4 blocks, signed normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(
            GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT),             // two lines through 1D space, 4x4 blocks, signed normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RED_RGTC1),         // line through 1D space, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RG_RGTC2),          // two lines through 1D space, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SIGNED_RED_RGTC1),  // line through 1D space, 4x4 blocks, signed normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SIGNED_RG_RGTC2),   // two lines through 1D space, 4x4 blocks, signed normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT),  // 3-component, 4x4 blocks, unsigned floating-point
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT),    // 3-component, 4x4 blocks, signed floating-point
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_BPTC_UNORM),          // 4-component, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM),    // 4-component, 4x4 blocks, sRGB

        //
        // ETC
        //
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_ETC1_RGB8_OES),         // 3-component ETC1, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGB8_ETC2),  // 3-component ETC2, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(
            GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2),              // 4-component ETC2 with 1-bit alpha, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA8_ETC2_EAC),  // 4-component ETC2, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ETC2),      // 3-component ETC2, 4x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(
            GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2),                    // 4-component ETC2 with 1-bit alpha, 4x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC),  // 4-component ETC2, 4x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_R11_EAC),                // 1-component ETC, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RG11_EAC),               // 2-component ETC, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SIGNED_R11_EAC),         // 1-component ETC, 4x4 blocks, signed normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SIGNED_RG11_EAC),        // 2-component ETC, 4x4 blocks, signed normalized

        //
        // ASTC
        //
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_4x4_KHR),            // 4-component ASTC, 4x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_5x4_KHR),            // 4-component ASTC, 5x4 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_5x5_KHR),            // 4-component ASTC, 5x5 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_6x5_KHR),            // 4-component ASTC, 6x5 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_6x6_KHR),            // 4-component ASTC, 6x6 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_8x5_KHR),            // 4-component ASTC, 8x5 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_8x6_KHR),            // 4-component ASTC, 8x6 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_8x8_KHR),            // 4-component ASTC, 8x8 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_10x5_KHR),           // 4-component ASTC, 10x5 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_10x6_KHR),           // 4-component ASTC, 10x6 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_10x8_KHR),           // 4-component ASTC, 10x8 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_10x10_KHR),          // 4-component ASTC, 10x10 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_12x10_KHR),          // 4-component ASTC, 12x10 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_ASTC_12x12_KHR),          // 4-component ASTC, 12x12 blocks, unsigned normalized
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR),    // 4-component ASTC, 4x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR),    // 4-component ASTC, 5x4 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR),    // 4-component ASTC, 5x5 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR),    // 4-component ASTC, 6x5 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR),    // 4-component ASTC, 6x6 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR),    // 4-component ASTC, 8x5 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR),    // 4-component ASTC, 8x6 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR),    // 4-component ASTC, 8x8 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR),   // 4-component ASTC, 10x5 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR),   // 4-component ASTC, 10x6 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR),   // 4-component ASTC, 10x8 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR),  // 4-component ASTC, 10x10 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR),  // 4-component ASTC, 12x10 blocks, sRGB
        ADD_GL_COLOR_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR),  // 4-component ASTC, 12x12 blocks, sRGB

        //
        // Depth/stencil
        //
        ADD_GL_DEPTH_FORMAT(GL_DEPTH_COMPONENT16), ADD_GL_DEPTH_FORMAT(GL_DEPTH_COMPONENT24), ADD_GL_DEPTH_FORMAT(GL_DEPTH_COMPONENT32F),
        ADD_GL_DEPTH_FORMAT(GL_DEPTH_COMPONENT32F_NV), ADD_GL_DEPTH_FORMAT(GL_STENCIL_INDEX8), ADD_GL_DEPTH_FORMAT(GL_DEPTH24_STENCIL8),
        ADD_GL_DEPTH_FORMAT(GL_DEPTH32F_STENCIL8), ADD_GL_DEPTH_FORMAT(GL_DEPTH32F_STENCIL8_NV)};
#undef ADD_GL_COLOR_FORMAT
#undef ADD_GL_COLOR_FORMAT2
#undef ADD_GL_DEPTH_FORMAT
#undef ADD_GL_DEPTH_FORMAT2

    // Returns a name for an image format.
    std::string OpenGLESGraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        SwapchainTestMap::const_iterator it = openGLESSwapchainTestMap.find(imageFormat);

        if (it != openGLESSwapchainTestMap.end()) {
            return it->second.imageFormatName;
        }

        return std::string("unknown");
    }

    bool OpenGLESGraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        SwapchainTestMap::const_iterator it = openGLESSwapchainTestMap.find(imageFormat);

        return (it != openGLESSwapchainTestMap.end());
    }

    // Retrieves SwapchainCreateTestParameters for the caller, handling plaform-specific functionality
    // internally.
    // Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
    bool OpenGLESGraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                                  int64_t imageFormat,
                                                                  SwapchainCreateTestParameters* swapchainTestParameters) noexcept(false)
    {
        // Swapchain image format support by the runtime is specified by the xrEnumerateSwapchainFormats function.
        // Runtimes should support R8G8B8A8 and R8G8B8A8 sRGB formats if possible.

        SwapchainTestMap::iterator it = openGLESSwapchainTestMap.find(imageFormat);

        // Verify that the image format is known. If it's not known then this test needs to be
        // updated to recognize new OpenGL formats.
        CAPTURE(imageFormat);
        CHECK_MSG(it != openGLESSwapchainTestMap.end(), "Unknown OpenGLES image format.");
        if (it == openGLESSwapchainTestMap.end()) {
            return false;
        }

        CAPTURE(it->second.imageFormatName);

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

    // Given an imageFormat and its test parameters and the XrSwapchain resulting from xrCreateSwapchain,
    // validate the images in any platform-specific way.
    // Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
    bool OpenGLESGraphicsPlugin::ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp,
                                                         XrSwapchain swapchain, uint32_t* imageCount) const noexcept(false)
    {
        *imageCount = 0;  // Zero until set below upon success.

        std::vector<XrSwapchainImageOpenGLESKHR> swapchainImageVector;
        uint32_t countOutput;

        XrResult result = xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr);
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput > 0);

        swapchainImageVector.resize(countOutput, XrSwapchainImageOpenGLESKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR, nullptr});

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
        swapchainImageVector.resize(countOutput, XrSwapchainImageOpenGLESKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
        result = xrEnumerateSwapchainImages(swapchain, countOutput, &countOutput,
                                            reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput == swapchainImageVector.size());
        REQUIRE(ValidateStructVectorType(swapchainImageVector, XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR));

        for (const XrSwapchainImageOpenGLESKHR& image : swapchainImageVector) {
            CHECK(glGetError() == GL_NO_ERROR);

            CHECK(glIsTexture(image.image));
            CHECK(glGetError() == GL_NO_ERROR);

            CHECK(imageFormat == tp->expectedCreatedImageFormat);
        }

        *imageCount = countOutput;
        return true;
    }

    bool OpenGLESGraphicsPlugin::ValidateSwapchainImageState(XrSwapchain /*swapchain*/, uint32_t /*index*/, int64_t /*imageFormat*/) const
    {
        // No resource state in OpenGLES
        return true;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t OpenGLESGraphicsPlugin::SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const
    {
        // List of supported color swapchain formats.
        const std::array<GLenum, 2> f{GL_RGBA8, GL_SRGB8_ALPHA8};

        const int64_t* formatArrayEnd = imageFormatArray + count;
        auto it = std::find_first_of(imageFormatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return imageFormatArray[0];
        }

        return *it;
    }

    int64_t OpenGLESGraphicsPlugin::SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const
    {
        // List of supported depth swapchain formats.
        const std::array<GLenum, 4> f{GL_DEPTH24_STENCIL8, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT32F};

        const int64_t* formatArrayEnd = imageFormatArray + count;
        auto it = std::find_first_of(imageFormatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return imageFormatArray[0];
        }

        return *it;
    }

    int64_t OpenGLESGraphicsPlugin::GetRGBA8UnormFormat() const
    {
        return GL_RGBA8;
    }

    std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
    OpenGLESGraphicsPlugin::AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& /*swapchainCreateInfo*/)
    {
        auto derivedResult = std::make_shared<OpenGLESSwapchainImageStructs>();

        derivedResult->imageVector.resize(size, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR, nullptr});

        for (XrSwapchainImageOpenGLESKHR& image : derivedResult->imageVector) {
            derivedResult->imagePtrVector.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
        }

        // Cast our derived type to the caller-expected type.
        std::shared_ptr<SwapchainImageStructs> result =
            std::static_pointer_cast<SwapchainImageStructs, OpenGLESSwapchainImageStructs>(derivedResult);

        return result;
    }

    void OpenGLESGraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* /*colorSwapchainImage*/, uint32_t /*imageArrayIndex*/,
                                                 int64_t /*colorSwapchainFormat*/)
    {
        IGRAPHICSPLUGIN_UNIMPLEMENTED_METHOD();
    }

    void OpenGLESGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& /*layerView*/,
                                            const XrSwapchainImageBaseHeader* /*colorSwapchainImage*/, int64_t /*colorSwapchainFormat*/,
                                            const std::vector<Cube>& /*cubes*/)
    {
        IGRAPHICSPLUGIN_UNIMPLEMENTED_METHOD();
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_OPENGL_ES
