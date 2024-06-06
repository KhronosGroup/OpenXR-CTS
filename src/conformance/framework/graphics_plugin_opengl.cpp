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

#include "RGBAImage.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "gltf_helpers.h"
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
#include "pbr/PbrCommon.h"
#include "utilities/Geometry.h"
#include "utilities/opengl_utils.h"
#include "utilities/swapchain_format_data.h"
#include "utilities/swapchain_parameters.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nonstd/span.hpp>
#include <nonstd/type.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <array>
#include <assert.h>
#include <cstdint>
#include <memory>
#include <stddef.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Conformance
{
    struct IPlatformPlugin;
}  // namespace Conformance
namespace Pbr
{
    class Model;
}  // namespace Pbr

// Note: mapping of OpenXR usage flags to OpenGL
//
// XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT: can be bound to a framebuffer as color
// XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT: can be bound to a framebuffer as depth (or stencil-only GL_STENCIL_INDEX8)
// XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT: image load/store and core since 4.2.
//   List of supported formats is in https://registry.khronos.org/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
// XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT & XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT: must be compatible format with glCopyTexImage* calls
// XR_SWAPCHAIN_USAGE_SAMPLED_BIT: can be sampled in a shader
// XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT: all GL formats are typed, but some can be reinterpreted with a different view.
//   OpenGL 4.2 / 4.3 with MSAA. Only for color formats and compressed ones
//   List with compatible textures: https://registry.khronos.org/OpenGL/extensions/ARB/ARB_texture_view.txt
//   Note: no GL formats are "mutableFormats" in the sense of SwapchainCreateTestParameters as this is intended for TYPELESS,
//   however, some are "supportsMutableFormat"

namespace Conformance
{

    // Only texture formats which are in OpenGL core and which are either color or depth renderable or
    // of a specific compressed format are listed below. Runtimes can support additional formats, but those
    // will not get tested.
    //
    // For now don't test XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT on GL since the semantics are unclear and some runtimes don't support this flag.
    // TODO in the future remove this workaround?
#define WORKAROUND NotMutable()
    static const SwapchainFormatDataMap& GetSwapchainFormatData()
    {
        static SwapchainFormatDataMap map{
            XRC_SWAPCHAIN_FORMAT(GL_RGBA8).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGB10_A2).WORKAROUND.ToPair(),

            XRC_SWAPCHAIN_FORMAT(GL_R8).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R16).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG8).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG16).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGB10_A2UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R16F).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG16F).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGB16F).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16F).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R32F).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG32F).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA32F).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R11F_G11F_B10F).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R8I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R8UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R16I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R16UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R32I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_R32UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG8I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG8UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG16I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG16UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG32I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RG32UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA8I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA8UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA16UI).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA32I).WORKAROUND.ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGBA32UI).WORKAROUND.ToPair(),

            XRC_SWAPCHAIN_FORMAT(GL_RGBA4).WORKAROUND.NotMutable().NoUnorderedAccess().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_RGB5_A1).WORKAROUND.NotMutable().NoUnorderedAccess().ToPair(),

            XRC_SWAPCHAIN_FORMAT(GL_SRGB8).WORKAROUND.NoUnorderedAccess().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_SRGB8_ALPHA8).WORKAROUND.NoUnorderedAccess().ToPair(),

            XRC_SWAPCHAIN_FORMAT(GL_RGB565).WORKAROUND.NotMutable().NoUnorderedAccess().ToPair(),

            XRC_SWAPCHAIN_FORMAT(GL_DEPTH_COMPONENT16).WORKAROUND.Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH_COMPONENT24).WORKAROUND.Depth().ToPair(),

            XRC_SWAPCHAIN_FORMAT(GL_DEPTH_COMPONENT32F).WORKAROUND.Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH24_STENCIL8).WORKAROUND.DepthStencil().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_DEPTH32F_STENCIL8).WORKAROUND.DepthStencil().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_STENCIL_INDEX8).WORKAROUND.Stencil().ToPair(),

            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RED_RGTC1).WORKAROUND.Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_RED_RGTC1).WORKAROUND.Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RG_RGTC2).WORKAROUND.Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_RG_RGTC2).WORKAROUND.Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA_BPTC_UNORM).WORKAROUND.Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM).WORKAROUND.Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT).WORKAROUND.Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT).WORKAROUND.Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB8_ETC2).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ETC2).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RGBA8_ETC2_EAC).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_R11_EAC).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_R11_EAC).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_RG11_EAC).WORKAROUND.Compressed().NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(GL_COMPRESSED_SIGNED_RG11_EAC).WORKAROUND.Compressed().NotMutable().ToPair(),
        };
        return map;
    }

    static const char* VertexShaderGlsl = R"_(
        #version 410

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
        #version 410

        in vec3 PSVertexColor;
        out vec4 FragColor;

        void main() {
           FragColor = vec4(PSVertexColor, 1);
        }
        )_";

    struct OpenGLMesh
    {
        bool valid{false};
        GLuint m_vao{0};
        GLuint m_vertexBuffer{0};
        GLuint m_indexBuffer{0};
        uint32_t m_numIndices;

        OpenGLMesh(GLint vertexAttribCoords, GLint vertexAttribColor,  //
                   const uint16_t* idx_data, uint32_t idx_count,       //
                   const Geometry::Vertex* vtx_data, uint32_t vtx_count)
        {
            m_numIndices = idx_count;

            XRC_CHECK_THROW_GLCMD(glGenBuffers(1, &m_vertexBuffer));
            XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer));
            XRC_CHECK_THROW_GLCMD(glBufferData(GL_ARRAY_BUFFER, vtx_count * sizeof(Geometry::Vertex), vtx_data, GL_STATIC_DRAW));

            XRC_CHECK_THROW_GLCMD(glGenBuffers(1, &m_indexBuffer));
            XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer));
            XRC_CHECK_THROW_GLCMD(glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_count * sizeof(uint16_t), idx_data, GL_STATIC_DRAW));

            XRC_CHECK_THROW_GLCMD(glGenVertexArrays(1, &m_vao));
            XRC_CHECK_THROW_GLCMD(glBindVertexArray(m_vao));
            XRC_CHECK_THROW_GLCMD(glEnableVertexAttribArray(vertexAttribCoords));
            XRC_CHECK_THROW_GLCMD(glEnableVertexAttribArray(vertexAttribColor));
            XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer));
            XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer));
            XRC_CHECK_THROW_GLCMD(glVertexAttribPointer(vertexAttribCoords, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex), nullptr));
            XRC_CHECK_THROW_GLCMD(glVertexAttribPointer(vertexAttribColor, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex),
                                                        reinterpret_cast<const void*>(sizeof(XrVector3f))));

            valid = true;
        }

        OpenGLMesh(OpenGLMesh&& other) noexcept : m_numIndices(0)
        {
            using std::swap;
            swap(valid, other.valid);
            swap(m_vao, other.m_vao);
            swap(m_vertexBuffer, other.m_vertexBuffer);
            swap(m_indexBuffer, other.m_indexBuffer);
            swap(m_numIndices, other.m_numIndices);
        }

        OpenGLMesh(const OpenGLMesh&) = delete;

        ~OpenGLMesh()
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

    struct OpenGLFallbackDepthTexture
    {
    public:
        OpenGLFallbackDepthTexture() = default;
        ~OpenGLFallbackDepthTexture()
        {
            if (Allocated()) {
                // As implementation as ::Reset(), but should not throw in destructor
                glDeleteTextures(1, &m_texture);
            }
            m_texture = 0;
        }
        void Reset()
        {
            if (Allocated()) {

                XRC_CHECK_THROW_GLCMD(glDeleteTextures(1, &m_texture));
            }
            m_texture = 0;
        }
        bool Allocated() const
        {
            return m_texture != 0;
        }

        void Allocate(GLuint width, GLuint height, uint32_t arraySize, uint32_t sampleCount)
        {
            Reset();
            const bool isArray = arraySize > 1;
            const bool isMultisample = sampleCount > 1;
            GLenum target = TexTarget(isArray, isMultisample);
            XRC_CHECK_THROW_GLCMD(glGenTextures(1, &m_texture));
            XRC_CHECK_THROW_GLCMD(glBindTexture(target, m_texture));
            if (!isMultisample) {
                XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
                XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
                XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
                XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
            }
            if (isMultisample) {
                if (isArray) {
                    XRC_CHECK_THROW_GLCMD(
                        glTexImage3DMultisample(target, sampleCount, GL_DEPTH_COMPONENT32, width, height, arraySize, true));
                }
                else {
                    XRC_CHECK_THROW_GLCMD(glTexImage2DMultisample(target, sampleCount, GL_DEPTH_COMPONENT32, width, height, true));
                }
            }
            else {
                if (isArray) {
                    XRC_CHECK_THROW_GLCMD(
                        glTexImage3D(target, 0, GL_DEPTH_COMPONENT32, width, height, arraySize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr));
                }
                else {
                    XRC_CHECK_THROW_GLCMD(
                        glTexImage2D(target, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr));
                }
            }
            m_image.image = m_texture;
        }
        const XrSwapchainImageOpenGLKHR& GetTexture() const
        {
            return m_image;
        }

    private:
        uint32_t m_texture{0};
        XrSwapchainImageOpenGLKHR m_image{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, NULL, 0};
    };
    class OpenGLSwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageOpenGLKHR>
    {
    public:
        OpenGLSwapchainImageData(uint32_t capacity, const XrSwapchainCreateInfo& createInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, capacity, createInfo), m_internalDepthTextures(capacity)
        {
        }

        OpenGLSwapchainImageData(uint32_t capacity, const XrSwapchainCreateInfo& createInfo, XrSwapchain depthSwapchain,
                                 const XrSwapchainCreateInfo& depthCreateInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, capacity, createInfo, depthSwapchain, depthCreateInfo)
            , m_internalDepthTextures(capacity)
        {
        }

    protected:
        const XrSwapchainImageOpenGLKHR& GetFallbackDepthSwapchainImage(uint32_t i) override
        {

            if (!m_internalDepthTextures[i].Allocated()) {
                m_internalDepthTextures[i].Allocate(this->Width(), this->Height(), this->ArraySize(), this->SampleCount());
            }

            return m_internalDepthTextures[i].GetTexture();
        }

    private:
        std::vector<OpenGLFallbackDepthTexture> m_internalDepthTextures;
    };
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

        void CheckState(const char* file_line) const override;
        void MakeCurrent(bool bindToThread) override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;
        void DebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message) const;
        void InitializeResources();
        void CheckFramebuffer(GLuint fb) const;

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

        // Format required by RGBAImage type.
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
        bool initialized = false;
        bool deviceInitialized = false;

        void deleteGLContext();

        XrVersion OpenGLVersionOfContext = 0;

        ksGpuWindow window{};

#if defined(XR_USE_PLATFORM_WIN32)
        XrGraphicsBindingOpenGLWin32KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
#elif defined(XR_USE_PLATFORM_XLIB)
        XrGraphicsBindingOpenGLXlibKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
#elif defined(XR_USE_PLATFORM_XCB)
        XrGraphicsBindingOpenGLXcbKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR};
#elif defined(XR_USE_PLATFORM_MACOS)
#error OpenGL bindings for Mac have not been implemented
#else
#error "Platform not (yet) supported."
#endif

        SwapchainImageDataMap<OpenGLSwapchainImageData> m_swapchainImageDataMap;
        GLuint m_swapchainFramebuffer{0};
        GLuint m_program{0};
        GLint m_modelViewProjectionUniformLocation{0};
        GLint m_tintColorUniformLocation{0};
        GLint m_vertexAttribCoords{0};
        GLint m_vertexAttribColor{0};
        MeshHandle m_cubeMesh{};
        VectorWithGenerationCountedHandles<OpenGLMesh, MeshHandle> m_meshes;
        // This is fine to be a shared_ptr because Model doesn't directly hold any graphics state.
        VectorWithGenerationCountedHandles<std::shared_ptr<Pbr::Model>, GLTFModelHandle> m_gltfModels;
        VectorWithGenerationCountedHandles<GLGLTF, GLTFModelInstanceHandle> m_gltfInstances;
        std::unique_ptr<Pbr::GLResources> m_pbrResources;
    };

    OpenGLGraphicsPlugin::OpenGLGraphicsPlugin(const std::shared_ptr<IPlatformPlugin>& /*unused*/)
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

    void OpenGLGraphicsPlugin::deleteGLContext()
    {
        if (deviceInitialized) {
            //ReportF("Destroying window");
            ksGpuWindow_Destroy(&window);
        }
        deviceInitialized = false;
    }

    bool OpenGLGraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                                                uint32_t /*deviceCreationFlags*/)
    {
        XrGraphicsRequirementsOpenGLKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        graphicsRequirements.minApiVersionSupported = XR_MAKE_VERSION(3, 2, 0);
        graphicsRequirements.maxApiVersionSupported = XR_MAKE_VERSION(4, 6, 0);

        // optional check to get the graphics requirements:
        if (checkGraphicsRequirements) {
            auto xrGetOpenGLGraphicsRequirementsKHR =
                GetInstanceExtensionFunction<PFN_xrGetOpenGLGraphicsRequirementsKHR>(instance, "xrGetOpenGLGraphicsRequirementsKHR");

            XrResult result = xrGetOpenGLGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements);
            XRC_CHECK_THROW(ValidateResultAllowed("xrGetOpenGLGraphicsRequirementsKHR", result));
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

        ksDriverInstance driverInstance{};
        ksGpuQueueInfo queueInfo{};
        ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
        ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
        ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
        if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat, depthFormat, sampleCount, 640, 480, false)) {
            XRC_THROW("Unable to create GL context");
        }
        //ReportF("Created window");
#if defined(XR_USE_PLATFORM_WIN32)
        graphicsBinding.hDC = window.context.hDC;
        graphicsBinding.hGLRC = window.context.hGLRC;
#elif defined(XR_USE_PLATFORM_XLIB)
        XRC_CHECK_THROW(window.context.xDisplay != nullptr);
        graphicsBinding.xDisplay = window.context.xDisplay;
        graphicsBinding.visualid = window.context.visualid;
        graphicsBinding.glxFBConfig = window.context.glxFBConfig;
        graphicsBinding.glxDrawable = window.context.glxDrawable;
        graphicsBinding.glxContext = window.context.glxContext;
#elif defined(XR_USE_PLATFORM_XCB)
        XRC_CHECK_THROW(window.context.connection != nullptr);
        graphicsBinding.connection = window.context.connection;
        graphicsBinding.screenNumber = window.context.screen_number;
        graphicsBinding.fbconfigid = window.context.fbconfigid;
        graphicsBinding.visualid = window.context.visualid;
        graphicsBinding.glxDrawable = window.context.glxDrawable;
        graphicsBinding.glxContext = window.context.glxContext;
#elif defined(XR_USE_PLATFORM_MACOS)
#error OpenGL bindings for Mac have not been implemented
#else
#error "Platform not (yet) supported."
#endif

        GLenum error = glGetError();
        XRC_CHECK_THROW(error == GL_NO_ERROR);

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

#if !defined(OS_APPLE_MACOS)
#if !defined(NDEBUG)
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(
            [](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
                ((OpenGLGraphicsPlugin*)userParam)->DebugMessageCallback(source, type, id, severity, length, message);
            },
            this);
#endif  // !defined(NDEBUG)
#endif  // !defined(OS_APPLE_MACOS)

        InitializeResources();

        deviceInitialized = true;
        return true;
    }

    void OpenGLGraphicsPlugin::DebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                                    const GLchar* message) const
    {
        (void)source;
        (void)type;
        (void)id;
        (void)length;
#if !defined(OS_APPLE_MACOS)
        const char* sev = "<unknown>";
        switch (severity) {
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            sev = "INFO";
            break;
        case GL_DEBUG_SEVERITY_LOW:
            sev = "LOW";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            sev = "MED";
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            sev = "HIGH";
            break;
        }
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
            return;
        ReportF("GL %s (0x%x): %s", sev, id, message);
#else
        (void)severity;
        (void)message;
#endif  // !defined(OS_APPLE_MACOS)
    }

    void OpenGLGraphicsPlugin::CheckState(const char* file_line) const
    {
        static std::string last_file_line;
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            ReportF("CheckState got %s at %s, last good check at %s", glResultString(err).c_str(), file_line, last_file_line.c_str());
        }
        last_file_line = file_line;
    }

    void OpenGLGraphicsPlugin::MakeCurrent(bool bindToThread)
    {
#if defined(OS_WINDOWS)
        if (!window.context.hGLRC) {
            return;
        }
#elif defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
        if (!window.context.xDisplay) {
            return;
        }
#elif defined(OS_LINUX_XCB)
        if (!window.context.connection) {
            return;
        }
#else
#error "Platform not (yet) supported."
#endif
        if (bindToThread) {
            ksGpuContext_SetCurrent(&window.context);
        }
        else {
            ksGpuContext_UnsetCurrent(&window.context);
        }
    }

    void OpenGLGraphicsPlugin::InitializeResources()
    {
#if !defined(OS_APPLE_MACOS)
#if !defined(NDEBUG)
        XRC_CHECK_THROW_GLCMD(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
#endif  // !defined(NDEBUG)
#endif  // !defined(OS_APPLE_MACOS)

        XRC_CHECK_THROW_GLCMD(glGenFramebuffers(1, &m_swapchainFramebuffer));
        //ReportF("Got fb %d", m_swapchainFramebuffer);

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &VertexShaderGlsl, nullptr);
        glCompileShader(vertexShader);
        CheckGLShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &FragmentShaderGlsl, nullptr);
        glCompileShader(fragmentShader);
        CheckGLShader(fragmentShader);

        m_program = glCreateProgram();
        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);
        glLinkProgram(m_program);
        CheckGLProgram(m_program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

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

    void OpenGLGraphicsPlugin::CheckFramebuffer(GLuint fb) const
    {
#if !defined(OS_APPLE_MACOS)
        GLenum st = glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER);
        if (st == GL_FRAMEBUFFER_COMPLETE)
            return;
        std::string status;
        switch (st) {
        case GL_FRAMEBUFFER_COMPLETE:
            status = "COMPLETE";
            break;
        case GL_FRAMEBUFFER_UNDEFINED:
            status = "UNDEFINED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            status = "INCOMPLETE_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            status = "INCOMPLETE_MISSING_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            status = "INCOMPLETE_DRAW_BUFFER";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            status = "INCOMPLETE_READ_BUFFER";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            status = "UNSUPPORTED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            status = "INCOMPLETE_MULTISAMPLE";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            status = "INCOMPLETE_LAYER_TARGETS";
            break;
        default:
            status = "<unknown " + std::to_string(st) + ">";
            break;
        }
        XRC_THROW("CheckFramebuffer " + std::to_string(fb) + " is " + status);
#else
        (void)fb;
#endif  // !defined(OS_APPLE_MACOS)
    }

    void OpenGLGraphicsPlugin::ClearSwapchainCache()
    {
        m_swapchainImageDataMap.Reset();
    }

    void OpenGLGraphicsPlugin::ShutdownDevice()
    {
        if (m_swapchainFramebuffer != 0) {
            glDeleteFramebuffers(1, &m_swapchainFramebuffer);
        }
        if (m_program != 0) {
            glDeleteProgram(m_program);
        }

        // Reset the swapchains to avoid calling Vulkan functions in the dtors after
        // we've shut down the device.
        m_swapchainImageDataMap.Reset();
        m_cubeMesh = {};
        m_meshes.clear();
        m_gltfInstances.clear();
        m_gltfModels.clear();
        m_pbrResources.reset();

        deleteGLContext();
    }

    std::string OpenGLGraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        return ::Conformance::GetImageFormatName(GetSwapchainFormatData(), imageFormat);
    }

    bool OpenGLGraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        return ::Conformance::IsImageFormatKnown(GetSwapchainFormatData(), imageFormat);
    }

    bool OpenGLGraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                                int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        *swapchainTestParameters = ::Conformance::GetSwapchainCreateTestParameters(GetSwapchainFormatData(), imageFormat);
        return true;
    }

    bool OpenGLGraphicsPlugin::ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                                       uint32_t* imageCount) const
    {
        // OK to use CHECK and REQUIRE in here because this is always called from within a test.
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
        // List of supported color swapchain formats, note sRGB formats skipped due to CTS bug.
        // The order of this list does not effect the priority of selecting formats, the runtime list defines that.
        const std::array<GLenum, 6> f{
            GL_RGB10_A2,
            GL_RGBA16,
            GL_RGBA16F,
            GL_RGBA32F,
            // The two below should only be used as a fallback, as they are linear color formats without enough bits for color
            // depth, thus leading to banding.
            GL_RGBA8,
            GL_RGBA8_SNORM,
        };

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
        const std::array<GLenum, 5> f{
            GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT16,
        };

        const int64_t* formatArrayEnd = imageFormatArray + count;
        auto it = std::find_first_of(imageFormatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return imageFormatArray[0];
        }

        return *it;
    }

    int64_t OpenGLGraphicsPlugin::SelectMotionVectorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const
    {
        // List of swapchain formats suitable for motion vectors.
        const std::array<GLenum, 2> f{
            GL_RGBA16F,
            GL_RGB16F,
        };

        const int64_t* formatArrayEnd = imageFormatArray + count;
        auto it = std::find_first_of(imageFormatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return imageFormatArray[0];
        }

        return *it;
    }

    int64_t OpenGLGraphicsPlugin::GetSRGBA8Format() const
    {
        return GL_SRGB8_ALPHA8;
    }

    ISwapchainImageData* OpenGLGraphicsPlugin::AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {

        auto typedResult = std::make_unique<OpenGLSwapchainImageData>(uint32_t(size), swapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    inline ISwapchainImageData* OpenGLGraphicsPlugin::AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo)
    {

        auto typedResult =
            std::make_unique<OpenGLSwapchainImageData>(uint32_t(size), colorSwapchainCreateInfo, depthSwapchain, depthSwapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    void OpenGLGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, uint32_t arraySlice, const RGBAImage& image)
    {
        OpenGLSwapchainImageData* swapchainData;
        uint32_t imageIndex;
        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImage);

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR*>(swapchainImage)->image;
        const GLint mip = 0;
        const GLint x = 0;
        const GLint z = arraySlice;
        const GLsizei w = swapchainData->Width();
        const GLsizei h = swapchainData->Height();
        if (swapchainData->HasMultipleSlices()) {
            XRC_CHECK_THROW_GLCMD(glBindTexture(GL_TEXTURE_2D_ARRAY, colorTexture));
            for (GLint y = 0; y < h; ++y) {
                const void* pixels = &image.pixels[(h - 1 - y) * w];
                XRC_CHECK_THROW_GLCMD(glTexSubImage3D(GL_TEXTURE_2D_ARRAY, mip, x, y, z, w, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
            }
        }
        else {
            XRC_CHECK_THROW_GLCMD(glBindTexture(GL_TEXTURE_2D, colorTexture));
            for (GLint y = 0; y < h; ++y) {
                const void* pixels = &image.pixels[(h - 1 - y) * w];
                XRC_CHECK_THROW_GLCMD(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, w, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
            }
        }
    }

    void OpenGLGraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                               XrColor4f color)
    {
        OpenGLSwapchainImageData* swapchainData;
        uint32_t imageIndex;
        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        XRC_CHECK_THROW_GLCMD(glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer));

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR*>(colorSwapchainImage)->image;
        const uint32_t depthTexture = swapchainData->GetDepthImageForColorIndex(imageIndex).image;

        bool imageArray = swapchainData->HasMultipleSlices();
        GLenum texTarget = TexTarget(imageArray, swapchainData->IsMultisample());
        if (imageArray) {
            XRC_CHECK_THROW_GLCMD(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0, imageArrayIndex));
            XRC_CHECK_THROW_GLCMD(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0, imageArrayIndex));
        }
        else {
            XRC_CHECK_THROW_GLCMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texTarget, colorTexture, 0));
            XRC_CHECK_THROW_GLCMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texTarget, depthTexture, 0));
        }

        CheckFramebuffer(m_swapchainFramebuffer);

        GLint x = 0;
        GLint y = 0;
        GLsizei w = swapchainData->Width();
        GLsizei h = swapchainData->Height();
        XRC_CHECK_THROW_GLCMD(glViewport(x, y, w, h));
        XRC_CHECK_THROW_GLCMD(glScissor(x, y, w, h));

        XRC_CHECK_THROW_GLCMD(glEnable(GL_SCISSOR_TEST));

        // Clear swapchain and depth buffer.
        XRC_CHECK_THROW_GLCMD(glClearColor(color.r, color.g, color.b, color.a));
        XRC_CHECK_THROW_GLCMD(glClearDepth(1.0f));
        XRC_CHECK_THROW_GLCMD(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    MeshHandle OpenGLGraphicsPlugin::MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx)
    {
        auto handle = m_meshes.emplace_back(m_vertexAttribCoords, m_vertexAttribColor, idx.data(), (uint32_t)idx.size(), vtx.data(),
                                            (uint32_t)vtx.size());

        return handle;
    }

    GLTFModelHandle OpenGLGraphicsPlugin::LoadGLTF(std::shared_ptr<tinygltf::Model> tinygltfModel)
    {
        std::shared_ptr<Pbr::Model> pbrModel = Gltf::FromGltfObject(*m_pbrResources, *tinygltfModel);
        auto handle = m_gltfModels.emplace_back(std::move(pbrModel));
        return handle;
    }

    std::shared_ptr<Pbr::Model> OpenGLGraphicsPlugin::GetPbrModel(GLTFModelHandle handle) const
    {
        return m_gltfModels[handle];
    }

    GLTFModelInstanceHandle OpenGLGraphicsPlugin::CreateGLTFModelInstance(GLTFModelHandle handle)
    {
        auto pbrModelInstance = Pbr::GLModelInstance(*m_pbrResources, GetPbrModel(handle));
        auto instanceHandle = m_gltfInstances.emplace_back(std::move(pbrModelInstance));
        return instanceHandle;
    }

    Pbr::ModelInstance& OpenGLGraphicsPlugin::GetModelInstance(GLTFModelInstanceHandle handle)
    {
        return m_gltfInstances[handle].GetModelInstance();
    }

    void OpenGLGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                          const XrSwapchainImageBaseHeader* colorSwapchainImage, const RenderParams& params)
    {
        OpenGLSwapchainImageData* swapchainData;
        uint32_t imageIndex;
        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        XRC_CHECK_THROW_GLCMD(glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer));

        GLint layer = layerView.subImage.imageArrayIndex;

        const GLuint colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR*>(colorSwapchainImage)->image;
        const GLuint depthTexture = swapchainData->GetDepthImageForColorIndex(imageIndex).image;

        bool imageArray = swapchainData->HasMultipleSlices();
        GLenum texTarget = TexTarget(imageArray, swapchainData->IsMultisample());
        if (imageArray) {
            XRC_CHECK_THROW_GLCMD(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0, layer));
            XRC_CHECK_THROW_GLCMD(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0, layer));
        }
        else {
            XRC_CHECK_THROW_GLCMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texTarget, colorTexture, 0));
            XRC_CHECK_THROW_GLCMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texTarget, depthTexture, 0));
        }

        CheckFramebuffer(m_swapchainFramebuffer);

        GLint x = layerView.subImage.imageRect.offset.x;
        GLint y = layerView.subImage.imageRect.offset.y;
        GLsizei w = layerView.subImage.imageRect.extent.width;
        GLsizei h = layerView.subImage.imageRect.extent.height;
        XRC_CHECK_THROW_GLCMD(glViewport(x, y, w, h));
        XRC_CHECK_THROW_GLCMD(glScissor(x, y, w, h));

        XRC_CHECK_THROW_GLCMD(glEnable(GL_SCISSOR_TEST));
        XRC_CHECK_THROW_GLCMD(glEnable(GL_DEPTH_TEST));
        XRC_CHECK_THROW_GLCMD(glEnable(GL_CULL_FACE));
        XRC_CHECK_THROW_GLCMD(glFrontFace(GL_CW));
        XRC_CHECK_THROW_GLCMD(glCullFace(GL_BACK));

        // Set shaders and uniform variables.
        XRC_CHECK_THROW_GLCMD(glUseProgram(m_program));

        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_OPENGL, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrMatrix4x4f_CreateFromRigidTransform(&toView, &pose);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);
        MeshHandle lastMeshHandle;

        const auto drawMesh = [this, &vp, &lastMeshHandle](const MeshDrawable mesh) {
            OpenGLMesh& glMesh = m_meshes[mesh.handle];
            if (mesh.handle != lastMeshHandle) {
                // We are now rendering a new mesh
                XRC_CHECK_THROW_GLCMD(glBindVertexArray(glMesh.m_vao));
                XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, glMesh.m_vertexBuffer));
                XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.m_indexBuffer));

                lastMeshHandle = mesh.handle;
            }

            // Compute the model-view-projection transform and set it..
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &mesh.params.pose.position, &mesh.params.pose.orientation,
                                                        &mesh.params.scale);
            XrMatrix4x4f mvp;
            XrMatrix4x4f_Multiply(&mvp, &vp, &model);
            glUniformMatrix4fv(m_modelViewProjectionUniformLocation, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&mvp));
            glUniform4fv(m_tintColorUniformLocation, 1, reinterpret_cast<const GLfloat*>(&mesh.tintColor));

            // Draw the mesh.
            glDrawElements(GL_TRIANGLES, GLsizei(glMesh.m_numIndices), GL_UNSIGNED_SHORT, nullptr);
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

        glBindVertexArray(0);
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGL(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<OpenGLGraphicsPlugin>(std::move(platformPlugin));
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_OPENGL
