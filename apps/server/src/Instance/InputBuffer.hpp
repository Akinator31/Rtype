#pragma once

#include "rtype/game/PlayerInput.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>

namespace rtype::server
{

/// The inputs of one player, kept for the ticks they are stamped with (docs/protocol/game.md, section 9.2).
///
/// A client sends, once per tick, the inputs of its last ticks, each stamped with the tick at which the game
/// should apply it. The game keeps those of the ticks to come, and at each tick applies the one of that tick:
///
/// @code
/// buffer.receive(message.tick, message.inputs, simulation.tick()); // INPUT, newest first
/// simulation.setInput(slot, buffer.take(simulation.tick() + 1));   // before each step
/// @endcode
///
/// - An input for a tick already simulated, or more than MaxAhead ticks ahead, is ignored.
/// - The same tick received twice (the inputs are sent again in the next datagrams) keeps the first one.
/// - A tick without its input repeats the last one applied: a lost datagram continues the movement.
/// - After MaxSilence ticks without any input, the input is empty: a ship whose player lost the connection
///   stops.
/// - Each INPUT received gives its **lead**: the tick of its newest input minus the last tick simulated, even
///   when it is ignored. 1 is just in time, 0 or less too late, above MaxAhead too early. The client learns
///   the smallest one in its snapshots and keeps it just above 1 (docs/protocol/game.md, section 9.2).
///
/// Pure: no clock, no thread. Used by the game thread.
class InputBuffer
{
public:
    /// How far ahead an input can be stamped.
    static constexpr std::uint32_t MaxAhead = 60;
    /// Ticks without an input before the input becomes empty.
    static constexpr std::uint32_t MaxSilence = 30;

private:
    std::map<std::uint32_t, game::PlayerInput> _inputs;
    game::PlayerInput _last;
    std::uint32_t _lastApplied = 0;
    std::uint32_t _silence = 0;
    std::optional<std::int64_t> _lead;

public:
    /// Keeps the inputs of an INPUT message.
    /// @param newest The tick of the first input.
    /// @param inputs The input of `newest`, then of `newest - 1`, and so on.
    /// @param current The last tick simulated: the inputs of this tick and before are too late.
    void receive(std::uint32_t newest, std::span<const game::PlayerInput> inputs, std::uint32_t current);

    /// The input to apply at `tick`, the tick about to be simulated: its own, or the last one applied, or an
    /// empty one after a silence. Forgets the inputs of this tick and before.
    [[nodiscard]] game::PlayerInput take(std::uint32_t tick);

    /// The tick the last input applied was stamped with, 0 if none: the "last input" of the snapshots.
    [[nodiscard]] std::uint32_t lastApplied() const { return _lastApplied; }

    /// The smallest lead of the INPUT messages received since the previous call, nothing if none came.
    [[nodiscard]] std::optional<std::int64_t> takeLead() { return std::exchange(_lead, std::nullopt); }

    /// Inputs kept for the ticks to come.
    [[nodiscard]] std::size_t size() const { return _inputs.size(); }

    /// Forgets everything: the player left, or a new one takes the slot.
    void reset() { *this = InputBuffer{}; }
};

} // namespace rtype::server
