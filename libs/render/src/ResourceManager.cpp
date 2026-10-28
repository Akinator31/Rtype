#include "rtype/render/ResourceManager.hpp"

#include "Resources.hpp"
#include "rtype/core/ReadyFuture.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace rtype::render
{

namespace
{

/// A texture is cached once per filtering, a font once per size.
std::string getTextureKey(const std::string &path, TextureFiltering filtering)
{
    return filtering == TextureFiltering::Smooth ? path + "@smooth" : path;
}

std::string getFontKey(const std::string &path, int size)
{
    return path + '@' + std::to_string(size);
}

} // namespace

// The window is only required to make sure that one is open: graphics resources cannot exist without it
ResourceManager::ResourceManager(const Window & /*window*/, std::filesystem::path assetsDirectory,
                                 std::shared_ptr<core::BackgroundLoader> loader)
    : _impl(std::make_unique<Impl>(std::move(assetsDirectory), std::move(loader)))
{
}

ResourceManager::~ResourceManager() = default;
ResourceManager::ResourceManager(ResourceManager &&) noexcept = default;
ResourceManager &ResourceManager::operator=(ResourceManager &&) noexcept = default;

TextureId ResourceManager::loadTexture(const std::string &path, TextureFiltering filtering)
{
    return _impl->textures.load(getTextureKey(path, filtering),
                                [&] { return detail::Texture(_impl->assetsDirectory / path, filtering); });
}

FontId ResourceManager::loadFont(const std::string &path, int size)
{
    return _impl->fonts.load(getFontKey(path, size),
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
    const std::string key = getTextureKey(path, filtering);
    if (const auto loaded = _impl->textures.findId(key)) {
        return core::makeReadyFuture(*loaded);
    }
    Impl *impl = _impl.get();
    return impl->pendingTextures.request(
        key, [file] { return detail::PreparedTexture(file); },
        [impl, key, filtering](detail::PreparedTexture prepared) {
            return impl->textures.load(key, [&] { return detail::Texture(prepared.image(), filtering); });
        });
}

std::shared_future<FontId> ResourceManager::loadFontAsync(const std::string &path, int size)
{
    const std::filesystem::path file = _impl->assetsDirectory / path;
    const std::string key = getFontKey(path, size);
    if (const auto loaded = _impl->fonts.findId(key)) {
        return core::makeReadyFuture(*loaded);
    }
    Impl *impl = _impl.get();
    return impl->pendingFonts.request(
        key, [file, size] { return detail::PreparedFont(file, size); },
        [impl, key](detail::PreparedFont prepared) {
            return impl->fonts.load(key, [&] { return detail::Font(std::move(prepared)); });
        });
}

std::shared_future<ModelId> ResourceManager::loadModelAsync(const std::string &path)
{
    const std::filesystem::path file = _impl->assetsDirectory / path;
    if (const auto loaded = _impl->models.findId(path)) {
        return core::makeReadyFuture(*loaded);
    }
    Impl *impl = _impl.get();
    return impl->pendingModels.request(
        path, [file] { return core::readAssetFile(file).size(); },
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

bool ResourceManager::unloadTexture(const std::string &path, TextureFiltering filtering)
{
    const std::string key = getTextureKey(path, filtering);
    _impl->pendingTextures.forgetRequest(key);
    return _impl->textures.unload(key);
}

bool ResourceManager::unloadFont(const std::string &path, int size)
{
    const std::string key = getFontKey(path, size);
    _impl->pendingFonts.forgetRequest(key);
    return _impl->fonts.unload(key);
}

bool ResourceManager::unloadModel(const std::string &path)
{
    _impl->pendingModels.forgetRequest(path);
    return _impl->models.unload(path);
}

int ResourceManager::getModelAnimation(ModelId model, const std::string &name) const
{
    const int animation = _impl->models.get(model).findAnimation(name);
    if (animation < 0) {
        throw std::out_of_range("No animation named " + name + " in this model");
    }
    return animation;
}

float ResourceManager::getModelAnimationDuration(ModelId model, int animation) const
{
    return _impl->models.get(model).getAnimationDuration(animation);
}

} // namespace rtype::render
