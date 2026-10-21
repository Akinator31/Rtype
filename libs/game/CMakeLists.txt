# The rules of the R-Type: players, shots, Bydos, waves, as components and systems on the ECS, run one tick at a
# time by a Simulation. Shared by the server instances and by the client offline (ADR 0015). The Simulation
# depends on the ECS, the physics and the log only: no raylib, no network. The messages of the game protocol
# (Messages.hpp, RemoteWorld.hpp) are written with the serialization of libs/network, a private dependency.
add_library(rtype_game
    src/Enemy/EnemyRegistry.cpp
    src/Entities.cpp
    src/Messages.cpp
    src/PlayerInput.cpp
    src/Projectile/ProjectileBehavior.cpp
    src/Projectile/ProjectileRegistry.cpp
    src/RemoteWorld.cpp
    src/Ship/ShipRegistry.cpp
    src/ShipMovement.cpp
    src/Simulation.cpp
    src/System/AreaSystem.cpp
    src/System/CombatSystem.cpp
    src/System/PlayerControlSystem.cpp
    src/System/ProjectileSystem.cpp
    src/System/WeaponSystem.cpp
    src/Wave/WaveRegistry.cpp
    src/Weapon/WeaponRegistry.cpp
)
add_library(rtype::game ALIAS rtype_game)
target_include_directories(rtype_game PUBLIC include)
target_link_libraries(rtype_game PUBLIC rtype::ecs rtype::physics PRIVATE rtype_options rtype::log rtype::network)
