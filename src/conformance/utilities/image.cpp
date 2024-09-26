// Copyright 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "image.h"

// These are required to cleanly use basis_universal
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

#include <transcoder/basisu_transcoder.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <iostream>
#include <sstream>
#include <numeric>
#include <unordered_map>
#include <mutex>

namespace
{
    template <typename T>
    T DivRoundingUp(T dividend, T divisor)
    {
        return (dividend + divisor - 1) / divisor;
    }
}  // namespace

namespace Conformance
{
    namespace Image
    {
        // This map is in preference order. Duplicate transcoder_texture_formats (e.g. BC7)
        // indicate that that format does not encode placeholder alpha data but uses an RGB mode internally.
        std::vector<std::pair<FormatParams, basist::transcoder_texture_format>> KTXFormatMetadata = {
            {{Codec::BC7, Channels::RGB, ColorSpaceType::sRGB}, basist::transcoder_texture_format::cTFBC7_RGBA},
            {{Codec::BC7, Channels::RGBA, ColorSpaceType::sRGB}, basist::transcoder_texture_format::cTFBC7_RGBA},
            {{Codec::ASTC, Channels::RGB, ColorSpaceType::sRGB}, basist::transcoder_texture_format::cTFASTC_4x4_RGBA},
            {{Codec::ASTC, Channels::RGBA, ColorSpaceType::sRGB}, basist::transcoder_texture_format::cTFASTC_4x4_RGBA},
            {{Codec::ETC, Channels::RGB, ColorSpaceType::sRGB}, basist::transcoder_texture_format::cTFETC1_RGB},
            {{Codec::ETC, Channels::RGBA, ColorSpaceType::sRGB}, basist::transcoder_texture_format::cTFETC2_RGBA},
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpaceType::sRGB}, basist::transcoder_texture_format::cTFRGBA32},

            {{Codec::BC7, Channels::RGB, ColorSpaceType::Linear}, basist::transcoder_texture_format::cTFBC7_RGBA},
            {{Codec::BC7, Channels::RGBA, ColorSpaceType::Linear}, basist::transcoder_texture_format::cTFBC7_RGBA},
            {{Codec::ASTC, Channels::RGB, ColorSpaceType::Linear}, basist::transcoder_texture_format::cTFASTC_4x4_RGBA},
            {{Codec::ASTC, Channels::RGBA, ColorSpaceType::Linear}, basist::transcoder_texture_format::cTFASTC_4x4_RGBA},
            {{Codec::ETC, Channels::RGB, ColorSpaceType::Linear}, basist::transcoder_texture_format::cTFETC1_RGB},
            {{Codec::ETC, Channels::RGBA, ColorSpaceType::Linear}, basist::transcoder_texture_format::cTFETC2_RGBA},
            {{Codec::Raw8bpc, Channels::RGBA, ColorSpaceType::Linear}, basist::transcoder_texture_format::cTFRGBA32},
        };
        std::unordered_map<FormatParams, basist::transcoder_texture_format, FormatParamsHash> KTXFormatMetadataMap = {
            KTXFormatMetadata.begin(), KTXFormatMetadata.end()};

        size_t ImageLevelMetadata::RowCount()
        {
            return DivRoundingUp(physicalDimensions.height, blockSize.height);
        }

        size_t ImageLevelMetadata::RowSizeInBlocksOrPixels()
        {
            return DivRoundingUp(physicalDimensions.width, blockSize.width);
        }

        // BasisU is not thread safe unless you pass a state pointer around,
        // so until we make everything else thread safe too, we can use a mutex
        std::mutex BasisUMutex;

        static void InitKTX2Impl(std::unique_lock<std::mutex>& lock, bool implicitInit)
        {
            static bool transcoderInitialized = false;
            assert(lock.owns_lock());

            if (!transcoderInitialized) {
                basist::basisu_transcoder_init();
                transcoderInitialized = true;
                if (implicitInit) {
                    std::cerr
                        << "Developer warning: Lazy-loading basisU. Calling InitKTX2() before starting your OpenXR session will reduce frame hitching."
                        << std::endl;
                }
            }
        }

        void InitKTX2()
        {
            std::unique_lock<std::mutex> lock(BasisUMutex);
            InitKTX2Impl(lock, false);
        }

        namespace FormatStrategies
        {
            enum MatchFidelity : uint8_t
            {
                Exact,
                NeedsTranscode,
                Uncompressed,
                NotPossible,
            };

            class FormatStrategy
            {
            public:
                virtual ~FormatStrategy() = default;

                /// Data used to pick a texture format for your graphics API
                virtual MatchFidelity TranscodeFidelity(basist::basis_tex_format sourceFormat, FormatParams destFormatParams) const = 0;

                virtual size_t RequiredScratchSpaceForLevel(FormatParams destFormatParams, basist::ktx2_transcoder& transcoder,
                                                            const basist::ktx2_image_level_info& imageLevelInfo) const = 0;

                virtual ImageLevel TranscodeLevel(FormatParams destFormatParams, basist::ktx2_transcoder& transcoder,
                                                  const basist::ktx2_image_level_info& imageLevelInfo,
                                                  span<uint8_t> scratchBuffer) const = 0;
            };

            class DecodeToRaw : public FormatStrategy
            {
            public:
                MatchFidelity TranscodeFidelity(basist::basis_tex_format /* sourceFormat */, FormatParams destFormatParams) const override
                {
                    if (IsCompressed(destFormatParams.codec)) {
                        return MatchFidelity::NotPossible;
                    }
                    // Other uncompressed formats may require code changes
                    if (destFormatParams.codec != Codec::Raw8bpc) {
                        return MatchFidelity::NotPossible;
                    }
                    if (destFormatParams.channels != Channels::RGBA) {
                        return MatchFidelity::NotPossible;
                    }
                    return MatchFidelity::Uncompressed;
                }

                size_t RequiredScratchSpaceForLevel(FormatParams destFormatParams, basist::ktx2_transcoder& transcoder,
                                                    const basist::ktx2_image_level_info& imageLevelInfo) const override
                {
                    if (TranscodeFidelity(transcoder.get_format(), destFormatParams) == MatchFidelity::NotPossible) {
                        throw std::logic_error("Invalid format params for DecodeToRaw");
                    }

                    auto targetFormat = KTXFormatMetadataMap[destFormatParams];
                    assert(basis_transcoder_format_is_uncompressed(targetFormat));

                    const uint32_t origWidth = imageLevelInfo.m_orig_width;
                    const uint32_t origHeight = imageLevelInfo.m_orig_height;

                    const uint32_t bytesPerPixel = basis_get_uncompressed_bytes_per_pixel(targetFormat);
                    const uint32_t numPixels = origWidth * origHeight;
                    const uint32_t bytesPerSlice = bytesPerPixel * numPixels;

                    return (size_t)bytesPerSlice;
                }

                ImageLevel TranscodeLevel(FormatParams destFormatParams, basist::ktx2_transcoder& transcoder,
                                          const basist::ktx2_image_level_info& imageLevelInfo, span<uint8_t> scratchBuffer) const override
                {

                    if (TranscodeFidelity(transcoder.get_format(), destFormatParams) == MatchFidelity::NotPossible) {
                        throw std::logic_error("Invalid format params for DecodeToRaw");
                    }

                    auto targetFormat = KTXFormatMetadataMap[destFormatParams];
                    assert(basis_transcoder_format_is_uncompressed(targetFormat));

                    const uint32_t origWidth = imageLevelInfo.m_orig_width;
                    const uint32_t origHeight = imageLevelInfo.m_orig_height;

                    const uint32_t bytesPerPixel = basis_get_uncompressed_bytes_per_pixel(targetFormat);
                    const uint32_t numPixels = origWidth * origHeight;
                    const uint32_t bytesPerSlice = bytesPerPixel * numPixels;
                    assert(scratchBuffer.size() == bytesPerSlice);

                    // if no alpha channel is present, transcoder still writes 255 to alpha
                    bool success = transcoder.transcode_image_level(  //
                        imageLevelInfo.m_level_index,                 // uint32_t level_index,
                        imageLevelInfo.m_layer_index,                 // uint32_t layer_index,
                        imageLevelInfo.m_face_index,                  // uint32_t face_index,
                        scratchBuffer.data(),                         // void* pOutput_blocks,
                        numPixels,                                    // uint32_t output_blocks_buf_size_in_blocks_or_pixels,
                        targetFormat,                                 // basist::transcoder_texture_format fmt,
                        0,                                            // uint32_t decode_flags = 0,
                        // using orig dims because it will chop off the excess when decoding to RGBA, probably?
                        origWidth,   // uint32_t output_row_pitch_in_blocks_or_pixels = 0,
                        origHeight,  // uint32_t output_rows_in_pixels = 0,
                        -1,          // int channel0 = -1,
                        -1,          // int channel1 = -1,
                        nullptr      // ktx2_transcoder_state *pState = nullptr,
                    );
                    if (!success) {
                        throw std::logic_error("CTS KTX2: Failed to transcode KTX2 image data.");
                    }

                    auto metadata = ImageLevelMetadata::MakeUncompressed(origWidth, origHeight);

                    return ImageLevel{metadata, scratchBuffer};
                }
            };

            class Recompress : public FormatStrategy
            {
            public:
                MatchFidelity TranscodeFidelity(basist::basis_tex_format sourceFormat, FormatParams destFormatParams) const override
                {
                    if (!IsCompressed(destFormatParams.codec)) {
                        return MatchFidelity::NotPossible;
                    }
                    // Other compressed formats may require code changes
                    if (destFormatParams.codec != Codec::ETC && destFormatParams.codec != Codec::ASTC &&
                        destFormatParams.codec != Codec::BC7) {
                        return MatchFidelity::NotPossible;
                    }

                    switch (sourceFormat) {
                    case basist::basis_tex_format::cETC1S:
                        if (destFormatParams.codec == Codec::ETC) {
                            return MatchFidelity::Exact;
                        }
                        break;
                    case basist::basis_tex_format::cUASTC4x4:
                        if (destFormatParams.codec == Codec::ASTC) {
                            return MatchFidelity::Exact;
                        }
                        break;
                    default:
                        throw std::logic_error("Invalid basis_tex_format");
                    }

                    return MatchFidelity::NeedsTranscode;
                }

                size_t RequiredScratchSpaceForLevel(FormatParams destFormatParams, basist::ktx2_transcoder& transcoder,
                                                    const basist::ktx2_image_level_info& imageLevelInfo) const override
                {
                    if (TranscodeFidelity(transcoder.get_format(), destFormatParams) == MatchFidelity::NotPossible) {
                        throw std::logic_error("Invalid format params for MatchFidelity");
                    }

                    auto targetFormat = KTXFormatMetadataMap[destFormatParams];
                    assert(!basis_transcoder_format_is_uncompressed(targetFormat));

                    const uint32_t dstBlocksX = DivRoundingUp(imageLevelInfo.m_width, basis_get_block_width(targetFormat));
                    const uint32_t dstBlocksY = DivRoundingUp(imageLevelInfo.m_height, basis_get_block_height(targetFormat));

                    const uint32_t blocksPerSlice = dstBlocksX * dstBlocksY;
                    const uint32_t bytesPerSlice = blocksPerSlice * basis_get_bytes_per_block_or_pixel(targetFormat);

                    return (size_t)bytesPerSlice;
                }

                ImageLevel TranscodeLevel(FormatParams destFormatParams, basist::ktx2_transcoder& transcoder,
                                          const basist::ktx2_image_level_info& imageLevelInfo, span<uint8_t> scratchBuffer) const override
                {

                    if (TranscodeFidelity(transcoder.get_format(), destFormatParams) == MatchFidelity::NotPossible) {
                        throw std::logic_error("Invalid format params for MatchFidelity");
                    }

                    auto targetFormat = KTXFormatMetadataMap[destFormatParams];
                    assert(!basis_transcoder_format_is_uncompressed(targetFormat));

                    const uint32_t origWidth = imageLevelInfo.m_orig_width;
                    const uint32_t origHeight = imageLevelInfo.m_orig_height;

                    const uint32_t blockWidth = basis_get_block_width(targetFormat);
                    const uint32_t blockHeight = basis_get_block_height(targetFormat);

                    const uint32_t dstBlocksX = DivRoundingUp(origWidth, blockWidth);
                    const uint32_t dstBlocksY = DivRoundingUp(origHeight, blockHeight);
                    if ((imageLevelInfo.m_level_index == 0) &&
                        (dstBlocksX * blockWidth != origWidth || dstBlocksY * blockHeight != origHeight)) {
                        throw std::logic_error(
                            "CTS KTX2: transcode setup failed: largest mip's source width (" + std::to_string(origWidth) + ") or height (" +
                            std::to_string(origHeight) + ") was not divisible by block size (" + std::to_string(blockWidth) + ", " +
                            std::to_string(blockHeight) + ") of target format " + basis_get_format_name(targetFormat) + ".");
                    }

                    const uint32_t blocksPerSlice = dstBlocksX * dstBlocksY;
                    const uint32_t bytesPerSlice = blocksPerSlice * basis_get_bytes_per_block_or_pixel(targetFormat);
                    assert(scratchBuffer.size() == bytesPerSlice);

                    // if no alpha channel is present, transcoder still writes 255 to alpha
                    bool success = transcoder.transcode_image_level(  //
                        imageLevelInfo.m_level_index,                 // uint32_t level_index,
                        imageLevelInfo.m_layer_index,                 // uint32_t layer_index,
                        imageLevelInfo.m_face_index,                  // uint32_t face_index,
                        scratchBuffer.data(),                         // void* pOutput_blocks,
                        dstBlocksX * dstBlocksY,                      // uint32_t output_blocks_buf_size_in_blocks_or_pixels,
                        targetFormat,                                 // basist::transcoder_texture_format fmt,
                        // cDecodeFlagsHighQuality seems to switch to more compute-expensive encoding algorithms
                        basist::cDecodeFlagsHighQuality,  // uint32_t decode_flags = 0,
                        // using orig dims because it will chop off the excess when decoding to RGBA, probably?
                        dstBlocksX,  // uint32_t output_row_pitch_in_blocks_or_pixels = 0,
                        dstBlocksY,  // uint32_t output_rows_in_pixels = 0,
                        // source channel overrides for R and RG textures.
                        // -1 (default) results in channel0 = 0 (R) and channel1 = 3 (A).
                        -1,      // int channel0 = -1,
                        -1,      // int channel1 = -1,
                        nullptr  // ktx2_transcoder_state *pState = nullptr,
                    );
                    if (!success) {
                        throw std::logic_error("CTS KTX2: Failed to transcode KTX2 image data.");
                    }

                    ImageLevelMetadata metadata = {
                        {(int32_t)origWidth, (int32_t)origHeight},    // XrExtent2Di dimensions{}
                        {(int32_t)blockWidth, (int32_t)blockHeight},  // XrExtent2Di blockSize{}
                    };

                    return ImageLevel{metadata, scratchBuffer};
                }
            };

            using FormatHandler = ImageLevel (*)(basist::ktx2_transcoder& transcoder, const basist::ktx2_image_level_info& imageLevelInfo,
                                                 std::vector<uint8_t>& scratchBuffer);
        }  // namespace FormatStrategies

        bool IsCompressed(Codec codec)
        {
            switch (codec) {
            case Codec::Raw8bpc:
                return false;
                break;
            case Codec::ETC:
            case Codec::ASTC:
            case Codec::BC7:
                return true;
                break;
            default:
                throw std::logic_error("Unhandled case in IsCompressed");
            };
        }

        size_t FormatParams::BytesPerBlockOrPixel() const
        {
            // partly based on values from basis_get_bytes_per_block_or_pixel
            switch (codec) {
            case Codec::Raw8bpc:
                return (size_t)channels;
                break;
            case Codec::ETC:
                // RGBA is ETC2, so 16 bit
                return channels == Channels::RGBA ? 16 : 8;
                break;
            case Codec::ASTC:
            case Codec::BC7:
                return 16;
                break;
            default:
                throw std::logic_error("Unhandled case in BytesPerBlockOrPixel");
            };
        }

        FormatParams FindRawFormat(Channels sourceChannels, ColorSpaceType colorSpaceType, span<const FormatParams> supportedFormats)
        {
            FormatParams const* maybeFormatParams = nullptr;
            const FormatParams convertibleFormats[] = {
                {Codec::Raw8bpc, Channels::RGB, colorSpaceType},
                {Codec::Raw8bpc, Channels::RGBA, colorSpaceType},
            };
            for (const auto& convertibleFormat : convertibleFormats) {
                if (convertibleFormat.channels < sourceChannels) {
                    continue;
                }
                if (std::find(supportedFormats.begin(), supportedFormats.end(), convertibleFormat) != supportedFormats.end()) {
                    maybeFormatParams = &convertibleFormat;
                    break;
                }
            }
            if (maybeFormatParams == nullptr) {
                throw std::runtime_error(
                    "FindRawFormat could not find appropriate graphics-plugin-supported format for codec: Raw8bpc, Channels: " +
                    std::to_string(sourceChannels) + ", sRGB:" + (colorSpaceType == ColorSpaceType::sRGB ? "sRGB" : "Linear"));
            }
            return *maybeFormatParams;
        }

        Image Image::LoadAndTranscodeKTX2(span<const uint8_t> encodedData, bool sRGB, span<const FormatParams> supportedFormats,
                                          std::vector<uint8_t>& scratchBuffer, const char* imageDesc, XrExtent2Di expectedDimensions)
        {

            std::unique_lock<std::mutex> lock(BasisUMutex);

            // Initializing the tables required for KTX2 decoding can take (~9) milliseconds,
            // so this should ideally be done at startup to avoid adding to the hitch on model load.
            InitKTX2Impl(lock, true);

            basist::ktx2_transcoder transcoder{};

            // Load a little metadata.
            transcoder.init(encodedData.data(), encodedData.size());
            if (!transcoder.start_transcoding()) {
                throw std::logic_error(std::string("CTS KTX2: Transcoding of KTX2 file failed at start for ") + imageDesc);
            }

            if (transcoder.get_faces() > 1) {
                throw std::logic_error(
                    std::string("CTS KTX2: KTX2 file had multiple cubemap faces - cubemaps are currently not supported for ") + imageDesc);
            }

            if (transcoder.get_layers() > 0) {
                throw std::logic_error(
                    std::string("CTS KTX2: KTX2 file had multiple array layers - texture arrays are currently not supported for ") +
                    imageDesc);
            }

            basist::basis_tex_format sourceFormat = transcoder.get_format();
            Channels sourceChannels = transcoder.get_has_alpha() ? Channels::RGBA : Channels::RGB;
            ColorSpaceType desiredColorSpace = sRGB ? ColorSpaceType::sRGB : ColorSpaceType::Linear;

            FormatParams targetFormat;
            // pair of (fidelity, extra channels)
            auto targetFormatFidelity = std::make_pair(FormatStrategies::MatchFidelity::NotPossible, int8_t(-128));
            FormatStrategies::FormatStrategy const* formatStrategy = nullptr;

            auto isSupported = [&](FormatParams format) {
                return std::find(supportedFormats.begin(), supportedFormats.end(), format) != supportedFormats.end();
            };

            auto decodeToRaw = FormatStrategies::DecodeToRaw();
            auto recompress = FormatStrategies::Recompress();
            const FormatStrategies::FormatStrategy* strategies[] = {
                &decodeToRaw,
                &recompress,
            };

            uint16_t unsupportedFormats = 0;
            uint16_t wrongColorSpaceFormats = 0;
            uint16_t insufficientChannelFormats = 0;
            for (auto& formatData : KTXFormatMetadata) {
                if (!isSupported(formatData.first)) {
                    ++unsupportedFormats;
                    continue;
                }
                if (formatData.first.colorSpaceType != desiredColorSpace) {
                    ++wrongColorSpaceFormats;
                    continue;
                }
                int8_t extraChannels = formatData.first.channels - sourceChannels;
                if (extraChannels < 0) {
                    // target has fewer channels than source
                    ++insufficientChannelFormats;
                    continue;
                }
                for (const FormatStrategies::FormatStrategy* strategy : strategies) {
                    auto candidateFidelity = std::make_pair(strategy->TranscodeFidelity(sourceFormat, formatData.first), extraChannels);
                    if (candidateFidelity < targetFormatFidelity) {
                        targetFormat = formatData.first;
                        formatStrategy = strategy;
                        targetFormatFidelity = candidateFidelity;
                    }
                }
                if (targetFormatFidelity.first == FormatStrategies::MatchFidelity::NotPossible) {
                    throw std::logic_error("No strategy found for format listed in KTXFormatMetadata");
                }
            }

            if (targetFormatFidelity.first == FormatStrategies::MatchFidelity::NotPossible) {
                std::ostringstream oss;
                oss << "LoadAndTranscodeKTX2: Unable to find valid transcode format: of " << KTXFormatMetadata.size() << " formats, "  //
                    << unsupportedFormats << " were marked as unsupported by the backend,"                                             //
                    << wrongColorSpaceFormats << " had the wrong color space (linear vs. sRGB), and"                                   //
                    << insufficientChannelFormats << " had too few channels to represent the source data";
                throw std::runtime_error(oss.str());
            }

            uint32_t mipLevels = transcoder.get_levels();

            // compute and validate these once
            std::vector<basist::ktx2_image_level_info> imageLevelInfos;
            imageLevelInfos.reserve(mipLevels);

            for (uint32_t mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
                auto formatError = [=](auto... error) {
                    std::ostringstream oss;
                    // apply error arguments to oss in order
                    (void)std::initializer_list<int>{(oss << error, 0)...};
                    oss << " for level " << mipLevel << "/" << mipLevels << " of KTX2 file " << imageDesc;
                    return oss.str();
                };

                basist::ktx2_image_level_info imageLevelInfo;
                if (!transcoder.get_image_level_info(imageLevelInfo, mipLevel, 0, 0)) {
                    throw std::logic_error(formatError("CTS KTX2: Failed to get image level info"));
                }

                if ((imageLevelInfo.m_orig_width < 1) || (imageLevelInfo.m_orig_height < 1)) {
                    throw std::logic_error(formatError("CTS KTX2: Invalid image data for image:",     //
                                                       " zero width (", imageLevelInfo.m_orig_width,  //
                                                       ") or height (", imageLevelInfo.m_orig_height, ")"));
                }

                if (mipLevel == 0) {
                    if (expectedDimensions.width > 0) {
                        if ((uint32_t)expectedDimensions.width != imageLevelInfo.m_orig_width) {
                            throw std::logic_error(formatError("CTS KTX2: Image width mismatch: ",           //
                                                               expectedDimensions.width, " (expected) != ",  //
                                                               imageLevelInfo.m_orig_width, " (actual)"));
                        }
                    }

                    if (expectedDimensions.height > 0) {
                        if ((uint32_t)expectedDimensions.height != imageLevelInfo.m_orig_height) {
                            throw std::logic_error(formatError("CTS KTX2: Image height mismatch: ",           //
                                                               expectedDimensions.height, " (expected) != ",  //
                                                               imageLevelInfo.m_orig_height, " (actual)"));
                        }
                    }

                    if (imageLevelInfo.m_orig_width != imageLevelInfo.m_width) {
                        throw std::logic_error(formatError("CTS KTX2: Image physical width ", imageLevelInfo.m_orig_width,
                                                           " does not match view width ", imageLevelInfo.m_width,
                                                           ", padding to reach block width while using a smaller view is not supported."));
                    }

                    if (imageLevelInfo.m_orig_height != imageLevelInfo.m_height) {
                        throw std::logic_error(formatError("CTS KTX2: Image physical height ", imageLevelInfo.m_orig_height,
                                                           " does not match view height ", imageLevelInfo.m_height,
                                                           ", padding to reach block height while using a smaller view is not supported."));
                    }
                }

                imageLevelInfos.push_back(imageLevelInfo);
            }

            std::vector<size_t> scratchBufferSizes;
            scratchBufferSizes.reserve(mipLevels);

            for (uint32_t mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
                scratchBufferSizes.push_back(
                    formatStrategy->RequiredScratchSpaceForLevel(targetFormat, transcoder, imageLevelInfos[mipLevel]));
            }

            size_t scratchBufferSize = std::accumulate(scratchBufferSizes.begin(), scratchBufferSizes.end(), size_t(0));
            scratchBuffer.resize(scratchBufferSize);

            Image ret{targetFormat};
            auto it = scratchBuffer.begin();
            for (uint32_t mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
                size_t size = scratchBufferSizes[mipLevel];
                assert(it + size <= scratchBuffer.end());
                ret.levels.emplace_back(
                    formatStrategy->TranscodeLevel(targetFormat, transcoder, imageLevelInfos[mipLevel], {it, it + size}));
                it += size;
            }

            return ret;
        }

    }  // namespace Image
}  // namespace Conformance
