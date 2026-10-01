add_executable(r-type_server
    src/main.cpp
)
target_link_libraries(r-type_server PRIVATE rtype_options)

install(TARGETS r-type_server RUNTIME DESTINATION . COMPONENT rtype)
