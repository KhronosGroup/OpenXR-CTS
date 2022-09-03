// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#include "graphics_plugin.h"

#ifdef XR_USE_GRAPHICS_API_OPENGL

#include "report.h"

#include "swapchain_parameters.h"

#include "xr_dependencies.h"

#include "conformance_framework.h"
#include "throw_helpers.h"
#include "Geometry.h"

#include <catch2/catch.hpp>
#include <openxr/openxr_platform.h>
#include <openxr/openxr.h>
#include <thread>

// Why was this needed? hello_xr doesn't need these #defines
//#include "graphics_plugin_opengl_loader.h"
#include "gfxwrapper_opengl.h"
#include "xr_linear.h"

// clang-format off

// Note: mapping of OpenXR usage flags to OpenGL
//
// XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT: can be bound to a framebuffer as color
// XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT: can be bound to a framebuffer as depth (or stencil-only GL_STENCIL_INDEX8)
// XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT: image load/store and core since 4.2.
//   List of supported formats is in https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
// XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT & XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT: must be compatible format with glCopyTexImage* calls
// XR_SWAPCHAIN_USAGE_SAMPLED_BIT: can be sampled in a shader
// XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT: all GL formats are typed, but some can be reinterpreted with a different view.
//   OpenGL 4.2 / 4.3 with MSAA. Only for color formats and compressed ones
//   List with compatible textures: https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_view.txt
//   Note: no GL formats are "mutableFormats" in the sense of SwapchainCreateTestParameters as this is intended for TYPELESS,
//   however, some are "supportsMutableFormat"

// For now don't test XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT on GL since the semantics are unclear and some runtimes don't support this flag.
#define XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT 0

#define XRC_ALL_CREATE_FLAGS \
{ \
    0, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT | XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT \
}

// the app might request any combination of flags
#define XRC_COLOR_UA_COPY_SAMPLED_MUTABLE_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
}
#define XRC_COLOR_UA_SAMPLED_MUTABLE_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
}
#define XRC_COLOR_COPY_SAMPLED_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
}
#define XRC_COLOR_COPY_SAMPLED_MUTABLE_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
}
#define XRC_COLOR_SAMPLED_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, \
    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
}
#define XRC_DEPTH_COPY_SAMPLED_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
}
#define XRC_DEPTH_SAMPLED_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, \
    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
}

#define XRC_COMPRESSED_SAMPLED_MUTABLE_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
    XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
    XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, \
}
#define XRC_COMPRESSED_SAMPLED_USAGE_FLAGS \
{                     \
    XR_SWAPCHAIN_USAGE_SAMPLED_BIT, \
}

#define ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, true, true, false, FORMAT, XRC_COLOR_UA_COPY_SAMPLED_MUTABLE_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT(X) ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT2(X, #X)

#define ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, true, true, false, FORMAT, XRC_COLOR_UA_SAMPLED_MUTABLE_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(X) ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT2(X, #X)

#define ADD_GL_COLOR_COPY_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, false, true, false, FORMAT, XRC_COLOR_COPY_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_COLOR_COPY_SAMPLED_FORMAT(X) ADD_GL_COLOR_COPY_SAMPLED_FORMAT2(X, #X)

#define ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, true, true, false, FORMAT, XRC_COLOR_COPY_SAMPLED_MUTABLE_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT(X) ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT2(X, #X)

#define ADD_GL_COLOR_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, false, true, false, FORMAT, XRC_COLOR_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_COLOR_SAMPLED_FORMAT(X) ADD_GL_COLOR_SAMPLED_FORMAT2(X, #X)

#define ADD_GL_DEPTH_COPY_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, false, false, false, FORMAT, XRC_DEPTH_COPY_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_DEPTH_COPY_SAMPLED_FORMAT(X) ADD_GL_DEPTH_COPY_SAMPLED_FORMAT2(X, #X)

#define ADD_GL_DEPTH_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, false, false, false, FORMAT, XRC_DEPTH_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_DEPTH_SAMPLED_FORMAT(X) ADD_GL_DEPTH_SAMPLED_FORMAT2(X, #X)

#define ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, true, true, true, FORMAT, XRC_COMPRESSED_SAMPLED_MUTABLE_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(X) ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT2(X, #X)

#define ADD_GL_COMPRESSED_SAMPLED_FORMAT2(FORMAT, NAME)                                     \
{                                                                              \
    {FORMAT},                                                                  \
    {                                                                          \
        NAME, false, false, true, true, FORMAT, XRC_COMPRESSED_SAMPLED_USAGE_FLAGS, XRC_ALL_CREATE_FLAGS, {}, {}, \
        {                                                                      \
        }                                                                      \
    }                                                                          \
}
#define ADD_GL_COMPRESSED_SAMPLED_FORMAT(X) ADD_GL_COMPRESSED_SAMPLED_FORMAT2(X, #X)

// clang-format off

namespace Conformance
{
    std::string glResultString(GLenum err)
    {
        switch (err) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW";
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW";
        }
        return "<unknown " + std::to_string(err) + ">";
    }

    [[noreturn]] inline void ThrowGLResult(GLenum res, const char* originator = nullptr, const char* sourceLocation = nullptr)
    {
        Throw("GL failure " + glResultString(res), originator, sourceLocation);
    }

    inline GLenum CheckThrowGLResult(GLenum res, const char* originator = nullptr, const char* sourceLocation = nullptr)
    {
        if ((res) != GL_NO_ERROR) {
            ThrowGLResult(res, originator, sourceLocation);
        }

        return res;
    }

#define XRC_THROW_GL(res, cmd) ThrowGLResult(res, #cmd, XRC_FILE_AND_LINE)
#define XRC_CHECK_THROW_GLCMD(cmd) CheckThrowGLResult(((cmd), glGetError()), #cmd, XRC_FILE_AND_LINE)
#define XRC_CHECK_THROW_GLRESULT(res, cmdStr) CheckThrowGLResult(res, cmdStr, XRC_FILE_AND_LINE)

    static const char* VertexShaderGlsl = R"_(
        #version 410

        in vec3 VertexPos;
        in vec3 VertexColor;

        out vec3 PSVertexColor;

        uniform mat4 ModelViewProjection;

        void main() {
           gl_Position = ModelViewProjection * vec4(VertexPos, 1.0);
           PSVertexColor = VertexColor;
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
        void CheckShader(GLuint shader) const;
        void CheckProgram(GLuint prog) const;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, int64_t imageFormat, uint32_t arraySlice,
                           const RGBAImage& image) override;

        std::string GetImageFormatName(int64_t imageFormat) const override;

        bool IsImageFormatKnown(int64_t imageFormat) const override;

        bool GetSwapchainCreateTestParameters(XrInstance instance, XrSession session, XrSystemId systemId, int64_t imageFormat,
                                              SwapchainCreateTestParameters* swapchainTestParameters) override;

        bool ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                     uint32_t* imageCount) const override;
        bool ValidateSwapchainImageState(XrSwapchain swapchain, uint32_t index, int64_t imageFormat) const override;

        int64_t SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        // Format required by RGBAImage type.
        int64_t GetSRGBA8Format() const override;

        std::shared_ptr<SwapchainImageStructs> AllocateSwapchainImageStructs(size_t size,
                                                                             const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                             int64_t colorSwapchainFormat, XrColor4f bgColor = DarkSlateGrey) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        int64_t colorSwapchainFormat, const std::vector<Cube>& cubes) override;

    protected:
        struct SwapchainImageContext : public IGraphicsPlugin::SwapchainImageStructs
        {
            // A packed array of XrSwapchainImageOpenGLKHR's for xrEnumerateSwapchainImages
            std::vector<XrSwapchainImageOpenGLKHR> swapchainImages;
            XrSwapchainCreateInfo createInfo{};
            class ArraySliceState
            {
            public:
                ArraySliceState() = default;
                ArraySliceState(const ArraySliceState&)
                {
                    ReportF("ArraySliceState copy ctor called");
                }
                GLuint depthBuffer{0};
            };
            std::vector<ArraySliceState> slice;

            SwapchainImageContext() = default;
            ~SwapchainImageContext() override
            {
                Reset();
            }

            std::vector<XrSwapchainImageBaseHeader*> Create(uint32_t capacity, const XrSwapchainCreateInfo& swapchainCreateInfo)
            {
                createInfo = swapchainCreateInfo;

                swapchainImages.resize(capacity);
                std::vector<XrSwapchainImageBaseHeader*> bases(capacity);
                for (uint32_t i = 0; i < capacity; ++i) {
                    swapchainImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR};
                    bases[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&swapchainImages[i]);
                }
                slice.resize(swapchainCreateInfo.arraySize);
                return bases;
            }

            void Reset()
            {
                swapchainImages.clear();
                createInfo = {};
                for (const auto& s : slice) {
                    if (s.depthBuffer) {
                        XRC_CHECK_THROW_GLCMD(glDeleteTextures(1, &s.depthBuffer));
                    }
                }
                slice.clear();
            }

            GLuint GetDepthTexture(GLuint level)
            {
                if (!slice[level].depthBuffer) {
                    XRC_CHECK_THROW_GLCMD(glGenTextures(1, &slice[level].depthBuffer));
                    XRC_CHECK_THROW_GLCMD(glBindTexture(GL_TEXTURE_2D, slice[level].depthBuffer));
                    XRC_CHECK_THROW_GLCMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
                    XRC_CHECK_THROW_GLCMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
                    XRC_CHECK_THROW_GLCMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
                    XRC_CHECK_THROW_GLCMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
                    XRC_CHECK_THROW_GLCMD(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, createInfo.width, createInfo.height, 0,
                                                       GL_DEPTH_COMPONENT, GL_FLOAT, nullptr));
                }
                return slice[level].depthBuffer;
            }
        };

    private:
        bool initialized = false;
        bool deviceInitialized = false;

        void deleteGLContext();

        XrVersion OpenGLVersionOfContext = 0;

        ksGpuWindow window{};

#if defined(XR_USE_PLATFORM_WIN32)
        XrGraphicsBindingOpenGLWin32KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
#endif
#if defined(XR_USE_PLATFORM_XLIB)
        XrGraphicsBindingOpenGLXlibKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
#endif

        std::map<const XrSwapchainImageBaseHeader*, std::shared_ptr<SwapchainImageContext>> m_swapchainImageContextMap;
        GLuint m_swapchainFramebuffer{0};
        GLuint m_program{0};
        GLint m_modelViewProjectionUniformLocation{0};
        GLint m_vertexAttribCoords{0};
        GLint m_vertexAttribColor{0};
        GLuint m_vao{0};
        GLuint m_cubeVertexBuffer{0};
        GLuint m_cubeIndexBuffer{0};
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
            CHECK(ValidateResultAllowed("xrGetOpenGLGraphicsRequirementsKHR", result));
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
#endif  // XR_USE_PLATFORM_WIN32

#ifdef XR_USE_PLATFORM_XLIB
        REQUIRE(window.context.xDisplay != nullptr);
        graphicsBinding.xDisplay = window.context.xDisplay;
        graphicsBinding.visualid = window.context.visualid;
        graphicsBinding.glxFBConfig = window.context.glxFBConfig;
        graphicsBinding.glxDrawable = window.context.glxDrawable;
        graphicsBinding.glxContext = window.context.glxContext;
#endif

        GLenum error = glGetError();
        CHECK(error == GL_NO_ERROR);

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

#if !defined(NDEBUG)
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(
            [](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
                ((OpenGLGraphicsPlugin*)userParam)->DebugMessageCallback(source, type, id, severity, length, message);
            },
            this);
#endif

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
#if !defined(NDEBUG)
        XRC_CHECK_THROW_GLCMD(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
#endif

        XRC_CHECK_THROW_GLCMD(glGenFramebuffers(1, &m_swapchainFramebuffer));
        //ReportF("Got fb %d", m_swapchainFramebuffer);

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &VertexShaderGlsl, nullptr);
        glCompileShader(vertexShader);
        CheckShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &FragmentShaderGlsl, nullptr);
        glCompileShader(fragmentShader);
        CheckShader(fragmentShader);

        m_program = glCreateProgram();
        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);
        glLinkProgram(m_program);
        CheckProgram(m_program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        m_modelViewProjectionUniformLocation = glGetUniformLocation(m_program, "ModelViewProjection");

        m_vertexAttribCoords = glGetAttribLocation(m_program, "VertexPos");
        m_vertexAttribColor = glGetAttribLocation(m_program, "VertexColor");

        XRC_CHECK_THROW_GLCMD(glGenBuffers(1, &m_cubeVertexBuffer));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, m_cubeVertexBuffer));
        XRC_CHECK_THROW_GLCMD(
            glBufferData(GL_ARRAY_BUFFER, sizeof(Geometry::c_cubeVertices), &Geometry::c_cubeVertices[0], GL_STATIC_DRAW));

        XRC_CHECK_THROW_GLCMD(glGenBuffers(1, &m_cubeIndexBuffer));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeIndexBuffer));
        XRC_CHECK_THROW_GLCMD(
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Geometry::c_cubeIndices), &Geometry::c_cubeIndices[0], GL_STATIC_DRAW));

        XRC_CHECK_THROW_GLCMD(glGenVertexArrays(1, &m_vao));
        XRC_CHECK_THROW_GLCMD(glBindVertexArray(m_vao));
        XRC_CHECK_THROW_GLCMD(glEnableVertexAttribArray(m_vertexAttribCoords));
        XRC_CHECK_THROW_GLCMD(glEnableVertexAttribArray(m_vertexAttribColor));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ARRAY_BUFFER, m_cubeVertexBuffer));
        XRC_CHECK_THROW_GLCMD(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeIndexBuffer));
        XRC_CHECK_THROW_GLCMD(glVertexAttribPointer(m_vertexAttribCoords, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex), nullptr));
        XRC_CHECK_THROW_GLCMD(glVertexAttribPointer(m_vertexAttribColor, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex),
                                                    reinterpret_cast<const void*>(sizeof(XrVector3f))));
    }

    void OpenGLGraphicsPlugin::CheckFramebuffer(GLuint fb) const
    {
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
    }

    void OpenGLGraphicsPlugin::CheckShader(GLuint shader) const
    {
        GLint r = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            glGetShaderInfoLog(shader, sizeof(msg), &length, msg);
            XRC_CHECK_THROW_MSG(r, msg);
        }
    }

    void OpenGLGraphicsPlugin::CheckProgram(GLuint prog) const
    {
        GLint r = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &r);
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            glGetProgramInfoLog(prog, sizeof(msg), &length, msg);
            XRC_CHECK_THROW_MSG(r, msg);
        }
    }

    void OpenGLGraphicsPlugin::ShutdownDevice()
    {
        if (m_swapchainFramebuffer != 0) {
            glDeleteFramebuffers(1, &m_swapchainFramebuffer);
        }
        if (m_program != 0) {
            glDeleteProgram(m_program);
        }
        if (m_vao != 0) {
            glDeleteVertexArrays(1, &m_vao);
        }
        if (m_cubeVertexBuffer != 0) {
            glDeleteBuffers(1, &m_cubeVertexBuffer);
        }
        if (m_cubeIndexBuffer != 0) {
            glDeleteBuffers(1, &m_cubeIndexBuffer);
        }

        // Reset the swapchains to avoid calling Vulkan functions in the dtors after
        // we've shut down the device.
        for (auto& ctx : m_swapchainImageContextMap) {
            ctx.second->Reset();
        }
        m_swapchainImageContextMap.clear();

        deleteGLContext();
    }

    // Only texture formats which are in OpenGL core and which are either color or depth renderable or
    // of a specific compressed format are listed below. Runtimes can support additional formats, but those
    // will not get tested.
    typedef std::map<int64_t, SwapchainCreateTestParameters> SwapchainTestMap;
    SwapchainTestMap openGLSwapchainTestMap{
        ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT(GL_RGBA8),
        ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT(GL_RGBA16),
        ADD_GL_COLOR_UA_COPY_SAMPLED_MUTABLE_FORMAT(GL_RGB10_A2),

        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R8),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R16),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG8),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG16),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGB10_A2UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R16F),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG16F),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGB16F),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA16F),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R32F),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG32F),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA32F),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R11F_G11F_B10F),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R8I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R8UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R16I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R16UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R32I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_R32UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG8I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG8UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG16I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG16UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG32I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RG32UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA8I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA8UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA16I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA16UI),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA32I),
        ADD_GL_COLOR_UA_SAMPLED_MUTABLE_FORMAT(GL_RGBA32UI),

        ADD_GL_COLOR_COPY_SAMPLED_FORMAT(GL_RGBA4),
        ADD_GL_COLOR_COPY_SAMPLED_FORMAT(GL_RGB5_A1),

        ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT(GL_SRGB8),
        ADD_GL_COLOR_COPY_SAMPLED_MUTABLE_FORMAT(GL_SRGB8_ALPHA8),

        ADD_GL_COLOR_SAMPLED_FORMAT(GL_RGB565),

        ADD_GL_DEPTH_COPY_SAMPLED_FORMAT(GL_DEPTH_COMPONENT16),
        ADD_GL_DEPTH_COPY_SAMPLED_FORMAT(GL_DEPTH_COMPONENT24),

        ADD_GL_DEPTH_SAMPLED_FORMAT(GL_DEPTH_COMPONENT32F),
        ADD_GL_DEPTH_SAMPLED_FORMAT(GL_DEPTH24_STENCIL8),
        ADD_GL_DEPTH_SAMPLED_FORMAT(GL_DEPTH32F_STENCIL8),
        ADD_GL_DEPTH_SAMPLED_FORMAT(GL_STENCIL_INDEX8),

        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RED_RGTC1),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_SIGNED_RED_RGTC1),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RG_RGTC2),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_SIGNED_RG_RGTC2),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RGBA_BPTC_UNORM),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT),
        ADD_GL_COMPRESSED_SAMPLED_MUTABLE_FORMAT(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT),

        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_RGB8_ETC2),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SRGB8_ETC2),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_RGBA8_ETC2_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_R11_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SIGNED_R11_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_RG11_EAC),
        ADD_GL_COMPRESSED_SAMPLED_FORMAT(GL_COMPRESSED_SIGNED_RG11_EAC),
    };

    std::string OpenGLGraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        SwapchainTestMap::const_iterator it = openGLSwapchainTestMap.find(imageFormat);

        if (it != openGLSwapchainTestMap.end()) {
            return it->second.imageFormatName;
        }

        return std::string("unknown");
    }

    bool OpenGLGraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        SwapchainTestMap::const_iterator it = openGLSwapchainTestMap.find(imageFormat);

        return (it != openGLSwapchainTestMap.end());
    }

    bool OpenGLGraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                                int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        // Swapchain image format support by the runtime is specified by the xrEnumerateSwapchainFormats function.
        // Runtimes should support R8G8B8A8 and R8G8B8A8 sRGB formats if possible.

        SwapchainTestMap::iterator it = openGLSwapchainTestMap.find(imageFormat);

        // Verify that the image format is known. If it's not known then this test needs to be
        // updated to recognize new OpenGL formats.
        CAPTURE(imageFormat);
        CHECK_MSG(it != openGLSwapchainTestMap.end(), "Unknown OpenGL image format.");
        if (it == openGLSwapchainTestMap.end()) {
            return false;
        }

        // We may now proceed with creating swapchains with the format.
        SwapchainCreateTestParameters& tp = it->second;
        tp.arrayCountVector = {1, 2};
        if (!tp.compressedFormat) {
            tp.mipCountVector = {1, 2};
        }
        else {
            tp.mipCountVector = {1};
        }

        *swapchainTestParameters = tp;
        return true;
    }

    bool OpenGLGraphicsPlugin::ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                                       uint32_t* imageCount) const
    {
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
        const std::array<GLenum, 5> f{GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F,
                                      GL_DEPTH_COMPONENT16};

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

    std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
    OpenGLGraphicsPlugin::AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {
        auto derivedResult = std::make_shared<SwapchainImageContext>();

        // Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
        // Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
        // Keep the buffer alive by adding it into the list of buffers.

        std::vector<XrSwapchainImageBaseHeader*> bases = derivedResult->Create(uint32_t(size), swapchainCreateInfo);

        for (auto& base : bases) {
            // Set the generic vector of base pointers
            derivedResult->imagePtrVector.push_back(base);
            // Map every swapchainImage base pointer to this context
            m_swapchainImageContextMap[base] = derivedResult;
        }

        // Cast our derived type to the caller-expected type.
        std::shared_ptr<SwapchainImageStructs> result =
            std::static_pointer_cast<SwapchainImageStructs, SwapchainImageContext>(derivedResult);

        return result;
    }

    void OpenGLGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, int64_t /*imageFormat*/, uint32_t arraySlice,
                                             const RGBAImage& image)
    {
        auto swapchainContext = m_swapchainImageContextMap[swapchainImage];

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR*>(swapchainImage)->image;
        const GLint mip = 0;
        const GLint x = 0;
        const GLint z = arraySlice;
        const GLsizei w = swapchainContext->createInfo.width;
        const GLsizei h = swapchainContext->createInfo.height;
        if (swapchainContext->createInfo.arraySize > 1) {
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
                                               int64_t /*colorSwapchainFormat*/, XrColor4f bgColor)
    {
        auto swapchainContext = m_swapchainImageContextMap[colorSwapchainImage];

        XRC_CHECK_THROW_GLCMD(glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer));

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR*>(colorSwapchainImage)->image;
        const uint32_t depthTexture = swapchainContext->GetDepthTexture(imageArrayIndex);

        if (swapchainContext->createInfo.arraySize > 1) {
            XRC_CHECK_THROW_GLCMD(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0, imageArrayIndex));
        }
        else {
            XRC_CHECK_THROW_GLCMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0));
        }
        XRC_CHECK_THROW_GLCMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0));

        CheckFramebuffer(m_swapchainFramebuffer);

        GLint x = 0;
        GLint y = 0;
        GLsizei w = swapchainContext->createInfo.width;
        GLsizei h = swapchainContext->createInfo.height;
        XRC_CHECK_THROW_GLCMD(glViewport(x, y, w, h));
        XRC_CHECK_THROW_GLCMD(glScissor(x, y, w, h));

        XRC_CHECK_THROW_GLCMD(glEnable(GL_SCISSOR_TEST));

        // Clear swapchain and depth buffer.
        XRC_CHECK_THROW_GLCMD(glClearColor(bgColor.r, bgColor.g, bgColor.b, bgColor.a));
        XRC_CHECK_THROW_GLCMD(glClearDepth(1.0f));
        XRC_CHECK_THROW_GLCMD(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                          const XrSwapchainImageBaseHeader* colorSwapchainImage, int64_t /*colorSwapchainFormat*/,
                                          const std::vector<Cube>& cubes)
    {
        auto swapchainContext = m_swapchainImageContextMap[colorSwapchainImage];

        XRC_CHECK_THROW_GLCMD(glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer));

        GLint layer = layerView.subImage.imageArrayIndex;

        const GLuint colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR*>(colorSwapchainImage)->image;
        const GLuint depthTexture = swapchainContext->GetDepthTexture(layer);

        if (swapchainContext->createInfo.arraySize > 1) {
            XRC_CHECK_THROW_GLCMD(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0, layer));
        }
        else {
            XRC_CHECK_THROW_GLCMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0));
        }
        XRC_CHECK_THROW_GLCMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0));

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
        XrVector3f scale{1.f, 1.f, 1.f};
        XrMatrix4x4f_CreateTranslationRotationScale(&toView, &pose.position, &pose.orientation, &scale);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        // Set cube primitive data.
        XRC_CHECK_THROW_GLCMD(glBindVertexArray(m_vao));

        // Render each cube
        for (const Cube& cube : cubes) {
            // Compute the model-view-projection transform and set it..
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &cube.Pose.position, &cube.Pose.orientation, &cube.Scale);
            XrMatrix4x4f mvp;
            XrMatrix4x4f_Multiply(&mvp, &vp, &model);
            glUniformMatrix4fv(m_modelViewProjectionUniformLocation, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&mvp));

            // Draw the cube.
            glDrawElements(GL_TRIANGLES, GLsizei(Geometry::c_cubeIndices.size()), GL_UNSIGNED_SHORT, nullptr);
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
