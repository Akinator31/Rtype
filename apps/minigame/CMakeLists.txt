# Orbit Breaker: a brick breaker built on the engine alone (libs/), with no code of the R-Type client, to show
# that the engine runs another game. The rules are in a library, so that the tests can use them.
add_library(orbit_breaker_core STATIC
    src/Arena.cpp
    src/BallSystem.cpp
    src/Bounce.cpp
    src/Effects.cpp
    src/Hud.cpp
    src/Levels.cpp
    src/PaddleSystem.cpp
    src/RulesSystem.cpp
    src/Scenes.cpp
    src/Systems.cpp
    src/Visuals.cpp
)
target_include_directories(orbit_breaker_core PUBLIC src)
target_link_libraries(orbit_breaker_core
    PUBLIC rtype::audio rtype::ecs rtype::engine rtype::input rtype::log rtype::physics rtype::render
    PRIVATE rtype_options)

add_executable(orbit_breaker src/main.cpp)
target_link_libraries(orbit_breaker PRIVATE rtype_options orbit_breaker_core)

# The assets of the game, taken from those of the client: each one as "file of the client=file of the game"
set(ORBIT_ASSETS
    fonts/Audiowide-Regular.ttf=fonts/Audiowide-Regular.ttf
    fonts/Audiowide-LICENSE.txt=fonts/Audiowide-LICENSE.txt
    fonts/Exo2-SemiBold.ttf=fonts/Exo2-SemiBold.ttf
    fonts/Exo2-LICENSE.txt=fonts/Exo2-LICENSE.txt
    fonts/Orbitron-Medium.ttf=fonts/Orbitron-Medium.ttf
    fonts/Orbitron-LICENSE.txt=fonts/Orbitron-LICENSE.txt
    sounds/fx/button/hover.wav=sounds/hover.wav
    sounds/fx/button/click.wav=sounds/click.wav
    sounds/fx/collision.wav=sounds/collision.wav
    sounds/fx/blaster/low_blaster.wav=sounds/brick.wav
    sounds/fx/destruction.wav=sounds/lost.wav
    images/fx/glow_soft.png=images/glow.png
    images/fx/spark.png=images/spark.png
    images/fx/shockwave_ring.png=images/shockwave.png
    images/backgrounds/space_bg_0_deep.png=images/sky_deep.png
    images/backgrounds/space_bg_1_nebula.png=images/sky_nebula.png
    images/backgrounds/space_bg_2_stars.png=images/sky_stars.png
    images/backgrounds/space_bg_3_near.png=images/sky_near.png
    images/backgrounds/space_planet_ringed.png=images/planet.png
)
# Copied next to the executable at each build, and installed with it
set(ORBIT_COPY_COMMANDS)
foreach(asset IN LISTS ORBIT_ASSETS)
    string(REPLACE "=" ";" asset "${asset}")
    list(GET asset 0 source)
    list(GET asset 1 target)
    set(source ${PROJECT_SOURCE_DIR}/apps/client/assets/${source})
    get_filename_component(directory ${target} DIRECTORY)
    get_filename_component(name ${target} NAME)
    list(APPEND ORBIT_COPY_COMMANDS
         COMMAND ${CMAKE_COMMAND} -E copy ${source} $<TARGET_FILE_DIR:orbit_breaker>/orbit_breaker_assets/${target})
    install(FILES ${source} DESTINATION orbit_breaker_assets/${directory} RENAME ${name} COMPONENT minigame)
endforeach()
add_custom_target(orbit_breaker_assets
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:orbit_breaker>/orbit_breaker_assets/fonts
            $<TARGET_FILE_DIR:orbit_breaker>/orbit_breaker_assets/sounds
            $<TARGET_FILE_DIR:orbit_breaker>/orbit_breaker_assets/images
    ${ORBIT_COPY_COMMANDS}
    COMMENT "Copying the assets of Orbit Breaker"
)
add_dependencies(orbit_breaker orbit_breaker_assets)

install(TARGETS orbit_breaker RUNTIME DESTINATION . COMPONENT minigame)
