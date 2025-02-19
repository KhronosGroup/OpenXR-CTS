// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#version 450

layout (local_size_x = 16, local_size_y = 16) in;
// layout (binding = 0) uniform sampler2D source;
layout (rgba8, binding = 0) uniform image2D target;
layout(set = 0, binding = 1, std140) uniform restrict Config
{
	vec4 color;
} ubo;

void main() {
    uint ix = gl_GlobalInvocationID.x;
    uint iy = gl_GlobalInvocationID.y;
    ivec2 extent = imageSize(target);
    if (ix >= extent.x || iy >= extent.y) {
        return;
    }
    vec4 rgba = vec4(1.0, 0.0, 0.0, 1.0);
    imageStore(target, ivec2(ix, iy), rgba);
}
