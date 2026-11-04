add_library(rtype_render
    src/AntialiasPass.cpp
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
# The window drives the fullscreen of raylib through GLFW, which raylib builds in
target_include_directories(rtype_render SYSTEM PRIVATE ${raylib_SOURCE_DIR}/src/external/glfw/include)
target_link_libraries(rtype_render PUBLIC rtype::core PRIVATE rtype_options rtype::log rtype::shaders raylib)
