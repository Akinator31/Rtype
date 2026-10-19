# The code of the client is a library of its own, so that the tests can use it, like the server's:
#   Commands/, Console/  the console and its commands
#   Game/                a game: its session, offline or online, and its scenes (ADR 0015)
#   MainMenu/            the home screen and its submenus
#   Online/              playing online: the lobby of a server, and its scenes
add_library(r-type_client_core STATIC
    src/Commands/Commands.cpp
    src/Commands/QuitCommand.cpp
    src/Commands/SceneCommand.cpp
    src/Commands/ToastCommand.cpp
    src/Console/Console.cpp
    src/Console/ConsoleScene.cpp
    src/Console/ConsoleSystem.cpp
    src/Game/GameScenes.cpp
    src/Game/GameSessionSystem.cpp
    src/Game/Interpolation.cpp
    src/Game/LocalSession.cpp
    src/Game/Prediction.cpp
    src/Game/RemoteSession.cpp
    src/MainMenu/FlightSystem.cpp
    src/MainMenu/IntroSystem.cpp
    src/MainMenu/MainMenu.cpp
    src/MainMenu/Planet.cpp
    src/MainMenu/Ships.cpp
    src/MainMenu/Station.cpp
    src/MainMenu/Submenus.cpp
    src/Online/LobbyClient.cpp
    src/Online/OnlineScenes.cpp
)
target_include_directories(r-type_client_core PUBLIC src)
target_link_libraries(r-type_client_core
    PUBLIC rtype::audio rtype::ecs rtype::engine rtype::game rtype::input rtype::lobby rtype::log rtype::network
           rtype::physics rtype::render
    PRIVATE rtype_options)

add_executable(r-type_client
    src/main.cpp
)
target_link_libraries(r-type_client PRIVATE rtype_options r-type_client_core)

# The client finds its assets next to its executable, both in the build tree and once installed.
# A custom target runs on every build, so that a modified asset is copied even if the code did not change.
add_custom_target(r-type_client_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:r-type_client>/assets
    COMMENT "Copying client assets"
)
add_dependencies(r-type_client r-type_client_assets)

install(TARGETS r-type_client RUNTIME DESTINATION . COMPONENT client)
install(DIRECTORY assets/ DESTINATION assets COMPONENT client PATTERN ".gitkeep" EXCLUDE)
