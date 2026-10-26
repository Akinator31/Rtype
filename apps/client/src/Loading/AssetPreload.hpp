#pragma once

#include "rtype/render/ResourceManager.hpp"

#include <cstddef>
#include <future>
#include <vector>

namespace rtype::client
{

/// Loads the pictures and the models of the client in the background (ADR 0017), while the loading screen
/// shows: the scenes are built once they are all loaded, and find them in the cache of the ResourceManager.
/// The fonts and the logo are not in it: the loading screen needs them to show.
class AssetPreload
{
    render::ResourceManager &_resources;
    std::vector<std::shared_future<render::TextureId>> _textures;
    std::vector<std::shared_future<render::ModelId>> _models;
    std::size_t _count = 0;
    std::size_t _loaded = 0;

public:
    /// Starts loading every asset.
    /// @param resources It must outlive the preload.
    explicit AssetPreload(render::ResourceManager &resources);

    /// Finishes the loads ready in the background (a texture goes to the graphics card on the main thread),
    /// and counts the assets loaded. Call it every frame until isDone().
    /// @throws std::runtime_error if an asset could not be loaded.
    void update();

    /// The part of the assets loaded, from 0 to 1.
    [[nodiscard]] float getProgress() const;
    [[nodiscard]] bool isDone() const;
};

} // namespace rtype::client
