#pragma once

#include "Console/Console.hpp"
#include "rtype/engine/Engine/Engine.hpp"

namespace rtype::client
{

/// "scene <name>": closes every scene and opens this one.
/// @param engine It must outlive the console.
void registerSceneCommand(Console &console, engine::Engine &engine);

} // namespace rtype::client
