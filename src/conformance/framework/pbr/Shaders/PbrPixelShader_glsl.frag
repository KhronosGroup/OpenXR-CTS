#version 450
precision mediump float;
precision highp int;
// Copyright (c) 2016 - 2017 Mohamad Moneimne and Contributors
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Copyright 2023-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT

layout(binding = 0, std140) uniform type_SceneBuffer
{
    mat4 ViewProjection; // offset 0
    vec3 EyePosition; // offset 64
    // implicit 4 bytes of padding
    vec3 LightDirection; // offset 80
    // implicit 4 bytes of padding
    vec3 LightColor; // offset 96
    // implicit 4 bytes of padding
    uint _pad; // explicit 4 bytes of padding so this matches the d3d offsets too
    lowp uint NumSpecularMipLevels; // offset 112
    // implicit 12 bytes of padding
}
SceneBuffer;

layout(binding = 2, std140) uniform type_MaterialConstantBuffer
{
    vec4 BaseColorFactor; // offset 0
    float MetallicFactor; // offset 16
    float RoughnessFactor; // offset 20
    vec3 EmissiveFactor; // offset 32
    float _pad; // Need explicit 4 bytes of padding
    float NormalScale; // offset 48
    float OcclusionStrength; // offset 52
    float AlphaCutoff; // offset 56
    // implicit 4 bytes of padding
}
MaterialConstantBuffer;

layout(binding = 4) uniform sampler2D BaseColorSampler;
layout(binding = 5) uniform sampler2D MetallicRoughnessSampler;
layout(binding = 6) uniform sampler2D NormalSampler;
layout(binding = 7) uniform sampler2D OcclusionTextureOcclusionSampler;
layout(binding = 8) uniform sampler2D EmissiveTextureEmissiveSampler;
layout(binding = 9) uniform sampler2D BRDFSampler;
layout(binding = 10) uniform samplerCube SpecularTextureIBLSampler;
layout(binding = 11) uniform samplerCube DiffuseTextureIBLSampler;

// input to fragment shader, output of vertex shader
layout(location = 0) in vec3 varying_POSITION1;
layout(location = 1) in mat3 varying_TANGENT;
layout(location = 4) in vec2 varying_TEXCOORD0;
layout(location = 5) in vec4 varying_COLOR0;

layout(location = 0) out vec4 out_var_SV_TARGET;

#define texture2D texture
#define textureCube texture
#define textureCubeLodEXT textureLod

const float M_PI = 3.141592653589793;
const float c_MinRoughness = 0.04;
const vec3 c_f0 = vec3(0.04, 0.04, 0.04);

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
#ifdef MANUAL_SRGB
#ifdef SRGB_FAST_APPROXIMATION
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
#else   //SRGB_FAST_APPROXIMATION
    vec3 bLess = step(vec3(0.04045), srgbIn.xyz);
    vec3 linOut = mix(srgbIn.xyz / vec3(12.92), pow((srgbIn.xyz + vec3(0.055)) / vec3(1.055), vec3(2.4)), bLess);
#endif  //SRGB_FAST_APPROXIMATION
    return vec4(linOut, srgbIn.w);
    ;
#else   //MANUAL_SRGB
    return srgbIn;
#endif  //MANUAL_SRGB
}

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    // Retrieve the tangent space matrix
    mat3 tbn = varying_TANGENT;  // ????

    vec3 n = texture2D(NormalSampler, varying_TEXCOORD0).rgb;
    n = normalize(tbn * ((2.0 * n - 1.0) * vec3(MaterialConstantBuffer.NormalScale, MaterialConstantBuffer.NormalScale, 1.0)));

    return n;
}

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
vec3 getIBLContribution(float perceptualRoughness, float NdotV, vec3 diffuseColor, vec3 specularColor, vec3 n, vec3 reflection)
{
    float lod = (perceptualRoughness * float(SceneBuffer.NumSpecularMipLevels));

    // retrieve a scale and bias to F0. See [1], Figure 3
    vec3 brdf = SRGBtoLINEAR(texture2D(BRDFSampler, vec2(NdotV, 1.0 - perceptualRoughness))).rgb;
    vec3 diffuseLight = SRGBtoLINEAR(textureCube(DiffuseTextureIBLSampler, n)).rgb;

    vec3 specularLight = SRGBtoLINEAR(textureCubeLodEXT(SpecularTextureIBLSampler, reflection, lod)).rgb;

    vec3 diffuse = diffuseLight * diffuseColor;
    vec3 specular = specularLight * (specularColor * brdf.x + brdf.y);

    // For presentation, this allows us to disable IBL terms
    // diffuse *= u_ScaleIBLAmbient.x;
    // specular *= u_ScaleIBLAmbient.y;

    return diffuse + specular;
}

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 diffuse(vec3 diffuseColor)
{
    return diffuseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(vec3 reflectance0, vec3 reflectance90, float VdotH)
{
    return reflectance0 + (reflectance90 - reflectance0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(float NdotL, float NdotV, float alphaRoughness)
{
    float r = alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
highp float microfacetDistribution(highp float NdotH, float alphaRoughness)
{
    highp float roughnessSq = alphaRoughness * alphaRoughness;
    highp float f = (NdotH * roughnessSq - NdotH) * NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}

void main()
{
    // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
    vec4 mrSample = texture(MetallicRoughnessSampler, varying_TEXCOORD0);
    vec4 baseColor = (texture(BaseColorSampler, varying_TEXCOORD0) * varying_COLOR0) * MaterialConstantBuffer.BaseColorFactor;

    // Discard if below alpha cutoff.
    if ((baseColor.w - MaterialConstantBuffer.AlphaCutoff) < 0.0) {
        discard;
    }
    float metallic = clamp(mrSample.z * MaterialConstantBuffer.MetallicFactor, 0.0, 1.0);
    float perceptualRoughness = clamp(mrSample.y * MaterialConstantBuffer.RoughnessFactor, c_MinRoughness, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    vec3 diffuseColor = (baseColor.xyz * (vec3(1) - c_f0)) * (1.0 - metallic);
    vec3 specularColor = mix(c_f0, baseColor.xyz, vec3(metallic));

    // Compute reflectance.
    // float _119 = specularColor.x;
    // float _120 = specularColor.y;
    // float _121 = isnan(_120) ? _119 : (isnan(_119) ? _120 : max(_119, _120));
    // float _122 = specularColor.z;
    // float reflectance = isnan(_122) ? _121 : (isnan(_121) ? _122 : max(_121, _122));
    float reflectance = max(max(specularColor.x, specularColor.y), specularColor.z);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp((reflectance)*25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.xyz;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    // normal at surface point
    highp vec3 n_orig = (texture(NormalSampler, varying_TEXCOORD0).xyz * 2.0) - vec3(1.0);
    n_orig = gl_FrontFacing ? n_orig : -n_orig;
    vec3 n = normalize(varying_TANGENT * (n_orig * vec3(MaterialConstantBuffer.NormalScale, MaterialConstantBuffer.NormalScale, 1.0)));
    highp vec3 v = normalize(SceneBuffer.EyePosition - varying_POSITION1);
    vec3 l = normalize(SceneBuffer.LightDirection);
    highp vec3 h = normalize(l + v);
    vec3 reflection = -normalize(reflect(v, n));

    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = abs(dot(n, v)) + 0.001;
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(specularEnvironmentR0, specularEnvironmentR90, VdotH);
    float G = geometricOcclusion(NdotL, NdotV, alphaRoughness);
    highp float D = microfacetDistribution(NdotH, alphaRoughness);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * diffuse(diffuseColor);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);

    vec3 color = SceneBuffer.LightColor * NdotL * (diffuseContrib + specContrib);

    vec3 ibl = getIBLContribution(perceptualRoughness, NdotV, diffuseColor, specularColor, n, reflection);
    float ao = texture(OcclusionTextureOcclusionSampler, varying_TEXCOORD0).x;
    vec3 iblWithAO = mix(ibl, ibl * ao, vec3(MaterialConstantBuffer.OcclusionStrength));

    vec3 colorWithIBL = color + iblWithAO;

    vec3 colorWithIBLandAO = mix(colorWithIBL, colorWithIBL * ao, vec3(MaterialConstantBuffer.OcclusionStrength));

    vec3 emissive = texture(EmissiveTextureEmissiveSampler, varying_TEXCOORD0).xyz * MaterialConstantBuffer.EmissiveFactor;

    out_var_SV_TARGET = vec4(colorWithIBLandAO + emissive, baseColor.w);
}
