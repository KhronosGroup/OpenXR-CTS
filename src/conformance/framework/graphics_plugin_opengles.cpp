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

#include "graphics_plugin.h"

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES

#include "swapchain_parameters.h"
#include "conformance_framework.h"
#include "Geometry.h"
#include "common/gfxwrapper_opengl.h"
#include <common/xr_linear.h>
#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <list>
#include <catch2/catch.hpp>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "report.h"

#define GL(glcmd)                                                                         \
    {                                                                                     \
        GLint err = glGetError();                                                         \
        if (err != GL_NO_ERROR) {                                                         \
            ReportF("GLES error=%d, %s:%d", err, __FUNCTION__, __LINE__);                 \
        }                                                                                 \
        glcmd;                                                                            \
        err = glGetError();                                                               \
        if (err != GL_NO_ERROR) {                                                         \
            ReportF("GLES error=%d, cmd=%s, %s:%d", err, #glcmd, __FUNCTION__, __LINE__); \
        }                                                                                 \
    }

namespace Conformance
{
    constexpr float DarkSlateGray[] = {0.184313729f, 0.309803933f, 0.309803933f, 1.0f};

    static const char* VertexShaderGlsl = R"_(
    #version 320 es

    in vec3 VertexPos;
    in vec3 VertexColor;

    out vec3 PSVertexColor;

    uniform mat4 ModelViewProjection;

    void main() {
       gl_Position = ModelViewProjection * vec4(VertexPos, 1.0);
       PSVertexColor = VertexColor;
    }
    )_";

    static const char* FragmentShaderGlsl = R"_(
    #version 320 es

    in lowp vec3 PSVertexColor;
    out lowp vec4 FragColor;

    void main() {
       FragColor = vec4(PSVertexColor, 1);
    }
    )_";

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

        void Flush() override;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

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

        int64_t GetRGBA8Format(bool sRGB) const override;

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

        void InitializeResources();
        void ShutdownResources();
        uint32_t GetDepthTexture(const XrSwapchainImageBaseHeader* colorSwapchainImage);
        XrVersion OpenGLESVersionOfContext = 0;

        bool deviceInitialized{false};

        ksGpuWindow window{};

        XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};

        GLuint m_swapchainFramebuffer{0};
        GLuint m_program{0};
        GLint m_modelViewProjectionUniformLocation{0};
        GLint m_vertexAttribCoords{0};
        GLint m_vertexAttribColor{0};
        GLuint m_vao{0};
        GLuint m_cubeVertexBuffer{0};
        GLuint m_cubeIndexBuffer{0};

        // The OpenGLES interface uses a standard 2D target type when
        // arraySize == 1, so we need this info in some situations where
        // we are only provided the XrSwapchainImageBaseHeader *.
        // Since this plugin allocates that memory, we can make the
        // association.
        // We could also change the interface to the plugin to provide
        // this info, but it feels like a GL-specific quirk that is better
        // hidden if possible.
        struct SwapchainInfo
        {
            XrSwapchainCreateInfo createInfo;
            int size;  // swapchain length is not known until after creation
        };
        std::vector<SwapchainInfo> m_swapchainInfo;
        struct ImageInfo
        {
            int swapchainIndex;  // index into m_swapchainInfo
            int imageIndex;      // image index into swapchain
        };
        std::unordered_map<const XrSwapchainImageBaseHeader*, ImageInfo> m_imageInfo;

        // Map color buffer to associated depth buffer. This map is populated on demand.
        std::unordered_map<const XrSwapchainImageBaseHeader*, uint32_t> m_colorToDepthMap;
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

    void OpenGLESGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, int64_t /* imageFormat */,
                                               uint32_t arraySlice, const RGBAImage& image)
    {
        auto imageInfoIt = m_imageInfo.find(swapchainImage);
        CHECK(imageInfoIt != m_imageInfo.end());

        SwapchainInfo& swapchainInfo = m_swapchainInfo[imageInfoIt->second.swapchainIndex];
        GLuint arraySize = swapchainInfo.createInfo.arraySize;
        bool isArray = arraySize > 1;
        GLuint width = image.width;
        GLuint height = image.height;
        GLenum target = isArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

        const uint32_t img = reinterpret_cast<const XrSwapchainImageOpenGLESKHR*>(swapchainImage)->image;
        GL(glBindTexture(target, img));
        if (isArray) {
            for (GLuint y = 0; y < height; ++y) {
                const void* pixels = &image.pixels[(height - 1 - y) * width];
                GL(glTexSubImage3D(target, 0, 0, y, arraySlice, width, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
            }
        }
        else {
            for (GLuint y = 0; y < height; ++y) {
                const void* pixels = &image.pixels[(height - 1 - y) * width];
                GL(glTexSubImage2D(target, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
            }
        }
        GL(glBindTexture(target, 0));
    }

    void OpenGLESGraphicsPlugin::Flush()
    {
        GL(glFlush());
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

            // delete the context and resources to make a new one:
            ShutdownResources();
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
        GL(glGetIntegerv(GL_MAJOR_VERSION, &major));
        GL(glGetIntegerv(GL_MINOR_VERSION, &minor));
        error = glGetError();
        if (error != GL_NO_ERROR) {
            ShutdownResources();
            return false;
        }

        OpenGLESVersionOfContext = XR_MAKE_VERSION(major, minor, 0);
        if (OpenGLESVersionOfContext < graphicsRequirements.minApiVersionSupported) {
            // OpenGL version of the conformance tests is lower than what the runtime requests -> can not be tested

            ShutdownResources();
            return false;
        }

        //ReportF("Initializing GLES plugin resources");
        InitializeResources();

        deviceInitialized = true;
        return true;
    }

    void OpenGLESGraphicsPlugin::ShutdownDevice()
    {
        ShutdownResources();
    }

    static void CheckShader(GLuint shader)
    {
        GLint r = 0;
        GL(glGetShaderiv(shader, GL_COMPILE_STATUS, &r));
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            GL(glGetShaderInfoLog(shader, sizeof(msg), &length, msg));

            throw std::runtime_error((std::string("Compile shader failed: ") + msg).c_str());
        }
    }

    static void CheckProgram(GLuint prog)
    {
        GLint r = 0;
        GL(glGetProgramiv(prog, GL_LINK_STATUS, &r));
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            GL(glGetProgramInfoLog(prog, sizeof(msg), &length, msg));
            throw std::runtime_error((std::string("Link program failed: ") + msg).c_str());
        }
    }

    void OpenGLESGraphicsPlugin::InitializeResources()
    {
        //ReportF("OpenGLESGraphicsPlugin::InitializeResources");
        GL(glGenFramebuffers(1, &m_swapchainFramebuffer));

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        GL(glShaderSource(vertexShader, 1, &VertexShaderGlsl, nullptr));
        GL(glCompileShader(vertexShader));
        CheckShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        GL(glShaderSource(fragmentShader, 1, &FragmentShaderGlsl, nullptr));
        GL(glCompileShader(fragmentShader));
        CheckShader(fragmentShader);

        m_program = glCreateProgram();
        GL(glAttachShader(m_program, vertexShader));
        GL(glAttachShader(m_program, fragmentShader));
        GL(glLinkProgram(m_program));
        CheckProgram(m_program);

        GL(glDeleteShader(vertexShader));
        GL(glDeleteShader(fragmentShader));

        m_modelViewProjectionUniformLocation = glGetUniformLocation(m_program, "ModelViewProjection");

        m_vertexAttribCoords = glGetAttribLocation(m_program, "VertexPos");
        m_vertexAttribColor = glGetAttribLocation(m_program, "VertexColor");

        GL(glGenBuffers(1, &m_cubeVertexBuffer));
        GL(glBindBuffer(GL_ARRAY_BUFFER, m_cubeVertexBuffer));
        GL(glBufferData(GL_ARRAY_BUFFER, sizeof(Geometry::c_cubeVertices), Geometry::c_cubeVertices.data(), GL_STATIC_DRAW));

        GL(glGenBuffers(1, &m_cubeIndexBuffer));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeIndexBuffer));
        GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Geometry::c_cubeIndices), Geometry::c_cubeIndices.data(), GL_STATIC_DRAW));

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glEnableVertexAttribArray(m_vertexAttribCoords);
        glEnableVertexAttribArray(m_vertexAttribColor);
        glBindBuffer(GL_ARRAY_BUFFER, m_cubeVertexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeIndexBuffer);
        glVertexAttribPointer(m_vertexAttribCoords, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex), nullptr);
        glVertexAttribPointer(m_vertexAttribColor, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex),
                              reinterpret_cast<const void*>(sizeof(XrVector3f)));
    }

    void OpenGLESGraphicsPlugin::ShutdownResources()
    {
        if (deviceInitialized) {
            if (m_swapchainFramebuffer != 0) {
                GL(glDeleteFramebuffers(1, &m_swapchainFramebuffer));
            }
            if (m_program != 0) {
                GL(glDeleteProgram(m_program));
            }
            if (m_vao != 0) {
                GL(glDeleteVertexArrays(1, &m_vao));
            }
            if (m_cubeVertexBuffer != 0) {
                GL(glDeleteBuffers(1, &m_cubeVertexBuffer));
            }
            if (m_cubeIndexBuffer != 0) {
                GL(glDeleteBuffers(1, &m_cubeIndexBuffer));
            }

            for (auto& colorToDepth : m_colorToDepthMap) {
                if (colorToDepth.second != 0) {
                    GL(glDeleteTextures(1, &colorToDepth.second));
                }
            }

            ksGpuWindow_Destroy(&window);
        }
        deviceInitialized = false;
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGLES(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<OpenGLESGraphicsPlugin>(platformPlugin);
    }

    // Shorthand constants for usage below.
    static const uint64_t XRC_COLOR_TEXTURE_USAGE = (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT);

    static const uint64_t XRC_COLOR_TEXTURE_USAGE_MUTABLE = (XRC_COLOR_TEXTURE_USAGE | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT);

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

#define ADD_GL_COLOR_COMPRESSED_FORMAT2(X, Y)                                                                     \
    {                                                                                                             \
        {X},                                                                                                      \
        {                                                                                                         \
            Y, false, false, true, true, X, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, \
            {                                                                                                     \
            }                                                                                                     \
        }                                                                                                         \
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
                                                         XrSwapchain swapchain, uint32_t* imageCount) const
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

    int64_t OpenGLESGraphicsPlugin::GetRGBA8Format(bool sRGB) const
    {
        return sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    }

    std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
    OpenGLESGraphicsPlugin::AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {
        auto derivedResult = std::make_shared<OpenGLESSwapchainImageStructs>();

        derivedResult->imageVector.resize(size, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR, nullptr});

        SwapchainInfo si;
        si.createInfo = swapchainCreateInfo;
        si.createInfo.next = nullptr;
        si.size = static_cast<int>(size);
        m_swapchainInfo.push_back(si);

        ImageInfo ii;
        ii.swapchainIndex = static_cast<int>(m_swapchainInfo.size() - 1);

        for (size_t i = 0; i < size; i++) {
            XrSwapchainImageOpenGLESKHR& image = derivedResult->imageVector[i];
            auto imageBaseHeader = reinterpret_cast<XrSwapchainImageBaseHeader*>(&image);
            derivedResult->imagePtrVector.push_back(imageBaseHeader);
            ii.imageIndex = static_cast<int>(i);
            m_imageInfo[imageBaseHeader] = ii;
        }

        // Cast our derived type to the caller-expected type.
        std::shared_ptr<SwapchainImageStructs> result =
            std::static_pointer_cast<SwapchainImageStructs, OpenGLESSwapchainImageStructs>(derivedResult);

        return result;
    }

    void OpenGLESGraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                                 int64_t /*colorSwapchainFormat*/)
    {
        auto imageInfoIt = m_imageInfo.find(colorSwapchainImage);
        CHECK(imageInfoIt != m_imageInfo.end());

        SwapchainInfo& swapchainInfo = m_swapchainInfo[imageInfoIt->second.swapchainIndex];
        GLuint arraySize = swapchainInfo.createInfo.arraySize;
        bool isArray = arraySize > 1;
        GLenum target = isArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

        GL(glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer));

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLESKHR*>(colorSwapchainImage)->image;
        const uint32_t depthTexture = GetDepthTexture(colorSwapchainImage);
        if (isArray) {
            GL(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0, imageArrayIndex));
            GL(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0, imageArrayIndex));
        }
        else {
            GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, colorTexture, 0));
            GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, depthTexture, 0));
        }

        GLint x = 0;
        GLint y = 0;
        GLsizei w = swapchainInfo.createInfo.width;
        GLsizei h = swapchainInfo.createInfo.height;
        GL(glViewport(x, y, w, h));
        GL(glScissor(x, y, w, h));

        GL(glEnable(GL_SCISSOR_TEST));

        // Clear swapchain and depth buffer.
        GL(glClearColor(DarkSlateGray[0], DarkSlateGray[1], DarkSlateGray[2], DarkSlateGray[3]));
        GL(glClearDepthf(1.0f));
        GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    uint32_t OpenGLESGraphicsPlugin::GetDepthTexture(const XrSwapchainImageBaseHeader* colorSwapchainImage)
    {
        auto imageInfoIt = m_imageInfo.find(colorSwapchainImage);
        CHECK(imageInfoIt != m_imageInfo.end());

        SwapchainInfo& swapchainInfo = m_swapchainInfo[imageInfoIt->second.swapchainIndex];
        GLuint arraySize = swapchainInfo.createInfo.arraySize;
        bool isArray = arraySize > 1;
        GLuint width = swapchainInfo.createInfo.width;
        GLuint height = swapchainInfo.createInfo.height;
        GLenum target = isArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLESKHR*>(colorSwapchainImage)->image;

        // If a depth-stencil view has already been created for this back-buffer, use it.
        auto depthBufferIt = m_colorToDepthMap.find(colorSwapchainImage);
        if (depthBufferIt != m_colorToDepthMap.end()) {
            return depthBufferIt->second;
        }

        // This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.

        uint32_t depthTexture;
        GL(glGenTextures(1, &depthTexture));
        GL(glBindTexture(target, depthTexture));
        GL(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        if (isArray) {
            GL(glTexImage3D(target, 0, GL_DEPTH_COMPONENT24, width, height, arraySize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr));
        }
        else {
            GL(glTexImage2D(target, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr));
        }

        m_colorToDepthMap.insert(std::make_pair(colorSwapchainImage, depthTexture));

        return depthTexture;
    }

    void OpenGLESGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                            const XrSwapchainImageBaseHeader* colorSwapchainImage, int64_t colorSwapchainFormat,
                                            const std::vector<Cube>& cubes)
    {
        auto imageInfoIt = m_imageInfo.find(colorSwapchainImage);
        CHECK(imageInfoIt != m_imageInfo.end());

        SwapchainInfo& swapchainInfo = m_swapchainInfo[imageInfoIt->second.swapchainIndex];
        GLuint arraySize = swapchainInfo.createInfo.arraySize;
        bool isArray = arraySize > 1;
        GLenum target = isArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

        UNUSED_PARM(colorSwapchainFormat);  // Not used in this function for now.

        GL(glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer));

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLESKHR*>(colorSwapchainImage)->image;
        const uint32_t depthTexture = GetDepthTexture(colorSwapchainImage);

        GLint x = static_cast<GLint>(layerView.subImage.imageRect.offset.x);
        GLint y = static_cast<GLint>(layerView.subImage.imageRect.offset.y);
        GLsizei w = static_cast<GLsizei>(layerView.subImage.imageRect.extent.width);
        GLsizei h = static_cast<GLsizei>(layerView.subImage.imageRect.extent.height);

        GL(glViewport(x, y, w, h));
        GL(glScissor(x, y, w, h));

        GL(glEnable(GL_SCISSOR_TEST));
        GL(glEnable(GL_DEPTH_TEST));
        GL(glEnable(GL_CULL_FACE));
        GL(glFrontFace(GL_CW));
        GL(glCullFace(GL_BACK));

        if (isArray) {
            GL(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0, layerView.subImage.imageArrayIndex));
            GL(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0, layerView.subImage.imageArrayIndex));
        }
        else {
            GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, colorTexture, 0));
            GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, depthTexture, 0));
        }

        // Set shaders and uniform variables.
        GL(glUseProgram(m_program));

        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_OPENGL_ES, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrVector3f scale{1.f, 1.f, 1.f};
        XrMatrix4x4f_CreateTranslationRotationScale(&toView, &pose.position, &pose.orientation, &scale);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        // Set cube primitive data.
        GL(glBindVertexArray(m_vao));

        // Render each cube
        for (const Cube& cube : cubes) {
            // Compute the model-view-projection transform and set it..
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &cube.Pose.position, &cube.Pose.orientation, &cube.Scale);
            XrMatrix4x4f mvp;
            XrMatrix4x4f_Multiply(&mvp, &vp, &model);
            GL(glUniformMatrix4fv(m_modelViewProjectionUniformLocation, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&mvp)));

            // Draw the cube.
            GL(glDrawElements(GL_TRIANGLES, Geometry::c_cubeIndices.size(), GL_UNSIGNED_SHORT, nullptr));
        }

        GL(glBindVertexArray(0));
        GL(glUseProgram(0));
        GL(glDisable(GL_SCISSOR_TEST));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

        // Not sure what's intended here, but it doesn't work for a pbuffer context,
        // which is how this function is being called.

        // Swap our window every other eye for RenderDoc
        static int everyOther = 0;
        if ((everyOther++ & 1) != 0) {
            ksGpuWindow_SwapBuffers(&window);
        }
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_OPENGL_ES
