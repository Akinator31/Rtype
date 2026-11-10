#pragma once

#include "rtype/render/Types.hpp"

namespace rtype::render
{

/// A light of one colour around a 3D model, cast on it by a shader (libs/shaders/glsl/Aura.frag): a shell a
/// little larger than the model, bright along its silhouette and clear where it faces the camera, breathing
/// slowly, added to what is behind it. It tells apart the ship of the player, those of its allies and those
/// of its enemies. See Renderer::drawAura().
struct Aura {
    /// Colour of the light; its alpha sets how bright it is.
    Color color = colors::White;
    /// How far the shell stands out of the model, in world units.
    float thickness = 6.0f;
    /// How much of the aura shines in the glow of the frame (Renderer::drawGlow()), from 0 (none) to 1: the
    /// light that spreads softly around the model.
    float bloom = 0.8f;
};

} // namespace rtype::render
