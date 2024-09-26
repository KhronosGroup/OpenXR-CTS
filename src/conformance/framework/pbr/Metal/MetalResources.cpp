// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalFormats.h"
#include "MetalMaterial.h"
#include "MetalPipelineStates.h"
#include "MetalResources.h"
#include "MetalTexture.h"

#include "utilities/metal_utils.h"

#include "../../gltf/GltfHelper.h"
#include <tinygltf/tiny_gltf.h>
#include "../../report.h"

namespace
{
    std::vector<Conformance::Image::FormatParams> MakeSupportedFormatsList(MTL::Device* device)
    {
        std::vector<Conformance::Image::FormatParams> supported;
        for (auto& format : Pbr::GetMetalFormatMap()) {
            if (!Pbr::IsKnownFormatSupportedByDriver(device, format.second)) {
                continue;
            }
            supported.push_back(format.first);
        }
        return supported;
    }
}  // namespace

namespace Pbr
{

    MetalResources::MetalResources(MTL::Device* mtlDevice) : m_device(NS::RetainPtr(mtlDevice))
    {
        CreateDeviceDependentResources(mtlDevice);
    }

    MetalResources::MetalResources(MetalResources&& resources) = default;

    MetalResources::~MetalResources()
    {
        ReleaseDeviceDependentResources();
    }

    /* IGltfBuilder implementations */
    std::shared_ptr<Material> MetalResources::CreateFlatMaterial(RGBAColor baseColorFactor, float roughnessFactor, float metallicFactor,
                                                                 RGBColor emissiveFactor)
    {
        return MetalMaterial::CreateFlat(*this, baseColorFactor, roughnessFactor, metallicFactor, emissiveFactor);
    }
    std::shared_ptr<Material> MetalResources::CreateMaterial()
    {
        return std::make_shared<MetalMaterial>(*this);
    }
    std::shared_ptr<ITexture> MetalResources::CreateSolidColorTexture(RGBAColor color, bool sRGB)
    {
        auto ret = std::make_shared<Pbr::MetalTextureAndSampler>();
        ret->mtlTexture = CreateTypedSolidColorTexture(color, sRGB);
        return ret;
    }

    // Create a Metal texture from a tinygltf Image.
    static NS::SharedPtr<MTL::Texture> MetalLoadGLTFImage(MetalResources& pbrResources, const tinygltf::Image& image, bool sRGB)
    {
        NS::SharedPtr<MTL::Texture> result;

        NS::String* label = MTLSTR("<unknown>");
        if (!image.name.empty()) {
            label = NS::String::string(image.name.c_str(), NS::UTF8StringEncoding);  // autorelease
        }

        // First convert the image to RGBA if it isn't already.
        std::vector<uint8_t> tempBuffer;
        Conformance::Image::Image decodedImage = GltfHelper::DecodeImage(image, sRGB, pbrResources.GetSupportedFormats(), tempBuffer);

        return Pbr::MetalTexture::CreateTexture(pbrResources, decodedImage, label);
    }

    static MTL::SamplerMinMagFilter MetalConvertFilter(int glMinMagFilter)
    {
        switch (glMinMagFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            return MTL::SamplerMinMagFilterNearest;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            return MTL::SamplerMinMagFilterLinear;
        default:
            return MTL::SamplerMinMagFilterLinear;
        }
    }

    static MTL::SamplerAddressMode MetalConvertWrapMode(int wrapMode)
    {
        switch (wrapMode) {
        case TINYGLTF_TEXTURE_WRAP_REPEAT:
            return MTL::SamplerAddressModeRepeat;
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            return MTL::SamplerAddressModeClampToEdge;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            return MTL::SamplerAddressModeMirrorRepeat;
        default:
            return MTL::SamplerAddressModeClampToEdge;
        }
    }

    /// Create a Metal sampler state from a tinygltf Sampler.
    static NS::SharedPtr<MTL::SamplerState> MetalCreateGLTFSampler(MetalResources& pbrResources, const tinygltf::Sampler& sampler)
    {
        auto samplerDesc = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
        samplerDesc->setMinFilter(MetalConvertFilter(sampler.minFilter));
        samplerDesc->setMagFilter(MetalConvertFilter(sampler.magFilter));
        samplerDesc->setSAddressMode(MetalConvertWrapMode(sampler.wrapS));
        samplerDesc->setTAddressMode(MetalConvertWrapMode(sampler.wrapT));
        samplerDesc->setRAddressMode(MTL::SamplerAddressModeRepeat);
        samplerDesc->setMaxAnisotropy(1);
        samplerDesc->setCompareFunction(MTL::CompareFunctionAlways);
        samplerDesc->setLodMinClamp(0.0f);
        samplerDesc->setLodMaxClamp(FLT_MAX);

        auto samplerState = NS::TransferPtr(pbrResources.GetDevice()->newSamplerState(samplerDesc.get()));
        return samplerState;
    }

    void MetalResources::LoadTexture(const std::shared_ptr<Material>& material, Pbr::ShaderSlots::PSMaterial slot,
                                     const tinygltf::Image* image, const tinygltf::Sampler* sampler, bool sRGB, Pbr::RGBAColor defaultRGBA)
    {
        auto pbrMaterial = std::dynamic_pointer_cast<MetalMaterial>(material);
        if (!pbrMaterial) {
            throw std::logic_error("Wrong type of material");
        }
        // Find or load the image referenced by the texture.
        const ImageKey imageKey = std::make_tuple(image, sRGB);
        NS::SharedPtr<MTL::Texture> texture =
            image != nullptr ? m_LoaderResources.imageMap[imageKey] : CreateTypedSolidColorTexture(defaultRGBA, sRGB);
        if (!texture)  // If not cached, load the image and store it in the texture cache.
        {
            // TODO: Generate mipmaps if sampler's minification filter (minFilter) uses mipmapping.
            // TODO: If texture is not power-of-two and (sampler has wrapping=repeat/mirrored_repeat OR minFilter uses
            // mipmapping), resize to power-of-two.
            texture = MetalLoadGLTFImage(*this, *image, sRGB);
            m_LoaderResources.imageMap[imageKey] = texture;
        }

        /// Find or create the sampler referenced by the texture.
        NS::SharedPtr<MTL::SamplerState> samplerState = m_LoaderResources.samplerMap[sampler];
        if (!samplerState)  // If not cached, create the sampler and store it in the sampler cache.
        {
            samplerState = sampler != nullptr ? MetalCreateGLTFSampler(*this, *sampler)
                                              : Pbr::MetalTexture::CreateSampler(GetDevice().get(), MTL::SamplerAddressModeRepeat);
            m_LoaderResources.samplerMap[sampler] = samplerState;
        }

        pbrMaterial->SetTexture(slot, texture.get(), samplerState.get());
    }

    void MetalResources::DropLoaderCaches()
    {
        m_LoaderResources = {};
    }

    void MetalResources::SetBrdfLut(MTL::Texture* brdfLut)
    {
        m_Resources.BrdfLut = NS::RetainPtr(brdfLut);
    }

    void MetalResources::CreateDeviceDependentResources(MTL::Device* device)
    {
        NS::String* libraryPath = MTLSTR("../framework/pbr/PbrShader.metallib");
        NS::URL* libraryUrl = NS::URL::fileURLWithPath(libraryPath);  // autorelease
        NS::Error* error = nullptr;
        NS::SharedPtr<MTL::Library> shaderLibrary = NS::TransferPtr(device->newLibrary(libraryUrl, &error));
        if (!shaderLibrary) {
            Conformance::ReportF("Load shader library from %s, error: %s", libraryUrl->fileSystemRepresentation(),
                                 error->localizedDescription()->utf8String());
            throw std::logic_error("Unable to load shader library");
        }
        m_Resources.PbrVertexShader = NS::TransferPtr(shaderLibrary->newFunction(MTLSTR("VertexShaderPbr")));
        if (!m_Resources.PbrVertexShader) {
            throw std::logic_error("Invalid vertex function (VertexShaderPbr)");
        }
        m_Resources.PbrVertexShader->setLabel(MTLSTR("PbrVertexShader"));

        m_Resources.PbrPixelShader = NS::TransferPtr(shaderLibrary->newFunction(MTLSTR("FragmentShaderPbr")));
        if (!m_Resources.PbrPixelShader) {
            throw std::logic_error("Invalid fragment function (FragmentShaderPbr)");
        }
        m_Resources.PbrPixelShader->setLabel(MTLSTR("PbrPixelShader"));

        static_assert(sizeof(Vertex) == 17 * 4, "Unexpected Vertex size");

        const uint32_t VertexDataBufferIndex = 4;  // matches ConstantBuffers.VertexData in PbrShader.metal

        auto vd = NS::TransferPtr(MTL::VertexDescriptor::alloc()->init());
        vd->layouts()->object(4)->setStride(sizeof(Vertex));
        // XrVector3f Position;
        vd->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
        vd->attributes()->object(0)->setOffset(offsetof(Vertex, Position));
        vd->attributes()->object(0)->setBufferIndex(VertexDataBufferIndex);
        // XrVector3f Normal;
        vd->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
        vd->attributes()->object(1)->setOffset(offsetof(Vertex, Normal));
        vd->attributes()->object(1)->setBufferIndex(VertexDataBufferIndex);
        // XrVector4f Tangent;
        vd->attributes()->object(2)->setFormat(MTL::VertexFormatFloat4);
        vd->attributes()->object(2)->setOffset(offsetof(Vertex, Tangent));
        vd->attributes()->object(2)->setBufferIndex(VertexDataBufferIndex);
        // XrColor4f Color0;
        vd->attributes()->object(3)->setFormat(MTL::VertexFormatFloat4);
        vd->attributes()->object(3)->setOffset(offsetof(Vertex, Color0));
        vd->attributes()->object(3)->setBufferIndex(VertexDataBufferIndex);
        // XrVector2f TexCoord0;
        vd->attributes()->object(4)->setFormat(MTL::VertexFormatFloat2);
        vd->attributes()->object(4)->setOffset(offsetof(Vertex, TexCoord0));
        vd->attributes()->object(4)->setBufferIndex(VertexDataBufferIndex);
        // NodeIndex_t ModelTransformIndex;
        vd->attributes()->object(5)->setFormat(MTL::VertexFormatUShort);
        vd->attributes()->object(5)->setOffset(offsetof(Vertex, ModelTransformIndex));
        vd->attributes()->object(5)->setBufferIndex(VertexDataBufferIndex);
        m_Resources.VertexDescriptor = vd;

        /// Samplers for environment map and BRDF.
        m_Resources.EnvironmentMapSampler = MetalTexture::CreateSampler(device);
        m_Resources.BrdfSampler = MetalTexture::CreateSampler(device);

        m_Resources.PipelineStates = std::make_unique<MetalPipelineStates>(
            m_Resources.PbrVertexShader.get(), m_Resources.PbrPixelShader.get(), m_Resources.VertexDescriptor.get());

        m_Resources.SolidColorTextureCache = MetalTextureCache(device);

        m_Resources.SupportedTextureFormats = MakeSupportedFormatsList(device);
    }

    void MetalResources::ReleaseDeviceDependentResources()
    {
        m_Resources = {};
        m_LoaderResources = {};
        m_Primitives.clear();
    }

    NS::SharedPtr<MTL::Device> MetalResources::GetDevice() const
    {
        return m_device;
    }

    MetalPipelineStateBundle MetalResources::GetOrCreatePipelineState(MTL::PixelFormat colorRenderTargetFormat,
                                                                      MTL::PixelFormat depthRenderTargetFormat, BlendState blendState) const
    {
        DepthDirection depthDirection = m_sharedState.GetDepthDirection();
        MetalPipelineStateBundle bundle = m_Resources.PipelineStates->GetOrCreatePipelineState(
            *this, colorRenderTargetFormat, depthRenderTargetFormat, blendState, depthDirection);
        return bundle;
    }

    void MetalResources::SetLight(const XrVector3f& direction, RGBColor diffuseColor)
    {
        m_SceneBuffer.LightDirection = {direction.x, direction.y, direction.z};
        m_SceneBuffer.LightDiffuseColor = {diffuseColor.x, diffuseColor.y, diffuseColor.z};
    }

    void MetalResources::SetModelToWorld(const XrMatrix4x4f& modelToWorld) const
    {
        m_ModelBuffer.ModelToWorld = Conformance::LoadXrMatrixToMetal(modelToWorld);
    }

    void MetalResources::SetViewProjection(const XrMatrix4x4f& view, const XrMatrix4x4f& projection)
    {
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &projection, &view);
        m_SceneBuffer.ViewProjection = Conformance::LoadXrMatrixToMetal(vp);

        XrMatrix4x4f inv;
        XrMatrix4x4f_Invert(&inv, &view);
        m_SceneBuffer.EyePosition = {inv.m[12], inv.m[13], inv.m[14], 1.0f};
    }

    void MetalResources::SetEnvironmentMap(MTL::Texture* specularEnvironmentMap, MTL::Texture* diffuseEnvironmentMap)
    {
        if (diffuseEnvironmentMap->textureType() != MTL::TextureTypeCube) {
            throw std::logic_error("diffuseEnvironmentMap Type is not MTL::TextureTypeCube");
        }

        if (specularEnvironmentMap->textureType() != MTL::TextureTypeCube) {
            throw std::logic_error("specularEnvironmentMap Type is not MTL::TextureTypeCube");
        }

        m_SceneBuffer.NumSpecularMipLevels = (uint32_t)specularEnvironmentMap->mipmapLevelCount();
        m_Resources.SpecularEnvironmentMap = NS::RetainPtr(specularEnvironmentMap);
        m_Resources.DiffuseEnvironmentMap = NS::RetainPtr(diffuseEnvironmentMap);
    }

    NS::SharedPtr<MTL::Texture> MetalResources::CreateTypedSolidColorTexture(RGBAColor color, bool sRGB) const
    {
        return m_Resources.SolidColorTextureCache.CreateTypedSolidColorTexture(*this, color, sRGB);
    }

    span<const Conformance::Image::FormatParams> MetalResources::GetSupportedFormats() const
    {
        if (m_Resources.SupportedTextureFormats.size() == 0) {
            throw std::logic_error("SupportedTextureFormats empty or not yet populated");
        }
        return m_Resources.SupportedTextureFormats;
    }

    void MetalResources::Bind(MTL::RenderCommandEncoder* renderCommandEncoder) const
    {
        renderCommandEncoder->pushDebugGroup(MTLSTR("MetalResources::Bind"));

        renderCommandEncoder->setVertexBytes(&m_SceneBuffer, sizeof(m_SceneBuffer), Pbr::ShaderSlots::ConstantBuffers::Scene);
        renderCommandEncoder->setVertexBytes(&m_ModelBuffer, sizeof(m_ModelBuffer), Pbr::ShaderSlots::ConstantBuffers::Model);
        renderCommandEncoder->setFragmentBytes(&m_SceneBuffer, sizeof(m_SceneBuffer), Pbr::ShaderSlots::ConstantBuffers::Scene);

        static_assert(ShaderSlots::DiffuseTexture == ShaderSlots::SpecularTexture + 1, "Diffuse must follow Specular slot");
        static_assert(ShaderSlots::SpecularTexture == ShaderSlots::Brdf + 1, "Specular must follow BRDF slot");

        MTL::Texture* textures[3] = {m_Resources.BrdfLut.get(), m_Resources.SpecularEnvironmentMap.get(),
                                     m_Resources.DiffuseEnvironmentMap.get()};
        renderCommandEncoder->setFragmentTextures(textures, NS::Range(Pbr::ShaderSlots::Brdf, 3));

        MTL::SamplerState* samplers[2] = {m_Resources.BrdfSampler.get(), m_Resources.EnvironmentMapSampler.get()};
        renderCommandEncoder->setFragmentSamplerStates(samplers, NS::Range(Pbr::ShaderSlots::Brdf, 2));

        renderCommandEncoder->popDebugGroup();
    }

    PrimitiveHandle MetalResources::MakePrimitive(const Pbr::PrimitiveBuilder& primitiveBuilder,
                                                  const std::shared_ptr<Pbr::Material>& material)
    {
        auto typedMaterial = std::dynamic_pointer_cast<Pbr::MetalMaterial>(material);
        if (!typedMaterial) {
            throw std::logic_error("Got the wrong type of material");
        }
        return m_Primitives.emplace_back(*this, primitiveBuilder, typedMaterial, false);
    }

    MetalPrimitive& MetalResources::GetPrimitive(PrimitiveHandle p)
    {
        return m_Primitives[p];
    }

    const MetalPrimitive& MetalResources::GetPrimitive(PrimitiveHandle p) const
    {
        return m_Primitives[p];
    }

    void MetalResources::SetFillMode(FillMode mode)
    {
        m_sharedState.SetFillMode(mode);
    }

    FillMode MetalResources::GetFillMode() const
    {
        return m_sharedState.GetFillMode();
    }

    void MetalResources::SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder)
    {
        m_sharedState.SetFrontFaceWindingOrder(windingOrder);
    }

    FrontFaceWindingOrder MetalResources::GetFrontFaceWindingOrder() const
    {
        return m_sharedState.GetFrontFaceWindingOrder();
    }

    void MetalResources::SetDepthDirection(DepthDirection depthDirection)
    {
        m_sharedState.SetDepthDirection(depthDirection);
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_METAL)
