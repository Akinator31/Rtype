#pragma once

#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>

namespace rtype::client
{

/// How an entity of the game is drawn without a model: a coloured box of its size, the ships in the colour of
/// their player. The size is that of its collider in the game.
struct EntityLook {
    engine::Box box;
    int order = 0;
};

/// The look of an entity of the game.
[[nodiscard]] EntityLook lookOf(const game::EntityState &entity);

/// The colour of a player: blue, red, green, yellow for the slots 0 to 3.
[[nodiscard]] render::Color playerColor(std::uint8_t slot);

} // namespace rtype::client
