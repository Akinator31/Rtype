#include "Loading/SceneAssetLoader.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <future>
#include <string>
#include <utility>

namespace rtype::client
{

namespace
{

/// Follows an asynchronous load: the asset is ready once its future is, and the future holds its error.
template <typename Id, typename Unload>
engine::SceneAssetRequest createRequest(std::shared_future<Id> future, Unload unload)
{
    return {
        [future] { return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; },
        [future] { future.get(); },
        std::move(unload),
    };
}

engine::SceneAssetRequest requestTexture(render::ResourceManager &resources, audio::AudioPlayer & /*audio*/,
                                         const std::string &path)
{
    return createRequest(resources.loadTextureAsync(path),
                         [&resources, path] { resources.unloadTexture(path); });
}

engine::SceneAssetRequest requestSmoothTexture(render::ResourceManager &resources,
                                               audio::AudioPlayer & /*audio*/, const std::string &path)
{
    return createRequest(
        resources.loadTextureAsync(path, render::TextureFiltering::Smooth),
        [&resources, path] { resources.unloadTexture(path, render::TextureFiltering::Smooth); });
}

engine::SceneAssetRequest requestFont(render::ResourceManager &resources, audio::AudioPlayer & /*audio*/,
                                      const std::string &path)
{
    return createRequest(resources.loadFontAsync(path), [&resources, path] { resources.unloadFont(path); });
}

engine::SceneAssetRequest requestModel(render::ResourceManager &resources, audio::AudioPlayer & /*audio*/,
                                       const std::string &path)
{
    return createRequest(resources.loadModelAsync(path), [&resources, path] { resources.unloadModel(path); });
}

engine::SceneAssetRequest requestSound(render::ResourceManager & /*resources*/, audio::AudioPlayer &audio,
                                       const std::string &path)
{
    return createRequest(audio.loadSoundAsync(path), [&audio, path] { audio.unloadSound(path); });
}

engine::SceneAssetRequest requestMusic(render::ResourceManager & /*resources*/, audio::AudioPlayer &audio,
                                       const std::string &path)
{
    return createRequest(audio.loadMusicAsync(path), [&audio, path] { audio.unloadMusic(path); });
}

using AssetRequester = engine::SceneAssetRequest (*)(render::ResourceManager &, audio::AudioPlayer &,
                                                     const std::string &);

/// In the order of the values of engine::AssetType.
constexpr std::array<AssetRequester, 6> AssetRequesters{requestTexture, requestSmoothTexture, requestFont,
                                                        requestModel,   requestSound,         requestMusic};

} // namespace

SceneAssetLoader::SceneAssetLoader(render::ResourceManager &resources, audio::AudioPlayer &audio)
    : _resources(resources), _audio(audio)
{
}

engine::SceneAssetRequest SceneAssetLoader::requestAsset(const engine::SceneAsset &asset)
{
    return AssetRequesters.at(static_cast<std::size_t>(asset.type))(_resources, _audio, asset.path);
}

} // namespace rtype::client
