#pragma once

#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/render/Types.hpp"

namespace rtype::client
{

/// The star of the logo, a Sprite drawn over it. Out of the screen until the Intro starts, it is then thrown
/// from the left of the screen and falls on `place` along a parabola, lands there with a flash and twinkles
/// from then on. The place is an offset from the centre of the logo at its full size, in game units: the
/// LogoFlareSystem follows the logo while it moves and shrinks.
///
/// A copy with a `lag` is a trail: it takes the same path `lag` seconds later, fainter, and fades out before
/// it lands.
struct LogoFlare {
    ecs::entity_t logo;
    /// The entity with the Intro that starts the flare.
    ecs::entity_t intro;
    render::Vector2 place;
    float lag = 0.0f;
    /// Opacity of a trail, and its size compared with the flare.
    float strength = 1.0f;
};

} // namespace rtype::client
