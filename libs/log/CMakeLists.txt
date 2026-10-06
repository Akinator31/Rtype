# Logging shared by the client and the server. Like the ECS, it must not depend on raylib.
add_library(rtype_log
    src/Log.cpp
    src/Logger.cpp
)
add_library(rtype::log ALIAS rtype_log)
target_include_directories(rtype_log PUBLIC include)
target_link_libraries(rtype_log PRIVATE rtype_options)
