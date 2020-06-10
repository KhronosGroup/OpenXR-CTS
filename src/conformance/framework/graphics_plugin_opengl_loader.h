// Copyright (c) 2020 The Khronos Group Inc.
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

// this file hold the required defines for OpenGL beyond 1.1.
// if more is needed, this could get replaced by a 3rd party extension loader
#include <GL/gl.h>

#if !defined(GL_MAJOR_VERSION)
#define GL_MAJOR_VERSION 0x821B
#endif
#if !defined(GL_MINOR_VERSION)
#define GL_MINOR_VERSION 0x821C
#endif

// additional texture formats
#if !defined(GL_R8)
#define GL_R8 0x8229
#endif
#if !defined(GL_R16)
#define GL_R16 0x822A
#endif
#if !defined(GL_RG8)
#define GL_RG8 0x822B
#endif
#if !defined(GL_RG16)
#define GL_RG16 0x822C
#endif
#if !defined(GL_RGB10_A2UI)
#define GL_RGB10_A2UI 0x906F
#endif
#if !defined(GL_R16F)
#define GL_R16F 0x822D
#endif
#if !defined(GL_RG16F)
#define GL_RG16F 0x822F
#endif
#if !defined(GL_RGBA16F)
#define GL_RGBA16F 0x881A
#endif
#if !defined(GL_R32F)
#define GL_R32F 0x822E
#endif
#if !defined(GL_RG32F)
#define GL_RG32F 0x8230
#endif
#if !defined(GL_RGBA32F)
#define GL_RGBA32F 0x8814
#endif
#if !defined(GL_R11F_G11F_B10F)
#define GL_R11F_G11F_B10F 0x8C3A
#endif
#if !defined(GL_R8I)
#define GL_R8I 0x8231
#endif
#if !defined(GL_R8UI)
#define GL_R8UI 0x8232
#endif
#if !defined(GL_R16I)
#define GL_R16I 0x8233
#endif
#if !defined(GL_R16UI)
#define GL_R16UI 0x8234
#endif
#if !defined(GL_R32I)
#define GL_R32I 0x8235
#endif
#if !defined(GL_R32UI)
#define GL_R32UI 0x8236
#endif
#if !defined(GL_RG8I)
#define GL_RG8I 0x8237
#endif
#if !defined(GL_RG8UI)
#define GL_RG8UI 0x8238
#endif
#if !defined(GL_RG16I)
#define GL_RG16I 0x8239
#endif
#if !defined(GL_RG16UI)
#define GL_RG16UI 0x823A
#endif
#if !defined(GL_RG32I)
#define GL_RG32I 0x823B
#endif
#if !defined(GL_RG32UI)
#define GL_RG32UI 0x823C
#endif
#if !defined(GL_RGBA8I)
#define GL_RGBA8I 0x8D8E
#endif
#if !defined(GL_RGBA8UI)
#define GL_RGBA8UI 0x8D7C
#endif
#if !defined(GL_RGBA16I)
#define GL_RGBA16I 0x8D88
#endif
#if !defined(GL_RGBA16UI)
#define GL_RGBA16UI 0x8D77
#endif
#if !defined(GL_RGBA32I)
#define GL_RGBA32I 0x8D82
#endif
#if !defined(GL_RGBA32UI)
#define GL_RGBA32UI 0x8D71
#endif
#if !defined(GL_SRGB8_ALPHA8)
#define GL_SRGB8_ALPHA8 0x8C43
#endif
#if !defined(GL_RGB565)
#define GL_RGB565 0x8D62
#endif
#if !defined(GL_DEPTH_COMPONENT16)
#define GL_DEPTH_COMPONENT16 0x81A5
#endif
#if !defined(GL_DEPTH_COMPONENT24)
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#if !defined(GL_DEPTH_COMPONENT32F)
#define GL_DEPTH_COMPONENT32F 0x8CAC
#endif
#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#if !defined(GL_DEPTH32F_STENCIL8)
#define GL_DEPTH32F_STENCIL8 0x8CAD
#endif
#if !defined(GL_STENCIL_INDEX8)
#define GL_STENCIL_INDEX8 0x8D48
#endif
#if !defined(GL_COMPRESSED_RED_RGTC1)
#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#endif
#if !defined(GL_COMPRESSED_SIGNED_RED_RGTC1)
#define GL_COMPRESSED_SIGNED_RED_RGTC1 0x8DBC
#endif
#if !defined(GL_COMPRESSED_RG_RGTC2)
#define GL_COMPRESSED_RG_RGTC2 0x8DBD
#endif
#if !defined(GL_COMPRESSED_SIGNED_RG_RGTC2)
#define GL_COMPRESSED_SIGNED_RG_RGTC2 0x8DBE
#endif
#if !defined(GL_COMPRESSED_RGBA_BPTC_UNORM)
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#endif
#if !defined(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM)
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#endif
#if !defined(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT)
#define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT 0x8E8E
#endif
#if !defined(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F
#endif
#if !defined(GL_COMPRESSED_RGB8_ETC2)
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#endif
#if !defined(GL_COMPRESSED_SRGB8_ETC2)
#define GL_COMPRESSED_SRGB8_ETC2 0x9275
#endif
#if !defined(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9276
#endif
#if !defined(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9277
#endif
#if !defined(GL_COMPRESSED_RGBA8_ETC2_EAC)
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#endif
#if !defined(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279
#endif
#if !defined(GL_COMPRESSED_R11_EAC)
#define GL_COMPRESSED_R11_EAC 0x9270
#endif
#if !defined(GL_COMPRESSED_SIGNED_R11_EAC)
#define GL_COMPRESSED_SIGNED_R11_EAC 0x9271
#endif
#if !defined(GL_COMPRESSED_RG11_EAC)
#define GL_COMPRESSED_RG11_EAC 0x9272
#endif
#if !defined(GL_COMPRESSED_SIGNED_RG11_EAC)
#define GL_COMPRESSED_SIGNED_RG11_EAC 0x9273
#endif

#if !defined(GL_TEXTURE_COMPRESSED)
#define GL_TEXTURE_COMPRESSED 0x86A1
#endif

#endif  // graphics API
