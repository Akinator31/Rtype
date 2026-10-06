add_library(rtype_ecs
    Component/ComponentCaches.cpp
    Component/ICache.cpp
    Ecs.cpp
    System/System.cpp
)
add_library(rtype::ecs ALIAS rtype_ecs)
# Headers are included as "ecs/<path>.hpp"
target_include_directories(rtype_ecs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(rtype_ecs PRIVATE rtype_options)
