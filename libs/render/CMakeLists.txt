add_library(rtype_render
    src/ColorFilter.cpp
    src/EnergyShader.cpp
    src/FilterShader.cpp
    src/FontCharacters.cpp
    src/GlowPass.cpp
    src/LightingShader.cpp
    src/Paths.cpp
    src/RaylibLog.cpp
    src/Renderer.cpp
    src/ResourceManager.cpp
    src/Viewport.cpp
    src/Window.cpp
)
add_library(rtype::render ALIAS rtype_render)
target_include_directories(rtype_render PUBLIC include)

# core is public: the public headers use its handles
target_link_libraries(rtype_render PUBLIC rtype::core PRIVATE rtype_options rtype::log rtype::shaders raylib)
