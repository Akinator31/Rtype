CPMAddPackage(
    NAME Catch2
    VERSION 3.16.0
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
    URL_HASH SHA256=0957cae5821b17ce07f0833aaa52b5137643a8382203221f363a8303c109af34
)
include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

# Unit tests: one executable, each TEST_CASE is registered as a separate CTest test
add_executable(rtype_tests
    action_map.cpp
    ecs.cpp
    log.cpp
    paths.cpp
    platform_assumptions.cpp
    resource_cache.cpp
    text_input.cpp
    viewport.cpp
    ecs.cpp
)
target_link_libraries(rtype_tests PRIVATE
    rtype_options rtype::ecs rtype::input rtype::log rtype::render Catch2::Catch2WithMain)
catch_discover_tests(rtype_tests DISCOVERY_MODE PRE_TEST)

# Smoke tests: the binaries start and exit cleanly
add_test(NAME smoke.server_starts COMMAND r-type_server)
