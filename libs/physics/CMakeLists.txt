# Physics on the ECS: movement, fixed time step and collisions, as components and systems. Depends on the ECS
# only, never on raylib nor on the engine, so that the server can use it.
add_library(rtype_physics
    src/System/MovementSystem.cpp
)
add_library(rtype::physics ALIAS rtype_physics)
target_include_directories(rtype_physics PUBLIC include)
target_link_libraries(rtype_physics PUBLIC rtype::ecs PRIVATE rtype_options)
