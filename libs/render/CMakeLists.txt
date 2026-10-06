add_library(rtype_render
    src/Paths.cpp
    src/Renderer.cpp
    src/ResourceManager.cpp
    src/Viewport.cpp
    src/Window.cpp
)
add_library(rtype::render ALIAS rtype_render)
target_include_directories(rtype_render PUBLIC include)

target_link_libraries(rtype_render PRIVATE rtype_options raylib)
