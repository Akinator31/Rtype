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
# raylib reads no JPEG by default, and some models (the pedestal of the hangar) have JPEG textures
target_compile_definitions(raylib PRIVATE SUPPORT_FILEFORMAT_JPG=1)
# The output at 48 kHz, the rate of the voice, instead of the one of the device: what it plays is the reference
# of the echo cancellation of the microphone (docs/architecture/voice.md#the-sound-processing), sample for
# sample. miniaudio converts to the device when it differs
target_compile_definitions(raylib PRIVATE AUDIO_DEVICE_SAMPLE_RATE=48000)

add_subdirectory(audio)
add_subdirectory(beat)
add_subdirectory(core)
add_subdirectory(database)
add_subdirectory(discord)
add_subdirectory(ecs)
add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(input)
add_subdirectory(lobby)
add_subdirectory(localization)
add_subdirectory(log)
add_subdirectory(network)
add_subdirectory(physics)
add_subdirectory(render)
add_subdirectory(scripting)
add_subdirectory(shaders)
add_subdirectory(voice)
add_subdirectory(xr)
