#pragma once

#include "Breaker.hpp"
#include "Looks.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <functional>

namespace orbit
{

/// The rules of a game, after the ball: once every brick is broken, the next level starts with the ball back
/// on the paddle and its number in a banner; once the last life is lost, the game is over.
class RulesSystem : public rtype::ecs::System
{
    rtype::ecs::Ecs &_ecs;
    const rtype::engine::Scenes &_scenes;
    Match &_match;
    const Looks &_looks;
    std::function<void()> _onGameOver;
    bool _over = false;
    rtype::ecs::ComponentCache<Ball> &_balls;
    rtype::ecs::ComponentCache<Brick> &_bricks;
    rtype::ecs::ComponentCache<rtype::engine::SceneMember> &_members;

    [[nodiscard]] bool hasBricksLeft() const;
    void startNextLevel(rtype::ecs::entity_t ball);

public:
    /// @param ecs, scenes, match, looks They must outlive the system.
    /// @param onGameOver Called once when the last life is lost.
    RulesSystem(rtype::ecs::ComponentCaches &caches, rtype::ecs::Ecs &ecs,
                const rtype::engine::Scenes &scenes, Match &match, const Looks &looks,
                std::function<void()> onGameOver);

    void update(float elapsed) override;
};

} // namespace orbit
