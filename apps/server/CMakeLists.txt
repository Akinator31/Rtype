# The code of the server is a library of its own, so that the tests can use it (ADR 0013):
#   Accounts/  the accounts of the players: their rules, what the database keeps of them, their passwords
#   Common/    what both modes share: the command line, the settings file, the lifecycle, the secret of the
#              instances
#   Master/    the master: the lobby, and the instances it starts
#   Instance/  an instance: one game
#   Web/       the web server of the master: the panel, its API, the local access token

# reproc starts the instances and stops them, on Linux and Windows (ADR 0013). A private dependency, behind
# Master/Process.hpp
CPMAddPackage(
    NAME reproc
    VERSION 14.2.8
    URL https://github.com/DaanDeMeyer/reproc/archive/refs/tags/14.2.8.tar.gz
    URL_HASH SHA256=27c3b452bfc419a2deda23969aa10c77909c4ff9e71c549eb65d09ae6aa7aa32
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "REPROC++ OFF"
        "REPROC_INSTALL OFF"
        "REPROC_TEST OFF"
        "REPROC_EXAMPLES OFF"
)

# toml++ reads the settings file of the master (docs/guides/configure-a-server.md). Header-only, behind
# Common/Settings.hpp
CPMAddPackage(
    NAME tomlplusplus
    VERSION 3.4.0
    URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
    URL_HASH SHA256=8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155
    DOWNLOAD_ONLY YES
)

# Crow serves the panel and its API (docs/architecture/web.md). Header-only, given the Asio of libs/network,
# so that the binary holds one Asio. A private dependency, behind Web/WebServer.hpp. Patched: a response ended
# after its handler returned freed its connection while completing it (CrowCpp/Crow#843), and an empty request
# left booleans uninitialized, which the parser copied after each response (undefined behavior)
CPMAddPackage(NAME asio VERSION 1.38.2)
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/include CACHE PATH "The Asio Crow uses: the one of libs/network")
CPMAddPackage(
    NAME Crow
    VERSION 1.3.4
    URL https://github.com/CrowCpp/Crow/archive/refs/tags/v1.3.4.tar.gz
    URL_HASH SHA256=6be759b3a648b41f0adc3a8408a53fd0243775217b4e32e8101d69e4aadf48cc
    PATCHES patches/crow-deferred-response.patch patches/crow-request-initialized.patch
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "CROW_BUILD_EXAMPLES OFF"
        "CROW_BUILD_TESTS OFF"
        "CROW_INSTALL OFF"
)

# libcurl: the mails of the accounts (Accounts/CurlMailer.hpp), Discord (Accounts/Discord.hpp), and the HTTP
# client of the tests. TLS from OpenSSL on Linux, from the system (Schannel) on Windows; only HTTP(S) and SMTP(S), nothing
# else it could reach
if(WIN32)
  set(curl_tls "CURL_USE_SCHANNEL ON" "CURL_USE_OPENSSL OFF")
else()
  set(curl_tls "CURL_USE_OPENSSL ON")
endif()
CPMAddPackage(
    NAME curl
    VERSION 8.23.0
    URL https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.xz
    URL_HASH SHA256=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        ${curl_tls}
        "BUILD_SHARED_LIBS OFF"
        "BUILD_STATIC_LIBS ON"
        "BUILD_CURL_EXE OFF"
        "BUILD_TESTING OFF"
        "BUILD_EXAMPLES OFF"
        "BUILD_LIBCURL_DOCS OFF"
        "BUILD_MISC_DOCS OFF"
        "ENABLE_CURL_MANUAL OFF"
        "CURL_DISABLE_INSTALL ON"
        "CURL_ENABLE_EXPORT_TARGET OFF"
        "PICKY_COMPILER OFF"
        "CURL_USE_LIBPSL OFF"
        "CURL_USE_LIBSSH2 OFF"
        "USE_LIBIDN2 OFF"
        "USE_NGHTTP2 OFF"
        "CURL_ZLIB OFF"
        "CURL_BROTLI OFF"
        "CURL_ZSTD OFF"
        "CURL_DISABLE_DICT ON"
        "CURL_DISABLE_FILE ON"
        "CURL_DISABLE_FTP ON"
        "CURL_DISABLE_GOPHER ON"
        "CURL_DISABLE_IMAP ON"
        "CURL_DISABLE_IPFS ON"
        "CURL_DISABLE_LDAP ON"
        "CURL_DISABLE_LDAPS ON"
        "CURL_DISABLE_MQTT ON"
        "CURL_DISABLE_POP3 ON"
        "CURL_DISABLE_RTSP ON"
        "CURL_DISABLE_TELNET ON"
        "CURL_DISABLE_TFTP ON"
        "CURL_DISABLE_WEBSOCKETS ON"
)

# The reference implementation of Argon2 hashes the passwords of the accounts (ADR 0024). Its C files in a
# target of ours: the portable code (ref.c, not the SSE of opt.c), without its threads, as a hash uses one lane.
# A private dependency, behind Accounts/PasswordHasher.hpp
CPMAddPackage(
    NAME argon2
    VERSION 20190702
    URL https://github.com/P-H-C/phc-winner-argon2/archive/refs/tags/20190702.tar.gz
    URL_HASH SHA256=daf972a89577f8772602bf2eb38b6a3dd3d922bf5724d45e7f9589b5e830442c
    DOWNLOAD_ONLY YES
)
add_library(rtype_argon2 STATIC
    ${argon2_SOURCE_DIR}/src/argon2.c
    ${argon2_SOURCE_DIR}/src/core.c
    ${argon2_SOURCE_DIR}/src/encoding.c
    ${argon2_SOURCE_DIR}/src/ref.c
    ${argon2_SOURCE_DIR}/src/blake2/blake2b.c
)
target_include_directories(rtype_argon2 SYSTEM PUBLIC ${argon2_SOURCE_DIR}/include)
target_compile_definitions(rtype_argon2 PRIVATE ARGON2_NO_THREADS)

add_library(r-type_server_core STATIC
    src/Server.cpp
    src/Accounts/AccountRules.cpp
    src/Accounts/AccountService.cpp
    src/Accounts/AccountStore.cpp
    src/Accounts/CurlMailer.cpp
    src/Accounts/Discord.cpp
    src/Accounts/Mailer.cpp
    src/Accounts/MailMessages.cpp
    src/Accounts/PanelAccounts.cpp
    src/Accounts/PasswordHasher.cpp
    src/Accounts/ProgressStore.cpp
    src/Common/Base64.cpp
    src/Common/GameConfig.cpp
    src/Common/InstanceMessages.cpp
    src/Common/Lifecycle.cpp
    src/Common/Options.cpp
    src/Common/RelayFrame.cpp
    src/Common/Secret.cpp
    src/Common/Settings.cpp
    src/Instance/DirectTransport.cpp
    src/Instance/GameHost.cpp
    src/Instance/GameThread.cpp
    src/Instance/InputBuffer.cpp
    src/Instance/RecentDestructions.cpp
    src/Instance/Instance.cpp
    src/Instance/MasterLink.cpp
    src/Instance/RelayedTransport.cpp
    src/Master/AddressBans.cpp
    src/Master/Administration.cpp
    src/Master/AdminStore.cpp
    src/Master/Games.cpp
    src/Master/Lobby.cpp
    src/Master/Master.cpp
    src/Master/Process.cpp
    src/Master/Relay.cpp
    src/Master/ServerDatabase.cpp
    src/Master/ServerKey.cpp
    src/Web/AdminJson.cpp
    src/Web/Health.cpp
    src/Web/LocalAccess.cpp
    src/Web/Visitor.cpp
    src/Web/WebServer.cpp
)
target_include_directories(r-type_server_core PUBLIC src)
# SYSTEM: the warnings of toml++'s headers are not ours
target_include_directories(r-type_server_core SYSTEM PRIVATE ${tomlplusplus_SOURCE_DIR}/include)
target_link_libraries(r-type_server_core PUBLIC rtype::database rtype::game rtype::lobby rtype::log rtype::network PRIVATE rtype_options reproc Crow::Crow CURL::libcurl rtype_argon2)
# Crow serves files itself only from CROW_STATIC_DIRECTORY: ours are served by Web/WebServer.cpp, inside the
# panel folder only
target_compile_definitions(r-type_server_core PRIVATE CROW_DISABLE_STATIC_DIR)
if(MSVC)
  # Crow and Asio in one file go past the number of sections of an object file
  target_compile_options(r-type_server_core PRIVATE /bigobj)
endif()
if(WIN32)
  # Windows 10 and later, as libs/network
  target_compile_definitions(r-type_server_core PRIVATE _WIN32_WINNT=0x0A00)
  target_link_libraries(r-type_server_core PRIVATE ws2_32 mswsock)
endif()
target_compile_definitions(r-type_server_core PRIVATE
    RTYPE_VERSION="${PROJECT_VERSION}"
    # A game without a master, with a fixed test key: never in the released binaries (ADR 0013)
    $<$<CONFIG:Debug>:RTYPE_STANDALONE_INSTANCE>
)

add_executable(r-type_server
    src/main.cpp
)
target_link_libraries(r-type_server PRIVATE rtype_options r-type_server_core rtype::ecs rtype::log rtype::physics)

install(TARGETS r-type_server RUNTIME DESTINATION . COMPONENT server)

# The administration panel (web/admin), built by Node apart from the game: given its dist/ folder, the server
# package holds it as admin/ beside the executable, where the master serves it from by default. Without it, the
# panel says it is not built; the game builds without Node
set(RTYPE_ADMIN_PANEL_DIR "" CACHE PATH "The built panel (web/admin/dist) to install beside the server, as admin/")
if(RTYPE_ADMIN_PANEL_DIR)
  file(TO_CMAKE_PATH "${RTYPE_ADMIN_PANEL_DIR}" admin_panel_dir)
  if(NOT EXISTS "${admin_panel_dir}/index.html")
    message(FATAL_ERROR "RTYPE_ADMIN_PANEL_DIR=${admin_panel_dir} holds no index.html: build the panel first "
                        "(npm run build in web/admin)")
  endif()
  install(DIRECTORY "${admin_panel_dir}/" DESTINATION admin COMPONENT server)
endif()
