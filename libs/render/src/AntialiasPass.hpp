#pragma once

// Private to the render library: the smoothing of the jagged edges of the 3D models in the frame.

#include <raylib.h>

namespace rtype::render::detail
{

/// Owns what smooths the edges of the 3D models (Renderer::drawGlow()): the FXAA shader
/// (libs/shaders/glsl/Antialiasing.frag), and an image the size of the frame, where the frame is drawn
/// through it before being copied back. If the shader cannot be compiled, a warning is logged and the edges
/// stay jagged.
class AntialiasPass
{
private:
    Shader _shader{};
    int _texelLocation = -1;
    bool _available = false;
    RenderTexture2D _image{};

    /// Makes the image the size of the frame, recreating it when the frame changed size. Returns whether it
    /// is usable.
    [[nodiscard]] bool fit(int width, int height);

public:
    AntialiasPass();
    ~AntialiasPass();

    AntialiasPass(const AntialiasPass &) = delete;
    AntialiasPass &operator=(const AntialiasPass &) = delete;
    AntialiasPass(AntialiasPass &&) = delete;
    AntialiasPass &operator=(AntialiasPass &&) = delete;

    /// Smooths the edges of what the frame holds. Must be called outside of any texture mode. Does nothing if
    /// the shader or the image is not available.
    void apply(const RenderTexture2D &frame);
};

} // namespace rtype::render::detail
