#pragma once

#include "rtype/core/AssetFile.hpp"

#include <raylib.h>

#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rtype::render::detail
{

class PreparedTexture
{
    Image _image{};

public:
    PreparedTexture(const std::filesystem::path &file)
    {
        const std::vector<unsigned char> bytes = core::readAssetFile(file);
        _image = LoadImageFromMemory(file.extension().string().c_str(), bytes.data(),
                                     static_cast<int>(bytes.size()));
        if (!IsImageValid(_image)) {
            if (_image.data != nullptr) {
                UnloadImage(_image);
            }
            throw std::runtime_error("Cannot load texture " + file.string());
        }
    }

    ~PreparedTexture() { UnloadImage(_image); }
    PreparedTexture(const PreparedTexture &) = delete;
    PreparedTexture &operator=(const PreparedTexture &) = delete;
    PreparedTexture(PreparedTexture &&other) noexcept : _image(std::exchange(other._image, {})) {}
    PreparedTexture &operator=(PreparedTexture &&other) noexcept
    {
        if (this != &other) {
            UnloadImage(_image);
            _image = std::exchange(other._image, {});
        }
        return *this;
    }

    [[nodiscard]] Image image() const { return _image; }
};

inline std::vector<int> fontCharacters()
{
    constexpr int softHyphen = 0xAD;
    std::vector<int> characters;
    for (int character = 0x20; character <= 0x7E; ++character) {
        characters.push_back(character);
    }
    for (int character = 0xA0; character <= 0xFF; ++character) {
        if (character != softHyphen) {
            characters.push_back(character);
        }
    }
    characters.insert(characters.end(), {0x152, 0x153, 0x20AC, 0x2190});
    return characters;
}

class PreparedFont
{
    ::Font _font{};
    Image _atlas{};

    void reset()
    {
        UnloadFontData(_font.glyphs, _font.glyphCount);
        if (_font.recs != nullptr) {
            MemFree(_font.recs);
        }
        UnloadImage(_atlas);
    }

public:
    PreparedFont(const std::filesystem::path &file, int size)
    {
        try {
            const std::vector<unsigned char> bytes = core::readAssetFile(file);
            const std::vector<int> characters = fontCharacters();
            _font.baseSize = size;
            _font.glyphPadding = 4;
            _font.glyphs = LoadFontData(bytes.data(), static_cast<int>(bytes.size()), size,
                                       characters.data(), static_cast<int>(characters.size()),
                                       FONT_DEFAULT, &_font.glyphCount);
            if (_font.glyphs == nullptr || _font.glyphCount == 0) {
                throw std::runtime_error("Cannot load font " + file.string());
            }
            _atlas = GenImageFontAtlas(_font.glyphs, &_font.recs, _font.glyphCount, size,
                                       _font.glyphPadding, 0);
            if (!IsImageValid(_atlas) || _font.recs == nullptr) {
                throw std::runtime_error("Cannot build font atlas " + file.string());
            }
            for (int index = 0; index < _font.glyphCount; ++index) {
                UnloadImage(_font.glyphs[index].image);
                _font.glyphs[index].image = ImageFromImage(_atlas, _font.recs[index]);
            }
        } catch (...) {
            reset();
            throw;
        }
    }

    ~PreparedFont() { reset(); }

    PreparedFont(const PreparedFont &) = delete;
    PreparedFont &operator=(const PreparedFont &) = delete;
    PreparedFont(PreparedFont &&other) noexcept
        : _font(std::exchange(other._font, {})), _atlas(std::exchange(other._atlas, {})) {}
    PreparedFont &operator=(PreparedFont &&other) noexcept
    {
        if (this != &other) {
            reset();
            _font = std::exchange(other._font, {});
            _atlas = std::exchange(other._atlas, {});
        }
        return *this;
    }

    [[nodiscard]] ::Font upload()
    {
        ::Texture2D texture = LoadTextureFromImage(_atlas);
        if (!IsTextureValid(texture)) {
            throw std::runtime_error("Cannot upload font texture");
        }
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        _font.texture = texture;
        return std::exchange(_font, {});
    }
};

}
