#include "Loading/AssetPreload.hpp"

#include "Assets.hpp"

#include <array>
#include <chrono>

namespace rtype::client
{

namespace
{

constexpr std::array texturePaths{
    RTYPE_IMAGE_STARFIELD,
    RTYPE_IMAGE_LOGO_FLARE,
    RTYPE_IMAGE_ENERGY_NOISE,
    RTYPE_IMAGE_GLOW_CORE,
};

constexpr std::array modelPaths{
    RTYPE_MODEL_PLANET,
    RTYPE_MODEL_PLANET_DUST,
    RTYPE_MODEL_RING_ASTEROIDS_INNER,
    RTYPE_MODEL_RING_ASTEROIDS_OUTER,
    RTYPE_MODEL_RING_DUST,
    RTYPE_MODEL_STATION,
    RTYPE_MODEL_SHIP_EZNO,
    RTYPE_MODEL_SHIP_SPACESHIP,
    RTYPE_MODEL_SHIP_BATTLESTAR,
    RTYPE_MODEL_SHIP_DROP_SHIP,
    RTYPE_MODEL_SHOT_BASIC,
    RTYPE_MODEL_SHOT_SPIRAL,
    RTYPE_MODEL_ORB_PLASMA,
    RTYPE_MODEL_IMPACT_SPARK,
    RTYPE_MODEL_IMPACT_BURST,
    RTYPE_MODEL_IMPACT_SPLASH,
    RTYPE_MODEL_IMPACT_EXPLOSION,
};

/// Removes the loads that are over from `pending`, and returns how many there were.
/// @throws std::runtime_error if one of them failed.
template <typename Id>
std::size_t takeFinishedLoads(std::vector<std::shared_future<Id>> &pending)
{
    return std::erase_if(pending, [](const std::shared_future<Id> &load) {
        if (load.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }
        static_cast<void>(load.get());
        return true;
    });
}

} // namespace

AssetPreload::AssetPreload(render::ResourceManager &resources) : _resources(resources)
{
    for (const char *path : texturePaths) {
        _textures.push_back(_resources.loadTextureAsync(path, render::TextureFiltering::Smooth));
    }
    for (const char *path : modelPaths) {
        _models.push_back(_resources.loadModelAsync(path));
    }
    _count = _textures.size() + _models.size();
}

void AssetPreload::update()
{
    if (isDone()) {
        return;
    }
    _resources.finishPendingLoads();
    _loaded += takeFinishedLoads(_textures);
    _loaded += takeFinishedLoads(_models);
}

float AssetPreload::getProgress() const
{
    return static_cast<float>(_loaded) / static_cast<float>(_count);
}

bool AssetPreload::isDone() const
{
    return _loaded == _count;
}

} // namespace rtype::client
