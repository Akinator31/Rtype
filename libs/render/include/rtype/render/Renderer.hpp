#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <memory>
#include <string>

namespace rtype::render
{

class Window;

/// Draws the game: textures, text and shapes.
///
/// The game is drawn at a fixed logical resolution (1920x1080 by default), whatever the window size:
/// positions and sizes are always in game units. At the end of each frame, the image is scaled to fit the
/// window while keeping its aspect ratio (see letterbox()).
///
/// Everything is drawn between beginFrame() and endFrame(), in order: what is drawn last is on top.
/// @code
/// Window window;
/// ResourceManager resources(window, applicationDirectory() / "assets");
/// Renderer renderer(window, resources);
///
/// const TextureId ship = resources.loadTexture("sprites/ship.png");
/// while (!window.shouldClose()) {
///     renderer.beginFrame();
///     renderer.drawTexture(ship, {100.0f, 540.0f});
///     renderer.endFrame();
/// }
/// @endcode
class Renderer
{
private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

public:
    /// Creates the renderer.
    /// @param window The open window. It must outlive the renderer.
    /// @param resources Where the textures and fonts to draw come from. It must outlive the renderer.
    /// @param gameSize Logical resolution of the game, in game units.
    /// @throws std::invalid_argument if gameSize is not strictly positive.
    Renderer(const Window &window, const ResourceManager &resources, Vector2 gameSize = {1920.0f, 1080.0f});
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&) noexcept;
    Renderer &operator=(Renderer &&) noexcept;

    /// Logical resolution of the game, in game units.
    [[nodiscard]] Vector2 gameSize() const;

    /// Starts a new frame and fills it with the background colour.
    /// @throws std::logic_error if a frame is already started.
    void beginFrame(Color background = colors::Black);
    /// Ends the frame and shows it in the window.
    /// @throws std::logic_error if no frame is started.
    void endFrame();

    /// Draws a whole texture at its original size.
    /// @param position Top-left corner, in game units.
    /// @param tint Colour multiplied with the texture: white keeps it unchanged, a lower alpha fades it.
    /// @throws std::logic_error if no frame is started.
    void drawTexture(TextureId texture, Vector2 position, Color tint = colors::White);
    /// Draws part of a texture, for instance one frame of a sprite sheet, stretched to a destination.
    /// @param source Part of the texture to draw, in texture pixels.
    /// @param destination Where to draw it, in game units. The source is stretched to this size.
    /// @param rotation Clockwise rotation around the centre of the destination, in degrees.
    /// @param tint Colour multiplied with the texture.
    /// @throws std::logic_error if no frame is started.
    void drawTexture(TextureId texture, Rectangle source, Rectangle destination, float rotation = 0.0f,
                     Color tint = colors::White);

    /// Draws a filled rectangle.
    /// @throws std::logic_error if no frame is started.
    void drawRectangle(Rectangle rectangle, Color color);

    /// Draws a filled circle.
    /// @param center Centre of the circle, in game units.
    /// @param radius Radius, in game units. Nothing is drawn if it is not strictly positive.
    /// @throws std::logic_error if no frame is started.
    void drawCircle(Vector2 center, float radius, Color color);

    /// Draws a straight line.
    /// @param start First end of the line, in game units.
    /// @param end Second end of the line, in game units.
    /// @param thickness Width of the line, in game units.
    /// @throws std::logic_error if no frame is started.
    void drawLine(Vector2 start, Vector2 end, float thickness, Color color);

    /// Draws a filled triangle. The corners can be given in any order.
    /// @throws std::logic_error if no frame is started.
    void drawTriangle(Vector2 first, Vector2 second, Vector2 third, Color color);

    /// Draws text with a loaded font.
    /// @param position Top-left corner of the text, in game units.
    /// @param size Height of the characters, in game units.
    /// @throws std::logic_error if no frame is started.
    void drawText(FontId font, const std::string &text, Vector2 position, float size, Color color);
    /// Draws text with raylib's built-in font, which is always available. Meant for debugging.
    /// @throws std::logic_error if no frame is started.
    void drawText(const std::string &text, Vector2 position, float size, Color color);

    /// Draws the number of frames per second, for debugging.
    /// @throws std::logic_error if no frame is started.
    void drawFps(Vector2 position);

    /// Size of a texture, in texture pixels.
    [[nodiscard]] Vector2 textureSize(TextureId texture) const;
    /// Size that the text would take once drawn with drawText(), for instance to centre it.
    [[nodiscard]] Vector2 measureText(FontId font, const std::string &text, float size) const;
    /// Size that the text would take once drawn with the built-in font.
    [[nodiscard]] Vector2 measureText(const std::string &text, float size) const;
};

} // namespace rtype::render
