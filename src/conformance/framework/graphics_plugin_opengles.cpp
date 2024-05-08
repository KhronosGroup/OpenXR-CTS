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

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES

#include "conformance_framework.h"
#include "graphics_plugin.h"
#include "graphics_plugin_impl_helpers.h"
#include "graphics_plugin_opengl_gltf.h"
#include "report.h"
#include "swapchain_image_data.h"

#include "common/gfxwrapper_opengl.h"
#include "common/xr_dependencies.h"
#include "common/xr_linear.h"
#include "pbr/OpenGL/GLCommon.h"
#include "pbr/OpenGL/GLResources.h"
#include "pbr/OpenGL/GLTexture.h"
#include "utilities/Geometry.h"
#include "utilities/swapchain_format_data.h"
#include "utilities/swapchain_parameters.h"
#include "utilities/throw_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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
    struct IPlatformPlugin;
    static const char* VertexShaderGlsl = R"_(
    #version 320 es

    in vec3 VertexPos;
    in vec3 VertexColor;

    out vec3 PSVertexColor;

    uniform mat4 ModelViewProjection;
    uniform vec4 tintColor;

    void main() {
       gl_Position = ModelViewProjection * vec4(VertexPos, 1.0);
       PSVertexColor = mix(VertexColor, tintColor.rgb, tintColor.a);
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

    struct OpenGLESMesh
    {
        bool valid{false};
        GLuint m_vao{0};
        GLuint m_vertexBuffer{0};
        GLuint m_indexBuffer{0};
        uint32_t m_numIndices;

        OpenGLESMesh(GLint vertexAttribCoords, GLint vertexAttribColor,  //
                     const uint16_t* idx_data, uint32_t idx_count,       //
                     const Geometry::Vertex* vtx_data, uint32_t vtx_count)
        {
            m_numIndices = idx_count;

            GL(glGenBuffers(1, &m_vertexBuffer));
            GL(glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer));
            GL(glBufferData(GL_ARRAY_BUFFER, vtx_count * sizeof(Geometry::Vertex), vtx_data, GL_STATIC_DRAW));

            GL(glGenBuffers(1, &m_indexBuffer));
            GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer));
            GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_count * sizeof(uint16_t), idx_data, GL_STATIC_DRAW));

            glGenVertexArrays(1, &m_vao);
            glBindVertexArray(m_vao);
            glEnableVertexAttribArray(vertexAttribCoords);
            glEnableVertexAttribArray(vertexAttribColor);
            glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
            glVertexAttribPointer(vertexAttribCoords, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex), nullptr);
            glVertexAttribPointer(vertexAttribColor, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex),
                                  reinterpret_cast<const void*>(sizeof(XrVector3f)));

            valid = true;
        }

        OpenGLESMesh(OpenGLESMesh&& other) noexcept : m_numIndices(0)
        {
            using std::swap;
            swap(valid, other.valid);
            swap(m_vao, other.m_vao);
            swap(m_vertexBuffer, other.m_vertexBuffer);
            swap(m_indexBuffer, other.m_indexBuffer);
            swap(m_numIndices, other.m_numIndices);
        }

        OpenGLESMesh(const OpenGLESMesh&) = delete;

        ~OpenGLESMesh()
        {
            if (!valid) {
                return;
            }
            if (m_vao != 0) {
                glDeleteVertexArrays(1, &m_vao);
            }
            if (m_vertexBuffer != 0) {
                glDeleteBuffers(1, &m_vertexBuffer);
            }
            if (m_indexBuffer != 0) {
                glDeleteBuffers(1, &m_indexBuffer);
            }
        }
    };

    struct OpenGLESFallbackDepthTexture
    {
    public:
        OpenGLESFallbackDepthTexture() = default;
        ~OpenGLESFallbackDepthTexture()
        {
            Reset();
        }
        void Reset()
        {
            if (Allocated()) {

                GL(glDeleteTextures(1, &m_texture));
            }
            m_texture = 0;
            m_xrImage.image = 0;
        }
        bool Allocated() const
        {
            return m_texture != 0;
        }

        void Allocate(GLuint width, GLuint height, uint32_t arraySize)
        {
            Reset();
            const bool isArray = arraySize > 1;
            GLenum target = isArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
            GL(glGenTextures(1, &m_texture));
            GL(glBindTexture(target, m_texture));
            GL(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
            GL(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
            GL(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            GL(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
            if (isArray) {
                GL(glTexImage3D(target, 0, GL_DEPTH_COMPONENT24, width, height, arraySize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                                nullptr));
            }
            else {
                GL(glTexImage2D(target, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr));
            }
            m_xrImage.image = m_texture;
        }
        const XrSwapchainImageOpenGLESKHR& GetTexture() const
        {
            return m_xrImage;
        }

    private:
        uint32_t m_texture{0};
        XrSwapchainImageOpenGLESKHR m_xrImage{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR, NULL, 0};
    };

    class OpenGLESSwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageOpenGLESKHR>
    {
    public:
        OpenGLESSwapchainImageData(uint32_t capacity, const XrSwapchainCreateInfo& createInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR, capacity, createInfo), m_internalDepthTextures(capacity)
        {
        }

        OpenGLESSwapchainImageData(uint32_t capacity, const XrSwapchainCreateInfo& createInfo, XrSwapchain depthSwapchain,
                                   const XrSwapchainCreateInfo& depthCreateInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR, capacity, createInfo, depthSwapchain, depthCreateInfo)
        {
        }

    protected:
        const XrSwapchainImageOpenGLESKHR& GetFallbackDepthSwapchainImage(uint32_t i) override
        {

            if (!m_internalDepthTextures[i].Allocated()) {
                m_internalDepthTextures[i].Allocate(this->Width(), this->Height(), this->ArraySize());
            }

            return m_internalDepthTextures[i].GetTexture();
        }

    private:
        std::vector<OpenGLESFallbackDepthTexture> m_internalDepthTextures;
    };

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

        void ClearSwapchainCache() override;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, uint32_t arraySlice, const RGBAImage& image) override;

        std::string GetImageFormatName(int64_t imageFormat) const override;

        bool IsImageFormatKnown(int64_t imageFormat) const override;

        bool GetSwapchainCreateTestParameters(XrInstance instance, XrSession session, XrSystemId systemId, int64_t imageFormat,
                                              SwapchainCreateTestParameters* swapchainTestParameters) override;

        bool ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                     uint32_t* imageCount) const override;
        bool ValidateSwapchainImageState(XrSwapchain swapchain, uint32_t index, int64_t imageFormat) const override;

        int64_t SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectMotionVectorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t GetSRGBA8Format() const override;

        ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(size_t size,
                                                                          const XrSwapchainCreateInfo& colorSwapchainCreateInfo,
                                                                          XrSwapchain depthSwapchain,
                                                                          const XrSwapchainCreateInfo& depthSwapchainCreateInfo) override;

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex, XrColor4f color) override;

        MeshHandle MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx) override;

        GLTFModelHandle LoadGLTF(std::shared_ptr<tinygltf::Model> tinygltfModel) override;
        std::shared_ptr<Pbr::Model> GetPbrModel(GLTFModelHandle handle) const override;
        GLTFModelInstanceHandle CreateGLTFModelInstance(GLTFModelHandle handle) override;
        Pbr::ModelInstance& GetModelInstance(GLTFModelInstanceHandle handle) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        const RenderParams& params) override;

    private:
        bool initialized{false};

        void InitializeResources();
        void ShutdownResources();
        XrVersion OpenGLESVersionOfContext = 0;

        bool deviceInitialized{false};

        ksGpuWindow window{};

        XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};

        GLuint m_swapchainFramebuffer{0};
        GLuint m_program{0};
        GLint m_modelViewProjectionUniformLocation{0};
        GLint m_tintColorUniformLocation{0};
        GLint m_vertexAttribCoords{0};
        GLint m_vertexAttribColor{0};
        MeshHandle m_cubeMesh{};
        VectorWithGenerationCountedHandles<OpenGLESMesh, MeshHandle> m_meshes;
        // This is fine to be a shared_ptr because Model doesn't directly hold any graphics state.
        VectorWithGenerationCountedHandles<std::shared_ptr<Pbr::Model>, GLTFModelHandle> m_gltfModels;
        VectorWithGenerationCountedHandles<GLGLTF, GLTFModelInstanceHandle> m_gltfInstances;
        std::unique_ptr<Pbr::GLResources> m_pbrResources;

        SwapchainImageDataMap<OpenGLESSwapchainImageData> m_swapchainImageDataMap;
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
        return {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
    }

    const XrBaseInStructure* OpenGLESGraphicsPlugin::GetGraphicsBinding() const
    {
        if (deviceInitialized) {
            return reinterpret_cast<const XrBaseInStructure*>(&graphicsBinding);
        }
        return nullptr;
    }

    void OpenGLESGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, uint32_t arraySlice,
                                               const RGBAImage& image)
    {
        OpenGLESSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImage);

        // auto imageInfoIt = m_imageInfo.find(swapchainImage);
        // XRC_CHECK_THROW(imageInfoIt != m_imageInfo.end());

        // SwapchainInfo& swapchainInfo = m_swapchainInfo[imageInfoIt->second.swapchainIndex];
        // GLuint arraySize = swapchainInfo.createInfo.arraySize;
        const bool isArray = swapchainData->HasMultipleSlices();
        GLuint width = swapchainData->Width();
        GLuint height = swapchainData->Height();
        GLenum target = isArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

        const uint32_t img = swapchainData->GetTypedImage(imageIndex).image;
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
            XRC_CHECK_THROW(ValidateResultAllowed("xrGetOpenGLESGraphicsRequirementsKHR", result));
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
            XRC_CHECK_THROW(window.display != EGL_NO_DISPLAY);
            XRC_CHECK_THROW(window.context.context != EGL_NO_CONTEXT);
            graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
            graphicsBinding.display = window.display;
            graphicsBinding.config = (EGLConfig)0;
            graphicsBinding.context = window.context.context;
        }

        GLenum error = glGetError();
        XRC_CHECK_THROW(error == GL_NO_ERROR);

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

    void OpenGLESGraphicsPlugin::ClearSwapchainCache()
    {
        m_swapchainImageDataMap.Reset();
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
        m_tintColorUniformLocation = glGetUniformLocation(m_program, "tintColor");

        m_vertexAttribCoords = glGetAttribLocation(m_program, "VertexPos");
        m_vertexAttribColor = glGetAttribLocation(m_program, "VertexColor");

        m_cubeMesh = MakeCubeMesh();

        m_pbrResources = std::make_unique<Pbr::GLResources>();
        m_pbrResources->SetLight({0.0f, 0.7071067811865475f, 0.7071067811865475f}, Pbr::RGB::White);

        auto blackCubeMap = std::make_shared<Pbr::ScopedGLTexture>(Pbr::GLTexture::CreateFlatCubeTexture(Pbr::RGBA::Black, GL_RGBA8));
        m_pbrResources->SetEnvironmentMap(blackCubeMap, blackCubeMap);

        // Read the BRDF Lookup Table used by the PBR system into a DirectX texture.
        std::vector<unsigned char> brdfLutFileData = ReadFileBytes("brdf_lut.png");
        auto brdLutResourceView = std::make_shared<Pbr::ScopedGLTexture>(
            Pbr::GLTexture::LoadTextureImage(brdfLutFileData.data(), (uint32_t)brdfLutFileData.size()));
        m_pbrResources->SetBrdfLut(brdLutResourceView);
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

            m_swapchainImageDataMap.Reset();

            m_cubeMesh = {};
            m_meshes.clear();
            m_gltfInstances.clear();
            m_gltfModels.clear();
            m_pbrResources.reset();

            ksGpuWindow_Destroy(&window);
        }
        deviceInitialized = false;
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGLES(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<OpenGLESGraphicsPlugin>(platformPlugin);
    }

    // For now don't test XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT on GL since the semantics are unclear and some runtimes don't support this flag.
    // TODO in the future remove this workaround?
#define WORKAROUND NotMutable()
    typedef std::map<int64_t, SwapchainCreateTestParameters> SwapchainTestMap;

    static const SwapchainFormatDataMap& GetSwapchainFormatData()
    {
        static SwapchainFormatDataMap map{

            //
            // 8 bits per component
            //
            XRC_SWAPCHAIN_FORMAT(GL_R8).WORKAROUND.ToPair(),            // 1-component, 8-bit unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RG8).WORKAROUND.ToPair(),           // 2-component, 8-bit unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB8).WORKAROUND.ToPair(),          // 3-component, 8-bit unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGBA8).WORKAROUND.ToPair(),         // 4-component, 8-bit unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_R8_SNORM).WORKAROUND.ToPair(),      // 1-component, 8-bit signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_RG8_SNORM).WORKAROUND.ToPair(),     // 2-component, 8-bit signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB8_SNORM).WORKAROUND.ToPair(),    // 3-component, 8-bit signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGBA8_SNORM).WORKAROUND.ToPair(),   // 4-component, 8-bit signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_R8UI).WORKAROUND.ToPair(),          // 1-component, 8-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RG8UI).WORKAROUND.ToPair(),         // 2-component, 8-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RGB8UI).WORKAROUND.ToPair(),        // 3-component, 8-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RGBA8UI).WORKAROUND.ToPair(),       // 4-component, 8-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_R8I).WORKAROUND.ToPair(),           // 1-component, 8-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RG8I).WORKAROUND.ToPair(),          // 2-component, 8-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RGB8I).WORKAROUND.ToPair(),         // 3-component, 8-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RGBA8I).WORKAROUND.ToPair(),        // 4-component, 8-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_SR8).WORKAROUND.ToPair(),           // 1-component, 8-bit sRGB
            XRC_SWAPCHAIN_FORMAT(GL_SRG8).WORKAROUND.ToPair(),          // 2-component, 8-bit sRGB
            XRC_SWAPCHAIN_FORMAT(GL_SRGB8).WORKAROUND.ToPair(),         // 3-component, 8-bit sRGB
            XRC_SWAPCHAIN_FORMAT(GL_SRGB8_ALPHA8).WORKAROUND.ToPair(),  // 4-component, 8-bit sRGB

            //
            // 16 bits per component
            //
            XRC_SWAPCHAIN_FORMAT(GL_R16).WORKAROUND.ToPair(),           // 1-component, 16-bit unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RG16).WORKAROUND.ToPair(),          // 2-component, 16-bit unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB16).WORKAROUND.ToPair(),         // 3-component, 16-bit unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16).WORKAROUND.ToPair(),        // 4-component, 16-bit unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_R16_SNORM).WORKAROUND.ToPair(),     // 1-component, 16-bit signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_RG16_SNORM).WORKAROUND.ToPair(),    // 2-component, 16-bit signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB16_SNORM).WORKAROUND.ToPair(),   // 3-component, 16-bit signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16_SNORM).WORKAROUND.ToPair(),  // 4-component, 16-bit signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_R16UI).WORKAROUND.ToPair(),         // 1-component, 16-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RG16UI).WORKAROUND.ToPair(),        // 2-component, 16-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RGB16UI).WORKAROUND.ToPair(),       // 3-component, 16-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16UI).WORKAROUND.ToPair(),      // 4-component, 16-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_R16I).WORKAROUND.ToPair(),          // 1-component, 16-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RG16I).WORKAROUND.ToPair(),         // 2-component, 16-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RGB16I).WORKAROUND.ToPair(),        // 3-component, 16-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16I).WORKAROUND.ToPair(),       // 4-component, 16-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_R16F).WORKAROUND.ToPair(),          // 1-component, 16-bit floating-point
            XRC_SWAPCHAIN_FORMAT(GL_RG16F).WORKAROUND.ToPair(),         // 2-component, 16-bit floating-point
            XRC_SWAPCHAIN_FORMAT(GL_RGB16F).WORKAROUND.ToPair(),        // 3-component, 16-bit floating-point
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16F).WORKAROUND.ToPair(),       // 4-component, 16-bit floating-point

            //
            // 32 bits per component
            //
            XRC_SWAPCHAIN_FORMAT(GL_R32UI).WORKAROUND.ToPair(),     // 1-component, 32-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RG32UI).WORKAROUND.ToPair(),    // 2-component, 32-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RGB32UI).WORKAROUND.ToPair(),   // 3-component, 32-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_RGBA32UI).WORKAROUND.ToPair(),  // 4-component, 32-bit unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_R32I).WORKAROUND.ToPair(),      // 1-component, 32-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RG32I).WORKAROUND.ToPair(),     // 2-component, 32-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RGB32I).WORKAROUND.ToPair(),    // 3-component, 32-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_RGBA32I).WORKAROUND.ToPair(),   // 4-component, 32-bit signed integer
            XRC_SWAPCHAIN_FORMAT(GL_R32F).WORKAROUND.ToPair(),      // 1-component, 32-bit floating-point
            XRC_SWAPCHAIN_FORMAT(GL_RG32F).WORKAROUND.ToPair(),     // 2-component, 32-bit floating-point
            XRC_SWAPCHAIN_FORMAT(GL_RGB32F).WORKAROUND.ToPair(),    // 3-component, 32-bit floating-point
            XRC_SWAPCHAIN_FORMAT(GL_RGBA32F).WORKAROUND.ToPair(),   // 4-component, 32-bit floating-point

            //
            // Packed
            //
            XRC_SWAPCHAIN_FORMAT(GL_RGB5).WORKAROUND.ToPair(),            // 3-component 5:5:5,       unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB565).WORKAROUND.ToPair(),          // 3-component 5:6:5,       unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB10).WORKAROUND.ToPair(),           // 3-component 10:10:10,    unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGBA4).WORKAROUND.ToPair(),           // 4-component 4:4:4:4,     unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB5_A1).WORKAROUND.ToPair(),         // 4-component 5:5:5:1,     unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB10_A2).WORKAROUND.ToPair(),        // 4-component 10:10:10:2,  unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_RGB10_A2UI).WORKAROUND.ToPair(),      // 4-component 10:10:10:2,  unsigned integer
            XRC_SWAPCHAIN_FORMAT(GL_R11F_G11F_B10F).WORKAROUND.ToPair(),  // 3-component 11:11:10,    floating-point
            XRC_SWAPCHAIN_FORMAT(GL_RGB9_E5).WORKAROUND.ToPair(),         // 3-component/exp 9:9:9/5, floating-point

            //
            // S3TC/DXT/BC
            //
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 3D space, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 3D space plus 1-bit alpha, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 3D space plus line through 1D space, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 3D space plus 4-bit alpha, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB_S3TC_DXT1_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 3D space, 4x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 3D space plus 1-bit alpha, 4x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 3D space plus line through 1D space, 4x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 3D space plus 4-bit alpha, 4x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_LUMINANCE_LATC1_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 1D space, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // two lines through 1D space, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 1D space, 4x4 blocks, signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // two lines through 1D space, 4x4 blocks, signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RED_RGTC1)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 1D space, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RG_RGTC2)
                .Compressed()
                .NotMutable()
                .ToPair(),  // two lines through 1D space, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_RED_RGTC1)
                .Compressed()
                .NotMutable()
                .ToPair(),  // line through 1D space, 4x4 blocks, signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_RG_RGTC2)
                .Compressed()
                .NotMutable()
                .ToPair(),  // two lines through 1D space, 4x4 blocks, signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 3-component, 4x4 blocks, unsigned floating-point
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 3-component, 4x4 blocks, signed floating-point
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_BPTC_UNORM)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM).Compressed().NotMutable().ToPair(),  // 4-component, 4x4 blocks, sRGB

            //
            // ETC
            //
            XRC_SWAPCHAIN_FORMAT(GL_ETC1_RGB8_OES).Compressed().NotMutable().ToPair(),  // 3-component ETC1, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB8_ETC2)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 3-component ETC2, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ETC2 with 1-bit alpha, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA8_ETC2_EAC)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ETC2, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ETC2).Compressed().NotMutable().ToPair(),  // 3-component ETC2, 4x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ETC2 with 1-bit alpha, 4x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ETC2, 4x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_R11_EAC)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 1-component ETC, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RG11_EAC)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 2-component ETC, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_R11_EAC)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 1-component ETC, 4x4 blocks, signed normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_RG11_EAC)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 2-component ETC, 4x4 blocks, signed normalized

            //
            // ASTC
            //
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_4x4_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 4x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_5x4_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 5x4 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_5x5_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 5x5 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_6x5_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 6x5 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_6x6_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 6x6 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_8x5_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 8x5 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_8x6_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 8x6 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_8x8_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 8x8 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_10x5_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 10x5 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_10x6_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 10x6 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_10x8_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 10x8 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_10x10_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 10x10 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_12x10_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 12x10 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_ASTC_12x12_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 12x12 blocks, unsigned normalized
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 4x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 5x4 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 5x5 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 6x5 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 6x6 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 8x5 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 8x6 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 8x8 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 10x5 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 10x6 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 10x8 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 10x10 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 12x10 blocks, sRGB
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR)
                .Compressed()
                .NotMutable()
                .ToPair(),  // 4-component ASTC, 12x12 blocks, sRGB

            //
            // Depth/stencil
            //
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH_COMPONENT16).WORKAROUND.Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH_COMPONENT24).WORKAROUND.Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH_COMPONENT32F).WORKAROUND.Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH_COMPONENT32F_NV).WORKAROUND.Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_STENCIL_INDEX8).WORKAROUND.Stencil().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH24_STENCIL8).WORKAROUND.DepthStencil().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH32F_STENCIL8).WORKAROUND.DepthStencil().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH32F_STENCIL8_NV).WORKAROUND.DepthStencil().ToPair(),
        };
        return map;
    }

    // Returns a name for an image format.
    std::string OpenGLESGraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        return ::Conformance::GetImageFormatName(GetSwapchainFormatData(), imageFormat);
    }

    bool OpenGLESGraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        return ::Conformance::IsImageFormatKnown(GetSwapchainFormatData(), imageFormat);
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

        *swapchainTestParameters = ::Conformance::GetSwapchainCreateTestParameters(GetSwapchainFormatData(), imageFormat);
        return true;
    }

    // Given an imageFormat and its test parameters and the XrSwapchain resulting from xrCreateSwapchain,
    // validate the images in any platform-specific way.
    // Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
    bool OpenGLESGraphicsPlugin::ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp,
                                                         XrSwapchain swapchain, uint32_t* imageCount) const
    {
        // OK to use CHECK and REQUIRE in here because this is always called from within a test.
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
        // The order of this list does not effect the priority of selecting formats, the runtime list defines that.
        const std::array<GLenum, 6> f{
            GL_RGB10_A2,
            GL_RGBA16,
            GL_RGBA16F,
            GL_RGBA32F,

            // The two below should only be used as a fallback, as they are linear color formats without enough bits for color
            // depth, thus leading to banding.
            GL_RGBA8,
            GL_SRGB8_ALPHA8,
        };

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
        const std::array<GLenum, 4> f{
            GL_DEPTH24_STENCIL8,
            GL_DEPTH_COMPONENT24,
            GL_DEPTH_COMPONENT16,
            GL_DEPTH_COMPONENT32F,
        };

        const int64_t* formatArrayEnd = imageFormatArray + count;
        auto it = std::find_first_of(imageFormatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return imageFormatArray[0];
        }

        return *it;
    }

    int64_t OpenGLESGraphicsPlugin::SelectMotionVectorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const
    {
        // List of swapchain formats suitable for motion vectors.
        const std::array<GLenum, 1> f{
            GL_RGBA16F,
        };

        const int64_t* formatArrayEnd = imageFormatArray + count;
        auto it = std::find_first_of(imageFormatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return imageFormatArray[0];
        }

        return *it;
    }

    int64_t OpenGLESGraphicsPlugin::GetSRGBA8Format() const
    {
        return GL_SRGB8_ALPHA8;
    }

    ISwapchainImageData* OpenGLESGraphicsPlugin::AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {
        auto typedResult = std::make_unique<OpenGLESSwapchainImageData>(uint32_t(size), swapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    inline ISwapchainImageData* OpenGLESGraphicsPlugin::AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo)
    {

        auto typedResult = std::make_unique<OpenGLESSwapchainImageData>(uint32_t(size), colorSwapchainCreateInfo, depthSwapchain,
                                                                        depthSwapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    void OpenGLESGraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                                 XrColor4f color)
    {
        OpenGLESSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        const bool isArray = swapchainData->HasMultipleSlices();
        GLenum target = isArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

        GL(glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer));

        const uint32_t colorTexture = swapchainData->GetTypedImage(imageIndex).image;
        const uint32_t depthTexture = swapchainData->GetDepthImageForColorIndex(imageIndex).image;
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
        GLsizei w = swapchainData->Width();
        GLsizei h = swapchainData->Height();
        GL(glViewport(x, y, w, h));
        GL(glScissor(x, y, w, h));

        GL(glEnable(GL_SCISSOR_TEST));

        // Clear swapchain and depth buffer.
        GL(glClearColor(color.r, color.g, color.b, color.a));
        GL(glClearDepthf(1.0f));
        GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    MeshHandle OpenGLESGraphicsPlugin::MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx)
    {
        auto handle = m_meshes.emplace_back(m_vertexAttribCoords, m_vertexAttribColor, idx.data(), (uint32_t)idx.size(), vtx.data(),
                                            (uint32_t)vtx.size());

        return handle;
    }

    GLTFModelHandle OpenGLESGraphicsPlugin::LoadGLTF(std::shared_ptr<tinygltf::Model> tinygltfModel)
    {
        std::shared_ptr<Pbr::Model> pbrModel = Gltf::FromGltfObject(*m_pbrResources, *tinygltfModel);
        auto handle = m_gltfModels.emplace_back(std::move(pbrModel));
        return handle;
    }

    std::shared_ptr<Pbr::Model> OpenGLESGraphicsPlugin::GetPbrModel(GLTFModelHandle handle) const
    {
        return m_gltfModels[handle];
    }

    GLTFModelInstanceHandle OpenGLESGraphicsPlugin::CreateGLTFModelInstance(GLTFModelHandle handle)
    {
        auto pbrModelInstance = Pbr::GLModelInstance(*m_pbrResources, GetPbrModel(handle));
        auto instanceHandle = m_gltfInstances.emplace_back(std::move(pbrModelInstance));
        return instanceHandle;
    }

    Pbr::ModelInstance& OpenGLESGraphicsPlugin::GetModelInstance(GLTFModelInstanceHandle handle)
    {
        return m_gltfInstances[handle].GetModelInstance();
    }

    void OpenGLESGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                            const XrSwapchainImageBaseHeader* colorSwapchainImage, const RenderParams& params)
    {
        OpenGLESSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        GLuint arraySize = swapchainData->ArraySize();
        bool isArray = arraySize > 1;
        GLenum target = isArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

        GL(glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer));

        const uint32_t colorTexture = swapchainData->GetTypedImage(imageIndex).image;
        const uint32_t depthTexture = swapchainData->GetDepthImageForColorIndex(imageIndex).image;

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
        XrMatrix4x4f_CreateFromRigidTransform(&toView, &pose);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        MeshHandle lastMeshHandle;

        const auto drawMesh = [this, &vp, &lastMeshHandle](const MeshDrawable mesh) {
            OpenGLESMesh& glMesh = m_meshes[mesh.handle];
            if (mesh.handle != lastMeshHandle) {
                // We are now rendering a new mesh
                GL(glBindVertexArray(glMesh.m_vao));
                glBindBuffer(GL_ARRAY_BUFFER, glMesh.m_vertexBuffer);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.m_indexBuffer);

                lastMeshHandle = mesh.handle;
            }

            // Compute the model-view-projection transform and set it..
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &mesh.params.pose.position, &mesh.params.pose.orientation,
                                                        &mesh.params.scale);
            XrMatrix4x4f mvp;
            XrMatrix4x4f_Multiply(&mvp, &vp, &model);
            GL(glUniformMatrix4fv(m_modelViewProjectionUniformLocation, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&mvp)));
            GL(glUniform4fv(m_tintColorUniformLocation, 1, reinterpret_cast<const GLfloat*>(&mesh.tintColor)));

            // Draw the mesh.
            GL(glDrawElements(GL_TRIANGLES, glMesh.m_numIndices, GL_UNSIGNED_SHORT, nullptr));
        };

        // Render each cube
        for (const Cube& cube : params.cubes) {
            drawMesh(MeshDrawable{m_cubeMesh, cube.params.pose, cube.params.scale, cube.tintColor});
        }

        // Render each mesh
        for (const auto& mesh : params.meshes) {
            drawMesh(mesh);
        }

        // Render each gltf
        for (const auto& gltfDrawable : params.glTFs) {
            GLGLTF& gltf = m_gltfInstances[gltfDrawable.handle];
            // Compute and update the model transform.

            XrMatrix4x4f modelToWorld;
            XrMatrix4x4f_CreateTranslationRotationScale(&modelToWorld, &gltfDrawable.params.pose.position,
                                                        &gltfDrawable.params.pose.orientation, &gltfDrawable.params.scale);

            m_pbrResources->SetViewProjection(view, proj);

            gltf.Render(*m_pbrResources, modelToWorld);
        }

        GL(glBindVertexArray(0));
        GL(glUseProgram(0));
        GL(glDisable(GL_SCISSOR_TEST));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_OPENGL_ES
