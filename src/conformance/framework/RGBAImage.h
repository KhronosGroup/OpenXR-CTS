// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace Conformance
{
    /**
     * @defgroup cts_rgba RGBA image support
     * @ingroup cts_framework
     */
    ///@{
    union RGBA8Color
    {
        struct
        {
            uint8_t R;
            uint8_t G;
            uint8_t B;
            uint8_t A;
        } Channels;

        uint32_t Pixel;
    };

    static_assert(sizeof(RGBA8Color) == 4, "Incorrect RGBA8Color size");

    enum class WordWrap
    {
        Disabled,
        Enabled,
    };

    /// A 2D, 32 bit-per-pixel RGBA image
    struct RGBAImage
    {
        RGBAImage(int width, int height);

        static RGBAImage Load(const char* path);

        void PutText(const XrRect2Di& rect, const char* text, int pixelHeight, XrColor4f color, WordWrap wordWrap = WordWrap::Enabled);
        void DrawRect(int x, int y, int w, int h, XrColor4f color);
        void DrawRectBorder(int x, int y, int w, int h, int thickness, XrColor4f color);
        void ConvertToSRGB();

        /// Copy image data row-by-row to a buffer with a (probably different) row pitch explicitly specified,
        /// and optionally an offset from the start of that buffer.
        void CopyWithStride(uint8_t* data, uint32_t rowPitch, uint32_t offset = 0) const;

        bool isSrgb = false;
        std::vector<RGBA8Color> pixels;
        int32_t width;
        int32_t height;
    };

    /// A 2D, 32 bit-per-pixel RGBA image
    class RGBAImageCache
    {
    public:
        RGBAImageCache() = default;

        RGBAImageCache(RGBAImageCache&&) = default;
        RGBAImageCache& operator=(RGBAImageCache&&) = default;

        void Init();

        bool IsValid() const noexcept
        {
            return m_cacheMutex != nullptr;
        }

        std::shared_ptr<RGBAImage> Load(const char* path);

    private:
        // in unique_ptr to make it moveable
        std::unique_ptr<std::mutex> m_cacheMutex;
        std::map<std::string, std::shared_ptr<RGBAImage>> m_imageCache;
    };

    /// Copy a contiguous image into a buffer for GPU usage - with stride/pitch.
    ///
    /// @param source Source buffer, with all pixels contiguous
    /// @param dest Destination buffer (with offset applied, if applicable)
    /// @param rowSize bytes in a row (bytes per pixel * width in pixels)
    /// @param rows number of rows to copy (height in pixels)
    /// @param rowPitch destination row pitch in bytes
    void CopyWithStride(const uint8_t* source, uint8_t* dest, uint32_t rowSize, uint32_t rows, uint32_t rowPitch);
    /// @}
}  // namespace Conformance
