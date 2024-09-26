// Copyright 2022-2024, The Khronos Group Inc.
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
#include "GLFormats.h"

#include "../PbrCommon.h"
#include "../PbrTexture.h"
#include <utilities/image.h>

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
        namespace Image = Conformance::Image;

        ScopedGLTexture LoadTextureImage(const GLResources& pbrResources, bool sRGB, const uint8_t* fileData, uint32_t fileSize)
        {
            StbiLoader::OwningImage<StbiLoader::stbi_unique_ptr> owningImage =
                StbiLoader::LoadTextureImage(pbrResources.GetSupportedFormats(), sRGB, fileData, fileSize);
            return CreateTexture(owningImage.image);
        }

        /// Creates a texture and fills all array members with the data in rgba
        ScopedGLTexture CreateTextureOrCubemapRepeat(const Image::Image& image, bool isCubemap)
        {
            assert(image.format.codec == Image::Codec::Raw8bpc);     // only 8bpc is implemented
            assert(image.format.channels == Image::Channels::RGBA);  // non-RGBA isn't implemented

            GLFormatData glFormat = ToGLFormatData(image.format);

            GLenum internalFormat = glFormat.InternalFormat;
            assert(internalFormat != GLFormatData::Unpopulated);
            GLenum uncompressedFormat = glFormat.UncompressedFormat;
            GLenum uncompressedType = glFormat.UncompressedType;

            bool isCompressed = Image::IsCompressed(image.format.codec);

            uint16_t baseMipWidth = image.levels[0].metadata.physicalDimensions.width;
            uint16_t baseMipHeight = image.levels[0].metadata.physicalDimensions.height;

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
            if (isCompressed) {
                assert(!isCubemap);  // compressed cubemaps aren't implemented
                XRC_CHECK_THROW_GLCMD(
                    glCompressedTexImage2D(target, 0, glFormat.InternalFormat, baseMipWidth, baseMipHeight, 0, 0, nullptr));
            }
            else {
                assert(uncompressedFormat != GLFormatData::Unpopulated);
                assert(uncompressedType != GLFormatData::Unpopulated);
                if (isCubemap) {
                    for (unsigned int i = 0; i < 6; i++)
                        XRC_CHECK_THROW_GLCMD(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, glFormat.InternalFormat, baseMipWidth,
                                                           baseMipHeight, 0, uncompressedFormat, uncompressedType, nullptr));
                }
                else {
                    XRC_CHECK_THROW_GLCMD(glTexImage2D(target, 0, glFormat.InternalFormat, baseMipWidth, baseMipHeight, 0,
                                                       uncompressedFormat, uncompressedType, nullptr));
                }
            }
            XRC_CHECK_THROW_GLCMD(glBindTexture(target, texture.get()));

            for (int mipLevel = 0; mipLevel < (int)image.levels.size(); mipLevel++) {
                auto levelData = image.levels[mipLevel];
                auto width = levelData.metadata.physicalDimensions.width;
                auto height = levelData.metadata.physicalDimensions.height;
                if (isCompressed) {
                    XRC_CHECK_THROW_GLCMD(glCompressedTexSubImage2D(target, mipLevel, 0, 0, width, height, glFormat.InternalFormat,
                                                                    levelData.data.size(), levelData.data.data()));
                }
                else {
                    assert(uncompressedFormat != GLFormatData::Unpopulated);
                    assert(uncompressedType != GLFormatData::Unpopulated);
                    if (isCubemap) {
                        for (unsigned int i = 0; i < 6; i++) {
                            XRC_CHECK_THROW_GLCMD(glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, width, height,
                                                                  uncompressedFormat, uncompressedType, levelData.data.data()));
                        }
                    }
                    else {
                        XRC_CHECK_THROW_GLCMD(
                            glTexSubImage2D(target, 0, 0, 0, width, height, uncompressedFormat, uncompressedType, levelData.data.data()));
                    }
                }
            }
            XRC_CHECK_THROW_GLCMD(glBindTexture(target, 0));

            return texture;
        }

        ScopedGLTexture CreateFlatCubeTexture(RGBAColor color, bool sRGB)
        {
            const std::array<uint8_t, 4> rgbaColor = LoadRGBAUI4(color);

            auto formatParams = Image::FormatParams::R8G8B8A8(sRGB);
            auto metadata = Image::ImageLevelMetadata::MakeUncompressed(1, 1);
            auto face = Image::Image{formatParams, {{metadata, rgbaColor}}};

            return CreateTextureOrCubemapRepeat(face, true);
        }

        ScopedGLTexture CreateTexture(const Image::Image& image)
        {
            return CreateTextureOrCubemapRepeat(image, false);
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
