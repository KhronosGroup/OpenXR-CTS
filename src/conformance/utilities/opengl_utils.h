// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "common/xr_dependencies.h"

#include "common/gfxwrapper_opengl.h"
#include "utilities/stringification.h"
#include "utilities/throw_helpers.h"

#include <string>

namespace Conformance
{
    std::string glResultString(GLenum err);

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

#define XRC_THROW_GL(res, cmd) ::Conformance::ThrowGLResult(res, #cmd, XRC_FILE_AND_LINE)
#define XRC_CHECK_THROW_GLCMD(cmd) ::Conformance::CheckThrowGLResult(((cmd), glGetError()), #cmd, XRC_FILE_AND_LINE)
#define XRC_CHECK_THROW_GLRESULT(res, cmdStr) ::Conformance::CheckThrowGLResult(res, cmdStr, XRC_FILE_AND_LINE)

    inline GLenum TexTarget(bool isArray, bool isMultisample)
    {
        if (isArray && isMultisample) {
            return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        }
        else if (isMultisample) {
            return GL_TEXTURE_2D_MULTISAMPLE;
        }
        else if (isArray) {
            return GL_TEXTURE_2D_ARRAY;
        }
        else {
            return GL_TEXTURE_2D;
        }
    }

    void CheckGLShader(GLuint shader);
    void CheckGLProgram(GLuint prog);

}  // namespace Conformance

#endif  // defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)
