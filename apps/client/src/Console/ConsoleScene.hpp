#pragma once

#include "Console/Console.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"

namespace rtype::client
{

/// Name of the console scene: an overlay over the top of the screen, opened and closed with F8 (see
/// ConsoleSystem). Escape closes it too.
inline constexpr const char *ConsoleScene = "console";

/// Fills the console scene: a dark band over the top half of the screen, with the newest lines of the console
/// and, below them, the command line (a TextFieldType::Console field) where commands are typed. Enter runs
/// the command and empties the field. The colours come from InterfaceStyle::console.
/// @param console, metrics, style They must outlive the scene.
void buildConsole(engine::Engine &engine, engine::SceneId scene, Console &console,
                  const engine::Metrics &metrics, const engine::InterfaceStyle &style);

} // namespace rtype::client
