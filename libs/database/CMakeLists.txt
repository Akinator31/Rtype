# The data of the server (ADR 0023): SOCI, one API over SQLite and PostgreSQL. Only this library sees SQLite;
# SOCI is in its public headers, since the code that writes the queries uses its session.

# SOCI formats its messages with fmt, which its release archive does not contain (a submodule)
CPMAddPackage(
    NAME fmt
    VERSION 12.2.0
    URL https://github.com/fmtlib/fmt/archive/refs/tags/12.2.0.tar.gz
    URL_HASH SHA256=8b852bb5aa6e7d8564f9e81394055395dd1d1936d38dfd3a17792a02bebd7af0
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "FMT_INSTALL OFF"
        "FMT_TEST OFF"
        "FMT_DOC OFF"
)
# SQLite as its single C file, built into the SQLite backend of SOCI: nothing to install on any system
CPMAddPackage(
    NAME sqlite
    VERSION 3.54.1
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
    URL_HASH SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
    DOWNLOAD_ONLY YES
)
CPMAddPackage(
    NAME soci
    VERSION 4.1.4
    URL https://github.com/SOCI/soci/archive/refs/tags/v4.1.4.tar.gz
    URL_HASH SHA256=144f017cccc2e2d806badb3313d6ab3c67a1925bccaa747a46fb3907108a615d
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "SOCI_SHARED OFF"
        "SOCI_TESTS OFF"
        "SOCI_INSTALL OFF"
        "SOCI_LTO OFF"
        "WITH_BOOST OFF"
        "SOCI_SQLITE3 ON"
        "SOCI_SQLITE3_BUILTIN ON"
        "SOCI_SQLITE3_DIRECTORY ${sqlite_SOURCE_DIR}"
        "SOCI_POSTGRESQL OFF"
        "SOCI_DB2 OFF"
        "SOCI_EMPTY OFF"
        "SOCI_FIREBIRD OFF"
        "SOCI_MYSQL OFF"
        "SOCI_ODBC OFF"
        "SOCI_ORACLE OFF"
)

find_package(Threads REQUIRED)

add_library(rtype_database
    src/Address.cpp
    src/Database.cpp
    src/DatabaseThread.cpp
)
add_library(rtype::database ALIAS rtype_database)
target_include_directories(rtype_database PUBLIC include)
target_link_libraries(rtype_database PUBLIC SOCI::Core Threads::Threads PRIVATE SOCI::SQLite3 rtype::log rtype_options)
