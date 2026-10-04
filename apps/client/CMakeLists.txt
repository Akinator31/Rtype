add_executable(r-type_client
    src/main.cpp
)
# raylib is still used directly for drawing, until the render library provides a renderer
target_link_libraries(r-type_client PRIVATE rtype_options rtype::render raylib)

install(TARGETS r-type_client RUNTIME DESTINATION . COMPONENT client)
