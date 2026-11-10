#pragma once

#include <algorithm>

namespace rtype::client
{

/// The arrival in the headset (docs/architecture/vr.md#the-arrival): each time the headset starts showing the
/// game, at the start of the client, after the menu of the runtime or once the VR page opened it, space and
/// the board fade in from black, then the panel unfolds with the menu it shows. Nothing appears at once in
/// front of the eyes.
/// @code
/// arrival.start();         // the headset starts showing the game
/// arrival.update(elapsed); // each frame
/// const float shown = arrival.brightness();
/// const bool panel = scene.panel && !arrival.holdsPanel();
/// @endcode
class Arrival
{
public:
    /// How long space and the board take to come in from black, in seconds.
    static constexpr float FadeIn = 1.5f;
    /// When the panel starts unfolding, in seconds: once the board is nearly there.
    static constexpr float PanelAt = 1.2f;
    /// When the arrival is over.
    static constexpr float End = std::max(FadeIn, PanelAt);

private:
    /// Time since the arrival began, in seconds; past it when there is none.
    float _time = End;

public:
    /// The headset starts showing the game: from black again.
    void start() { _time = 0.0f; }
    /// Moves the arrival on.
    /// @param elapsed Time since the previous frame, in seconds.
    void update(float elapsed) { _time = std::min(_time + std::max(elapsed, 0.0f), End); }

    /// How bright space and the board are, from 0 (black) to 1: eased, slow at the end.
    [[nodiscard]] float brightness() const
    {
        const float done = std::clamp(_time / FadeIn, 0.0f, 1.0f);
        return 1.0f - ((1.0f - done) * (1.0f - done));
    }
    /// The panel stays folded: the player first sees where they are.
    [[nodiscard]] bool holdsPanel() const { return _time < PanelAt; }
};

} // namespace rtype::client
