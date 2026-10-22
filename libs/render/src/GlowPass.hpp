#pragma once

// Private to the render library: the glow of the energy models, blurred and added over the frame.

#include <raylib.h>

#include <array>
#include <functional>

namespace rtype::render::detail
{

/// Owns what makes the energy models shine (Renderer::drawGlow()): two images at a quarter of the resolution
/// of the frame, where the models are drawn again then blurred back and forth by a shader, so that their
/// light spreads around them. If the shader cannot be compiled, a warning is logged and nothing glows.
class GlowPass
{
private:
    Shader _shader{};
    int _directionLocation = -1;
    int _gainLocation = -1;
    bool _available = false;
    std::array<RenderTexture2D, 2> _images{};

    /// Makes the images the given size, recreating them when the frame changed size.
    void fit(int width, int height);
    /// Draws `from` blurred along `direction` (one texel along x or y) into `to`, `gain` times brighter.
    void blur(const RenderTexture2D &from, const RenderTexture2D &to, ::Vector2 direction, float gain);

public:
    GlowPass();
    ~GlowPass();

    GlowPass(const GlowPass &) = delete;
    GlowPass &operator=(const GlowPass &) = delete;
    GlowPass(GlowPass &&) = delete;
    GlowPass &operator=(GlowPass &&) = delete;

    /// Draws the energy models with `drawModels` into the glow of a frame of the given resolution, and blurs
    /// it. Must be called outside of any texture mode. Returns the blurred image, to add over the frame, or
    /// nullptr if the shader or the images are not available.
    [[nodiscard]] const Texture2D *render(::Vector2 frameResolution, const std::function<void()> &drawModels);
};

} // namespace rtype::render::detail
