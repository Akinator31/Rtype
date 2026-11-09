#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <array>

namespace orbit
{

/// A picture to draw whole: a texture and its size.
struct Picture {
    rtype::render::TextureId texture;
    rtype::render::Rectangle source;
};

/// What the effects and the sky are drawn with, loaded once by the program.
struct Looks {
    rtype::render::FontId font;
    /// A soft light, behind the ball and the paddle.
    Picture glow;
    /// A spark, thrown by a broken brick.
    Picture spark;
    /// A ring, growing where a brick broke.
    Picture shockwave;
    /// The layers of the sky, from the farthest.
    std::array<rtype::render::TextureId, 4> sky;
    /// A planet passing slowly in the sky.
    Picture planet;
};

} // namespace orbit
