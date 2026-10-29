#pragma once

#include "rtype/render/ColorFilter.hpp"
#include "rtype/render/Energy.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace rtype::render
{

class Window;

/// Draws the game: textures, text and shapes.
///
/// The game has a fixed size (1920x1080 by default), whatever the window: positions and sizes are always in
/// game units, the same on every screen. The whole game area is always shown; a window of another shape
/// shows more around it, on both sides, instead of black bars (see visibleArea() and computeVisibleArea()).
/// Place the interface relative to the visible area, so that it follows the edges of the screen. The image
/// is drawn at the resolution of the window, so it stays sharp on a large screen.
///
/// Everything is drawn between beginFrame() and endFrame(), in order: what is drawn last is on top.
/// @code
/// Window window;
/// ResourceManager resources(window, applicationDirectory() / "assets",
///                           engine.getThreads().getAssetsWorker());
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
    /// @param gameSize Size of the game, in game units.
    /// @throws std::invalid_argument if gameSize is not strictly positive.
    Renderer(const Window &window, const ResourceManager &resources, Vector2 gameSize = {1920.0f, 1080.0f});
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&) noexcept;
    Renderer &operator=(Renderer &&) noexcept;

    /// Size of the game, in game units: the area from (0, 0) that is shown on every screen.
    [[nodiscard]] Vector2 gameSize() const;
    /// Part of the game shown in the window, in game units, for its current size: the game area, plus what is
    /// around it when the window has another shape. Its origin can be negative.
    [[nodiscard]] Rectangle visibleArea() const;

    /// Converts a position in the window, such as the mouse pointer, into game units, taking the scaling
    /// into account. Near the edges of a window of another shape than the game, it can be outside of the game
    /// area: negative, or larger than gameSize().
    /// @code
    /// const MousePosition mouse = devices.mousePosition();
    /// const Vector2 pointer = renderer.windowToGame({mouse.x, mouse.y});
    /// @endcode
    [[nodiscard]] Vector2 windowToGame(Vector2 position) const;

    /// Applies a colour filter to the whole image, interface included, for colour-blind players or players
    /// with low vision. The filter is applied when the frame is shown, so the game draws with its normal
    /// colours. If the graphics card cannot run the filter, a warning is logged once and the image is shown
    /// unfiltered.
    void setColorFilter(ColorFilter filter);
    /// The colour filter applied to the image, ColorFilter::None by default.
    [[nodiscard]] ColorFilter colorFilter() const;

    /// Darkens the corners of the world, under the interface, to draw the eye to the middle of the screen. It
    /// is drawn by drawGlow().
    /// @param strength How dark the corners are, from 0 (no vignette) to 1 (black); 0.22 by default.
    void setVignette(float strength);
    /// How dark the vignette makes the corners, from 0 to 1.
    [[nodiscard]] float vignette() const;
    /// Smooths the jagged edges of the 3D models (FXAA), when drawGlow() is called. On by default.
    void setAntialiasing(bool enabled);
    /// Whether the edges of the 3D models are smoothed.
    [[nodiscard]] bool antialiasing() const;

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

    /// Draws a filled rectangle coloured with a vertical gradient: a shadow along an edge of the screen.
    /// @param top Colour of the top edge, blended into bottom towards the bottom edge.
    /// @throws std::logic_error if no frame is started.
    void drawGradientRectangle(Rectangle rectangle, Color top, Color bottom);

    /// Draws a filled circle.
    /// @param center Centre of the circle, in game units.
    /// @param radius Radius, in game units. Nothing is drawn if it is not strictly positive.
    /// @throws std::logic_error if no frame is started.
    void drawCircle(Vector2 center, float radius, Color color);

    /// Draws the outline of a rectangle, for instance a hitbox or the frame of a button.
    /// @param thickness Width of the outline, in game units. It is drawn inside the rectangle, so that the
    /// outer edge of the outline is the edge of the rectangle. Nothing is drawn if it is not strictly
    /// positive.
    /// @throws std::logic_error if no frame is started.
    void drawRectangleOutline(Rectangle rectangle, float thickness, Color color);

    /// Draws a filled rectangle with rounded corners, coloured with a horizontal gradient: give the same
    /// colour twice for a plain one.
    /// @param radius Radius of the corners, in game units, at most half of the smaller side.
    /// @param left Colour of the left edge, blended into right towards the right edge.
    /// @throws std::logic_error if no frame is started.
    void drawRoundedRectangle(Rectangle rectangle, float radius, Color left, Color right);

    /// Draws the outline of a rectangle with rounded corners, for instance the border of a button.
    /// @param radius Radius of the outer corners, in game units, at most half of the smaller side.
    /// @param thickness Width of the outline, in game units, drawn inside the rectangle. Nothing is drawn if
    /// it is not strictly positive.
    /// @throws std::logic_error if no frame is started.
    void drawRoundedRectangleOutline(Rectangle rectangle, float radius, float thickness, Color color);

    /// Draws the outline of a circle, for instance a round hitbox.
    /// @param center Centre of the circle, in game units.
    /// @param radius Radius, in game units. Nothing is drawn if it is not strictly positive.
    /// @param thickness Width of the outline, in game units. It is drawn inside the circle, so that the outer
    /// edge of the outline is the edge of the circle. Nothing is drawn if it is not strictly positive.
    /// @throws std::logic_error if no frame is started.
    void drawCircleOutline(Vector2 center, float radius, float thickness, Color color);

    /// Draws an arc of a ring, for instance a circle open on its sides.
    /// @param center Centre of the circle, in game units.
    /// @param radius Outer radius, in game units. Nothing is drawn if it is not strictly positive.
    /// @param thickness Width of the ring, in game units, drawn inside the radius.
    /// @param startAngle, endAngle Ends of the arc, in degrees, turning clockwise on the screen from the
    /// right: 0 is on the right of the centre, 90 below it.
    /// @throws std::logic_error if no frame is started.
    void drawArc(Vector2 center, float radius, float thickness, float startAngle, float endAngle,
                 Color color);

    /// Draws a straight line.
    /// @param start First end of the line, in game units.
    /// @param end Second end of the line, in game units.
    /// @param thickness Width of the line, in game units.
    /// @throws std::logic_error if no frame is started.
    void drawLine(Vector2 start, Vector2 end, float thickness, Color color);

    /// Draws a filled triangle. The corners can be given in any order.
    /// @throws std::logic_error if no frame is started.
    void drawTriangle(Vector2 first, Vector2 second, Vector2 third, Color color);

    /// Draws a 3D model seen through a camera and lit by a light. Models hide each other by depth, whatever
    /// their drawing order, except transparent ones, which must be drawn after what they cover. What is drawn
    /// afterwards in 2D is on top of them.
    /// @param position Where the origin of the model is, in world units.
    /// @param rotationAxis Axis the model is turned around, and rotationAngle how much, in degrees.
    /// @param scale Size factor of the model.
    /// @param tint Colour multiplied with the model.
    /// @param spinAngle How much the model is turned around its own vertical axis (y), in degrees, before the
    /// rotation around rotationAxis: a tilted ring keeps its tilt while it spins.
    /// @param hiddenMeshes Meshes not drawn, by index in the model file: the flame of a ship that does not
    /// fly.
    /// @throws std::logic_error if no frame is started.
    void drawModel(ModelId model, const Camera &camera, const Light &light, Vector3 position,
                   Vector3 rotationAxis = {0.0f, 1.0f, 0.0f}, float rotationAngle = 0.0f, float scale = 1.0f,
                   Color tint = colors::White, float spinAngle = 0.0f,
                   const std::vector<int> &hiddenMeshes = {});
    /// Draws a 3D model of energy (a shot, a beam, a plasma orb) seen through the camera: it glows with the
    /// colour of its texture, added to what is behind it, white hot where it faces the camera, flickering
    /// with the noise of `energy`, with the halo of `energy` around it. It hides nothing: draw it after the
    /// models it may cover. Its light also spreads around it when drawGlow() is called.
    /// The other parameters are those of drawModel().
    /// @throws std::logic_error if no frame is started.
    void drawEnergyModel(ModelId model, const Camera &camera, const Energy &energy, Vector3 position,
                         Vector3 rotationAxis = {0.0f, 1.0f, 0.0f}, float rotationAngle = 0.0f,
                         float scale = 1.0f, Color tint = colors::White, float spinAngle = 0.0f);
    /// Smooths the edges of everything drawn so far (setAntialiasing()). Then makes the energy models, and
    /// the light the models emit (engine flames, windows), drawn since the start of the frame (or the
    /// previous call) shine: their light is blurred around them and added over everything drawn so far. Then
    /// darkens the corners (setVignette()). Call it after the 3D models and
    /// before the interface, which it would otherwise cover. If the graphics card cannot blur, a warning is
    /// logged once and nothing glows.
    /// @throws std::logic_error if no frame is started.
    void drawGlow();

    /// Poses an animated model at a time of one of its animations: the next draws of this model show this
    /// pose, until it is animated again. Several entities sharing a model each pose it before being drawn.
    /// Animating a model without animations does nothing.
    /// @param animation Index of the animation, from ResourceManager::getModelAnimation().
    /// @param time Time in the animation, in seconds. The animation loops.
    void animateModel(ModelId model, int animation, float time);

    /// Poses an animated model between two of its animations, like animateModel(): to go smoothly from one to
    /// the other.
    /// @param from, to Indexes of the animations, from ResourceManager::getModelAnimation().
    /// @param fromTime, toTime Time in each animation, in seconds. The animations loop.
    /// @param amount How far the pose is from the first animation (0) to the second one (1).
    void blendModelAnimations(ModelId model, int from, float fromTime, int to, float toTime, float amount);

    /// Draws text with a loaded font.
    /// @param position Top-left corner of the text, in game units.
    /// @param size Height of the characters, in game units.
    /// @param spacing Space added between two characters, in game units: widely spaced capitals.
    /// @throws std::logic_error if no frame is started.
    void drawText(FontId font, const std::string &text, Vector2 position, float size, Color color,
                  float spacing = 0.0f);
    /// Draws text with a loaded font, its colour changing from the top of the line to the bottom of it: the
    /// chrome of a title.
    /// @param position Top-left corner of the text, in game units.
    /// @param size Height of the characters, in game units. It is also the height of the gradient.
    /// @param stops Colours from the top (position 0) to the bottom (position 1) of the line, in the order of
    /// their positions. A single stop gives a plain colour; none draws nothing.
    /// @param spacing Space added between two characters, in game units.
    /// @throws std::logic_error if no frame is started.
    void drawGradientText(FontId font, const std::string &text, Vector2 position, float size,
                          const std::vector<ColorStop> &stops, float spacing = 0.0f);
    /// Draws text with raylib's built-in font, which is always available. Meant for debugging.
    /// @throws std::logic_error if no frame is started.
    void drawText(const std::string &text, Vector2 position, float size, Color color);

    /// Draws the number of frames per second, for debugging.
    /// @throws std::logic_error if no frame is started.
    void drawFps(Vector2 position);

    /// Size of a texture, in texture pixels.
    [[nodiscard]] Vector2 textureSize(TextureId texture) const;
    /// Size that the text would take once drawn with drawText(), for instance to centre it.
    [[nodiscard]] Vector2 measureText(FontId font, const std::string &text, float size,
                                      float spacing = 0.0f) const;
    /// Size that the text would take once drawn with the built-in font.
    [[nodiscard]] Vector2 measureText(const std::string &text, float size) const;
};

} // namespace rtype::render
