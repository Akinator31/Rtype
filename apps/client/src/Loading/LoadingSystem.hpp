#pragma once

#include "Loading/AssetPreload.hpp"
#include "Loading/LoadingScreen.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::client
{

/// Plays the LoadingScreen while its AssetPreload loads: fades the logo and the bar in, while the assets are
/// read in the background, then finishes the loads and fills the bar as they end, at most at a steady speed, waits until it is full and the logo fully
/// shown, holds it full for a moment, then fades the bar out and is finished. The logo stays, for the scene
/// that comes next.
class LoadingSystem : public ecs::System
{
    AssetPreload &_preload;
    ecs::ComponentCache<LoadingScreen> &_screens;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    bool _finished = false;

    void fill(LoadingScreen &screen, float step);
    void play(ecs::entity_t bar, LoadingScreen &screen, float elapsed);

public:
    /// @param preload It must outlive the system.
    LoadingSystem(ecs::ComponentCaches &caches, AssetPreload &preload);

    void update(float elapsed) override;

    /// Whether the assets are loaded and the bar has faded out.
    [[nodiscard]] bool isFinished() const;
};

} // namespace rtype::client
