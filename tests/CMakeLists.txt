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
    audio.cpp
    color_filter.cpp
    collider_debug_renderer.cpp
    controls.cpp
    datagram.cpp
    ecs.cpp
    ecs_world.cpp
    engine.cpp
    framing.cpp
    game_simulation.cpp
    keygen.cpp
    log.cpp
    network.cpp
    paths.cpp
    platform_assumptions.cpp
    process.cpp
    physics_broad_phase.cpp
    physics_bounds.cpp
    physics_collider.cpp
    physics_collision.cpp
    physics_collision_filter.cpp
    physics_geometry.cpp
    physics_fast_moving.cpp
    physics_movement.cpp
    physics_order.cpp
    physics_resolution.cpp
    physics_step.cpp
    reliability.cpp
    resource_cache.cpp
    scenes.cpp
    serialization.cpp
    server_lifecycle.cpp
    server_options.cpp
    session.cpp
    shaders.cpp
    text_input.cpp
    timer.cpp
    tls.cpp
    viewport.cpp
)
target_link_libraries(rtype_tests PRIVATE
    rtype_options rtype::audio rtype::ecs rtype::engine rtype::game rtype::input rtype::log rtype::network rtype::physics rtype::render
    rtype::shaders r-type_keygen_core r-type_server_core
    Catch2::Catch2WithMain)
# The programs the tests of Process start: a helper that does what its arguments say, and the server itself
add_executable(rtype_test_child helpers/process_child.cpp)
target_link_libraries(rtype_test_child PRIVATE rtype_options)
add_dependencies(rtype_tests rtype_test_child r-type_server)
target_compile_definitions(rtype_tests PRIVATE
    RTYPE_TEST_CHILD="$<TARGET_FILE:rtype_test_child>"
    RTYPE_TEST_SERVER="$<TARGET_FILE:r-type_server>")
catch_discover_tests(rtype_tests DISCOVERY_MODE PRE_TEST)

# Smoke tests: the binaries start and exit cleanly
add_test(NAME smoke.server_help COMMAND r-type_server --help)
add_test(NAME smoke.server_invalid_arguments COMMAND r-type_server --unknown)
set_tests_properties(smoke.server_invalid_arguments PROPERTIES WILL_FAIL TRUE)
# The server only starts with a key: one is made for the check
add_test(NAME smoke.server_keys
    COMMAND r-type_keygen --out ${CMAKE_CURRENT_BINARY_DIR}/smoke_keys --force)
set_tests_properties(smoke.server_keys PROPERTIES FIXTURES_SETUP server_keys)
add_test(NAME smoke.server_check
    COMMAND r-type_server --check --port 0 --key ${CMAKE_CURRENT_BINARY_DIR}/smoke_keys/server.key)
set_tests_properties(smoke.server_check PROPERTIES FIXTURES_REQUIRED server_keys)
add_test(NAME smoke.keygen_help COMMAND r-type_keygen --help)
