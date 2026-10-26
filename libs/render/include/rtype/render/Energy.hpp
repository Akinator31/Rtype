#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <optional>

namespace rtype::render
{

/// How a 3D model of energy is drawn (shots, beams, plasma orbs): it glows over what is behind it, white hot
/// where it faces the camera, its colour flickering with a noise that flows over it. Its rings and spirals
/// turn and its shells pulse by themselves, as stored in the model (see libs/shaders/glsl/Energy.vert). See
/// Renderer::drawEnergyModel().
struct Energy {
    /// A soft light around the model, always facing the camera, added to what is behind it: it makes the
    /// model stand out and lights up what it passes over.
    struct Halo {
        /// The picture of the light, white on transparent: images/fx/glow_core.png.
        TextureId texture{};
        /// Colour of the light; its alpha sets how bright it is.
        Color color = colors::White;
        /// Its centre, in model units: the centre of the core of a shot.
        Vector3 centre{};
        /// Its width and height on screen, in model units: larger than the core, so that the light spreads
        /// around it.
        Vector2 size{1.0f, 1.0f};
    };

    /// The noise that makes it flicker: images/fx/energy_noise.png, repeated over the model.
    TextureId noise;
    /// Extra brightness, 0 by default: more for a charged shot, the firing phase of a beam or a hit flash.
    float boost = 0.0f;
    /// From 0 (whole, by default) to 1 (gone): the noise eats the model with a hot edge, to make it appear or
    /// vanish.
    float dissolve = 0.0f;
    /// The light around the model, none by default.
    std::optional<Halo> halo{};
    /// How much of the model shines in the glow of the frame (Renderer::drawGlow()), from 0 (none) to 1 (all,
    /// by default).
    float bloom = 1.0f;
};

} // namespace rtype::render
