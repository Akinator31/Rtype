# The development key pair (ADR 0020), built into the programs as headers:
#   rtype/keys/DevelopmentKey.hpp         the public key, in every build: Debug clients trust it, Release
#                                         clients and servers refuse it
#   rtype/keys/DevelopmentPrivateKey.hpp  the private key, which a Debug server uses when it has no key. It
#                                         can only be included in Debug builds: never in a release
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/server.pub RTYPE_DEVELOPMENT_PUBLIC_KEY)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/server.key RTYPE_DEVELOPMENT_PRIVATE_KEY)
# Read again when they change
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS server.pub server.key)

set(RTYPE_DEVELOPMENT_KEY_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
configure_file(DevelopmentKey.hpp.in ${RTYPE_DEVELOPMENT_KEY_INCLUDE}/rtype/keys/DevelopmentKey.hpp @ONLY)
configure_file(DevelopmentPrivateKey.hpp.in ${RTYPE_DEVELOPMENT_KEY_INCLUDE}/rtype/keys/DevelopmentPrivateKey.hpp
               @ONLY)

add_library(rtype_development_key INTERFACE)
add_library(rtype::development_key ALIAS rtype_development_key)
target_include_directories(rtype_development_key INTERFACE ${RTYPE_DEVELOPMENT_KEY_INCLUDE})
target_link_libraries(rtype_development_key INTERFACE rtype::network)
target_compile_definitions(rtype_development_key INTERFACE $<$<CONFIG:Debug>:RTYPE_DEVELOPMENT_KEY>)
