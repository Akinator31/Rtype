# Keyboard and gamepad input, turned into game actions.
# Built on raylib, which stays out of the public headers.
add_library(rtype_input
    src/Binding.cpp
    src/Devices.cpp
)
add_library(rtype::input ALIAS rtype_input)
target_include_directories(rtype_input PUBLIC include)
target_link_libraries(rtype_input PRIVATE rtype_options raylib)
