#pragma once

// Private to the render library: the raylib resources behind the handles of the ResourceManager, shared
// between the ResourceManager, which loads them, and the Renderer, which draws them.

#include "rtype/render/ResourceCache.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <raylib.h>

#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rtype::render
{

namespace detail
{

// Owns a texture in graphics memory. A moved-from texture is empty and frees nothing.
class Texture
{
private:
    Texture2D _texture;

public:
    explicit Texture(const std::filesystem::path &file) : _texture(LoadTexture(file.string().c_str()))
    {
        if (!IsTextureValid(_texture)) {
            throw std::runtime_error("Cannot load texture " + file.string());
        }
    }
    ~Texture()
    {
        if (IsTextureValid(_texture)) {
            UnloadTexture(_texture);
        }
    }

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;
    Texture(Texture &&other) noexcept : _texture(std::exchange(other._texture, {})) {}
    Texture &operator=(Texture &&) = delete;

    [[nodiscard]] const Texture2D &get() const { return _texture; }
};

// Characters rendered when a font is loaded: by default, raylib only renders ASCII, and any other character
// is drawn as '?'. Latin-1 adds the accented letters of most Western European languages, completed with the
// French "œ" and the euro sign.
inline std::vector<int> fontCharacters()
{
    std::vector<int> characters;
    for (int character = 0x20; character <= 0x7E; ++character) {
        characters.push_back(character);
    }
    for (int character = 0xA0; character <= 0xFF; ++character) {
        characters.push_back(character);
    }
    characters.insert(characters.end(), {0x152, 0x153, 0x20AC});
    return characters;
}

// Owns a font: its glyphs in memory and its texture in graphics memory. A moved-from font frees nothing.
class Font
{
private:
    ::Font _font;

    static ::Font load(const std::filesystem::path &file, int size)
    {
        std::vector<int> characters = fontCharacters();
        return LoadFontEx(file.string().c_str(), size, characters.data(),
                          static_cast<int>(characters.size()));
    }

public:
    Font(const std::filesystem::path &file, int size) : _font(load(file, size))
    {
        if (!IsFontValid(_font)) {
            throw std::runtime_error("Cannot load font " + file.string());
        }
        // Fonts are often drawn at another size than the one they were loaded at: smooth the scaling
        SetTextureFilter(_font.texture, TEXTURE_FILTER_BILINEAR);
    }
    ~Font()
    {
        if (IsFontValid(_font)) {
            UnloadFont(_font);
        }
    }

    Font(const Font &) = delete;
    Font &operator=(const Font &) = delete;
    Font(Font &&other) noexcept : _font(std::exchange(other._font, {})) {}
    Font &operator=(Font &&) = delete;

    [[nodiscard]] const ::Font &get() const { return _font; }
};

} // namespace detail

struct ResourceManager::Impl {
    std::filesystem::path assetsDirectory;
    ResourceCache<detail::Texture, TextureId> textures;
    ResourceCache<detail::Font, FontId> fonts;
};

} // namespace rtype::render
