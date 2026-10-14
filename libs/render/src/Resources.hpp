#pragma once

// Private to the render library: the raylib resources behind the handles of the ResourceManager, shared
// between the ResourceManager, which loads them, and the Renderer, which draws them.

#include "rtype/core/ResourceCache.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
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

// Owns a 3D model: its meshes, materials and textures, and the animations of its skeleton if it has one. A
// moved-from model frees nothing.
class Model
{
private:
    // raylib samples the animations of glTF files at this rate
    static constexpr float framesPerSecond = 60.0f;

    ::Model _model;
    ModelAnimation *_animations = nullptr;
    int _animationCount = 0;

    // IsModelValid() rejects every skinned model in raylib 6.0: it requires the bone buffers in graphics
    // memory, which raylib only uploads with GPU skinning, disabled by default (it skins on the CPU instead).
    // A model is usable once its meshes and materials are loaded and its vertices are uploaded.
    static bool isLoaded(const ::Model &model)
    {
        if (model.meshes == nullptr || model.materials == nullptr || model.meshCount <= 0 ||
            model.materialCount <= 0) {
            return false;
        }
        for (int index = 0; index < model.meshCount; ++index) {
            if (model.meshes[index].vboId == nullptr || model.meshes[index].vboId[0] == 0) {
                return false;
            }
        }
        return true;
    }

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
        if (!isLoaded(_model)) {
            UnloadModel(_model);
            throw std::runtime_error("Cannot load model " + file.string());
        }
        for (int index = 0; index < _model.materialCount; ++index) {
            smoothTexture(_model.materials[index].maps[MATERIAL_MAP_ALBEDO].texture);
            smoothTexture(_model.materials[index].maps[MATERIAL_MAP_NORMAL].texture);
        }
        if (_model.skeleton.boneCount > 0) {
            _animations = LoadModelAnimations(file.string().c_str(), &_animationCount);
        }
    }
    ~Model()
    {
        if (_animations != nullptr) {
            UnloadModelAnimations(_animations, _animationCount);
        }
        if (isLoaded(_model)) {
            UnloadModel(_model);
        }
    }

    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;
    Model(Model &&other) noexcept
        : _model(std::exchange(other._model, {})), _animations(std::exchange(other._animations, nullptr)),
          _animationCount(std::exchange(other._animationCount, 0))
    {
    }
    Model &operator=(Model &&) = delete;

    [[nodiscard]] const ::Model &get() const { return _model; }

    [[nodiscard]] bool hasAnimation(int animation) const
    {
        return animation >= 0 && animation < _animationCount;
    }

    // Index of the animation with this name, or -1 if the model has none by this name.
    [[nodiscard]] int findAnimation(const std::string &name) const
    {
        for (int index = 0; index < _animationCount; ++index) {
            if (name == _animations[index].name) {
                return index;
            }
        }
        return -1;
    }

    // Moves the vertices of the model to their place at a time of an animation, in seconds. The animation
    // loops. The model keeps this pose until it is animated again. Const like drawing: only the vertices in
    // graphics memory change, and the renderer, which poses models, only reads the resources.
    void animate(int animation, float time) const
    {
        if (hasAnimation(animation)) {
            UpdateModelAnimation(_model, _animations[animation], time * framesPerSecond);
        }
    }

    // Like animate(), with a pose between two animations: `amount` goes from 0 for the first one to 1 for the
    // second one.
    void blendAnimations(int first, float firstTime, int second, float secondTime, float amount) const
    {
        if (!hasAnimation(first) || !hasAnimation(second)) {
            animate(second, secondTime);
            return;
        }
        UpdateModelAnimationEx(_model, _animations[first], firstTime * framesPerSecond, _animations[second],
                               secondTime * framesPerSecond, std::clamp(amount, 0.0f, 1.0f));
    }
};

} // namespace detail

struct ResourceManager::Impl {
    std::filesystem::path assetsDirectory;
    core::ResourceCache<detail::Texture, TextureId> textures;
    core::ResourceCache<detail::Font, FontId> fonts;
    core::ResourceCache<detail::Model, ModelId> models;
};

} // namespace rtype::render
