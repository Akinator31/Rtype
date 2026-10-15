#pragma once

/// @file
/// Everything of the physics in one include, like rtype/ecs/Ecs.hpp for the ECS.
///
/// The physics is kinematic and knows no game: it provides the components (velocity, collider, collision
/// layers) and the systems working on them (movement, collisions), and each game gives them its values, names
/// its own collision layers and reacts to the collisions with its own systems.
///
/// - Component/: the data attached to entities.
/// - System/: the systems working on them, one per file.

#include "rtype/physics/Component/Motion.hpp"
#include "rtype/physics/Component/Collider.hpp"
#include "rtype/physics/Component/CollisionFilter.hpp"
#include "rtype/physics/Component/Bounds.hpp"
#include "rtype/physics/Component/Static.hpp"
#include "rtype/physics/Component/FastMoving.hpp"
#include "rtype/physics/Geometry/Intersections.hpp"
#include "rtype/physics/Geometry/BroadPhase.hpp"
#include "rtype/physics/System/MovementSystem.hpp"
#include "rtype/physics/System/BoundsSystem.hpp"
#include "rtype/physics/System/CollisionSystem.hpp"
#include "rtype/physics/System/PhysicsStep.hpp"
