#pragma once

/// What a player does with its voice, whatever carries it
/// (docs/architecture/voice.md#the-status-of-each-player).
namespace rtype::voice
{

/// What a player tells the others about its voice (`VOICE_STATUS`): the HUD of the others shows it beside its
/// name.
struct Status {
    /// Muted by itself: by its key, or its voice turned off. Nobody hears it.
    bool muted = false;
    /// Deafened by itself: it hears nobody.
    bool deafened = false;
    /// Its microphone cannot be opened: it listens only.
    bool noMicrophone = false;

    friend bool operator==(const Status &, const Status &) = default;
};

} // namespace rtype::voice
