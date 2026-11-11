#pragma once

#include "Game/ShotPrediction.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Weapon/WeaponDefinition.hpp"
#include "rtype/game/Weapon/WeaponFiring.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <vector>

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
/// - **The weapon**, once armed(): each input also steps it, as the game does (game::stepWeapon()), and each
///   tick it fires gives a shot, from where the ship was (game::playerShot()). A snapshot gives the state of
///   the weapon at its tick: the shots of the later ticks are replayed from there, like the ship. Those of
///   the ticks up to the snapshot are the game's: they are no longer predicted. With two weapons, each input
///   first takes in hand the one it asks for (game::selectWeapon()), and the snapshot gives the state of
///   both.
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

    /// A weapon of the loadout of the ship.
    struct Weapon {
        game::WeaponDefinition definition;
        /// Half the width of the collider of the projectile of the weapon.
        float projectileHalfWidth = 0.0f;
        /// The index of the projectile in the configuration (PredictedShot::projectile).
        std::uint8_t projectile = game::NoShip;
    };

private:
    struct Sent {
        std::uint32_t tick = 0;
        game::PlayerInput input;
    };

    game::Rules _rules;
    /// The factor of the speed of the ship of the hangar the player flies (game::statFactor()).
    float _speedFactor = 1.0f;
    std::deque<Sent> _sent;
    std::optional<ecs::Position> _predicted;
    /// The weapons of the ship, once armed(): one, or the two of its loadout.
    std::vector<Weapon> _weapons;
    std::uint8_t _slot = 0;
    /// The state of the weapon in hand once every input sent is applied, while there is a ship, and of the
    /// one put away with two weapons.
    std::optional<game::Armed> _armed;
    std::optional<game::Holster> _holster;
    /// The shots of the ticks after the last snapshot, by the tick they are fired at.
    std::map<std::uint32_t, PredictedShot> _shots;

    /// One tick of the ship and its weapons, from `position`, `armed` and `holster`: the shot it fires, if
    /// any.
    std::optional<PredictedShot> step(ecs::Position &position, std::optional<game::Armed> &armed,
                                      std::optional<game::Holster> &holster, game::PlayerInput input) const;
    /// The weapons as the game equips them: ready, magazines full, the first in hand.
    void equip();
    /// What is drawn on top of the prediction while a correction fades out.
    float _offsetX = 0.0f;
    float _offsetY = 0.0f;

public:
    explicit Prediction(game::Rules rules = {}) : _rules(rules) {}

    /// The ship of the player of `slot` has this weapon: its shots are predicted from now.
    /// @param projectileHalfWidth Half the width of the collider of the projectile of the weapon.
    void arm(game::WeaponDefinition weapon, float projectileHalfWidth, std::uint8_t slot);
    /// The ship of the player of `slot` has these weapons, the first in hand when it appears: one, or the two
    /// of its loadout.
    void arm(std::vector<Weapon> weapons, std::uint8_t slot);

    /// The ship moves at Rules::shipSpeed times this factor, as the game moves the ship of the hangar the
    /// player flies (game::statFactor() of its speed). 1 until set.
    void setSpeedFactor(float factor) { _speedFactor = factor; }

    /// An input sent for `tick`: the predicted ship moves at once.
    void apply(std::uint32_t tick, game::PlayerInput input);

    /// A snapshot of the game.
    /// @param ship Where the game had the ship of the local player, nothing if it has none.
    /// @param tick The tick of the snapshot: the inputs up to it are applied by the game.
    /// @param weapon The weapon of the ship at that tick, when the snapshot says it.
    void reconcile(std::optional<ecs::Position> ship, std::uint32_t tick,
                   std::optional<game::WeaponState> weapon = std::nullopt);

    /// Time goes by: the correction drawn fades out.
    void advance(double elapsed);

    /// Where to draw the ship, nothing while it has none.
    [[nodiscard]] std::optional<ecs::Position> position() const;

    /// Where to draw the ship between two inputs sent: `fraction` of the way to the next tick, moved with the
    /// input held now, which that tick will apply if it does not change. At the frame rate, without waiting
    /// for the next input: a key pressed shows at once.
    /// @param fraction 0 at the tick of the newest input sent, 1 at the next one.
    [[nodiscard]] std::optional<ecs::Position> position(double fraction, game::PlayerInput held) const;

    /// Where the ship will be once every input sent is applied, without the correction drawn.
    [[nodiscard]] std::optional<ecs::Position> predicted() const { return _predicted; }

    /// The shots predicted for the ticks after the last snapshot, by the tick they are fired at: where they
    /// start, their velocity and how they move. Empty without a weapon or a ship.
    [[nodiscard]] const std::map<std::uint32_t, PredictedShot> &shots() const { return _shots; }

    /// The weapon in hand once every input sent is applied, nothing without a weapon or a ship.
    [[nodiscard]] const std::optional<game::Armed> &weapon() const { return _armed; }
    /// The weapon put away once every input sent is applied, nothing without two weapons or a ship.
    [[nodiscard]] const std::optional<game::Holster> &holster() const { return _holster; }

    /// Inputs sent and not applied by the game yet, as far as the snapshots tell.
    [[nodiscard]] std::size_t pending() const { return _sent.size(); }
};

} // namespace rtype::client
