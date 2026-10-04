#include "rtype/render/ResourceManager.hpp"

#include "Resources.hpp"

#include <utility>

namespace rtype::render
{

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
    return _impl->textures.load(path, [&] { return detail::Texture(_impl->assetsDirectory / path); });
}

FontId ResourceManager::loadFont(const std::string &path, int size)
{
    // The same file can be loaded at several sizes, each one is a different font
    return _impl->fonts.load(path + '@' + std::to_string(size),
                             [&] { return detail::Font(_impl->assetsDirectory / path, size); });
}

} // namespace rtype::render
