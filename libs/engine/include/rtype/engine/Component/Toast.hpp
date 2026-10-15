#pragma once

#include <string>

namespace rtype::engine
{

/// How serious the message of a toast is: it sets the colour of its accent (see ToastStyle).
enum class ToastType {
    Info,
    Success,
    Warning,
    Error,
};

/// A short message shown in a corner of the screen for a few seconds, over every scene: "Settings saved",
/// "Connection lost". Created by Engine::showToast(), which is all a caller needs.
///
/// A toast holds only its message: its look comes from InterfaceStyle::toast, so that changing the look of
/// every toast is done there. The ToastSystem places it, fades it in and out and destroys it; the
/// SceneRenderer draws it.
struct Toast {
    std::string text;
    ToastType type = ToastType::Info;
    /// Time since it appeared, in seconds. Written by the ToastSystem.
    float time = 0.0f;
    /// Place in the stack of toasts, 0 being the nearest to the corner; in between while it glides from one
    /// place to another. Written by the ToastSystem.
    float place = 0.0f;
    /// Width of the toast, in game units, from the width of its text. Written by the ToastSystem.
    float width = 0.0f;
};

} // namespace rtype::engine
