#pragma once

#include "Console/Console.hpp"
#include "Settings/SettingsTargets.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/game/Config/GameConfig.hpp"

namespace rtype::client
{

/// Loads the configuration of the game (ADR 0018), next to the executable. Unlike the server, the client
/// starts with a configuration that has errors: it goes without it (an offline game has no enemies nor
/// weapons), and says why in the console and in the logs.
[[nodiscard]] game::GameConfig readGameConfig(Console &console);

/// Reads the settings of the player, in its data directory: the servers it trusts, for now. Settings that
/// cannot be read are logged, and the client starts without them.
void readSettings(engine::Settings &settings);

/// Applies the choices of the player read by readSettings(): the keys of the game, the window, the language,
/// the volumes, the colour filter and the colours of the auras. Call it once the translations are loaded.
void applySavedSettings(engine::Engine &engine, const SettingsTargets &targets);

} // namespace rtype::client
