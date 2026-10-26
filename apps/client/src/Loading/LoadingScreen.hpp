#pragma once

#include "rtype/ecs/Entity/Entity.hpp"

#include <optional>

namespace rtype::client
{

/// The loading screen, on the entity of its bar: the logo fades in over black above the bar, the bar fills
/// while the assets load, stays full for a moment, then fades out and leaves the logo alone (see
/// LoadingSystem).
struct LoadingScreen {
    ecs::entity_t logo;
    /// Time since the screen opened, in seconds.
    float time = 0.0f;
    /// How full the bar is, from 0 to 1: it follows the part of the assets loaded, without going faster than a
    /// steady speed, so that it fills smoothly even when the assets load in jumps.
    float progress = 0.0f;
    /// Time since the bar became full, in seconds, once the assets are loaded: empty until then.
    std::optional<float> leaving = std::nullopt;
};

} // namespace rtype::client
