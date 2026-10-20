# Building blocks shared by every library and program: handles and resource caches. Header-only, and
# without any dependency, so that the server can use it.
add_library(rtype_core INTERFACE)
add_library(rtype::core ALIAS rtype_core)
target_include_directories(rtype_core INTERFACE include)
find_package(Threads REQUIRED)
target_link_libraries(rtype_core INTERFACE Threads::Threads)
