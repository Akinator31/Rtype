# Physics on the ECS: movement, fixed time step and collisions, as components and systems. Depends on the ECS
# only, never on raylib nor on the engine, so that the server can use it.
# Header-only until the first system: it becomes a compiled library (src/) with its first source file.
add_library(rtype_physics INTERFACE)
add_library(rtype::physics ALIAS rtype_physics)
target_include_directories(rtype_physics INTERFACE include)
target_link_libraries(rtype_physics INTERFACE rtype::ecs)
