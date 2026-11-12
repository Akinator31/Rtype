# Keyboard, mouse and gamepad input, turned into game actions, and a virtual gamepad and pointer (the
# controllers of a VR headset) over them.
# Built on raylib, which stays out of the public headers.
add_library(rtype_input
    src/Binding.cpp
    src/Devices.cpp
    src/Keys.cpp
    src/TextInput.cpp
    src/VirtualInput.cpp
)
add_library(rtype::input ALIAS rtype_input)
target_include_directories(rtype_input PUBLIC include)
# The names of the keys on the layout of the player come from GLFW, which raylib builds in
target_include_directories(rtype_input SYSTEM PRIVATE ${raylib_SOURCE_DIR}/src/external/glfw/include)
target_link_libraries(rtype_input PRIVATE rtype_options raylib)
