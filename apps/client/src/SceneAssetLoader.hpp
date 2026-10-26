#pragma once

#include "rtype/engine/Scene/SceneAsset.hpp"

namespace rtype::audio
{
class AudioPlayer;
}

namespace rtype::render
{
class ResourceManager;
}

namespace rtype::client
{

class SceneAssetLoader : public engine::SceneAssetLoader
{
    render::ResourceManager &_resources;
    audio::AudioPlayer &_audio;

public:
    SceneAssetLoader(render::ResourceManager &resources, audio::AudioPlayer &audio);
    engine::SceneAssetRequest request(const engine::SceneAsset &asset) override;
};

}
