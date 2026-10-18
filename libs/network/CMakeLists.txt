# Networking shared by the client and the server: addresses, UDP and TCP sockets, the event loop that runs
# them, the binary serialization of the protocol, and TLS. Asio and Mbed TLS are private dependencies: the
# public headers only use our own types (ADR 0011, ADR 0014).
CPMAddPackage(
    NAME asio
    VERSION 1.38.2
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz
    URL_HASH SHA256=9f2648fa483e58a6bf848d970ee0ea650ca19ed7769dfa520ed4f7b8d27af1db
    DOWNLOAD_ONLY YES
)
# The release archive, not the one GitHub generates from the tag: only the release contains the TF-PSA-Crypto
# submodule (ADR 0014)
CPMAddPackage(
    NAME mbedtls
    VERSION 4.2.0
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-4.2.0/mbedtls-4.2.0.tar.bz2
    URL_HASH SHA256=2bed9d713b4668f76553b097e72b8aa30bc8f112a940d7ae228d524bbde6ffea
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "ENABLE_PROGRAMS OFF"
        "ENABLE_TESTING OFF"
        # Its own warnings, with compilers newer than its CI, must not break our build
        "MBEDTLS_FATAL_WARNINGS OFF"
        "TF_PSA_CRYPTO_FATAL_WARNINGS OFF"
)
find_package(Threads REQUIRED)

add_library(rtype_network
    src/Address.cpp
    src/Datagram.cpp
    src/DatagramBody.cpp
    src/Errors.cpp
    src/EventLoop.cpp
    src/Frame.cpp
    src/FramedConnection.cpp
    src/MessageChannel.cpp
    src/Reader.cpp
    src/Session.cpp
    src/TcpConnection.cpp
    src/TcpListener.cpp
    src/Timer.cpp
    src/Text.cpp
    src/Tls.cpp
    src/TlsConnection.cpp
    src/UdpSocket.cpp
    src/Writer.cpp
)
add_library(rtype::network ALIAS rtype_network)
target_include_directories(rtype_network PUBLIC include)
# SYSTEM: the warnings of Asio's headers are not ours
target_include_directories(rtype_network SYSTEM PRIVATE ${asio_SOURCE_DIR}/include)
target_compile_definitions(rtype_network PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
if(WIN32)
  # Windows 10 and later
  target_compile_definitions(rtype_network PRIVATE _WIN32_WINNT=0x0A00)
  target_link_libraries(rtype_network PRIVATE ws2_32 mswsock)
endif()
target_link_libraries(rtype_network PRIVATE rtype_options rtype::log Threads::Threads mbedtls)
