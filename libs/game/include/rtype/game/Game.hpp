#pragma once

/// @file
/// Everything of the game in one include, like rtype/physics/Physics.hpp for the physics.
///
/// - Simulation.hpp: one game, stepped one tick at a time. What the server and the client use.
/// - PlayerInput.hpp, Snapshot.hpp: what goes in (the controls of a player) and out (the world).
/// - Messages.hpp, RemoteWorld.hpp: the messages of the game protocol, and the world a client rebuilds.
/// - Standalone.hpp: the public sessions of a game without a master, for development.
/// - Rules.hpp, Constants.hpp: the numbers of the gameplay and of the game area.
/// - Component/, System/, Entities.hpp, GameContext.hpp: the rules themselves, to add gameplay.

#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Constants.hpp"
#include "rtype/game/Entities.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/RemoteWorld.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Simulation.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/game/Standalone.hpp"
#include "rtype/game/System/AreaSystem.hpp"
#include "rtype/game/System/CombatSystem.hpp"
#include "rtype/game/System/EnemySystem.hpp"
#include "rtype/game/System/PlayerControlSystem.hpp"
