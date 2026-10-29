#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace rtype::engine
{

/// How serious the message of a toast is: it sets its colour and its icon (see ToastStyle).
enum class ToastType {
    /// An information: a circled "i".
    Info,
    /// A gain: a star.
    Success,
    /// A warning: a triangle with an exclamation mark, like an error.
    Warning,
    /// An error: a triangle with an exclamation mark.
    Error,
};

/// Number of values of ToastType: the size of the tables indexed by a type, which follow its order.
inline constexpr std::size_t toastTypeCount = 4;

/// The shape of a toast.
enum class ToastKind {
    /// A line of text in a corner of the screen, stacked with the others: an event.
    Line,
    /// A title with a line of light under it and a line of detail, centred at the top of the screen: an
    /// announcement.
    Announcement,
    /// A band across the whole screen, with a title and a detail: a banner, for what the player must not
    /// miss.
    Banner,
};

/// Number of values of ToastKind: the size of the tables indexed by a kind, which follow its order.
inline constexpr std::size_t toastKindCount = 3;

/// A short message shown in a corner of the screen for a few seconds, over every scene: "Settings saved",
/// "Connection lost". Created by Engine::showToast(), which is all a caller needs.
///
/// A toast reads as a line: the subject, the message, the actor, then its icon. A toast with only a message
/// shows it as its subject. An announcement or a banner shows its
/// subject as its title and its message as its detail.
///
/// A toast holds only its words: its look comes from InterfaceStyle::toast, so that changing the look of
/// every toast is done there. The ToastSystem places it, fades it in and out and destroys it; the
/// SceneRenderer draws it.
struct Toast {
    std::string text;
    ToastType type = ToastType::Info;
    /// What the message is about, in capitals before it: "VORTEX" in "VORTEX has joined the room". Can be
    /// empty.
    std::string subject;
    /// Who the message is about, in capitals after it: "KAZE" in "Orbital module taken by KAZE". Can be
    /// empty.
    std::string actor;
    ToastKind kind = ToastKind::Line;
    /// Time it stays fully shown, between its fade in and its fade out, in seconds: ToastStyle::duration
    /// when empty.
    std::optional<float> duration;
    /// Time since it appeared, in seconds. Written by the ToastSystem.
    float time = 0.0f;
    /// Place in the stack of toasts, 0 being the nearest to the corner; in between while it glides from one
    /// place to another. Written by the ToastSystem.
    float place = 0.0f;
    /// Width of the toast, in game units, from the width of its text. Written by the ToastSystem.
    float width = 0.0f;

    /// The bright words: the subject, or the message when there is no subject.
    [[nodiscard]] const std::string &getHeadline() const { return subject.empty() ? text : subject; }
    /// The grey words after the subject, empty when the message is shown as the subject.
    [[nodiscard]] std::string getBody() const { return subject.empty() ? std::string() : text; }
};

} // namespace rtype::engine
