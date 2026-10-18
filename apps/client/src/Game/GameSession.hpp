#pragma once

#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>
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

    /// Offline, stops the game while the pause menu is open; online, the game goes on without the player.
    virtual void setPaused(bool paused) = 0;

    /// The game is over: the scene can go back to the menu.
    [[nodiscard]] virtual bool isOver() const = 0;
};

} // namespace rtype::client
