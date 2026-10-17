# The logic is a library of its own, so that the tests can use it
add_library(r-type_keygen_core STATIC
    src/KeyGen.cpp
)
target_include_directories(r-type_keygen_core PUBLIC src)
target_link_libraries(r-type_keygen_core PUBLIC rtype::network PRIVATE rtype_options rtype::log)

add_executable(r-type_keygen
    src/main.cpp
)
target_link_libraries(r-type_keygen PRIVATE rtype_options r-type_keygen_core rtype::log)

install(TARGETS r-type_keygen RUNTIME DESTINATION . COMPONENT server)
