# Libraries shared by the programs: each subdirectory builds one library, with its public headers in
# include/rtype/<name>/ and its implementation in src/

# raylib is used by the client libraries only (render, input), never by the ECS nor by the server. Its headers
# are system headers: their warnings (raymath.h in C++) are not ours
CPMAddPackage(
    NAME raylib
    VERSION 6.0
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
    URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
    SYSTEM YES
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_SHARED_LIBS OFF"
)

add_subdirectory(audio)
add_subdirectory(core)
add_subdirectory(ecs)
add_subdirectory(engine)
add_subdirectory(input)
add_subdirectory(log)
add_subdirectory(physics)
add_subdirectory(render)
add_subdirectory(shaders)
