# Physics on the ECS: movement, fixed time step and collisions, as components and systems. Depends on the ECS
# only, never on raylib nor on the engine, so that the server can use it.
add_library(rtype_physics
    src/CollisionFilter.cpp
    src/Geometry/BroadPhase.cpp
    src/Geometry/Intersections.cpp
    src/System/MovementSystem.cpp
    src/System/PhysicsStep.cpp
)
add_library(rtype::physics ALIAS rtype_physics)
target_include_directories(rtype_physics PUBLIC include)
target_link_libraries(rtype_physics PUBLIC rtype::ecs PRIVATE rtype_options)
