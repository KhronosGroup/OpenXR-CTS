// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalMaterial.h"

#include "MetalResources.h"
#include "MetalTexture.h"

#include "../PbrMaterial.h"

#include "utilities/throw_helpers.h"

#include <algorithm>

namespace Pbr
{

    MetalMaterial::MetalMaterial(const MetalResources& pbrResources)
    {
        m_constantBuffer =
            NS::TransferPtr(pbrResources.GetDevice()->newBuffer(sizeof(ConstantBufferData), MTL::ResourceStorageModeManaged));
    }

    std::shared_ptr<MetalMaterial> MetalMaterial::Clone(const MetalResources& pbrResources) const
    {
        auto clone = std::make_shared<MetalMaterial>(pbrResources);
        clone->CopyFrom(*this);
        clone->m_textures = m_textures;
        clone->m_samplers = m_samplers;
        return clone;
    }

    std::shared_ptr<MetalMaterial> MetalMaterial::CreateFlat(const MetalResources& pbrResources, RGBAColor baseColorFactor,
                                                             float roughnessFactor, float metallicFactor, RGBColor emissiveFactor)
    {
        std::shared_ptr<MetalMaterial> material = std::make_shared<MetalMaterial>(pbrResources);

        if (baseColorFactor.a < 1.0f) {  // Alpha channel
            material->SetAlphaBlended(BlendState::AlphaBlended);
        }

        Pbr::MetalMaterial::ConstantBufferData& parameters = material->Parameters();
        parameters.BaseColorFactor = baseColorFactor;
        parameters.EmissiveFactor = emissiveFactor;
        parameters.MetallicFactor = metallicFactor;
        parameters.RoughnessFactor = roughnessFactor;

        NS::SharedPtr<MTL::SamplerState> defaultSampler = Pbr::MetalTexture::CreateSampler(pbrResources.GetDevice().get());
        material->SetTexture(ShaderSlots::BaseColor, pbrResources.CreateTypedSolidColorTexture(RGBA::White).get(), defaultSampler.get());
        material->SetTexture(ShaderSlots::MetallicRoughness, pbrResources.CreateTypedSolidColorTexture(RGBA::White).get(),
                             defaultSampler.get());
        /// No occlusion.
        material->SetTexture(ShaderSlots::Occlusion, pbrResources.CreateTypedSolidColorTexture(RGBA::White).get(), defaultSampler.get());
        /// Flat normal.
        material->SetTexture(ShaderSlots::Normal, pbrResources.CreateTypedSolidColorTexture(RGBA::FlatNormal).get(), defaultSampler.get());
        material->SetTexture(ShaderSlots::Emissive, pbrResources.CreateTypedSolidColorTexture(RGBA::White).get(), defaultSampler.get());

        return material;
    }

    void MetalMaterial::SetTexture(ShaderSlots::PSMaterial slot, MTL::Texture* texture, MTL::SamplerState* sampler)
    {
        m_textures[slot] = NS::RetainPtr(texture);
        if (sampler) {
            m_samplers[slot] = NS::RetainPtr(sampler);
        }
        else {
            m_samplers[slot].reset();
        }
    }

    void MetalMaterial::Bind(MTL::RenderCommandEncoder* renderCommandEncoder, const MetalResources& pbrResources) const
    {
        renderCommandEncoder->pushDebugGroup(MTLSTR("MetalMaterial::Bind"));

        if (m_parametersChanged) {
            assert(m_constantBuffer->length() == sizeof(m_parameters));
            memcpy(m_constantBuffer->contents(), &m_parameters, m_constantBuffer->length());
            m_constantBuffer->didModifyRange(NS::Range::Make(0, m_constantBuffer->length()));
            m_parametersChanged = false;
        }
        renderCommandEncoder->setFragmentBuffer(m_constantBuffer.get(), 0, Pbr::ShaderSlots::ConstantBuffers::Material);
        static_assert(Pbr::ShaderSlots::BaseColor == 0, "BaseColor must be the first slot");

        MTL::TriangleFillMode mtlFillMode =
            (pbrResources.GetFillMode() == FillMode::Solid) ? MTL::TriangleFillModeFill : MTL::TriangleFillModeLines;
        renderCommandEncoder->setTriangleFillMode(mtlFillMode);
        MTL::Winding mtlWinding = (pbrResources.GetFrontFaceWindingOrder() == FrontFaceWindingOrder::ClockWise)
                                      ? MTL::WindingClockwise
                                      : MTL::WindingCounterClockwise;
        renderCommandEncoder->setFrontFacingWinding(mtlWinding);
        MTL::CullMode mtlCullMode = (GetDoubleSided() == DoubleSided::DoubleSided) ? MTL::CullModeNone : MTL::CullModeBack;
        renderCommandEncoder->setCullMode(mtlCullMode);

        renderCommandEncoder->setFragmentTexture(m_textures[ShaderSlots::PSMaterial::BaseColor].get(), ShaderSlots::PSMaterial::BaseColor);
        renderCommandEncoder->setFragmentTexture(m_textures[ShaderSlots::PSMaterial::MetallicRoughness].get(),
                                                 ShaderSlots::PSMaterial::MetallicRoughness);
        renderCommandEncoder->setFragmentTexture(m_textures[ShaderSlots::PSMaterial::Normal].get(), ShaderSlots::PSMaterial::Normal);
        renderCommandEncoder->setFragmentTexture(m_textures[ShaderSlots::PSMaterial::Occlusion].get(), ShaderSlots::PSMaterial::Occlusion);
        renderCommandEncoder->setFragmentTexture(m_textures[ShaderSlots::PSMaterial::Emissive].get(), ShaderSlots::PSMaterial::Emissive);

        renderCommandEncoder->setFragmentSamplerState(m_samplers[ShaderSlots::PSMaterial::BaseColor].get(),
                                                      ShaderSlots::PSMaterial::BaseColor);
        renderCommandEncoder->setFragmentSamplerState(m_samplers[ShaderSlots::PSMaterial::MetallicRoughness].get(),
                                                      ShaderSlots::PSMaterial::MetallicRoughness);
        renderCommandEncoder->setFragmentSamplerState(m_samplers[ShaderSlots::PSMaterial::Normal].get(), ShaderSlots::PSMaterial::Normal);
        renderCommandEncoder->setFragmentSamplerState(m_samplers[ShaderSlots::PSMaterial::Occlusion].get(),
                                                      ShaderSlots::PSMaterial::Occlusion);
        renderCommandEncoder->setFragmentSamplerState(m_samplers[ShaderSlots::PSMaterial::Emissive].get(),
                                                      ShaderSlots::PSMaterial::Emissive);

        renderCommandEncoder->popDebugGroup();
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_METAL)
