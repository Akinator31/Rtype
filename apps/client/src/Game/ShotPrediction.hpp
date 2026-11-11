#pragma once

#include "rtype/game/Projectile/ProjectileCategory.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/game/Weapon/WeaponFiring.hpp"

#include <cstdint>
#include <map>
#include <optional>

namespace rtype::client
{

/// A shot the Prediction gives: where it starts, its velocity, and the mechanic of the weapon that fires it,
/// which moves it.
struct PredictedShot {
    game::ShotLaunch launch;
    game::ProjectileCategory mechanic = game::ProjectileCategory::Bullet;
    /// The projectile of the weapon, by its index in the configuration (game::EntityState::ship): the model
    /// it is drawn with.
    std::uint8_t projectile = game::NoShip;
};

/// Draws the shots of the player online from the moment it fires, from the nose of its ship, then in the time
/// of the world they hit (docs/architecture/client-online.md).
///
/// The ship of the player is predicted: drawn at the tick of its newest input, ahead of the game. The rest of
/// the world is drawn in the past, between two snapshots. Its own shots, drawn with the rest, would trail
/// behind the ship by a round trip and the delay of the interpolation, and appear that late. Drawn in the
/// time of the ship all along, they would meet the Bydos that far ahead of them: through a Bydo it misses,
/// short of one it hits.
///
/// @code
/// shots.arm(rules);                                          // the ship of the player has a weapon
/// shots.received(entity, part.tick);                         // each shot of the player in a SNAPSHOT
/// shots.destroyed(id, tick);                                 // ENTITIES_DESTROYED of one of them
/// shots.predict(prediction.shots(), world.tick());           // each frame
/// shots.apply(snapshot, slot, drawnTick, inputTick);         // then, on the world drawn
/// @endcode
///
/// - **Predicted**: each shot the Prediction gives is drawn at once, from the tick it is fired at, along the
///   path of the game (game::moveShot()), under an id of its own (from FirstGhostId). One the prediction no
///   longer gives (the snapshots said the game did not fire then), or that the game did not send within Grace
///   ticks, is no longer drawn.
/// - **Matched**: a shot of the game, fired at the tick its age tells, takes the place of the predicted one
/// of
///   that tick: drawn under its id, along the same path, so that the drawing goes on without a jump and
///   without a new entity.
/// - **Retimed**: a shot of the game that was not predicted is moved on along its velocity, from the drawn
///   tick to its own time.
/// - **Its time** (timeOf()): fired, a shot is drawn in the time of the ship, from its nose. As it flies, its
///   time glides back to the drawn tick, the time of the Bydos it may hit: it is drawn where the game has it
///   when it meets them, and what it seems to hit is what it hits. It seems to slow down a little while it
///   glides, never below half its speed.
/// - **Its end**: once its destruction is known and its time is past it, the shot is drawn one last frame
///   where it was destroyed, for the impact, then no more.
///
/// Pure: no clock, no network.
class ShotPrediction
{
public:
    /// The ids of the shots drawn before the game has them: far above those of the game, which count from 1.
    static constexpr std::uint32_t FirstGhostId = 0xF0000000;
    /// Ticks after its tick before a predicted shot that no snapshot brought is no longer drawn: a snapshot
    /// every 2 ticks, a part lost.
    static constexpr std::uint32_t Grace = 6;
    /// Ticks of flight over which the time of a shot glides from the ship to the drawn tick, at least.
    static constexpr double MinGlide = 15.0;
    /// The glide lasts this many times the gap between the ship and the drawn tick, at least: the shot never
    /// seems to fly at less than half its speed.
    static constexpr double GlidePerTick = 3.0;

private:
    struct End {
        std::uint32_t tick = 0;
        /// Drawn once where it ended: it is no longer drawn.
        bool shown = false;
    };
    /// A shot predicted, by the tick it is fired at.
    struct Ghost {
        std::uint32_t id = 0;
        game::ShotLaunch launch;
        game::ProjectileCategory mechanic = game::ProjectileCategory::Bullet;
        std::uint8_t projectile = game::NoShip;
        /// The id of the shot of the game it became.
        std::optional<std::uint32_t> server;
        /// The shot of the game was drawn: once it is no longer, this one is done.
        bool seen = false;
    };
    /// The rules the shots move by, once armed().
    std::optional<game::Rules> _rules;
    std::map<std::uint32_t, Ghost> _ghosts;
    /// The id of the predicted shot each shot of the game took the place of.
    std::map<std::uint32_t, std::uint32_t> _aliases;
    /// The shots of the player destroyed, by the id of the game, until the drawn tick passes their
    /// destruction.
    std::map<std::uint32_t, End> _ends;
    std::uint32_t _nextId = FirstGhostId;

    /// Where a predicted shot is at a tick, and its velocity then, as the game moves it.
    [[nodiscard]] game::EntityState place(const Ghost &ghost, std::uint32_t fired, double tick,
                                          std::uint8_t slot) const;
    /// The tick to draw a shot of the game at, from its time (timeOf()): its end, once. Nothing once past
    /// it.
    [[nodiscard]] std::optional<double> targetOf(std::uint32_t id, double time);
    void forget(std::map<std::uint32_t, Ghost>::iterator ghost);

public:
    /// Whether an entity is a shot of the player of this slot.
    [[nodiscard]] static bool isShotOf(const game::EntityState &entity, std::uint8_t slot);

    /// The tick a shot fired at `fired` is drawn at: the tick of the ship when it is fired, gliding to the
    /// drawn tick as it flies (a smoothstep over max(MinGlide, GlidePerTick × the gap) ticks), then the drawn
    /// tick. The tick of the ship if it is not ahead of the drawn tick.
    [[nodiscard]] static double timeOf(double fired, double drawn, double ship);

    /// The ship of the player has a weapon: its shots are drawn as the game moves them, each with the
    /// mechanic of the weapon that fired it.
    void arm(game::Rules rules);

    /// A shot of the player in a snapshot of `tick`: it takes the place of the shot predicted at the tick it
    /// was fired at, if there is one.
    void received(const game::EntityState &shot, std::uint32_t tick);

    /// A shot of the player destroyed at a tick (ENTITIES_DESTROYED), by the id of the game.
    void destroyed(std::uint32_t id, std::uint32_t tick);

    /// The shots the prediction gives, for the ticks after the latest snapshot.
    /// @param confirmed The tick of the latest snapshot: up to it, the game says what was fired.
    void predict(const std::map<std::uint32_t, PredictedShot> &shots, std::uint32_t confirmed);

    /// Draws the shots of the player in the world drawn, the predicted ones too, each in its time
    /// (timeOf()), and leaves out those past their end.
    /// @param slot The slot of the player.
    /// @param drawn The tick the world is drawn at (Interpolation::drawnTick()).
    /// @param ship The tick its ship is drawn at: the one of its newest input, and the fraction of the way to
    ///   the next one.
    void apply(game::Snapshot &snapshot, std::uint8_t slot, double drawn, double ship);

    /// The id a shot of the game is drawn with: the one of the predicted shot it took the place of, else its
    /// own. For the destructions given to the drawing.
    [[nodiscard]] std::uint32_t idOf(std::uint32_t id) const;

    /// Shots predicted and not done yet.
    [[nodiscard]] std::size_t predicted() const { return _ghosts.size(); }
    /// Destructions remembered.
    [[nodiscard]] std::size_t size() const { return _ends.size(); }
};

} // namespace rtype::client
