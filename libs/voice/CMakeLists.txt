# The voice chat of an online game (docs/architecture/voice.md, ADR 0032), in three targets:
#   rtype::voice         the interface the client knows (rtype/voice/VoiceChannel.hpp), what carries the voice
#                        of each player (rtype/voice/Backend.hpp), the format of the voice
#                        (rtype/voice/Format.hpp), the devices as interfaces (rtype/voice/Devices.hpp) and the
#                        ring buffer they share with the game loop (rtype/voice/RingBuffer.hpp). Header only: the
#                        game protocol and the server use it without Opus
#   rtype::voice_codec   the encoder and the decoder of the voice (rtype/voice/Codec.hpp), built on libopus, a
#                        private dependency. For the client
#   rtype::voice_processing  what the microphone goes through before the encoder
#                        (rtype/voice/Processing.hpp): the echo cancellation and the gain control of speexdsp, the
#                        noise suppression of RNNoise, both private dependencies. For the client
#   rtype::voice_client  the voice of the game for the client: the jitter buffer (rtype/voice/JitterBuffer.hpp),
#                        the channel of the game (rtype/voice/GameVoice.hpp), the talk modes
#                        (rtype/voice/Talk.hpp) and the microphone (rtype/voice/Microphone.hpp), through the
#                        miniaudio raylib compiles
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

# speexdsp, for its echo canceller (mdf.c) and its preprocessor (preprocess.c: the residual echo and the gain
# control). Its build is autotools only: the sources it needs are compiled here, in floating point, with its
# KISS FFT. speexdsp_config_types.h, which configure writes, is written here
CPMAddPackage(
    NAME speexdsp
    VERSION 1.2.1
    URL https://downloads.xiph.org/releases/speex/speexdsp-1.2.1.tar.gz
    URL_HASH SHA256=8c777343e4a6399569c72abc38a95b24db56882c83dbdb6c6424a5f4aeb54d3d
    DOWNLOAD_ONLY YES
)
set(SPEEXDSP_CONFIG_DIR ${CMAKE_CURRENT_BINARY_DIR}/speexdsp)
file(CONFIGURE OUTPUT ${SPEEXDSP_CONFIG_DIR}/speexdsp_config_types.h CONTENT [[
#ifndef __SPEEX_TYPES_H__
#define __SPEEX_TYPES_H__

#include <stdint.h>

typedef int16_t spx_int16_t;
typedef uint16_t spx_uint16_t;
typedef int32_t spx_int32_t;
typedef uint32_t spx_uint32_t;

#endif
]])
add_library(rtype_speexdsp STATIC
    ${speexdsp_SOURCE_DIR}/libspeexdsp/mdf.c
    ${speexdsp_SOURCE_DIR}/libspeexdsp/preprocess.c
    ${speexdsp_SOURCE_DIR}/libspeexdsp/filterbank.c
    ${speexdsp_SOURCE_DIR}/libspeexdsp/fftwrap.c
    ${speexdsp_SOURCE_DIR}/libspeexdsp/kiss_fft.c
    ${speexdsp_SOURCE_DIR}/libspeexdsp/kiss_fftr.c
)
target_include_directories(rtype_speexdsp SYSTEM PUBLIC ${speexdsp_SOURCE_DIR}/include ${SPEEXDSP_CONFIG_DIR})
target_compile_definitions(rtype_speexdsp PRIVATE FLOATING_POINT USE_KISS_FFT EXPORT=)
if(NOT MSVC)
  target_link_libraries(rtype_speexdsp PRIVATE m)
endif()

# RNNoise 0.2, the noise suppression: a small recurrent network trained on voices and noises. Its release
# archive carries the weights of its model (src/rnnoise_data.c), which the archive of the tag does not. Built
# in plain C, without the code for AVX2 chosen at run time: two frames of 10 ms each 20 ms stay well under a
# millisecond
CPMAddPackage(
    NAME rnnoise
    VERSION 0.2
    URL https://github.com/xiph/rnnoise/releases/download/v0.2/rnnoise-0.2.tar.gz
    URL_HASH SHA256=90fce4b00b9ff24c08dbfe31b82ffd43bae383d85c5535676d28b0a2b11c0d37
    DOWNLOAD_ONLY YES
)
add_library(rtype_rnnoise STATIC
    ${rnnoise_SOURCE_DIR}/src/denoise.c
    ${rnnoise_SOURCE_DIR}/src/rnn.c
    ${rnnoise_SOURCE_DIR}/src/pitch.c
    ${rnnoise_SOURCE_DIR}/src/kiss_fft.c
    ${rnnoise_SOURCE_DIR}/src/celt_lpc.c
    ${rnnoise_SOURCE_DIR}/src/nnet.c
    ${rnnoise_SOURCE_DIR}/src/nnet_default.c
    ${rnnoise_SOURCE_DIR}/src/parse_lpcnet_weights.c
    ${rnnoise_SOURCE_DIR}/src/rnnoise_data.c
    ${rnnoise_SOURCE_DIR}/src/rnnoise_tables.c
)
target_include_directories(rtype_rnnoise SYSTEM PUBLIC ${rnnoise_SOURCE_DIR}/include
                           PRIVATE ${rnnoise_SOURCE_DIR}/src)
target_compile_definitions(rtype_rnnoise PRIVATE RNNOISE_BUILD DISABLE_DEBUG_FLOAT)
if(NOT MSVC)
  # Its #warning about AVX2, in every file (a #pragma message with MSVC, not a warning)
  target_compile_options(rtype_rnnoise PRIVATE -Wno-cpp)
  target_link_libraries(rtype_rnnoise PRIVATE m)
endif()

add_library(rtype_voice_processing STATIC
    src/EchoDelay.cpp
    src/Processing.cpp
)
add_library(rtype::voice_processing ALIAS rtype_voice_processing)
target_link_libraries(rtype_voice_processing PUBLIC rtype::voice PRIVATE rtype_options rtype::log rtype_speexdsp
                      rtype_rnnoise)

# Their licenses go with the client, as the one of Opus: both are BSD and ask for their notice
install(FILES ${speexdsp_SOURCE_DIR}/COPYING DESTINATION licenses RENAME speexdsp.txt COMPONENT client)
install(FILES ${rnnoise_SOURCE_DIR}/COPYING DESTINATION licenses RENAME rnnoise.txt COMPONENT client)

add_library(rtype_voice_client STATIC
    src/GameVoice.cpp
    src/JitterBuffer.cpp
    src/Microphone.cpp
    src/Talk.cpp
)
add_library(rtype::voice_client ALIAS rtype_voice_client)
# raylib for its miniaudio: its header (external/miniaudio.h, a system header) and its symbols, which raudio.c
# compiles. Private: no miniaudio in the headers
target_link_libraries(rtype_voice_client PUBLIC rtype::voice_codec rtype::voice_processing PRIVATE rtype_options rtype::log raylib)
