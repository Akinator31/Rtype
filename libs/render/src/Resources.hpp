#pragma once

// Private to the render library: the raylib resources behind the handles of the ResourceManager, shared
// between the ResourceManager, which loads them, and the Renderer, which draws them.

#include "rtype/core/ResourceCache.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <raylib.h>
#include <rlgl.h>

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
// French "œ" and the euro sign. The soft hyphen is left out: it is never drawn, and some fonts (Inter) do not
// have it, which makes raylib warn.
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

// Owns a 3D model: its meshes, materials and textures. A moved-from model frees nothing.
class Model
{
private:
    ::Model _model;

    // Models are mostly seen smaller than their textures: without mipmaps, colours and relief shimmer.
    // Materials without a texture share raylib's default one, which is left alone.
    static void smoothTexture(Texture2D &texture)
    {
        if (IsTextureValid(texture) && texture.id != rlGetTextureIdDefault()) {
            GenTextureMipmaps(&texture);
            SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        }
    }

public:
    explicit Model(const std::filesystem::path &file) : _model(LoadModel(file.string().c_str()))
    {
        if (!IsModelValid(_model)) {
            throw std::runtime_error("Cannot load model " + file.string());
        }
        for (int index = 0; index < _model.materialCount; ++index) {
            smoothTexture(_model.materials[index].maps[MATERIAL_MAP_ALBEDO].texture);
            smoothTexture(_model.materials[index].maps[MATERIAL_MAP_NORMAL].texture);
        }
    }
    ~Model()
    {
        if (IsModelValid(_model)) {
            UnloadModel(_model);
        }
    }

    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;
    Model(Model &&other) noexcept : _model(std::exchange(other._model, {})) {}
    Model &operator=(Model &&) = delete;

    [[nodiscard]] const ::Model &get() const { return _model; }
};

} // namespace detail

struct ResourceManager::Impl {
    std::filesystem::path assetsDirectory;
    core::ResourceCache<detail::Texture, TextureId> textures;
    core::ResourceCache<detail::Font, FontId> fonts;
    core::ResourceCache<detail::Model, ModelId> models;
};

} // namespace rtype::render
