// Copyright 2023-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

#include <metal_stdlib>

using namespace metal;

// Check pbr/PbrSharedState.h before changing the binding indices
namespace
{
    // Constant buffer index
    enum class ConstantBuffers : uint8_t
    {
        Scene = 0,     // Used by VS and PS
        Model,     // VS only
        Material,  // PS only
        // Extra slot for Metal VS
        Transforms,  // must be 3
        VertexData   // must be 4
    };

    // Sampler/texture index
    enum class PSMaterial : uint8_t
    {
        // For both samplers and textures.
        BaseColor = 0,
        MetallicRoughness,
        Normal,
        Occlusion,
        Emissive,
        LastMaterialSlot = Emissive,
        NumMaterialSlots = LastMaterialSlot + 1,
        
        // Extra material slots
        Brdf = NumMaterialSlots,
        
        // Texture only
        SpecularTexture = Brdf + 1,
        DiffuseTexture = SpecularTexture + 1,
        
        // Sampler only
        EnvironmentMapSampler = Brdf + 1
    };
}

struct SceneBuffer
{
    float4x4 ViewProjection;
    float3 EyePosition;
    float3 LightDirection;
    float3 LightColor;
    uint NumSpecularMipLevels;
};

static_assert(sizeof(SceneBuffer) == 128, "Size must be the same as known");

struct ModelConstantBuffer
{
    float4x4 ModelToWorld;
};

struct MaterialConstantBuffer
{
    // packoffset(c0)
    float4 BaseColorFactor;
    
    // packoffset(c1.x and c1.y)
    float MetallicFactor;
    float RoughnessFactor;
    float _pad0[2];
    
    // packoffset(c2)
    float3 EmissiveFactor;
    // float _pad1;
    
    // packoffset(c3.x, c3.y and c3.z)
    float NormalScale;
    float OcclusionStrength;
    float AlphaCutoff;
    float _pad2;
};

struct VertexDataPbr
{
    float4 Position [[attribute(0)]];
    float3 Normal [[attribute(1)]];
    float4 Tangent [[attribute(2)]];
    float4 Color0 [[attribute(3)]];
    float2 TexCoord0 [[attribute(4)]];
    uint ModelTransformIndex [[attribute(5)]];
};

struct VertexOutputPbr
{
    float4 PositionProj [[position]];
    float3 PositionWorld;
    
    // float3x3 TBN;
    float3 tangentW;
    float3 bitangentW;
    float3 normalW;
    
    float2 TexCoord0;
    float4 Color0;
};

constant float3 f0 = float3(0.04, 0.04, 0.04);
constant float MinRoughness = 0.04;
constant float PI = 3.141592653589793;

VertexOutputPbr vertex VertexShaderPbr(VertexDataPbr input [[stage_in]],
                                       device const SceneBuffer* sceneBuffer [[buffer(ConstantBuffers::Scene)]],
                                       device const ModelConstantBuffer* modelConstantBuffer [[buffer(ConstantBuffers::Model)]],
                                       device const float4x4* transforms [[buffer(ConstantBuffers::Transforms)]])
{
    VertexOutputPbr output;

    const float4x4 modelTransform = modelConstantBuffer->ModelToWorld * transforms[input.ModelTransformIndex];
    float4 transformedPosWorld = modelTransform * input.Position;
    output.PositionProj = sceneBuffer->ViewProjection * transformedPosWorld;
    output.PositionWorld = transformedPosWorld.xyz / transformedPosWorld.w;
    
    const float3 normalW = normalize((modelTransform * float4(input.Normal, 0.0)).xyz);
    const float3 tangentW = normalize((modelTransform * float4(input.Tangent.xyz, 0.0)).xyz);
    const float3 bitangentW = cross(normalW, tangentW) * input.Tangent.w;
    output.tangentW = tangentW;
    output.bitangentW = bitangentW;
    output.normalW = normalW;
    
    output.TexCoord0 = input.TexCoord0;
    output.Color0 = input.Color0;

    return output;
}

float3 getIBLContribution(float perceptualRoughness,
                          float NdotV,
                          float3 diffuseColor,
                          float3 specularColor,
                          float3 n,
                          float3 reflection,
                          device const SceneBuffer* sceneBuffer,
                          texture2d<float> BRDFTexture,
                          sampler BRDFSampler,
                          texturecube<float> DiffuseTexture,
                          texturecube<float> SpecularTexture,
                          sampler IBLSampler)
{
    const float lod = perceptualRoughness * sceneBuffer->NumSpecularMipLevels;

    const float3 brdf = BRDFTexture.sample(BRDFSampler, float2(NdotV, 1.0 - perceptualRoughness)).rgb;

    const float3 diffuseLight = DiffuseTexture.sample(IBLSampler, n).rgb;
    const float3 specularLight = SpecularTexture.sample(IBLSampler, reflection, level(lod)).rgb;

    const float3 diffuse = diffuseLight * diffuseColor;
    const float3 specular = specularLight * (specularColor * brdf.x + brdf.y);

    return diffuse + specular;
}

float3 diffuse(float3 diffuseColor)
{
    return diffuseColor / PI;
}

float3 specularReflection(float3 reflectance0, float3 reflectance90, float VdotH)
{
    return reflectance0 + (reflectance90 - reflectance0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float geometricOcclusion(float NdotL, float NdotV, float alphaRoughness)
{
    const float attenuationL = 2.0 * NdotL / (NdotL + sqrt(alphaRoughness * alphaRoughness + (1.0 - alphaRoughness * alphaRoughness) * (NdotL * NdotL)));
    const float attenuationV = 2.0 * NdotV / (NdotV + sqrt(alphaRoughness * alphaRoughness + (1.0 - alphaRoughness * alphaRoughness) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(float NdotH, float alphaRoughness)
{
    const float roughnessSq = alphaRoughness * alphaRoughness;
    const float f = (NdotH * roughnessSq - NdotH) * NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

float4 fragment FragmentShaderPbr(VertexOutputPbr input [[stage_in]],
                                  bool isFrontFace [[front_facing]],
                                  device const SceneBuffer* sceneBuffer [[buffer(ConstantBuffers::Scene)]],
                                  device const MaterialConstantBuffer* materialConstantBuffer [[buffer(ConstantBuffers::Material)]],
                                  texture2d<float> BaseColorTexture [[texture(PSMaterial::BaseColor)]],
                                  texture2d<float> MetallicRoughnessTexture [[texture(PSMaterial::MetallicRoughness)]],
                                  texture2d<float> NormalTexture [[texture(PSMaterial::Normal)]],
                                  texture2d<float> OcclusionTexture [[texture(PSMaterial::Occlusion)]],
                                  texture2d<float> EmissiveTexture [[texture(PSMaterial::Emissive)]],
                                  texture2d<float> BRDFTexture [[texture(PSMaterial::Brdf)]],
                                  texturecube<float> SpecularTexture [[texture(PSMaterial::SpecularTexture)]],
                                  texturecube<float> DiffuseTexture [[texture(PSMaterial::DiffuseTexture)]],
                                  sampler BaseColorSampler [[sampler(PSMaterial::BaseColor)]],
                                  sampler MetallicRoughnessSampler [[sampler(PSMaterial::MetallicRoughness)]],
                                  sampler NormalSampler [[sampler(PSMaterial::Normal)]],
                                  sampler OcclusionSampler [[sampler(PSMaterial::Occlusion)]],
                                  sampler EmissiveSampler [[sampler(PSMaterial::Emissive)]],
                                  sampler BRDFSampler [[sampler(PSMaterial::Brdf)]],
                                  sampler IBLSampler [[sampler(PSMaterial::EnvironmentMapSampler)]])
{
    // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
    const float3 mrSample = MetallicRoughnessTexture.sample(MetallicRoughnessSampler, input.TexCoord0).rgb;
    const float4 baseColor = BaseColorTexture.sample(BaseColorSampler, input.TexCoord0) * input.Color0 * materialConstantBuffer->BaseColorFactor;

    // Discard if below alpha cutoff.
    if (baseColor.a < materialConstantBuffer->AlphaCutoff) {
        discard_fragment();
    }

    const float metallic = saturate(mrSample.b * materialConstantBuffer->MetallicFactor);
    const float perceptualRoughness = clamp(mrSample.g * materialConstantBuffer->RoughnessFactor, MinRoughness, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    const float alphaRoughness = perceptualRoughness * perceptualRoughness;

    const float3 diffuseColor = (baseColor.rgb * (float3(1.0, 1.0, 1.0) - f0)) * (1.0 - metallic);
    const float3 specularColor = mix(f0, baseColor.rgb, metallic);

    // Compute reflectance.
    const float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    const float reflectance90 = saturate(reflectance * 25.0);
    const float3 specularEnvironmentR0 = specularColor.rgb;
    const float3 specularEnvironmentR90 = float3(1.0, 1.0, 1.0) * reflectance90;

    // normal at surface point
    float3 n = 2.0 * NormalTexture.sample(NormalSampler, input.TexCoord0).rgb - 1.0;
    n = isFrontFace ? n : -n;
    float3x3 TBN(input.tangentW, input.bitangentW, input.normalW);
    float3 n_scaled = n * float3(materialConstantBuffer->NormalScale, materialConstantBuffer->NormalScale, 1.0);
    n = normalize(TBN * n_scaled);

    const float3 v = normalize(sceneBuffer->EyePosition - input.PositionWorld);   // Vector from surface point to camera
    const float3 l = normalize(sceneBuffer->LightDirection);                      // Vector from surface point to light
    const float3 h = normalize(l + v);                                            // Half vector between both l and v
    const float3 reflection = -normalize(reflect(v, n));

    const float NdotL = clamp(dot(n, l), 0.001, 1.0);
    const float NdotV = abs(dot(n, v)) + 0.001;
    const float NdotH = saturate(dot(n, h));
    // const float LdotH = saturate(dot(l, h));
    const float VdotH = saturate(dot(v, h));

    // Calculate the shading terms for the microfacet specular shading model
    const float3 F = specularReflection(specularEnvironmentR0, specularEnvironmentR90, VdotH);
    const float G = geometricOcclusion(NdotL, NdotV, alphaRoughness);
    const float D = microfacetDistribution(NdotH, alphaRoughness);

    // Calculation of analytical lighting contribution
    const float3 diffuseContrib = (1.0 - F) * diffuse(diffuseColor);
    const float3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    float3 color = NdotL * sceneBuffer->LightColor * (diffuseContrib + specContrib);

    // Calculate lighting contribution from image based lighting source (IBL)
    color += getIBLContribution(perceptualRoughness, NdotV, diffuseColor, specularColor, n, reflection, sceneBuffer, BRDFTexture, BRDFSampler, DiffuseTexture, SpecularTexture, IBLSampler);

    // Apply optional PBR terms for additional (optional) shading
    const float ao = OcclusionTexture.sample(OcclusionSampler, input.TexCoord0).r;
    color = mix(color, color * ao, materialConstantBuffer->OcclusionStrength);

    const float3 emissive = EmissiveTexture.sample(EmissiveSampler, input.TexCoord0).rgb * materialConstantBuffer->EmissiveFactor;
    color += emissive;

    return float4(color, baseColor.a);
}
