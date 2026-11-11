include_guard(GLOBAL)

# libcurl, CURL::libcurl: the mails of the accounts and Discord in the server (apps/server/src/Accounts), the
# HTTP client of the tests, and the pictures of the Discord friends in the client
# (apps/client/src/Discord/FriendPictures.hpp). TLS from OpenSSL on Linux, from the system (Schannel) on Windows;
# only HTTP(S) and SMTP(S), nothing else it could reach
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
