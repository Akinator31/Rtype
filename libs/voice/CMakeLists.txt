# The voice chat of an online game (docs/architecture/voice.md, ADR 0032), in two targets:
#   rtype::voice        the interface the client knows (rtype/voice/VoiceChannel.hpp) and what carries the voice
#                       of each player (rtype/voice/Backend.hpp), and the format of the voice
#                       (rtype/voice/Format.hpp). Header only: the game protocol and the server use it without Opus
#   rtype::voice_codec  the encoder and the decoder of the voice (rtype/voice/Codec.hpp), built on libopus, a
#                       private dependency. For the client
add_library(rtype_voice INTERFACE)
add_library(rtype::voice ALIAS rtype_voice)
target_include_directories(rtype_voice INTERFACE include)

# The release archive of Xiph, the only place 1.6.1 is published (not in the releases of GitHub). It has the
# version of the library in package_version, which its CMake reads without git: the archive GitHub generates
# from a tag does not
CPMAddPackage(
    NAME opus
    VERSION 1.6.1
    URL https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz
    URL_HASH SHA256=6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "OPUS_BUILD_SHARED_LIBRARY OFF"
        "OPUS_BUILD_PROGRAMS OFF"
        "OPUS_BUILD_TESTING OFF"
        "OPUS_INSTALL_PKG_CONFIG_MODULE OFF"
        "OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF"
)

add_library(rtype_voice_codec STATIC
    src/Codec.cpp
)
add_library(rtype::voice_codec ALIAS rtype_voice_codec)
target_link_libraries(rtype_voice_codec PUBLIC rtype::voice PRIVATE rtype_options rtype::log Opus::opus)

# Its license goes with the client that links it: the BSD license of Opus asks for its notice in a binary
# distribution
install(FILES ${opus_SOURCE_DIR}/COPYING DESTINATION licenses RENAME opus.txt COMPONENT client)
