# The lobby protocol: the messages between a client and the master, in the frames of their TLS connection
# (docs/protocol/lobby.md). Shared by the client and the server. Written with the serialization of
# libs/network, a private dependency.
add_library(rtype_lobby
    src/Messages.cpp
)
add_library(rtype::lobby ALIAS rtype_lobby)
target_include_directories(rtype_lobby PUBLIC include)
target_link_libraries(rtype_lobby PRIVATE rtype_options rtype::network)
