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


layout(binding = 1, std140) uniform type_ModelConstantBuffer
{
    mat4 ModelToWorld;
}
ModelConstantBuffer;

layout(binding = 3, std430) readonly buffer type_StructuredBuffer_mat4v4float
{
    mat4 _m0[];
}
Transforms;


layout(location = 0) in vec4 in_var_POSITION;
layout(location = 1) in vec3 in_var_NORMAL;
layout(location = 2) in vec4 in_var_TANGENT;
layout(location = 3) in vec4 in_var_COLOR0;
layout(location = 4) in vec2 in_var_TEXCOORD0;
layout(location = 5) in mediump uint in_var_TRANSFORMINDEX;

// output of vertex shader, input to fragment shader
layout(location = 0) out vec3 varying_POSITION1;
layout(location = 1) out mat3 varying_TANGENT;
layout(location = 4) out vec2 varying_TEXCOORD0;
layout(location = 5) out vec4 varying_COLOR0;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    mat4 modelTransform = ModelConstantBuffer.ModelToWorld * Transforms._m0[(in_var_TRANSFORMINDEX)];
    vec4 transformedPosWorld = modelTransform * in_var_POSITION;
    vec3 normalW = normalize((modelTransform * vec4(in_var_NORMAL, 0.0)).xyz);
    vec3 tangentW = normalize((modelTransform * vec4(in_var_TANGENT.xyz, 0.0)).xyz);
    // aka output.PositionProj
    gl_Position = SceneBuffer.ViewProjection * transformedPosWorld;
    // aka output.PositionWorld
    varying_POSITION1 = transformedPosWorld.xyz / vec3(transformedPosWorld.w);

    vec3 bitangentW = cross(normalW, tangentW) * in_var_TANGENT.w;
    varying_TANGENT = mat3(tangentW, bitangentW, normalW);

    varying_TEXCOORD0 = in_var_TEXCOORD0;
    varying_COLOR0 = in_var_COLOR0;
}
