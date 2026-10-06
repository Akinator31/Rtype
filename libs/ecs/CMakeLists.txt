# Entity Component System of the engine, shared by the client and the server.
# It must not depend on raylib nor on the network: the server and the tests use it without a window.
add_library(rtype_ecs
    src/Ecs.cpp
)
add_library(rtype::ecs ALIAS rtype_ecs)
target_include_directories(rtype_ecs PUBLIC include)
target_link_libraries(rtype_ecs PRIVATE rtype_options)
