add_library(rtype_ecs
    src/Component/ComponentCaches.cpp
    src/Component/ICache.cpp
    src/Ecs.cpp
    src/System/System.cpp
)
add_library(rtype::ecs ALIAS rtype_ecs)
target_include_directories(rtype_ecs PUBLIC include)
target_link_libraries(rtype_ecs PRIVATE rtype_options)
