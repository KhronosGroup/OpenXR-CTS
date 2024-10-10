// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "GLMaterial.h"

#include "GLResources.h"
#include "GLTexture.h"

#include "../PbrMaterial.h"

#include "common/gfxwrapper_opengl.h"
#include "utilities/opengl_utils.h"

#include <stdint.h>

namespace Pbr
{
    GLMaterial::GLMaterial(Pbr::GLResources const& /* pbrResources */)
    {
        XRC_CHECK_THROW_GLCMD(glGenBuffers(1, m_constantBuffer.resetAndPut()));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_constantBuffer.get()));
        XRC_CHECK_THROW_GLCMD(glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ConstantBufferData), &m_parameters, GL_STATIC_DRAW));
    }

    std::shared_ptr<GLMaterial> GLMaterial::Clone(Pbr::GLResources const& pbrResources) const
    {
        auto clone = std::make_shared<GLMaterial>(pbrResources);
        clone->CopyFrom(*this);
        clone->m_textures = m_textures;
        clone->m_samplers = m_samplers;
        return clone;
    }

    /* static */
    std::shared_ptr<GLMaterial> GLMaterial::CreateFlat(const GLResources& pbrResources, RGBAColor baseColorFactor,
                                                       float roughnessFactor /* = 1.0f */, float metallicFactor /* = 0.0f */,
                                                       RGBColor emissiveFactor /* = XMFLOAT3(0, 0, 0) */)
    {
        std::shared_ptr<GLMaterial> material = std::make_shared<GLMaterial>(pbrResources);

        if (baseColorFactor.a < 1.0f) {  // Alpha channel
            material->SetAlphaBlended(BlendState::AlphaBlended);
        }

        Pbr::GLMaterial::ConstantBufferData& parameters = material->Parameters();
        parameters.BaseColorFactor = baseColorFactor;
        parameters.EmissiveFactor = emissiveFactor;
        parameters.MetallicFactor = metallicFactor;
        parameters.RoughnessFactor = roughnessFactor;

        auto defaultSampler = std::make_shared<ScopedGLSampler>(Pbr::GLTexture::CreateSampler());
        material->SetTexture(ShaderSlots::BaseColor, pbrResources.CreateTypedSolidColorTexture(RGBA::White, true), defaultSampler);
        material->SetTexture(ShaderSlots::MetallicRoughness, pbrResources.CreateTypedSolidColorTexture(RGBA::White, false), defaultSampler);
        // No occlusion.
        material->SetTexture(ShaderSlots::Occlusion, pbrResources.CreateTypedSolidColorTexture(RGBA::White, false), defaultSampler);
        // Flat normal.
        material->SetTexture(ShaderSlots::Normal, pbrResources.CreateTypedSolidColorTexture(RGBA::FlatNormal, false), defaultSampler);
        material->SetTexture(ShaderSlots::Emissive, pbrResources.CreateTypedSolidColorTexture(RGBA::White, true), defaultSampler);

        return material;
    }

    void GLMaterial::SetTexture(ShaderSlots::PSMaterial slot, std::shared_ptr<ScopedGLTexture> textureView,
                                std::shared_ptr<ScopedGLSampler> sampler)
    {
        m_textures[slot] = textureView;

        if (sampler) {
            m_samplers[slot] = sampler;
        }
    }

    void GLMaterial::Bind(const GLResources& pbrResources) const
    {
        // If the parameters of the constant buffer have changed, update the constant buffer.
        if (m_parametersChanged) {
            m_parametersChanged = false;
            XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_UNIFORM_BUFFER, m_constantBuffer.get()));
            XRC_CHECK_THROW_GLCMD(glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ConstantBufferData), &m_parameters));
        }

        pbrResources.SetBlendState(m_alphaBlended == BlendState::AlphaBlended);
        pbrResources.SetDepthStencilState(m_alphaBlended == BlendState::AlphaBlended);
        pbrResources.SetRasterizerState(m_doubleSided == DoubleSided::DoubleSided);

        XRC_CHECK_THROW_GLCMD(glBindBufferBase(GL_UNIFORM_BUFFER, Pbr::ShaderSlots::ConstantBuffers::Material, m_constantBuffer.get()));

        static_assert(Pbr::ShaderSlots::BaseColor == 0, "BaseColor must be the first slot");

        std::array<std::shared_ptr<ScopedGLTexture>, TextureCount> textures;
        for (uint32_t texIndex = 0; texIndex < ShaderSlots::NumMaterialSlots; ++texIndex) {
            GLuint unit = ShaderSlots::GLSL::MaterialTexturesOffset + texIndex;
            XRC_CHECK_THROW_GLCMD(glActiveTexture(GL_TEXTURE0 + unit));
            XRC_CHECK_THROW_GLCMD(glBindTexture(GL_TEXTURE_2D, m_textures[texIndex]->get()));
            XRC_CHECK_THROW_GLCMD(glBindSampler(unit, m_samplers[texIndex]->get()));
        }
    }
}  // namespace Pbr

#endif
