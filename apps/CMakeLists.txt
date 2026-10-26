# Executables: each subdirectory builds one program
add_subdirectory(client)
add_subdirectory(server)

# The configuration of the game (enemies, weapons..., ADR 0018), read by the client and the server from a config/
# folder next to their executable. Copied again on every build, after removing the old copy, so that a file
# deleted or renamed in the sources does not linger; installed with both programs.
add_custom_target(r-type_config
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:r-type_server>/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config $<TARGET_FILE_DIR:r-type_server>/config
    COMMENT "Copying the game configuration"
)
add_dependencies(r-type_client r-type_config)
add_dependencies(r-type_server r-type_config)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/config/ DESTINATION config COMPONENT client)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/config/ DESTINATION config COMPONENT server)
