#pragma once

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"

#include <cstdint>
#include <deque>
#include <optional>

namespace rtype::client
{

/// Predicts the ship of the local player online, so that it answers the controls at once, as offline: the
/// game stays the one that decides, the client guesses what it will say (docs/architecture/client-online.md).
///
/// @code
/// prediction.apply(tick, input);                 // each INPUT sent: the ship moves at once
/// prediction.reconcile(ship, snapshotTick);      // each snapshot: where the game had it
/// prediction.advance(elapsed);                   // each frame
/// const auto drawn = prediction.position();      // where to draw it
/// @endcode
///
/// - **Applying**: each input sent moves the predicted ship by one tick, with the movement of the game
///   (game::moveShip(), the same to the bit). The inputs are kept with their tick. A tick skipped when
///   stamping repeats the previous input, as the game does.
/// - **Reconciling**: a snapshot gives where the game had the ship at its tick, every input up to that tick
///   applied. The prediction starts again from there and replays the inputs of the later ticks: when nothing
///   was lost, it lands where it already was.
/// - **Correcting**: when it lands elsewhere (an input lost, a wall the game saw first), the ship is not
///   teleported: the gap is drawn and fades out over a few frames. A gap too large (the ship came back
///   elsewhere) is not smoothed.
/// - **No ship** (destroyed, waiting to come back): nothing is predicted; the game decides when it comes
///   back.
///
/// Pure: no clock, no network.
class Prediction
{
public:
    /// Inputs kept: two seconds of ticks, far more than a round trip.
    static constexpr std::size_t MaxInputs = 120;
    /// A tick skipped when stamping is filled with the previous input, up to this many.
    static constexpr std::uint32_t MaxFilled = 30;
    /// How fast a correction fades out: this fraction of what is left disappears per second.
    static constexpr double Smoothing = 15.0;
    /// A correction larger than this, in game units, is applied at once.
    static constexpr float MaxSmoothed = 120.0f;

private:
    struct Sent {
        std::uint32_t tick = 0;
        game::PlayerInput input;
    };

    game::Rules _rules;
    std::deque<Sent> _sent;
    std::optional<ecs::Position> _predicted;
    /// What is drawn on top of the prediction while a correction fades out.
    float _offsetX = 0.0f;
    float _offsetY = 0.0f;

public:
    explicit Prediction(game::Rules rules = {}) : _rules(rules) {}

    /// An input sent for `tick`: the predicted ship moves at once.
    void apply(std::uint32_t tick, game::PlayerInput input);

    /// A snapshot of the game.
    /// @param ship Where the game had the ship of the local player, nothing if it has none.
    /// @param tick The tick of the snapshot: the inputs up to it are applied by the game.
    void reconcile(std::optional<ecs::Position> ship, std::uint32_t tick);

    /// Time goes by: the correction drawn fades out.
    void advance(double elapsed);

    /// Where to draw the ship, nothing while it has none.
    [[nodiscard]] std::optional<ecs::Position> position() const;

    /// Where the ship will be once every input sent is applied, without the correction drawn.
    [[nodiscard]] std::optional<ecs::Position> predicted() const { return _predicted; }

    /// Inputs sent and not applied by the game yet, as far as the snapshots tell.
    [[nodiscard]] std::size_t pending() const { return _sent.size(); }
};

} // namespace rtype::client
