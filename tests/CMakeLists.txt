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
    async_resources.cpp
    audio.cpp
    aura_settings.cpp
    beat.cpp
    client_frame_profile.cpp
    client_game.cpp
    client_input_clock.cpp
    client_interpolation.cpp
    client_lore.cpp
    client_menu_transitions.cpp
    client_online.cpp
    client_voice.cpp
    client_vr.cpp
    client_vr_controllers.cpp
    client_vr_experience.cpp
    client_profiles.cpp
    client_shot_prediction.cpp
    client_prediction.cpp
    color_filter.cpp
    collider_debug_renderer.cpp
    control_settings.cpp
    controls.cpp
    crypto.cpp
    database.cpp
    database_thread.cpp
    datagram.cpp
    debug_commands.cpp
    discord.cpp
    discord_friends.cpp
    discord_invite.cpp
    discord_presence.cpp
    discord_voice.cpp
    display_settings.cpp
    ecs.cpp
    ecs_world.cpp
    engine.cpp
    font_characters.cpp
    framing.cpp
    game_config.cpp
    game_items.cpp
    game_loadout.cpp
    game_messages.cpp
    game_rescue.cpp
    game_ship_stats.cpp
    game_simulation.cpp
    known_servers.cpp
    lobby_messages.cpp
    log.cpp
    localization.cpp
    network.cpp
    orbit_breaker.cpp
    paths.cpp
    platform_assumptions.cpp
    player_settings.cpp
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
    relay_frame.cpp
    render_board.cpp
    render_passes.cpp
    reliability.cpp
    resource_cache.cpp
    scene_assets.cpp
    scenes.cpp
    scripting.cpp
    serialization.cpp
    server_account_store.cpp
    server_accounts.cpp
    server_admin.cpp
    server_database.cpp
    server_instance.cpp
    server_key.cpp
    server_lifecycle.cpp
    server_games.cpp
    server_lobby.cpp
    server_mail.cpp
    server_panel_accounts.cpp
    server_options.cpp
    server_relay.cpp
    server_replication.cpp
    server_settings.cpp
    server_web.cpp
    session.cpp
    settings.cpp
    shaders.cpp
    text_input.cpp
    timer.cpp
    tls.cpp
    viewport.cpp
    virtual_input.cpp
    voice.cpp
    voice_codec.cpp
    voice_game.cpp
    voice_jitter.cpp
    voice_processing.cpp
    voice_talk.cpp
    xr_math.cpp
    helpers/FakeDiscord.cpp
)
# The fake Discord is a Crow server: Asio for Windows 10 and later, as libs/network, and on MSVC, Crow and Asio in
# one file go past the number of sections of an object file
if(WIN32)
  set_source_files_properties(helpers/FakeDiscord.cpp PROPERTIES COMPILE_DEFINITIONS _WIN32_WINNT=0x0A00)
endif()
if(MSVC)
  set_source_files_properties(helpers/FakeDiscord.cpp PROPERTIES COMPILE_OPTIONS /bigobj)
endif()
target_link_libraries(rtype_tests PRIVATE
    rtype_options rtype::audio rtype::beat rtype::database rtype::ecs rtype::engine rtype::game rtype::input rtype::lobby rtype::localization rtype::log rtype::network rtype::physics rtype::render
    rtype::scripting rtype::shaders rtype::voice rtype::voice_codec rtype::voice_client rtype::voice_processing rtype::xr r-type_client_core
    r-type_server_core orbit_breaker_core
    CURL::libcurl Crow::Crow Catch2::Catch2WithMain)
# The fake mail server of the tests speaks TLS with OpenSSL, there where libcurl uses it: not on Windows, whose
# libcurl uses Schannel
if(NOT WIN32)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(rtype_tests PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(rtype_tests PRIVATE RTYPE_TEST_OPENSSL)
endif()
# The programs the tests of Process start: a helper that does what its arguments say, and the server itself
add_executable(rtype_test_child helpers/process_child.cpp)
target_link_libraries(rtype_test_child PRIVATE rtype_options)
add_dependencies(rtype_tests rtype_test_child r-type_server)
# The players of a game for the end-to-end tests of the panel (web/admin/e2e), which run a real master
add_executable(rtype_panel_players helpers/panel_players.cpp)
target_include_directories(rtype_panel_players PRIVATE helpers)
target_link_libraries(rtype_panel_players PRIVATE rtype_options rtype::lobby rtype::network)
add_executable(rtype_panel_accounts helpers/panel_accounts.cpp)
target_link_libraries(rtype_panel_accounts PRIVATE rtype_options r-type_server_core)
# Discord and a mail server for the smoke test of the Docker image with accounts (docker/smoke-test-accounts.sh):
# the fake Discord over HTTPS, as discord.com, with the OpenSSL of Linux
if(NOT WIN32)
  add_executable(rtype_fake_services helpers/fake_services.cpp helpers/FakeDiscord.cpp)
  target_link_libraries(rtype_fake_services PRIVATE
      rtype_options r-type_server_core rtype::network Crow::Crow OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(rtype_fake_services PRIVATE CROW_ENABLE_SSL)
endif()
target_compile_definitions(rtype_tests PRIVATE
    RTYPE_TEST_CHILD="$<TARGET_FILE:rtype_test_child>"
    RTYPE_TEST_SERVER="$<TARGET_FILE:r-type_server>"
    RTYPE_TEST_CONFIG="${PROJECT_SOURCE_DIR}/config"
    RTYPE_TEST_SOUNDS="${PROJECT_SOURCE_DIR}/apps/client/assets/sounds"
    RTYPE_TEST_FONTS="${PROJECT_SOURCE_DIR}/apps/client/assets/fonts"
    RTYPE_TEST_LOCALES="${PROJECT_SOURCE_DIR}/apps/client/assets/locales")
# The helpers the tests share
target_include_directories(rtype_tests PRIVATE helpers)
catch_discover_tests(rtype_tests DISCOVERY_MODE PRE_TEST)

# Smoke tests: the binaries start and exit cleanly
add_test(NAME smoke.server_help COMMAND r-type_server --help)
add_test(NAME smoke.server_invalid_arguments COMMAND r-type_server --unknown)
set_tests_properties(smoke.server_invalid_arguments PROPERTIES WILL_FAIL TRUE)
# The server makes its key at its first start: the check makes it there, then reads it. Its panel on a free
# port, as the other tests run beside it
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/smoke_keys/web.toml "[web]\nlisten = \"127.0.0.1:0\"\n")
add_test(NAME smoke.server_check
    COMMAND r-type_server --check --port 0 --key ${CMAKE_CURRENT_BINARY_DIR}/smoke_keys/server.key
        --database sqlite:${CMAKE_CURRENT_BINARY_DIR}/smoke_keys/rtype.db
        --settings ${CMAKE_CURRENT_BINARY_DIR}/smoke_keys/web.toml)

# The sanitizers find a bug made on purpose: a build where they would miss it fails here
if(NOT RTYPE_SANITIZERS STREQUAL "")
  add_executable(rtype_sanitizer_canary helpers/sanitizer_canary.cpp)
  if(RTYPE_SANITIZERS STREQUAL "thread")
    set(canaries "race=ThreadSanitizer: data race")
  else()
    set(canaries
        "leak=LeakSanitizer: detected memory leaks"
        "freed=AddressSanitizer: heap-use-after-free"
        "undefined=runtime error: signed integer overflow")
  endif()
  foreach(canary IN LISTS canaries)
    string(REPLACE "=" ";" canary "${canary}")
    list(GET canary 0 bug)
    list(GET canary 1 report)
    add_test(NAME sanitizers.${bug} COMMAND rtype_sanitizer_canary ${bug})
    set_tests_properties(sanitizers.${bug} PROPERTIES PASS_REGULAR_EXPRESSION "${report}")
  endforeach()
endif()
