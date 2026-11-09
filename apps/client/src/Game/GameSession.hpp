#pragma once

#include "Game/EntityLook.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/physics/Component/Collider.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace rtype::client
{

/// A game, as the game scene sees it: inputs go in, snapshots of the world come out (ADR 0015). Offline, a
/// LocalSession runs the Simulation in the client; online, a RemoteSession talks to the instance. The scene
/// does not know which one it plays.
///
/// @code
/// session.setInput(input);    // the controls of the local player, once per frame
/// session.update(elapsed);    // time goes by
/// for (const rtype::game::Snapshot &snapshot : session.takeSnapshots()) { /* draw the world */ }
/// @endcode
class GameSession
{
public:
    virtual ~GameSession() = default;

    /// The slot of the local player, 0 to 3: the colour of its ship.
    [[nodiscard]] virtual std::uint8_t localSlot() const = 0;

    /// The input of the local player, held until the next call.
    virtual void setInput(game::PlayerInput input) = 0;

    /// Time goes by: offline, the simulation steps once per tick elapsed; online, the network is handled.
    /// @param elapsed Time since the previous update, in seconds.
    virtual void update(float elapsed) = 0;

    /// The snapshots of the world since the previous call, oldest first: usually one per frame, sometimes
    /// none or several. Each one holds the whole world at its tick.
    [[nodiscard]] virtual std::vector<game::Snapshot> takeSnapshots() = 0;

    /// Where to draw the entities at this frame, between the last tick and the next, after the snapshots:
    /// positions only, no events. Nothing when the snapshots are already drawn at the frame rate (online, the
    /// interpolation), or when nothing moves (paused).
    [[nodiscard]] virtual std::optional<game::Snapshot> between() const { return std::nullopt; }

    /// Offline, stops the game while the pause menu is open; online, the game goes on without the player.
    virtual void setPaused(bool paused) = 0;

    /// The game is over: the scene can go back to the menu.
    [[nodiscard]] virtual bool isOver() const = 0;

    /// Online, the waves follow each other without end and earn experience (game::Rules::endless): no final
    /// boss, no victory.
    [[nodiscard]] virtual bool isEndless() const { return false; }

    /// The game is played on a server (RemoteSession), not in the client.
    [[nodiscard]] virtual bool isOnline() const { return false; }

    /// The waves of the game, when it has an end: none online, where they follow each other without end.
    [[nodiscard]] virtual std::optional<std::uint16_t> waveCount() const { return std::nullopt; }

    /// The round-trip time to the game, once measured; none offline, where there is no network.
    [[nodiscard]] virtual std::optional<std::chrono::milliseconds> ping() const = 0;

    /// The hitbox of an entity of the game, for the debug overlay (F3, the "hitboxes" command), none if it
    /// has none. By default the box it is drawn as (lookOf()): the snapshots do not carry the hitboxes.
    [[nodiscard]] virtual std::optional<physics::Collider> findCollider(const game::EntityState &state)
    {
        const engine::Box box = lookOf(state).box;
        return physics::Collider{physics::Aabb{{}, {box.width / 2.0f, box.height / 2.0f, 0.0f}}, {}, false};
    }
};

} // namespace rtype::client
