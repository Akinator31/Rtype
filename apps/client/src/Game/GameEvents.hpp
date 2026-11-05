#pragma once

namespace rtype::client
{

/// Events of the engine triggered by the game scene, for whatever listens to the game: the quests. They carry
/// nothing: a listener that needs more reads it elsewhere.
/// A Bydo shot down by the local player.
inline constexpr const char *BydoKilledEvent = "game:bydo_killed";
/// A boss shot down, by anyone.
inline constexpr const char *BossKilledEvent = "game:boss_killed";
/// The ship of the local player shot down.
inline constexpr const char *PlayerKilledEvent = "game:player_killed";
/// A wave of Bydos started.
inline constexpr const char *WaveStartedEvent = "game:wave_started";

} // namespace rtype::client
