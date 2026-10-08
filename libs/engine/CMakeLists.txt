# Client side of the engine: components and systems connecting the ECS to the rendering, the input and the
# interface (menus as entities, see ADR 0007).
add_library(rtype_engine
    src/InterfaceSystem.cpp
    src/Layout.cpp
    src/MenuBuilder.cpp
    src/Metrics.cpp
    src/Navigation.cpp
    src/SceneRenderer.cpp
    src/Tween.cpp
    src/Widgets.cpp
)
add_library(rtype::engine ALIAS rtype_engine)
target_include_directories(rtype_engine PUBLIC include)
# Public: the public headers use their types
target_link_libraries(rtype_engine PUBLIC rtype::ecs rtype::input rtype::render PRIVATE rtype_options)
