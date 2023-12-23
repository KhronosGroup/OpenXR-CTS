// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#include "GLTexture.h"

#include "GLCommon.h"
#include "stb_image.h"

#include "../PbrCommon.h"

#include "common/gfxwrapper_opengl.h"
#include "utilities/opengl_utils.h"

#include <assert.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <stdlib.h>

namespace Pbr
{
    namespace GLTexture
    {
        std::array<uint8_t, 4> LoadRGBAUI4(RGBAColor color)
        {
            return std::array<uint8_t, 4>{(uint8_t)(color.r * 255.), (uint8_t)(color.g * 255.), (uint8_t)(color.b * 255.),
                                          (uint8_t)(color.a * 255.)};
        }

        ScopedGLTexture LoadTextureImage(const uint8_t* fileData, uint32_t fileSize)
        {
            auto freeImageData = [](unsigned char* ptr) { ::free(ptr); };
            using stbi_unique_ptr = std::unique_ptr<unsigned char, decltype(freeImageData)>;

            constexpr uint32_t DesiredComponentCount = 4;

            int w, h, c;
            // If c == 3, a component will be padded with 1.0f
            stbi_unique_ptr rgbaData(stbi_load_from_memory(fileData, fileSize, &w, &h, &c, DesiredComponentCount), freeImageData);
            if (!rgbaData) {
                throw std::runtime_error("Failed to load image file data.");
            }

            return CreateTexture(rgbaData.get(), DesiredComponentCount, w, h, GL_RGBA8);
        }

        /// Creates a texture and fills all array members with the data in rgba
        ScopedGLTexture CreateTextureOrCubemapRepeat(const uint8_t* rgba, uint32_t elemSize, uint32_t width, uint32_t height, GLenum format,
                                                     bool isCubemap)
        {
            assert(elemSize == 4);  // non-RGBA isn't implemented

            ScopedGLTexture texture{};

            const GLenum target = isCubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
            XRC_CHECK_THROW_GLCMD(glGenTextures(1, texture.resetAndPut()));
            XRC_CHECK_THROW_GLCMD(glBindTexture(target, texture.get()));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0));  // if we add mipmaps we need to change this
            if (isCubemap) {
                for (unsigned int i = 0; i < 6; i++)
                    XRC_CHECK_THROW_GLCMD(
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
            }
            else {
                XRC_CHECK_THROW_GLCMD(glTexImage2D(target, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
            }

            XRC_CHECK_THROW_GLCMD(glBindTexture(target, texture.get()));
            if (isCubemap) {
                for (unsigned int i = 0; i < 6; i++)
                    for (GLuint y = 0; y < height; ++y) {
                        const void* pixels = &rgba[(height - 1 - y) * width * elemSize];
                        XRC_CHECK_THROW_GLCMD(
                            glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
                    }
            }
            else {
                for (GLuint y = 0; y < height; ++y) {
                    const void* pixels = &rgba[(height - 1 - y) * width * elemSize];
                    XRC_CHECK_THROW_GLCMD(glTexSubImage2D(target, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
                }
            }
            XRC_CHECK_THROW_GLCMD(glBindTexture(target, 0));

            return texture;
        }

        ScopedGLTexture CreateFlatCubeTexture(RGBAColor color, GLenum format)
        {
            const std::array<uint8_t, 4> rgbaColor = LoadRGBAUI4(color);
            return CreateTextureOrCubemapRepeat(rgbaColor.data(), 4, 1, 1, format, true);
        }

        ScopedGLTexture CreateTexture(const uint8_t* rgba, uint32_t elemSize, uint32_t width, uint32_t height, GLenum format)
        {
            return CreateTextureOrCubemapRepeat(rgba, elemSize, width, height, format, false);
        }

        ScopedGLSampler CreateSampler(GLenum edgeSamplingMode)
        {
            ScopedGLSampler sampler{};
            XRC_CHECK_THROW_GLCMD(glGenSamplers(1, sampler.resetAndPut()));

            XRC_CHECK_THROW_GLCMD(glSamplerParameteri(sampler.get(), GL_TEXTURE_WRAP_S, edgeSamplingMode));
            XRC_CHECK_THROW_GLCMD(glSamplerParameteri(sampler.get(), GL_TEXTURE_WRAP_T, edgeSamplingMode));
            XRC_CHECK_THROW_GLCMD(glSamplerParameteri(sampler.get(), GL_TEXTURE_WRAP_R, edgeSamplingMode));

            return sampler;
        }
    }  // namespace GLTexture
}  // namespace Pbr

#endif
