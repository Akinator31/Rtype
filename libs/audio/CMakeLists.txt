# Sound effects and music of the client. Built on raylib, which stays out of the public headers.
add_library(rtype_audio
    src/AudioDevice.cpp
    src/AudioPlayer.cpp
)
add_library(rtype::audio ALIAS rtype_audio)
target_include_directories(rtype_audio PUBLIC include)
# core is public: the public headers use its handles
target_link_libraries(rtype_audio PUBLIC rtype::core PRIVATE rtype_options rtype::log raylib)
