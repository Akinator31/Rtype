CPMAddPackage(
    NAME raylib
    VERSION 6.0
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
    URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_SHARED_LIBS OFF"
)

add_executable(r-type_client
    src/main.cpp
)
target_link_libraries(r-type_client PRIVATE rtype_options raylib)

install(TARGETS r-type_client RUNTIME DESTINATION . COMPONENT client)
