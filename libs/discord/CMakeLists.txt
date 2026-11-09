# Discord in the client (docs/architecture/client-online.md#discord), in two targets:
#   rtype::discord      the interfaces the client and its tests know, one per feature (rtype/discord/Authorizer.hpp,
#                       Presence.hpp, Connection.hpp), faked in tests/helpers/FakeSocialSdk.hpp
#   rtype::discord_sdk  their implementations through the Discord Social SDK, on the one client of a Session,
#                       linked by the executable of the client alone: the tests never load the SDK
# The SDK is downloaded only from the developer portal of Discord, with an account: its files for Linux x86-64
# and Windows x64 are kept in sdk/ (sdk/README.md). Elsewhere (Linux aarch64, the image of the server for
# linux/arm64), rtype::discord_sdk does not exist and the executable of the client is not built
add_library(rtype_discord INTERFACE)
add_library(rtype::discord ALIAS rtype_discord)
target_include_directories(rtype_discord INTERFACE include)

set(discord_sdk_dir ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")
  add_library(discord_partner_sdk SHARED IMPORTED GLOBAL)
  set_target_properties(discord_partner_sdk PROPERTIES
      IMPORTED_LOCATION ${discord_sdk_dir}/bin/release/discord_partner_sdk.dll
      IMPORTED_IMPLIB ${discord_sdk_dir}/lib/release/discord_partner_sdk.lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  add_library(discord_partner_sdk SHARED IMPORTED GLOBAL)
  set_target_properties(discord_partner_sdk PROPERTIES
      IMPORTED_LOCATION ${discord_sdk_dir}/lib/release/libdiscord_partner_sdk.so
      IMPORTED_SONAME libdiscord_partner_sdk.so)
else()
  message(WARNING "The Discord Social SDK of libs/discord/sdk is kept for Linux x86-64 and Windows x64 only, "
                  "not for ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}: the client is not built, the server is")
  return()
endif()
set_target_properties(discord_partner_sdk PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${discord_sdk_dir}/include)

add_library(rtype_discord_sdk STATIC
    src/Session.cpp
    src/SocialSdkAuthorizer.cpp
    src/SocialSdkPresence.cpp
)
add_library(rtype::discord_sdk ALIAS rtype_discord_sdk)
target_link_libraries(rtype_discord_sdk PUBLIC rtype::discord PRIVATE rtype_options rtype::log discord_partner_sdk)

# Puts the library of the SDK beside an executable, in the build tree and in its package: it is loaded from
# there
function(rtype_add_discord_runtime target component)
  add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:discord_partner_sdk> $<TARGET_FILE_DIR:${target}>)
  if(NOT WIN32)
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN")
  endif()
  install(FILES $<TARGET_FILE:discord_partner_sdk> DESTINATION . COMPONENT ${component})
  install(FILES ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sdk/License-Notices.txt DESTINATION licenses
      RENAME discord-social-sdk-notices.txt COMPONENT ${component})
endfunction()
