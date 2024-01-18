// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "GLResources.h"

#include "GLCommon.h"
#include "GLMaterial.h"
#include "GLPrimitive.h"
#include "GLTexture.h"
#include "GLTextureCache.h"
#include "../GlslBuffers.h"

#include "../../gltf/GltfHelper.h"
#include "../PbrCommon.h"
#include "../PbrHandles.h"
#include "../PbrSharedState.h"

#include "common/gfxwrapper_opengl.h"
#include "utilities/opengl_utils.h"

#include <nonstd/type.hpp>
#include <tinygltf/tiny_gltf.h>

#include <map>
#include <stdexcept>
#include <stdint.h>
#include <tuple>
#include <utility>
#include <vector>

namespace Pbr
{
    struct ITexture;
    struct Material;
}  // namespace Pbr

// IWYU pragma: begin_keep
static const char* g_PbrVertexShader =
#ifdef XR_USE_GRAPHICS_API_OPENGL
#include <PbrVertexShader_glsl_src.h>
#elif XR_USE_GRAPHICS_API_OPENGL_ES
#include <PbrVertexShader_glsl_src_es.h>
#endif
    ;

static const char* g_PbrPixelShader =
#ifdef XR_USE_GRAPHICS_API_OPENGL
#include <PbrPixelShader_glsl_src.h>
#elif XR_USE_GRAPHICS_API_OPENGL_ES
#include <PbrPixelShader_glsl_src_es.h>
#endif
    ;
// IWYU pragma: end_keep

namespace Pbr
{
    using ImageKey = std::tuple<const tinygltf::Image*, bool>;  // Item1 is a pointer to the image, Item2 is sRGB.

    class Program
    {
    public:
        Program() = default;
        Program(const char** vertexShader, const char** fragmentShader)
        {
            m_vertexShader.adopt(glCreateShader(GL_VERTEX_SHADER));
            XRC_CHECK_THROW_GLCMD(glShaderSource(m_vertexShader.get(), 1, vertexShader, nullptr));
            XRC_CHECK_THROW_GLCMD(glCompileShader(m_vertexShader.get()));
            Conformance::CheckGLShader(m_vertexShader.get());

            m_fragmentShader.adopt(glCreateShader(GL_FRAGMENT_SHADER));
            XRC_CHECK_THROW_GLCMD(glShaderSource(m_fragmentShader.get(), 1, fragmentShader, nullptr));
            XRC_CHECK_THROW_GLCMD(glCompileShader(m_fragmentShader.get()));
            Conformance::CheckGLShader(m_fragmentShader.get());

            m_program.adopt(glCreateProgram());
            XRC_CHECK_THROW_GLCMD(glAttachShader(m_program.get(), m_vertexShader.get()));
            XRC_CHECK_THROW_GLCMD(glAttachShader(m_program.get(), m_fragmentShader.get()));
            XRC_CHECK_THROW_GLCMD(glLinkProgram(m_program.get()));
            Conformance::CheckGLProgram(m_program.get());
        }

        void Bind()
        {
            XRC_CHECK_THROW_GLCMD(glUseProgram(m_program.get()));
        }

    private:
        ScopedGLShader m_vertexShader{};
        ScopedGLShader m_fragmentShader{};
        ScopedGLProgram m_program{};
    };

    struct GLResources::Impl
    {
        void Initialize()
        {
            Resources.PbrProgram = Program(&g_PbrVertexShader, &g_PbrPixelShader);

            // Set up the constant buffers.
            XRC_CHECK_THROW_GLCMD(glGenBuffers(1, Resources.SceneConstantBuffer.resetAndPut()));
            XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_UNIFORM_BUFFER, Resources.SceneConstantBuffer.get()));
            XRC_CHECK_THROW_GLCMD(glBufferData(GL_UNIFORM_BUFFER, sizeof(Glsl::SceneConstantBuffer), nullptr, GL_DYNAMIC_DRAW));

            // Samplers for environment map and BRDF.
            Resources.BrdfSampler = GLTexture::CreateSampler();
            Resources.EnvironmentMapSampler = GLTexture::CreateSampler();

            Resources.SolidColorTextureCache.Init();
        }

        struct DeviceResources
        {
            Program PbrProgram{};
            ScopedGLSampler BrdfSampler;
            ScopedGLSampler EnvironmentMapSampler;
            ScopedGLBuffer SceneConstantBuffer;
            std::shared_ptr<ScopedGLTexture> BrdfLut;
            std::shared_ptr<ScopedGLTexture> SpecularEnvironmentMap;
            std::shared_ptr<ScopedGLTexture> DiffuseEnvironmentMap;
            mutable GLTextureCache SolidColorTextureCache{};
        };
        PrimitiveCollection<GLPrimitive> Primitives;

        DeviceResources Resources;
        Glsl::SceneConstantBuffer SceneBuffer;

        struct LoaderResources
        {
            // Create D3D cache for reuse of texture views and samplers when possible.
            std::map<ImageKey, std::shared_ptr<ScopedGLTexture>> imageMap;
            std::map<const tinygltf::Sampler*, std::shared_ptr<ScopedGLSampler>> samplerMap;
        };
        LoaderResources loaderResources;
    };

    GLResources::GLResources() : m_impl(std::make_unique<Impl>())
    {
        m_impl->Initialize();
    }

    GLResources::GLResources(GLResources&& resources) noexcept = default;

    GLResources::~GLResources() = default;

    // Create a GL texture from a tinygltf Image.
    static ScopedGLTexture LoadGLTFImage(const tinygltf::Image& image, bool sRGB)
    {
        // First convert the image to RGBA if it isn't already.
        std::vector<uint8_t> tempBuffer;
        const uint8_t* rgbaBuffer = GltfHelper::ReadImageAsRGBA(image, &tempBuffer);
        Internal::ThrowIf(rgbaBuffer == nullptr, "Failed to read image");

        const GLenum format = sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        return Pbr::GLTexture::CreateTexture(rgbaBuffer, 4, image.width, image.height, format);
    }

    static GLenum ConvertMinFilter(int glMinFilter)
    {
        return glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST
                   ? GL_NEAREST
                   : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR
                         ? GL_LINEAR
                         : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
                               ? GL_NEAREST_MIPMAP_NEAREST
                               : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST
                                     ? GL_LINEAR_MIPMAP_NEAREST
                                     : glMinFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR
                                           ? GL_NEAREST_MIPMAP_LINEAR
                                           : glMinFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR ? GL_LINEAR_MIPMAP_LINEAR
                                                                                                         : GL_NEAREST;
    }
    static GLenum ConvertMagFilter(int glMagFilter)
    {
        return glMagFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ? GL_NEAREST
                                                              : glMagFilter == TINYGLTF_TEXTURE_FILTER_LINEAR ? GL_LINEAR : GL_NEAREST;
    }

    // Create a GL sampler from a tinygltf Sampler.
    static ScopedGLSampler CreateGLTFSampler(const tinygltf::Sampler& sampler)
    {
        ScopedGLSampler glSampler{};
        XRC_CHECK_THROW_GLCMD(glGenSamplers(1, glSampler.resetAndPut()));

        GLenum minFilter = ConvertMinFilter(sampler.minFilter);
        GLenum magFilter = ConvertMagFilter(sampler.magFilter);

        XRC_CHECK_THROW_GLCMD(glSamplerParameteri(glSampler.get(), GL_TEXTURE_MIN_FILTER, minFilter));
        XRC_CHECK_THROW_GLCMD(glSamplerParameteri(glSampler.get(), GL_TEXTURE_MAG_FILTER, magFilter));

        GLenum addressModeS = sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE
                                  ? GL_CLAMP_TO_EDGE
                                  : sampler.wrapS == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? GL_MIRRORED_REPEAT : GL_REPEAT;
        GLenum addressModeT = sampler.wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE
                                  ? GL_CLAMP_TO_EDGE
                                  : sampler.wrapT == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? GL_MIRRORED_REPEAT : GL_REPEAT;
        GLenum addressModeR = GL_REPEAT;

        XRC_CHECK_THROW_GLCMD(glSamplerParameteri(glSampler.get(), GL_TEXTURE_WRAP_S, addressModeS));
        XRC_CHECK_THROW_GLCMD(glSamplerParameteri(glSampler.get(), GL_TEXTURE_WRAP_T, addressModeT));
        XRC_CHECK_THROW_GLCMD(glSamplerParameteri(glSampler.get(), GL_TEXTURE_WRAP_R, addressModeR));

        return glSampler;
    }

    /* IResources implementations */
    std::shared_ptr<Material> GLResources::CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor, float metallicFactor,
                                                              RGBColor emissiveFactor)
    {
        return GLMaterial::CreateFlat(*this, baseColorFactor, roughnessFactor, metallicFactor, emissiveFactor);
    }
    std::shared_ptr<Material> GLResources::CreateMaterial()
    {
        return std::make_shared<GLMaterial>(*this);
    }
    std::shared_ptr<ITexture> GLResources::CreateSolidColorTexture(RGBAColor color)
    {
        // TODO maybe unused
        auto ret = std::make_shared<Pbr::GLTextureAndSampler>();
        ret->srv = CreateTypedSolidColorTexture(color);
        return ret;
    }

    void GLResources::LoadTexture(const std::shared_ptr<Material>& material, Pbr::ShaderSlots::PSMaterial slot,
                                  const tinygltf::Image* image, const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA)
    {
        auto pbrMaterial = std::dynamic_pointer_cast<GLMaterial>(material);
        if (!pbrMaterial) {
            throw std::logic_error("Wrong type of material");
        }
        // Find or load the image referenced by the texture.
        const ImageKey imageKey = std::make_tuple(image, sRGB);
        std::shared_ptr<ScopedGLTexture> textureView =
            image != nullptr ? m_impl->loaderResources.imageMap[imageKey] : CreateTypedSolidColorTexture(defaultRGBA);
        if (!textureView)  // If not cached, load the image and store it in the texture cache.
        {
            // TODO: Generate mipmaps if sampler's minification filter (minFilter) uses mipmapping.
            // TODO: If texture is not power-of-two and (sampler has wrapping=repeat/mirrored_repeat OR minFilter uses
            // mipmapping), resize to power-of-two.
            textureView = std::make_shared<ScopedGLTexture>(LoadGLTFImage(*image, sRGB));
            m_impl->loaderResources.imageMap[imageKey] = textureView;
        }

        // Find or create the sampler referenced by the texture.
        std::shared_ptr<ScopedGLSampler> samplerState = m_impl->loaderResources.samplerMap[sampler];
        if (!samplerState)  // If not cached, create the sampler and store it in the sampler cache.
        {
            samplerState = std::make_shared<ScopedGLSampler>(sampler != nullptr ? CreateGLTFSampler(*sampler)
                                                                                : Pbr::GLTexture::CreateSampler(GL_REPEAT));
            m_impl->loaderResources.samplerMap[sampler] = samplerState;
        }

        pbrMaterial->SetTexture(slot, textureView, samplerState);
    }
    void GLResources::DropLoaderCaches()
    {
        m_impl->loaderResources = {};
    }

    void GLResources::SetBrdfLut(std::shared_ptr<ScopedGLTexture> brdfLut)
    {
        m_impl->Resources.BrdfLut = std::move(brdfLut);
    }

    void GLResources::SetLight(XrVector3f direction, RGBColor diffuseColor)
    {
        m_impl->SceneBuffer.LightDirection = direction;
        m_impl->SceneBuffer.LightDiffuseColor = diffuseColor;
    }

    void GLResources::SetViewProjection(XrMatrix4x4f view, XrMatrix4x4f projection) const
    {
        XrMatrix4x4f_Multiply(&m_impl->SceneBuffer.ViewProjection, &projection, &view);

        XrMatrix4x4f inv;
        XrMatrix4x4f_Invert(&inv, &view);
        m_impl->SceneBuffer.EyePosition = {inv.m[12], inv.m[13], inv.m[14]};
    }

    void GLResources::SetEnvironmentMap(std::shared_ptr<ScopedGLTexture> specularEnvironmentMap,
                                        std::shared_ptr<ScopedGLTexture> diffuseEnvironmentMap)
    {
        // TODO: get number of mip levels
        int mipLevels = 1;
        m_impl->SceneBuffer.NumSpecularMipLevels = mipLevels;
        m_impl->Resources.SpecularEnvironmentMap = std::move(specularEnvironmentMap);
        m_impl->Resources.DiffuseEnvironmentMap = std::move(diffuseEnvironmentMap);
    }

    std::shared_ptr<ScopedGLTexture> GLResources::CreateTypedSolidColorTexture(RGBAColor color) const
    {
        return m_impl->Resources.SolidColorTextureCache.CreateTypedSolidColorTexture(color);
    }

    void GLResources::Bind() const
    {
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_UNIFORM_BUFFER, m_impl->Resources.SceneConstantBuffer.get()));
        XRC_CHECK_THROW_GLCMD(glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Glsl::SceneConstantBuffer), &m_impl->SceneBuffer));

        m_impl->Resources.PbrProgram.Bind();

        XRC_CHECK_THROW_GLCMD(
            glBindBufferBase(GL_UNIFORM_BUFFER, ShaderSlots::ConstantBuffers::Scene, m_impl->Resources.SceneConstantBuffer.get()));
        // ModelConstantBuffer is bound in GLModelInstance::Render

        XRC_CHECK_THROW_GLCMD(  //
            glActiveTexture(GL_TEXTURE0 + ShaderSlots::GLSL::MaterialTexturesOffset + ShaderSlots::Brdf));
        XRC_CHECK_THROW_GLCMD(glBindTexture(GL_TEXTURE_2D, m_impl->Resources.BrdfLut->get()));
        XRC_CHECK_THROW_GLCMD(glBindSampler(ShaderSlots::Pbr::Brdf, m_impl->Resources.BrdfSampler.get()));

        XRC_CHECK_THROW_GLCMD(  //
            glActiveTexture(GL_TEXTURE0 + ShaderSlots::GLSL::MaterialTexturesOffset + ShaderSlots::EnvironmentMap::DiffuseTexture));
        XRC_CHECK_THROW_GLCMD(glBindTexture(GL_TEXTURE_CUBE_MAP, m_impl->Resources.DiffuseEnvironmentMap->get()));
        XRC_CHECK_THROW_GLCMD(glBindSampler(ShaderSlots::EnvironmentMap::DiffuseTexture, m_impl->Resources.EnvironmentMapSampler.get()));

        XRC_CHECK_THROW_GLCMD(  //
            glActiveTexture(GL_TEXTURE0 + ShaderSlots::GLSL::MaterialTexturesOffset + ShaderSlots::EnvironmentMap::SpecularTexture));
        XRC_CHECK_THROW_GLCMD(glBindTexture(GL_TEXTURE_CUBE_MAP, m_impl->Resources.SpecularEnvironmentMap->get()));
        XRC_CHECK_THROW_GLCMD(glBindSampler(ShaderSlots::EnvironmentMap::SpecularTexture, m_impl->Resources.EnvironmentMapSampler.get()));
    }

    PrimitiveHandle GLResources::MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                               const std::shared_ptr<Pbr::Material>& material)
    {
        auto typedMaterial = std::dynamic_pointer_cast<Pbr::GLMaterial>(material);
        if (!typedMaterial) {
            throw std::logic_error("Got the wrong type of material");
        }
        return m_impl->Primitives.emplace_back(primitiveBuilder, typedMaterial);
    }

    GLPrimitive& GLResources::GetPrimitive(PrimitiveHandle p)
    {
        return m_impl->Primitives[p];
    }

    const GLPrimitive& GLResources::GetPrimitive(PrimitiveHandle p) const
    {
        return m_impl->Primitives[p];
    }

    void GLResources::SetFillMode(FillMode mode)
    {
        m_sharedState.SetFillMode(mode);
    }

    FillMode GLResources::GetFillMode() const
    {
        return m_sharedState.GetFillMode();
    }

    void GLResources::SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder)
    {
        m_sharedState.SetFrontFaceWindingOrder(windingOrder);
    }

    FrontFaceWindingOrder GLResources::GetFrontFaceWindingOrder() const
    {
        return m_sharedState.GetFrontFaceWindingOrder();
    }

    void GLResources::SetDepthDirection(DepthDirection depthDirection)
    {
        m_sharedState.SetDepthDirection(depthDirection);
    }

    void GLResources::SetBlendState(bool enabled) const
    {
        if (enabled) {
            XRC_CHECK_THROW_GLCMD(glEnable(GL_BLEND));
            XRC_CHECK_THROW_GLCMD(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));
            XRC_CHECK_THROW_GLCMD(glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));
        }
        else {

            XRC_CHECK_THROW_GLCMD(glDisable(GL_BLEND));
        }
    }

    void GLResources::SetRasterizerState(bool doubleSided) const
    {
        if (doubleSided) {
            XRC_CHECK_THROW_GLCMD(glDisable(GL_CULL_FACE));
        }
        else {

            XRC_CHECK_THROW_GLCMD(glEnable(GL_CULL_FACE));
        }
#ifdef XR_USE_GRAPHICS_API_OPENGL
        // This does not set double-sided rendering, it says we control both front and back
        XRC_CHECK_THROW_GLCMD(glPolygonMode(GL_FRONT_AND_BACK, m_sharedState.GetFillMode() == FillMode::Wireframe ? GL_LINE : GL_FILL));
#elif XR_USE_GRAPHICS_API_OPENGL_ES
        // done during rendering using GL_LINES instead
#endif
    }

    void GLResources::SetDepthStencilState(bool disableDepthWrite) const
    {
        XRC_CHECK_THROW_GLCMD(glDepthFunc(m_sharedState.GetDepthDirection() == DepthDirection::Reversed ? GL_GREATER : GL_LESS));
        XRC_CHECK_THROW_GLCMD(glDepthMask(disableDepthWrite ? GL_FALSE : GL_TRUE));
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)
