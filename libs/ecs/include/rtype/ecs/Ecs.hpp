#pragma once

/// @file
/// Entry point of the Entity Component System, shared by the client and the server.

#include <string>

/// Entity Component System: entities, components and the systems that update them.
///
/// The ECS holds the game state on both sides. It does not depend on raylib nor on the network, so that the
/// server and the tests can use it without a window.
namespace rtype::ecs
{

/// Example function showing how the library is built and used. To be removed once the ECS has real code.
/// @return A greeting from the ECS library.
[[nodiscard]] std::string helloWorld();

} // namespace rtype::ecs
