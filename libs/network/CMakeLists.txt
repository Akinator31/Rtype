# Networking shared by the client and the server: addresses, UDP and TCP sockets, and the event loop that runs
# them. Asio is a private dependency: the public headers only use our own types (ADR 0011).
CPMAddPackage(
    NAME asio
    VERSION 1.38.2
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz
    URL_HASH SHA256=9f2648fa483e58a6bf848d970ee0ea650ca19ed7769dfa520ed4f7b8d27af1db
    DOWNLOAD_ONLY YES
)
find_package(Threads REQUIRED)

add_library(rtype_network
    src/Address.cpp
    src/EventLoop.cpp
    src/TcpConnection.cpp
    src/TcpListener.cpp
    src/UdpSocket.cpp
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
target_link_libraries(rtype_network PRIVATE rtype_options rtype::log Threads::Threads)
