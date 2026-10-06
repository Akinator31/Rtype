#include "rtype/render/Window.hpp"

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
    SetConfigFlags(flags);
    InitWindow(settings.width, settings.height, settings.title.c_str());
    if (!IsWindowReady()) {
        throw std::runtime_error("Cannot open the window");
    }
    // raylib closes the window when Escape is pressed: disable it, the game decides what Escape does
    SetExitKey(KEY_NULL);
    SetTargetFPS(settings.targetFps);
    windowOpen = true;
}

Window::~Window()
{
    CloseWindow();
    windowOpen = false;
}

bool Window::shouldClose() const
{
    return WindowShouldClose();
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

} // namespace rtype::render
