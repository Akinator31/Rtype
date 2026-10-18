# The code of the server is a library of its own, so that the tests can use it (ADR 0013):
#   Common/    what both modes share: the command line, the lifecycle, the secret of the instances
#   Master/    the master: the lobby, and the instances it starts
#   Instance/  an instance: one game
add_library(r-type_server_core STATIC
    src/Server.cpp
    src/Common/Lifecycle.cpp
    src/Common/Options.cpp
    src/Common/Secret.cpp
    src/Instance/Instance.cpp
    src/Master/Master.cpp
)
target_include_directories(r-type_server_core PUBLIC src)
target_link_libraries(r-type_server_core PUBLIC rtype::log rtype::network PRIVATE rtype_options)
target_compile_definitions(r-type_server_core PRIVATE
    RTYPE_VERSION="${PROJECT_VERSION}"
    # A game without a master, with a fixed test key: never in the released binaries (ADR 0013)
    $<$<CONFIG:Debug>:RTYPE_STANDALONE_INSTANCE>
)

add_executable(r-type_server
    src/main.cpp
)
target_link_libraries(r-type_server PRIVATE rtype_options r-type_server_core rtype::ecs rtype::log rtype::physics)

install(TARGETS r-type_server RUNTIME DESTINATION . COMPONENT server)
