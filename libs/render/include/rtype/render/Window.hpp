#pragma once

#include <string>
#include <vector>

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

/// A screen connected to the computer, as Window::monitors() lists it.
struct Monitor {
    /// Its name as the system gives it, for instance "DELL U2720Q". It can be empty.
    std::string name;
    /// The resolution of its current video mode, in pixels.
    int width = 0;
    int height = 0;
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
    /// A place and a size of the window, in screen pixels.
    struct Area {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };
    /// A size asked for the window, asked again for a while: the system may still change it on its own.
    struct PendingSize {
        int width = 0;
        int height = 0;
        int framesLeft = 0;
    };

    bool _closeRequested = false;
    int _targetFps = 0;
    bool _vsync = true;
    /// See setPacedElsewhere().
    bool _pacedElsewhere = false;
    /// Where the window was before fullscreen, to go back there.
    Area _windowed;
    PendingSize _pendingSize;

    void watchSize(int width, int height);

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

    /// Finishes the changes of size the system has not applied yet. Call it once per frame.
    void update();

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

    /// Resizes the window, in pixels, and centres it on its screen. Leaves fullscreen first if needed. The
    /// system may take a few frames to apply it: see update().
    void setSize(int width, int height);

    /// The screens connected to the computer, the first one being the primary screen.
    [[nodiscard]] std::vector<Monitor> monitors() const;
    /// The index in monitors() of the screen the window is on.
    [[nodiscard]] int monitor() const;
    /// Moves the window to a screen of monitors(): in fullscreen, it covers the new screen; otherwise it
    /// keeps its size and is centred on it. Does nothing for an index out of the list.
    void moveToMonitor(int monitor);
    /// Changes the title of the window.
    void setTitle(const std::string &title);

    /// Shows or hides the mouse pointer while it is over the window, for instance to hide it during the game
    /// and show it in menus.
    void setCursorVisible(bool visible);
    /// Tells whether the mouse pointer is shown over the window.
    [[nodiscard]] bool isCursorVisible() const;

    /// Turns vertical synchronisation on or off, see WindowSettings::vsync.
    void setVsync(bool vsync);
    /// Tells whether vertical synchronisation is on: the frames are then limited to the refresh rate of the
    /// screen, whatever setTargetFps() asks.
    [[nodiscard]] bool isVsync() const;
    /// Sets the maximum number of frames per second, or 0 for no limit.
    void setTargetFps(int fps);
    /// The maximum number of frames per second, 0 for no limit.
    [[nodiscard]] int targetFps() const;

    /// Lets something else pace the frames: a VR headset, which waits for its own refresh. While true, the
    /// window neither waits for the screen (no vertical synchronisation) nor limits the frames per second,
    /// which would make the headset miss its frames. setVsync() and setTargetFps() are kept, isVsync() and
    /// targetFps() still say what they asked, and false applies them again.
    void setPacedElsewhere(bool paced);
    [[nodiscard]] bool isPacedElsewhere() const;
};

} // namespace rtype::render
