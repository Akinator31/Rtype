# The voice chat of an online game (docs/architecture/voice.md, ADR 0032): the interface the client knows
# (rtype/voice/VoiceChannel.hpp) and what carries the voice of each player (rtype/voice/Backend.hpp). Header
# only for now; the codec and the devices come with the next steps of the voice, as private dependencies.
add_library(rtype_voice INTERFACE)
add_library(rtype::voice ALIAS rtype_voice)
target_include_directories(rtype_voice INTERFACE include)
