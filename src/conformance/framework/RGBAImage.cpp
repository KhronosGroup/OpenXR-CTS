// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <array>
#include <memory>
#include <unordered_map>
#include "RGBAImage.h"

#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
extern void* Conformance_Android_Get_Asset_Manager();
#endif

// Only one compilation unit can have the STB implementations.
#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_truetype.h"

// Some platforms require reading files from specific
// sandboxed directories.
#ifndef PATH_PREFIX
#define PATH_PREFIX ""
#endif

namespace
{
#ifdef XR_USE_PLATFORM_ANDROID

    /// Wrapper for AAsset
    class UniqueAsset
    {
    public:
        explicit UniqueAsset(AAsset* asset) : asset_(asset)
        {
        }
        UniqueAsset(UniqueAsset const&) = delete;
        UniqueAsset(UniqueAsset&&) = delete;
        UniqueAsset& operator=(UniqueAsset const&) = delete;
        UniqueAsset& operator=(UniqueAsset&&) = delete;
        ~UniqueAsset()
        {
            reset();
        }
        explicit operator bool() const
        {
            return asset_ != nullptr;
        }

        void reset() noexcept
        {

            if (asset_ != nullptr) {
                AAsset_close(asset_);
                asset_ = nullptr;
            }
        }

        AAsset* get() const noexcept
        {
            return asset_;
        }

    private:
        AAsset* asset_;
    };

#endif
    // Convert R32G32B32A_FLOAT to R8G8B8A8_UNORM.
    Conformance::RGBA8Color AsRGBA(float r, float g, float b, float a)
    {
        return {{(uint8_t)(255 * r), (uint8_t)(255 * g), (uint8_t)(255 * b), (uint8_t)(255 * a)}};
    };

    // Cached TrueType font baked as glyphs.
    struct BakedFont
    {
        BakedFont(int pixelHeight)
        {
            constexpr const char* FontPath = PATH_PREFIX "SourceCodePro-Regular.otf";

#ifdef XR_USE_PLATFORM_ANDROID
            AAssetManager* assetManager = (AAssetManager*)Conformance_Android_Get_Asset_Manager();
            UniqueAsset asset(AAssetManager_open(assetManager, "SourceCodePro-Regular.otf", AASSET_MODE_BUFFER));

            if (!asset) {
                throw std::runtime_error((std::string("Unable to open font ") + FontPath).c_str());
            }

            size_t length = AAsset_getLength(asset.get());
            const uint8_t* buf = (const uint8_t*)AAsset_getBuffer(asset.get());
            if (!buf) {
                throw std::runtime_error((std::string("Unable to open font ") + FontPath).c_str());
            }
            std::vector<uint8_t> fontData(buf, buf + length);
#else
            std::ifstream file;
            file.open(FontPath, std::ios::in | std::ios::binary);
            if (!file) {
                throw std::runtime_error((std::string("Unable to open font ") + FontPath).c_str());
            }

            file.seekg(0, std::ios::end);
            std::vector<uint8_t> fontData((uint32_t)file.tellg());
            file.seekg(0, std::ios::beg);

            file.read(reinterpret_cast<char*>(fontData.data()), fontData.size());
#endif
            // This is just a starting size.
            m_bitmapWidth = 1024;
            m_bitmapHeight = 64;

        retry:
            glyphBitmap.resize(m_bitmapWidth * m_bitmapHeight);

            int res = stbtt_BakeFontBitmap(fontData.data(), 0, (float)pixelHeight, glyphBitmap.data(), m_bitmapWidth, m_bitmapHeight,
                                           StartChar, (int)m_bakedChars.size(), m_bakedChars.data());
            if (res == 0) {
                throw std::runtime_error((std::string("Unable to parse font") + FontPath).c_str());
            }
            else if (res < 0) {
                // Bitmap was not big enough to fit so double size and try again.
                m_bitmapHeight *= 2;
                goto retry;
            }
        }

        static std::shared_ptr<const BakedFont> GetOrCreate(int pixelHeight)
        {
            std::unordered_map<int, std::shared_ptr<BakedFont>> s_bakedFonts;
            auto it = s_bakedFonts.find(pixelHeight);
            if (it == s_bakedFonts.end()) {
                std::shared_ptr<BakedFont> font = std::make_shared<BakedFont>(pixelHeight);
                s_bakedFonts.insert({pixelHeight, font});
                return font;
            }

            return it->second;
        }

        const stbtt_bakedchar& GetBakedChar(char c) const
        {
            const char safeChar = (c < StartChar || c > EndChar) ? '_' : c;
            return m_bakedChars[safeChar - StartChar];
        }

        const uint8_t* GetBakedCharRow(const stbtt_bakedchar& bc, int charY) const
        {
            return glyphBitmap.data() + ((charY + bc.y0) * m_bitmapWidth);
        }

    private:
        static constexpr int StartChar = ' ';  // 32
        static constexpr int EndChar = '~';    // 126

        std::vector<uint8_t> glyphBitmap;  // Glyphs are single channel
        std::array<stbtt_bakedchar, EndChar - StartChar + 1> m_bakedChars;
        int m_bitmapWidth;
        int m_bitmapHeight;
    };
}  // namespace

namespace Conformance
{
    RGBAImage::RGBAImage(int width, int height) : width(width), height(height)
    {
        pixels.resize(width * height);
    }

    /* static */ RGBAImage RGBAImage::Load(const char* path)
    {
        constexpr int RequiredComponents = 4;  // RGBA

        int width, height;

#ifdef XR_USE_PLATFORM_ANDROID
        // for use by the exception, if required.
        auto fullPath = path;
        stbi_uc* uc = nullptr;
        {
            AAssetManager* assetManager = (AAssetManager*)Conformance_Android_Get_Asset_Manager();
            UniqueAsset asset(AAssetManager_open(assetManager, path, AASSET_MODE_BUFFER));

            if (!asset) {
                throw std::runtime_error((std::string("Unable to load asset ") + path).c_str());
            }

            size_t length = AAsset_getLength(asset.get());

            auto buf = AAsset_getBuffer(asset.get());

            if (!buf) {
                throw std::runtime_error((std::string("Unable to load asset ") + path).c_str());
            }

            uc = stbi_load_from_memory((const stbi_uc*)buf, length, &width, &height, nullptr, RequiredComponents);
        }
#else
        char fullPath[512];
        strcpy(fullPath, PATH_PREFIX);
        strcat(fullPath, path);
        stbi_uc* const uc = stbi_load(fullPath, &width, &height, nullptr, RequiredComponents);
#endif
        if (uc == nullptr) {
            throw std::runtime_error((std::string("Unable to load file ") + fullPath).c_str());
        }

        RGBAImage image(width, height);
        memcpy(image.pixels.data(), uc, width * height * RequiredComponents);

        stbi_image_free(uc);

        // Images loaded from files are assumed to be SRGB
        image.isSrgb = true;

        return image;
    }

    void RGBAImage::PutText(const XrRect2Di& rect, const char* text, int pixelHeight, XrColor4f color)
    {
        const std::shared_ptr<const BakedFont> font = BakedFont::GetOrCreate(pixelHeight);
        if (!font)
            return;

        float xadvance = (float)rect.offset.x;
        int yadvance =
            rect.offset.y + (int)(pixelHeight * 0.8f);  // Adjust down because glyphs are relative to the font baseline. This is hacky.

        // Loop through each character and copy over the chracters' glyphs.
        for (; *text; text++) {
            if (*text == '\n') {
                xadvance = (float)rect.offset.x;
                yadvance += pixelHeight;
                continue;
            }

            // Word wrap.
            {
                float remainingWordWidth = 0;
                for (const char* w = text; *w > ' '; w++) {
                    const stbtt_bakedchar& bakedChar = font->GetBakedChar(*text);
                    remainingWordWidth += bakedChar.xadvance;
                }

                // Wrap to new line if there isn't enough room for this word.
                if (xadvance + remainingWordWidth > rect.offset.x + rect.extent.width) {
                    // But only if the word isn't longer than the destination.
                    if (remainingWordWidth <= (rect.extent.width - rect.offset.x)) {
                        xadvance = (float)rect.offset.x;
                        yadvance += pixelHeight;
                    }
                }
            }

            const stbtt_bakedchar& bakedChar = font->GetBakedChar(*text);
            const int characterWidth = (int)(bakedChar.x1 - bakedChar.x0);
            const int characterHeight = (int)(bakedChar.y1 - bakedChar.y0);

            if (xadvance + characterWidth > rect.offset.x + rect.extent.width) {
                // Wrap to new line if there isn't enough room for this char.
                xadvance = (float)rect.offset.x;
                yadvance += pixelHeight;
            }

            // For each row of the glyph bitmap
            for (int cy = 0; cy < characterHeight; cy++) {
                // Compute the destination row in the image.
                const int destY = yadvance + cy + (int)bakedChar.yoff;
                if (destY < 0 || destY >= height || destY < rect.offset.y || destY >= rect.offset.y + rect.extent.height) {
                    continue;  // Don't bother copying if out of bounds.
                }

                // Get a pointer to the src and dest row.
                const uint8_t* const srcGlyphRow = font->GetBakedCharRow(bakedChar, cy);
                RGBA8Color* const destImageRow = pixels.data() + (destY * width);

                for (int cx = 0; cx < characterWidth; cx++) {
                    const int destX = (int)(bakedChar.xoff + xadvance + 0.5f) + cx;
                    if (destX < 0 || destX >= width || destX < rect.offset.x || destX >= rect.offset.x + rect.extent.width) {
                        continue;  // Don't bother copying if out of bounds.
                    }

                    // Glyphs are 0-255 intensity.
                    const uint8_t srcGlyphPixel = srcGlyphRow[cx + bakedChar.x0];

                    // Do blending (assuming premultiplication).
                    RGBA8Color pixel = destImageRow[destX];
                    pixel.Channels.R = (uint8_t)(srcGlyphPixel * color.r) + (pixel.Channels.R * (255 - srcGlyphPixel) / 255);
                    pixel.Channels.G = (uint8_t)(srcGlyphPixel * color.g) + (pixel.Channels.G * (255 - srcGlyphPixel) / 255);
                    pixel.Channels.B = (uint8_t)(srcGlyphPixel * color.b) + (pixel.Channels.B * (255 - srcGlyphPixel) / 255);
                    pixel.Channels.A = (uint8_t)(srcGlyphPixel * color.a) + (pixel.Channels.A * (255 - srcGlyphPixel) / 255);
                    destImageRow[destX] = pixel;
                }
            }

            xadvance += bakedChar.xadvance;
        }
    }

    void RGBAImage::DrawRect(int x, int y, int w, int h, XrColor4f color)
    {
        if (x + w > width || y + h > height) {
            throw std::out_of_range("Rectangle out of bounds");
        }

        const RGBA8Color color32 = AsRGBA(color.r, color.g, color.b, color.a);
        for (int row = 0; row < h; row++) {
            RGBA8Color* start = pixels.data() + ((row + y) * width) + x;
            for (int col = 0; col < w; col++) {
                *(start + col) = color32;
            }
        }
    }

    void RGBAImage::DrawRectBorder(int x, int y, int w, int h, int thickness, XrColor4f color)
    {
        if (x < 0 || y < 0 || w < 0 || h < 0 || x + w > width || y + h > height) {
            throw std::out_of_range("Rectangle out of bounds");
        }

        const RGBA8Color color32 = AsRGBA(color.r, color.g, color.b, color.a);
        for (int row = 0; row < h; row++) {
            RGBA8Color* start = pixels.data() + ((row + y) * width) + x;
            if (row < thickness || row >= h - thickness) {
                for (int col = 0; col < w; col++) {
                    *(start + col) = color32;
                }
            }
            else {
                int leftBorderEnd = std::min(thickness, w);
                for (int col = 0; col < leftBorderEnd; col++) {
                    *(start + col) = color32;
                }

                int rightBorderBegin = std::max(w - thickness, 0);
                for (int col = rightBorderBegin; col < w; col++) {
                    *(start + col) = color32;
                }
            }
        }
    }

    inline double ToSRGB(double linear)
    {
        if (linear < 0.04045 / 12.92)
            return linear * 12.92;
        else
            return 1.055 * std::pow(linear, (1.0 / 2.4)) - 0.055;
    }
    inline double FromSRGB(double srgb)
    {
        if (srgb < 0.04045)
            return srgb / 12.92;
        return std::pow((srgb + .055) / 1.055, 2.4);
    }

    void RGBAImage::ConvertToSRGB()
    {
        for (RGBA8Color& pixel : pixels) {
            pixel.Channels.R = (uint8_t)(ToSRGB((double)pixel.Channels.R / 255.0) * 255.0);
            pixel.Channels.G = (uint8_t)(ToSRGB((double)pixel.Channels.G / 255.0) * 255.0);
            pixel.Channels.B = (uint8_t)(ToSRGB((double)pixel.Channels.B / 255.0) * 255.0);
        }
    }

}  // namespace Conformance
