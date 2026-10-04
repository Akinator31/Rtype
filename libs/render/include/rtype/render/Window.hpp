#pragma once

#include <string>

namespace rtype::render
{

/// Settings applied when the window is opened.
struct WindowSettings {
    int width = 1280;
    int height = 720;
    std::string title = "R-Type";
    /// Lets the player resize the window.
    bool resizable = true;
    /// Synchronises frames with the screen refresh rate to avoid tearing.
    bool vsync = true;
    /// Maximum number of frames per second, or 0 for no limit.
    int targetFps = 60;
};

/// The game window, open for as long as this object exists.
///
/// The window is opened by the constructor and closed by the destructor (RAII). Only one window can exist
/// at a time, as raylib supports a single window. Graphics resources (textures, fonts, shaders) need the
/// window: create them after it and destroy them before it.
class Window
{
public:
    /// Opens the window.
    /// @throws std::logic_error if a window is already open.
    /// @throws std::runtime_error if the window cannot be created (no display, no OpenGL support...).
    explicit Window(const WindowSettings &settings = {});
    ~Window();

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    /// Tells whether the player asked to close the window (close button or Escape key).
    [[nodiscard]] bool shouldClose() const;

    /// Current width of the drawable area, in pixels.
    [[nodiscard]] int width() const;
    /// Current height of the drawable area, in pixels.
    [[nodiscard]] int height() const;

    /// Duration of the last frame, in seconds. Use it to make movement independent of the frame rate.
    [[nodiscard]] float frameTime() const;
};

} // namespace rtype::render
