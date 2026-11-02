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
    /// Opens the window in fullscreen (see Window::setFullscreen()).
    bool fullscreen = false;
};

/// The game window, open for as long as this object exists.
///
/// The window is opened by the constructor and closed by the destructor (RAII). Only one window can exist
/// at a time, as raylib supports a single window. Graphics resources (textures, fonts, shaders) need the
/// window: create them after it and destroy them before it.
///
/// On Linux, a laptop with an NVIDIA GPU next to an integrated one draws with the NVIDIA GPU, unless
/// __GLX_VENDOR_LIBRARY_NAME is already set (mesa keeps the integrated GPU).
class Window
{
private:
    bool _closeRequested = false;

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

    /// Tells whether the window should close: the player used its close button or the system shortcut (Alt+F4
    /// for instance), or the game called requestClose(). The Escape key does not close the window: what it
    /// does is up to the game.
    [[nodiscard]] bool shouldClose() const;
    /// Asks the window to close, for instance from a "Quit" button: shouldClose() returns true from now on.
    void requestClose();

    /// Current width of the drawable area, in pixels.
    [[nodiscard]] int width() const;
    /// Current height of the drawable area, in pixels.
    [[nodiscard]] int height() const;

    /// Duration of the last frame, in seconds. Use it to make movement independent of the frame rate.
    [[nodiscard]] float frameTime() const;

    /// Switches between a window and fullscreen.
    ///
    /// Fullscreen is a borderless window covering the screen it is on, at the resolution of that screen: the
    /// screen keeps its video mode, so switching is instant and other windows are not disturbed. Leaving
    /// fullscreen restores the previous size and position of the window.
    void setFullscreen(bool fullscreen);
    /// Tells whether the window is in fullscreen.
    [[nodiscard]] bool isFullscreen() const;

    /// Resizes the window, in pixels. Leaves fullscreen first if needed.
    void setSize(int width, int height);
    /// Changes the title of the window.
    void setTitle(const std::string &title);

    /// Shows or hides the mouse pointer while it is over the window, for instance to hide it during the game
    /// and show it in menus.
    void setCursorVisible(bool visible);
    /// Tells whether the mouse pointer is shown over the window.
    [[nodiscard]] bool isCursorVisible() const;

    /// Turns vertical synchronisation on or off, see WindowSettings::vsync.
    void setVsync(bool vsync);
    /// Sets the maximum number of frames per second, or 0 for no limit.
    void setTargetFps(int fps);
};

} // namespace rtype::render
