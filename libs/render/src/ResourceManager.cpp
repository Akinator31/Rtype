#include "rtype/render/ResourceManager.hpp"

#include "rtype/render/ResourceCache.hpp"

#include <raylib.h>

#include <stdexcept>
#include <utility>

namespace rtype::render
{

namespace
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
};

// Owns a font: its glyphs in memory and its texture in graphics memory. A moved-from font frees nothing.
class Font
{
private:
    ::Font _font;

public:
    Font(const std::filesystem::path &file, int size)
        : _font(LoadFontEx(file.string().c_str(), size, nullptr, 0))
    {
        if (!IsFontValid(_font)) {
            throw std::runtime_error("Cannot load font " + file.string());
        }
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
};

} // namespace

struct ResourceManager::Impl {
    std::filesystem::path assetsDirectory;
    ResourceCache<Texture, TextureId> textures;
    ResourceCache<Font, FontId> fonts;
};

// The window is only required to make sure that one is open: graphics resources cannot exist without it
ResourceManager::ResourceManager(const Window & /*window*/, std::filesystem::path assetsDirectory)
    : _impl(std::make_unique<Impl>(Impl{std::move(assetsDirectory), {}, {}}))
{
}

ResourceManager::~ResourceManager() = default;
ResourceManager::ResourceManager(ResourceManager &&) noexcept = default;
ResourceManager &ResourceManager::operator=(ResourceManager &&) noexcept = default;

TextureId ResourceManager::loadTexture(const std::string &path)
{
    return _impl->textures.load(path, [&] { return Texture(_impl->assetsDirectory / path); });
}

FontId ResourceManager::loadFont(const std::string &path, int size)
{
    // The same file can be loaded at several sizes, each one is a different font
    return _impl->fonts.load(path + '@' + std::to_string(size),
                             [&] { return Font(_impl->assetsDirectory / path, size); });
}

} // namespace rtype::render
