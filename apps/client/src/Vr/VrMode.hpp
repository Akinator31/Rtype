#pragma once

#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/input/Keys.hpp"
#include "rtype/xr/Headset.hpp"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace rtype::client
{

/// The key that puts the cockpit back around the player (VrMode::recenter()), until the controllers can
/// (docs/plan/vr.md, X5).
inline constexpr input::Key RecenterKey = input::Key::F9;

/// Where the choice of starting in the headset is kept in the settings. Not on the Settings page yet
/// (docs/architecture/vr.md#next-steps): written by hand in the settings file.
inline constexpr std::string_view VrSettingsKey = "vr.enabled";
/// The option of the command line that starts the client in the headset.
inline constexpr std::string_view VrOption = "--xr";

/// Whether the client starts in VR: --xr among the arguments of the command line, or the VR setting on.
[[nodiscard]] bool wantsVr(std::span<const std::string_view> arguments, const engine::Settings &settings);

/// What VrMode asks of the rest of the client.
struct VrHooks {
    /// Opens the headset: xr::openHeadset() in the game, a fake in the tests.
    std::function<xr::OpenedHeadset()> open;
    /// The headset paces the frames (true) or the window does again (false):
    /// render::Window::setPacedElsewhere().
    std::function<void(bool paced)> pace;
    /// The session of the headset is over: the client goes on on the screen, and says so.
    std::function<void()> lost;
    /// Draws what an eye sees, its image bound by the headset: VrView::drawEye() in the game. None leaves the
    /// eyes to the background colour of the headset.
    std::function<void(int eye, const xr::EyeView &view)> drawEye;
};

/// The game in a VR headset (docs/architecture/vr.md). It opens the headset, gives it a frame after each
/// frame of the game, its eyes drawn by VrHooks::drawEye, and lets it pace the game while it shows it.
/// Without a headset, or once its session is lost, the client goes on on the screen as if VR had never been
/// asked: nothing here is ever an error.
///
/// Destroy it before the window: the images of the eyes live in its OpenGL context.
/// @code
/// VrMode vr({.open = [] { return xr::openHeadset(); },
///            .pace = [&](bool paced) { window.setPacedElsewhere(paced); },
///            .lost = [&] { engine.showToast("...", engine::ToastType::Warning); },
///            .drawEye = [&](int eye, const xr::EyeView &view) { vrView.drawEye(eye, view); }});
/// vr.start();
/// while (...) {
///     // ... the frame of the game, into the board while vr.isShowing()
///     vr.update();
/// }
/// @endcode
class VrMode
{
    VrHooks _hooks;
    std::unique_ptr<xr::Headset> _headset;
    std::string _error;
    bool _paced = false;

    void pace(bool paced);
    /// The session is over: the headset is closed, the window paces the frames again.
    void stop();

public:
    explicit VrMode(VrHooks hooks);
    ~VrMode() = default;

    VrMode(const VrMode &) = delete;
    VrMode &operator=(const VrMode &) = delete;
    VrMode(VrMode &&) = delete;
    VrMode &operator=(VrMode &&) = delete;

    /// Opens the headset, if not open yet. False when there is none: error() says why, and the client stays
    /// on the screen.
    bool start();
    /// Once per frame, after the frame of the game: reads what the runtime says, and draws the eyes while the
    /// headset shows the game, which waits for the next refresh of the headset.
    void update();

    /// Puts the cockpit back around the player: the origin of the headset at their head, facing where they
    /// look (xr::Headset::recenter()). Nothing without a headset.
    void recenter();

    /// A headset is open: shown or waiting for the runtime.
    [[nodiscard]] bool isActive() const { return _headset != nullptr; }
    /// The headset shows the game and paces it.
    [[nodiscard]] bool isShowing() const { return _paced; }
    /// Why start() opened no headset, empty otherwise.
    [[nodiscard]] const std::string &error() const { return _error; }
};

} // namespace rtype::client
