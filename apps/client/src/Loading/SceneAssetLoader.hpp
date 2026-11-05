#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Scene/SceneAssetLoader.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Loads the assets of the scenes for the engine (see Engine::setSceneAssetLoader()): the images, fonts and
/// models with the ResourceManager, the sounds and musics with the AudioPlayer, read on the assets worker.
/// Their loads end in ResourceManager::finishPendingLoads() and AudioPlayer::finishPendingLoads().
class SceneAssetLoader : public engine::SceneAssetLoader
{
    render::ResourceManager &_resources;
    audio::AudioPlayer &_audio;

public:
    /// @param resources, audio They must outlive the assets loaded.
    SceneAssetLoader(render::ResourceManager &resources, audio::AudioPlayer &audio);

    engine::SceneAssetRequest requestAsset(const engine::SceneAsset &asset) override;
};

} // namespace rtype::client
