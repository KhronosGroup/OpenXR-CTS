// Copyright 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <nonstd/span.hpp>
#include <openxr/openxr.h>

#include <cstddef>
#include <stdint.h>
#include <vector>

namespace Conformance
{
    namespace Image
    {
        using nonstd::span;

        /// Texture storage type: either a raw channel arrangement or some texture codec.
        /// Like formats only distinguished by the presence of an alpha channel or sRGB-ness
        /// (e.g. ETC1 vs. ETC2) may be combined, as they are distinguished by the other flags.
        // TODO: implement ASTC
        enum class Codec : uint8_t
        {
            /// Just raw RGB or RGBA.... Everybody supports at least RGBA, but they're very large.
            Raw8bpc,
            /// ETC1 block compression. Some basisu ktx2 textures are a subset of ETC1 and so can be uploaded unmodified from the raw data if this format is supported.
            ETC,
            /// ASTC block compression. Some basisu ktx2 textures are a subset of ASTC and so can be uploaded unmodified from the raw data if this format is supported.
            ASTC,
            /// BC7 block compression.
            BC7,

        };

        /// Whether a TextureCodec represents a compressed format.
        bool IsCompressed(Codec codec);

        enum Channels : uint8_t
        {
            RGB = 3,
            RGBA = 4,
        };

        enum class ColorSpaceType : uint8_t
        {
            Linear,
            sRGB,
        };

        /// Data to compute an API-specific image format
        // TODO this is basically the same stuff as we store for each texture format in the plugins... can we reuse that structure?
        struct FormatParams
        {
            Codec codec;

            Channels channels;

            ColorSpaceType colorSpaceType;

            /// The number of bytes per block or pixel, depending on whether this is compressed.
            size_t BytesPerBlockOrPixel() const;

            static inline FormatParams R8G8B8A8(bool sRGB)
            {
                return FormatParams{
                    Codec::Raw8bpc,
                    Channels::RGBA,
                    sRGB ? ColorSpaceType::sRGB : ColorSpaceType::Linear,
                };
            }
        };
        inline bool operator==(const FormatParams& lhs, const FormatParams& rhs)
        {
            return lhs.codec == rhs.codec && lhs.channels == rhs.channels && lhs.colorSpaceType == rhs.colorSpaceType;
        }
        struct FormatParamsHash
        {
            std::size_t operator()(const FormatParams& k) const
            {
                using hash = std::hash<uint8_t>;
                return ((hash()((uint8_t)k.codec) ^ (hash()((uint8_t)k.channels) << 1)) >> 1) ^ (hash()((uint8_t)k.colorSpaceType) << 1);
            }
        };

        FormatParams FindRawFormat(Channels sourceChannels, ColorSpaceType colorSpaceType, span<const FormatParams> supportedFormats);

        /// Data for a single 2D texture image, at a single mip level.
        struct ImageLevelMetadata
        {
            /// Width and height of the image.
            /// Must be a multiple of the block size.
            /// If this is a compressed format and this level is not the mip level,
            /// the physical size and view size (what is read during sampling)
            /// must be the same, otherwise the view size may be smaller than this.
            XrExtent2Di physicalDimensions{};

            /// The size in pixels of a single block.
            /// For uncompressed formats, this must be set to {1, 1}.
            XrExtent2Di blockSize{};

            size_t RowCount();
            size_t RowSizeInBlocksOrPixels();

            static inline ImageLevelMetadata MakeUncompressed(int32_t width, int32_t height)
            {
                return ImageLevelMetadata{
                    {int32_t(width), int32_t(height)},  // XrExtent2Di dimensions{}
                    {1, 1},                             // XrExtent2Di blockSize{}
                };
            }
        };

        /// Data for a single mip level of @ref Image
        struct ImageLevel
        {
            /// Metadata for a single mip level
            ImageLevelMetadata metadata;

            /// Warning, this is a non-owning reference into somebody else's buffer.
            /// Be careful not to hang on to it longer than that scope.
            span<const uint8_t> data;
        };

        // Does the costly initialization of basis_universal's internal tables.
        // (According to libktx, "Requires ~9 milliseconds when compiled and executed natively on a Core i7 2.2 GHz.")
        void InitKTX2();

        /// An image, possibly with multiple mip levels.
        struct Image
        {
            // This does not currently handle multiple layers or faces.

            /// Data used to pick a texture format for your graphics API
            FormatParams format{};

            /// Data references and metadata for each mip level, from largest to smallest.
            std::vector<ImageLevel> levels;

            /// Parse KTX2 binary data into an image that can be loaded.
            /// Will perform transcoding if required.
            ///
            /// @note that the returned image may contain a reference to the supplied @p encodedData and/or @p scratchBuffer
            /// so its lifetime should be considered to be tied to that.
            ///
            /// @param encodedData a KTX2 blob.
            /// @param supportedCompressionFormats The compression formats that are acceptable. Image will be transcoded or (worst case) decoded to one of those formats.
            /// @param scratchBuffer a vector that can be cleared, assigned, etc. In case of transcoding being required, the image will be transcoded into this buffer.
            /// @param imageDesc a string to include in thrown errors to aid in identifying the specific image at issue.
            /// @param expectedDimensions the expected dimensions of the base mip level, or {0, 0} to skip this validation at this stage.
            static Image LoadAndTranscodeKTX2(span<const uint8_t> encodedData, bool sRGB, span<const FormatParams> supportedFormats,
                                              std::vector<uint8_t>& scratchBuffer, const char* imageDesc,
                                              XrExtent2Di expectedDimensions = {0, 0});
        };
    }  // namespace Image
}  // namespace Conformance
