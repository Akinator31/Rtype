#pragma once

#include <string>

namespace rtype::game
{

/// The sounds of a kind of ship, as paths (empty: no sound). The game only carries them: it has no audio, the
/// client plays them.
struct ShipSounds {
    /// While it moves.
    std::string move;
    /// When it is destroyed.
    std::string destruction;
    /// When it collides with something.
    std::string collision;
};

} // namespace rtype::game
