# The code of the server is a library of its own, so that the tests can use it (ADR 0013):
#   Common/    what both modes share: the command line, the lifecycle, the secret of the instances
#   Master/    the master: the lobby, and the instances it starts
#   Instance/  an instance: one game

# reproc starts the instances and stops them, on Linux and Windows (ADR 0013). A private dependency, behind
# Master/Process.hpp
CPMAddPackage(
    NAME reproc
    VERSION 14.2.8
    URL https://github.com/DaanDeMeyer/reproc/archive/refs/tags/14.2.8.tar.gz
    URL_HASH SHA256=27c3b452bfc419a2deda23969aa10c77909c4ff9e71c549eb65d09ae6aa7aa32
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "REPROC++ OFF"
        "REPROC_INSTALL OFF"
        "REPROC_TEST OFF"
        "REPROC_EXAMPLES OFF"
)

add_library(r-type_server_core STATIC
    src/Server.cpp
    src/Common/GameConfig.cpp
    src/Common/InstanceMessages.cpp
    src/Common/Lifecycle.cpp
    src/Common/Options.cpp
    src/Common/Secret.cpp
    src/Instance/GameHost.cpp
    src/Instance/GameThread.cpp
    src/Instance/InputBuffer.cpp
    src/Instance/Instance.cpp
    src/Instance/MasterLink.cpp
    src/Master/Games.cpp
    src/Master/Lobby.cpp
    src/Master/Master.cpp
    src/Master/Process.cpp
    src/Master/ServerDatabase.cpp
    src/Master/ServerKey.cpp
)
target_include_directories(r-type_server_core PUBLIC src)
target_link_libraries(r-type_server_core PUBLIC rtype::database rtype::game rtype::lobby rtype::log rtype::network PRIVATE rtype_options reproc)
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
