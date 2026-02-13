// Copyright 2022-2026 The Khronos Group Inc.
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

#include "PbrCommon.h"
#include "PbrTexture.h"
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
        static ScopedGLTexture CreateTextureOrCubemapRepeat(const Image::Image& image, bool isCubemap)
        {
            GLFormatData glFormat = ToGLFormatData(image.format);

            GLenum internalFormat = glFormat.InternalFormat;
            assert(internalFormat != GLFormatData::Unpopulated);
            (void)internalFormat;

            GLenum uncompressedFormat = glFormat.UncompressedFormat;
            GLenum uncompressedType = glFormat.UncompressedType;

            bool isCompressed = Image::IsCompressed(image.format.codec);

            ScopedGLTexture texture{};

            const GLenum target = isCubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

            XRC_CHECK_THROW_GLCMD(glGenTextures(1, texture.resetAndPut()));
            XRC_CHECK_THROW_GLCMD(glBindTexture(target, texture.get()));

            // note that these are overridden by the sampler
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0));
            XRC_CHECK_THROW_GLCMD(glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, image.levels.size() - 1));
            XRC_CHECK_THROW_GLCMD(glBindTexture(target, texture.get()));

            for (int mipLevel = 0; mipLevel < (int)image.levels.size(); mipLevel++) {
                auto levelData = image.levels[mipLevel];
                auto width = levelData.metadata.physicalDimensions.width;
                auto height = levelData.metadata.physicalDimensions.height;

                auto uploadForSubtarget = [&](GLenum subtarget) {
                    if (isCompressed) {
                        XRC_CHECK_THROW_GLCMD(glCompressedTexImage2D(subtarget, mipLevel, glFormat.InternalFormat, width, height, 0,
                                                                     levelData.data.size(), levelData.data.data()));
                    }
                    else {
                        assert(uncompressedFormat != GLFormatData::Unpopulated);
                        assert(uncompressedType != GLFormatData::Unpopulated);
                        XRC_CHECK_THROW_GLCMD(glTexImage2D(subtarget, mipLevel, glFormat.InternalFormat, width, height, 0,
                                                           uncompressedFormat, uncompressedType, levelData.data.data()));
                    }
                };

                if (isCubemap) {
                    for (unsigned int i = 0; i < 6; i++) {
                        uploadForSubtarget(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
                    }
                }
                else {
                    uploadForSubtarget(target);
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
