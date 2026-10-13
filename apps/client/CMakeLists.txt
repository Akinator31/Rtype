add_executable(r-type_client
    src/main.cpp
    src/Commands/Commands.cpp
    src/Commands/QuitCommand.cpp
    src/Commands/SceneCommand.cpp
    src/Console/Console.cpp
    src/Console/ConsoleScene.cpp
    src/Console/ConsoleSystem.cpp
    src/MainMenu/MainMenu.cpp
    src/MainMenu/Planet.cpp
)
target_include_directories(r-type_client PRIVATE src)
target_link_libraries(r-type_client PRIVATE
    rtype_options rtype::audio rtype::ecs rtype::engine rtype::input rtype::log rtype::physics rtype::render)

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
