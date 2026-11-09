# Executables: each subdirectory builds one program
add_subdirectory(client)
add_subdirectory(minigame)
add_subdirectory(server)

# The configuration of the game (enemies, weapons..., ADR 0018), read by the client and the server from a config/
# folder next to their executable. Copied again on every build, after removing the old copy, so that a file
# deleted or renamed in the sources does not linger; installed with both programs.
add_custom_target(r-type_config
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:r-type_server>/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config $<TARGET_FILE_DIR:r-type_server>/config
    COMMENT "Copying the game configuration"
)
add_dependencies(r-type_server r-type_config)
# No client where there is no Discord Social SDK (libs/discord)
if(TARGET r-type_client)
  add_dependencies(r-type_client r-type_config)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/config/ DESTINATION config COMPONENT client)
endif()
install(DIRECTORY ${PROJECT_SOURCE_DIR}/config/ DESTINATION config COMPONENT server)
# The server analyses the music of the waves that follow one (placement "music"): it needs those files, not the
# other assets of the client
install(FILES ${PROJECT_SOURCE_DIR}/apps/client/assets/sounds/christmas_rock.mp3
              ${PROJECT_SOURCE_DIR}/apps/client/assets/sounds/cyberpunk_main.wav
              ${PROJECT_SOURCE_DIR}/apps/client/assets/sounds/cyberpunk_short.wav
              ${PROJECT_SOURCE_DIR}/apps/client/assets/sounds/drums_style.mp3
              ${PROJECT_SOURCE_DIR}/apps/client/assets/sounds/epic_anthem.mp3
              ${PROJECT_SOURCE_DIR}/apps/client/assets/sounds/punk.wav
              ${PROJECT_SOURCE_DIR}/apps/client/assets/sounds/space_voyager.wav
              ${PROJECT_SOURCE_DIR}/apps/client/assets/sounds/the_sports_music.mp3
        DESTINATION assets/sounds COMPONENT server)
