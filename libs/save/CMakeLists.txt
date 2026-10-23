# Values the client keeps from one run to the next (the settings, and more later), each under a key. It does
# not depend on raylib.
add_library(rtype_save src/Save/Save.cpp)
add_library(rtype::save ALIAS rtype_save)
target_include_directories(rtype_save PUBLIC include)
target_link_libraries(rtype_save PRIVATE rtype_options)
