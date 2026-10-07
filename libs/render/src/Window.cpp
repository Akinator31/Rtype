#include "rtype/render/Window.hpp"

#include "RaylibLog.hpp"

#include <raylib.h>

#include <stdexcept>

namespace rtype::render
{

namespace
{

bool windowOpen = false;

} // namespace

Window::Window(const WindowSettings &settings)
{
    if (windowOpen) {
        throw std::logic_error("A window is already open");
    }

    unsigned int flags = 0;
    if (settings.resizable) {
        flags |= FLAG_WINDOW_RESIZABLE;
    }
    if (settings.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
    detail::redirectRaylibLogs();
    SetConfigFlags(flags);
    InitWindow(settings.width, settings.height, settings.title.c_str());
    if (!IsWindowReady()) {
        throw std::runtime_error("Cannot open the window");
    }
    // raylib closes the window when Escape is pressed: disable it, the game decides what Escape does
    SetExitKey(KEY_NULL);
    SetTargetFPS(settings.targetFps);
    windowOpen = true;
    if (settings.fullscreen) {
        setFullscreen(true);
    }
}

Window::~Window()
{
    CloseWindow();
    windowOpen = false;
}

bool Window::shouldClose() const
{
    return _closeRequested || WindowShouldClose();
}

void Window::requestClose()
{
    _closeRequested = true;
}

int Window::width() const
{
    return GetScreenWidth();
}

int Window::height() const
{
    return GetScreenHeight();
}

float Window::frameTime() const
{
    return GetFrameTime();
}

// Fullscreen is raylib's "borderless windowed" mode: its exclusive fullscreen changes the video mode of the
// screen, which is slow, can rearrange the other screens, and behaves badly when switching to another window
void Window::setFullscreen(bool fullscreen)
{
    if (fullscreen != isFullscreen()) {
        ToggleBorderlessWindowed();
    }
}

bool Window::isFullscreen() const
{
    return IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
}

void Window::setSize(int width, int height)
{
    setFullscreen(false);
    SetWindowSize(width, height);
}

void Window::setTitle(const std::string &title)
{
    SetWindowTitle(title.c_str());
}

void Window::setCursorVisible(bool visible)
{
    // Only call raylib on a change: the game may call this every frame
    if (visible && IsCursorHidden()) {
        ShowCursor();
    } else if (!visible && !IsCursorHidden()) {
        HideCursor();
    }
}

bool Window::isCursorVisible() const
{
    return !IsCursorHidden();
}

void Window::setVsync(bool vsync)
{
    if (vsync) {
        SetWindowState(FLAG_VSYNC_HINT);
    } else {
        ClearWindowState(FLAG_VSYNC_HINT);
    }
}

void Window::setTargetFps(int fps)
{
    SetTargetFPS(fps);
}

} // namespace rtype::render
