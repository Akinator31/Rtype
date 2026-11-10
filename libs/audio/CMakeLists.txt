# Sound effects, music, the voices of the other players and what the output plays (the reference of the echo
# cancellation), for the client. Built on raylib, which stays out of the public headers.
add_library(rtype_audio
    src/AudioDevice.cpp
    src/AudioPlayer.cpp
    src/OutputLoopback.cpp
    src/VoiceStreams.cpp
)
add_library(rtype::audio ALIAS rtype_audio)
target_include_directories(rtype_audio PUBLIC include)
# core and voice are public: the public headers use the handles of core and the devices of voice (header only)
target_link_libraries(rtype_audio PUBLIC rtype::core rtype::voice PRIVATE rtype_options rtype::log raylib)
