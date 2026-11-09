#include "rtype/render/Window.hpp"

#include "RaylibLog.hpp"
#include "rtype/log/Logger.hpp"

#include <raylib.h>
// raylib is built on GLFW: its fullscreen is driven here directly (see setFullscreen())
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

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

// How many frames a size asked is asked again, and every how many frames: on Wayland, the compositor applies
// the end of fullscreen a few frames late, and can shrink the window by its title bar meanwhile
constexpr int SizeWatchFrames = 60;
constexpr int SizeRetryInterval = 5;

// The window of raylib: GetWindowHandle() gives the one of the system, not the one of GLFW
GLFWwindow *handle()
{
    return glfwGetCurrentContext();
}

GLFWmonitor *monitorAt(int index)
{
    int count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&count);
    return index >= 0 && index < count ? monitors[index] : nullptr;
}

int indexOf(GLFWmonitor *monitor)
{
    int count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&count);
    for (int index = 0; index < count; ++index) {
        if (monitors[index] == monitor) {
            return index;
        }
    }
    return 0;
}

// Makes the window cover a screen, at the resolution of that screen: its video mode does not change
void cover(GLFWmonitor *monitor)
{
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(handle(), monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    // A new video mode can lose the synchronisation of the frames
    if (IsWindowState(FLAG_VSYNC_HINT)) {
        glfwSwapInterval(1);
    }
}

// The top left corner of a window of this size centred on the free area of a screen
std::pair<int, int> centredOn(GLFWmonitor *monitor, int width, int height)
{
    int x = 0;
    int y = 0;
    int areaWidth = 0;
    int areaHeight = 0;
    glfwGetMonitorWorkarea(monitor, &x, &y, &areaWidth, &areaHeight);
    return {x + ((areaWidth - width) / 2), y + ((areaHeight - height) / 2)};
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
    _vsync = settings.vsync;
    setTargetFps(settings.targetFps);
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

void Window::update()
{
    if (_pendingSize.framesLeft <= 0 || isFullscreen()) {
        return;
    }
    --_pendingSize.framesLeft;
    int width = 0;
    int height = 0;
    glfwGetWindowSize(handle(), &width, &height);
    const bool wrong = width != _pendingSize.width || height != _pendingSize.height;
    if (wrong && _pendingSize.framesLeft % SizeRetryInterval == 0) {
        glfwSetWindowSize(handle(), _pendingSize.width, _pendingSize.height);
    }
}

void Window::watchSize(int width, int height)
{
    _pendingSize = {width, height, SizeWatchFrames};
}

// Fullscreen is driven with GLFW, not with raylib: raylib also removes the title bar and puts it back, after
// which the compositor of Wayland keeps the window in fullscreen and refuses any other size. Its exclusive
// fullscreen changes the video mode of the screen, which is slow and can rearrange the other screens
void Window::setFullscreen(bool fullscreen)
{
    if (fullscreen == isFullscreen()) {
        return;
    }
    if (fullscreen) {
        glfwGetWindowPos(handle(), &_windowed.x, &_windowed.y);
        glfwGetWindowSize(handle(), &_windowed.width, &_windowed.height);
        cover(monitorAt(monitor()));
        return;
    }
    glfwSetWindowMonitor(handle(), nullptr, _windowed.x, _windowed.y, _windowed.width, _windowed.height,
                         GLFW_DONT_CARE);
    watchSize(_windowed.width, _windowed.height);
}

bool Window::isFullscreen() const
{
    return glfwGetWindowMonitor(handle()) != nullptr;
}

void Window::setSize(int width, int height)
{
    const auto [x, y] = centredOn(monitorAt(monitor()), width, height);
    if (isFullscreen()) {
        glfwSetWindowMonitor(handle(), nullptr, x, y, width, height, GLFW_DONT_CARE);
    } else {
        glfwSetWindowSize(handle(), width, height);
        glfwSetWindowPos(handle(), x, y);
    }
    watchSize(width, height);
}

std::vector<Monitor> Window::monitors() const
{
    std::vector<Monitor> list;
    for (int index = 0; index < GetMonitorCount(); ++index) {
        const char *name = GetMonitorName(index);
        list.push_back({name != nullptr ? name : "", GetMonitorWidth(index), GetMonitorHeight(index)});
    }
    return list;
}

int Window::monitor() const
{
    GLFWmonitor *covered = glfwGetWindowMonitor(handle());
    return covered != nullptr ? indexOf(covered) : GetCurrentMonitor();
}

void Window::moveToMonitor(int monitor)
{
    GLFWmonitor *target = monitorAt(monitor);
    if (target == nullptr || monitor == this->monitor()) {
        return;
    }
    if (isFullscreen()) {
        cover(target);
        return;
    }
    const auto [x, y] = centredOn(target, width(), height());
    glfwSetWindowPos(handle(), x, y);
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
    _vsync = vsync;
    if (_pacedElsewhere) {
        return;
    }
    if (vsync) {
        SetWindowState(FLAG_VSYNC_HINT);
    } else {
        ClearWindowState(FLAG_VSYNC_HINT);
    }
}

bool Window::isVsync() const
{
    return _vsync;
}

void Window::setTargetFps(int fps)
{
    _targetFps = std::max(fps, 0);
    if (!_pacedElsewhere) {
        SetTargetFPS(_targetFps);
    }
}

int Window::targetFps() const
{
    return _targetFps;
}

void Window::setPacedElsewhere(bool paced)
{
    if (paced == _pacedElsewhere) {
        return;
    }
    if (paced) {
        ClearWindowState(FLAG_VSYNC_HINT);
        SetTargetFPS(0);
        _pacedElsewhere = true;
    } else {
        _pacedElsewhere = false;
        setVsync(_vsync);
        setTargetFps(_targetFps);
    }
}

bool Window::isPacedElsewhere() const
{
    return _pacedElsewhere;
}

} // namespace rtype::render
