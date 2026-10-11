add_executable(r-type_server
    src/main.cpp
)
target_link_libraries(r-type_server PRIVATE rtype_options rtype::ecs rtype::log rtype::physics)

install(TARGETS r-type_server RUNTIME DESTINATION . COMPONENT server)
