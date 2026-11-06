# Finds the beats of a piece of music: decodes an mp3 file and detects its attacks in three bands of frequencies
# (bass, mid, high). Pure computation: no raylib, no audio device, so the server can use it (the waves that
# follow a music). dr_mp3 is a single header, kept private.
CPMAddPackage(
    NAME dr_libs
    URL https://github.com/mackron/dr_libs/archive/35b5c392cc15456736c440a68dbe073178bbffe0.tar.gz
    URL_HASH SHA256=d46fa0a831a65b16fcf2b169e6b739c66536437ee78049893788582eb93e39de
    DOWNLOAD_ONLY YES
)

add_library(rtype_beat
    src/Analysis/BeatDetector.cpp
    src/Decoder/Mp3Decoder.cpp
)
add_library(rtype::beat ALIAS rtype_beat)
target_include_directories(rtype_beat PUBLIC include PRIVATE src)
# A system directory: the warnings of dr_mp3 are not ours
target_include_directories(rtype_beat SYSTEM PRIVATE ${dr_libs_SOURCE_DIR})
target_link_libraries(rtype_beat PRIVATE rtype_options)
