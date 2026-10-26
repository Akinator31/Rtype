#include "SceneAssetLoader.hpp"

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <chrono>
#include <future>
#include <stdexcept>
#include <utility>

namespace rtype::client
{

namespace
{

template <typename Id, typename Release>
engine::SceneAssetRequest ticket(std::shared_future<Id> future, Release release)
{
    return {
        [future] { return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; },
        [future] { future.get(); },
        std::move(release),
    };
}

} // namespace

SceneAssetLoader::SceneAssetLoader(render::ResourceManager &resources, audio::AudioPlayer &audio)
    : _resources(resources), _audio(audio)
{
}

engine::SceneAssetRequest SceneAssetLoader::request(const engine::SceneAsset &asset)
{
    switch (asset.type) {
    case engine::AssetType::Texture:
        return ticket(_resources.loadTextureAsync(asset.path),
                      [this, path = asset.path] { _resources.releaseTexture(path); });
    case engine::AssetType::SmoothTexture:
        return ticket(
            _resources.loadTextureAsync(asset.path, render::TextureFiltering::Smooth),
            [this, path = asset.path] { _resources.releaseTexture(path, render::TextureFiltering::Smooth); });
    case engine::AssetType::Font:
        return ticket(_resources.loadFontAsync(asset.path),
                      [this, path = asset.path] { _resources.releaseFont(path); });
    case engine::AssetType::Model:
        return ticket(_resources.loadModelAsync(asset.path),
                      [this, path = asset.path] { _resources.releaseModel(path); });
    case engine::AssetType::Sound:
        return ticket(_audio.loadSoundAsync(asset.path),
                      [this, path = asset.path] { _audio.releaseSound(path); });
    case engine::AssetType::Music:
        return ticket(_audio.loadMusicAsync(asset.path),
                      [this, path = asset.path] { _audio.releaseMusic(path); });
    }
    throw std::invalid_argument("Unknown scene asset type");
}

} // namespace rtype::client
