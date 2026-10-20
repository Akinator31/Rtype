#include "rtype/render/ResourceManager.hpp"

#include "Resources.hpp"

#include <stdexcept>
#include <utility>

namespace rtype::render
{

// The window is only required to make sure that one is open: graphics resources cannot exist without it
ResourceManager::ResourceManager(const Window & /*window*/, std::filesystem::path assetsDirectory)
    : _impl(std::make_unique<Impl>(std::move(assetsDirectory)))
{
}

ResourceManager::~ResourceManager() = default;
ResourceManager::ResourceManager(ResourceManager &&) noexcept = default;
ResourceManager &ResourceManager::operator=(ResourceManager &&) noexcept = default;

TextureId ResourceManager::loadTexture(const std::string &path, TextureFiltering filtering)
{
    const std::string key = filtering == TextureFiltering::Smooth ? path + "@smooth" : path;
    return _impl->textures.load(key,
                                [&] { return detail::Texture(_impl->assetsDirectory / path, filtering); });
}

FontId ResourceManager::loadFont(const std::string &path, int size)
{
    return _impl->fonts.load(path + '@' + std::to_string(size),
                             [&] { return detail::Font(_impl->assetsDirectory / path, size); });
}

ModelId ResourceManager::loadModel(const std::string &path)
{
    return _impl->models.load(path, [&] { return detail::Model(_impl->assetsDirectory / path); });
}

std::shared_future<TextureId> ResourceManager::loadTextureAsync(const std::string &path,
                                                                 TextureFiltering filtering)
{
    const std::filesystem::path file = _impl->assetsDirectory / path;
    const std::string key = filtering == TextureFiltering::Smooth ? path + "@smooth" : path;
    Impl *impl = _impl.get();
    return impl->pendingTextures.request(key, [file] { return detail::PreparedTexture(file); },
                                         [impl, key, filtering](detail::PreparedTexture prepared) {
        return impl->textures.load(key, [&] { return detail::Texture(prepared.image(), filtering); });
    });
}

std::shared_future<FontId> ResourceManager::loadFontAsync(const std::string &path, int size)
{
    const std::filesystem::path file = _impl->assetsDirectory / path;
    const std::string key = path + '@' + std::to_string(size);
    Impl *impl = _impl.get();
    return impl->pendingFonts.request(key, [file, size] { return detail::PreparedFont(file, size); },
                                      [impl, key](detail::PreparedFont prepared) {
        return impl->fonts.load(key, [&] { return detail::Font(std::move(prepared)); });
    });
}

std::shared_future<ModelId> ResourceManager::loadModelAsync(const std::string &path)
{
    const std::filesystem::path file = _impl->assetsDirectory / path;
    Impl *impl = _impl.get();
    return impl->pendingModels.request(path, [file] { return core::readAssetFile(file).size(); },
                                       [impl, file, path](std::size_t) {
        return impl->models.load(path, [&] { return detail::Model(file); });
    });
}

void ResourceManager::finishPendingLoads()
{
    _impl->pendingTextures.pump();
    _impl->pendingFonts.pump();
    _impl->pendingModels.pump();
}

int ResourceManager::getModelAnimation(ModelId model, const std::string &name) const
{
    const int animation = _impl->models.get(model).findAnimation(name);
    if (animation < 0) {
        throw std::out_of_range("No animation named " + name + " in this model");
    }
    return animation;
}

} // namespace rtype::render
