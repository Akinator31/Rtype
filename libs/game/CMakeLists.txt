# The rules of the R-Type: players, shots, Bydos, waves, as components and systems on the ECS, run one tick at a
# time by a Simulation. Shared by the server instances and by the client offline (ADR 0015). Depends on the ECS,
# the physics and the log only: no raylib, no network.
add_library(rtype_game
    src/Entities.cpp
    src/PlayerInput.cpp
    src/Simulation.cpp
    src/System/AreaSystem.cpp
    src/System/PlayerControlSystem.cpp
)
add_library(rtype::game ALIAS rtype_game)
target_include_directories(rtype_game PUBLIC include)
target_link_libraries(rtype_game PUBLIC rtype::ecs rtype::physics PRIVATE rtype_options rtype::log)
