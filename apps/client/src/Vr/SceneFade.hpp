#pragma once

#include <algorithm>

namespace rtype::client
{

/// The fade between two scenes in a VR headset, instead of a cut: when the scenes are switched, the board and
/// the panel go dark, still showing the scene left, then the new one comes in from black. A cut that changes
/// all that the player sees at once is harsh in a headset. The cockpit stays lit: the player never leaves
/// their seat.
/// @code
/// fade.start();          // the scenes were switched
/// fade.update(elapsed);  // each frame
/// if (!fade.holding()) { /* draw the scene into the images */ }
/// renderer.drawImageQuad(board, transform, {.tint = grey(fade.brightness())});
/// @endcode
class SceneFade
{
    /// Time since the switch, in seconds; past the fade when there is none.
    float _time = Duration;

public:
    /// How long the whole fade lasts, out then in, in seconds.
    static constexpr float Duration = 0.2f;

    /// The scenes were switched: the fade starts again from the scene left, wherever it was.
    void start() { _time = 0.0f; }
    /// Moves the fade on.
    /// @param elapsed Time since the previous frame, in seconds.
    void update(float elapsed) { _time = std::min(_time + std::max(elapsed, 0.0f), Duration); }

    /// How bright the scene is, from 0 (black, between the two scenes) to 1 (no fade).
    [[nodiscard]] float brightness() const
    {
        const float half = Duration / 2.0f;
        return _time < half ? 1.0f - (_time / half) : (_time - half) / half;
    }
    /// While the scene left fades out, its last images are kept: nothing is drawn into them.
    [[nodiscard]] bool holding() const { return _time < Duration / 2.0f; }
};

} // namespace rtype::client
