// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)
#include "opengl_utils.h"

#include "common/gfxwrapper_opengl.h"

#include <string>

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

    void CheckGLShader(GLuint shader)
    {
        GLint r = 0;
        XRC_CHECK_THROW_GLCMD(glGetShaderiv(shader, GL_COMPILE_STATUS, &r));
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            XRC_CHECK_THROW_GLCMD(glGetShaderInfoLog(shader, sizeof(msg), &length, msg));
            XRC_CHECK_THROW_MSG(r, msg);
        }
    }

    void CheckGLProgram(GLuint prog)
    {
        GLint r = 0;
        XRC_CHECK_THROW_GLCMD(glGetProgramiv(prog, GL_LINK_STATUS, &r));
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            XRC_CHECK_THROW_GLCMD(glGetProgramInfoLog(prog, sizeof(msg), &length, msg));
            XRC_CHECK_THROW_MSG(r, msg);
        }
    }
}  // namespace Conformance

#endif  // defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)
