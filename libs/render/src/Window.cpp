#include "rtype/render/Window.hpp"

#include "RaylibLog.hpp"
#include "rtype/log/Logger.hpp"

#include <raylib.h>

#include <stdexcept>

#ifdef __linux__
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#endif

namespace rtype::render
{

namespace
{

bool windowOpen = false;

#ifdef __linux__

// PCI vendor of the NVIDIA GPUs, as /sys/class/drm/card*/device/vendor gives it
constexpr std::string_view NvidiaVendor = "0x10de";

std::string readVendor(const std::filesystem::path &card)
{
    std::ifstream file(card / "device" / "vendor");
    std::string vendor;
    file >> vendor;
    return vendor;
}

// A NVIDIA GPU, with its proprietary driver loaded (not nouveau), next to another GPU that drives the screen:
// a laptop with two GPUs
bool hasNvidiaBesideAnotherGpu()
{
    std::error_code error;
    if (!std::filesystem::exists("/proc/driver/nvidia/version", error)) {
        return false;
    }
    bool nvidia = false;
    bool other = false;
    for (const auto &entry : std::filesystem::directory_iterator("/sys/class/drm", error)) {
        const std::string name = entry.path().filename().string();
        if (!name.starts_with("card") || name.find('-') != std::string::npos) {
            continue;
        }
        const std::string vendor = readVendor(entry.path());
        nvidia = nvidia || vendor == NvidiaVendor;
        other = other || (!vendor.empty() && vendor != NvidiaVendor);
    }
    return nvidia && other;
}

#endif

// On a laptop with two GPUs, Linux draws with the integrated one unless told otherwise, too slow for the game
// in high resolution: ask for the NVIDIA one (PRIME render offload), as its driver does by itself on Windows.
// The OpenGL library reads these variables when it loads: must be called before the window opens. A vendor
// already set in the environment wins: __GLX_VENDOR_LIBRARY_NAME=mesa keeps the integrated GPU.
void preferDedicatedGpu()
{
#ifdef __linux__
    if (std::getenv("__GLX_VENDOR_LIBRARY_NAME") != nullptr || !hasNvidiaBesideAnotherGpu()) {
        return;
    }
    setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
    setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
    log::Logger("render").info("Two GPUs: drawing with the NVIDIA one");
#endif
}

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
    preferDedicatedGpu();
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
