#pragma once

#include "rtype/render/ResourceManager.hpp"

namespace rtype::render
{

/// How a 3D model of energy is drawn (shots, beams, plasma orbs): it glows over what is behind it, white hot
/// where it faces the camera, its colour flickering with a noise that flows over it. Its rings and spirals
/// turn and its shells pulse by themselves, as stored in the model (see libs/shaders/glsl/Energy.vert). See
/// Renderer::drawEnergyModel().
struct Energy {
    /// The noise that makes it flicker: images/fx/energy_noise.png, repeated over the model.
    TextureId noise;
    /// Extra brightness, 0 by default: more for a charged shot, the firing phase of a beam or a hit flash.
    float boost = 0.0f;
    /// From 0 (whole, by default) to 1 (gone): the noise eats the model with a hot edge, to make it appear or
    /// vanish.
    float dissolve = 0.0f;
};

} // namespace rtype::render
