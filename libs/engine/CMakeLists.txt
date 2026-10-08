# Game engine built on the ECS. Like the ECS, it must not depend on raylib.
add_library(rtype_engine
    src/Engine/Engine.cpp
    src/Event/Event.cpp
)
add_library(rtype::engine ALIAS rtype_engine)
target_include_directories(rtype_engine PUBLIC include)
target_link_libraries(rtype_engine PUBLIC rtype::ecs PRIVATE rtype_options)
