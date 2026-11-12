# VR through OpenXR (docs/architecture/vr.md, ADR 0037): the session of a headset on the OpenGL context of the
# window, the images of its eyes and its controllers. OpenXR stays out of the public headers.
#
# Built with RTYPE_OPENXR on Linux and Windows. Off, or on macOS (no OpenXR runtime), the library has the same
# interface and never opens a headset: the client has no case of its own for it.
if(WIN32 OR (UNIX AND NOT APPLE))
  set(RTYPE_OPENXR_DEFAULT ON)
else()
  set(RTYPE_OPENXR_DEFAULT OFF)
endif()
option(RTYPE_OPENXR "Build VR with OpenXR (the client starts in a headset with --xr)" ${RTYPE_OPENXR_DEFAULT})

add_library(rtype_xr
    src/Math.cpp
)
add_library(rtype::xr ALIAS rtype_xr)
target_include_directories(rtype_xr PUBLIC include)
target_link_libraries(rtype_xr PRIVATE rtype_options)

if(NOT RTYPE_OPENXR)
  target_sources(rtype_xr PRIVATE src/NoHeadset.cpp)
  message(STATUS "VR: off (RTYPE_OPENXR)")
  return()
endif()

# The loader of OpenXR, built here and linked statically on every system, rather than the one of the system:
# a client linked to libopenxr_loader.so would not start at all on a computer without it, headset or not. The
# loader finds the active runtime by itself (active_runtime.json on Linux, the registry on Windows). Its own
# jsoncpp, not one of the system, for the same reason
CPMAddPackage(
    NAME OpenXR
    VERSION 1.1.60
    URL https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.1.60.tar.gz
    URL_HASH SHA256=a55afd26df6cb41ea8217e8375e3d4ffabdef047141f22ffe725d1c3adb1c43b
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "DYNAMIC_LOADER OFF"
        "BUILD_WITH_SYSTEM_JSONCPP OFF"
        "BUILD_API_LAYERS OFF"
        "BUILD_TESTS OFF"
        "BUILD_CONFORMANCE_TESTS OFF"
)

target_sources(rtype_xr PRIVATE
    src/Controllers.cpp
    src/Eyes.cpp
    src/OpenXrHeadset.cpp
    src/Session.cpp
)
target_compile_definitions(rtype_xr PUBLIC RTYPE_WITH_OPENXR)
# raylib for rlgl only (the framebuffers of the eyes), never raylib.h: see src/Platform.hpp
target_link_libraries(rtype_xr PRIVATE rtype::log raylib OpenXR::openxr_loader)
# OpenXR structures are written {XR_TYPE_...}: the type, every other field zero. It is the idiom of the API,
# which -Wmissing-field-initializers would flag on each of them
if(NOT MSVC)
  target_compile_options(rtype_xr PRIVATE -Wno-missing-field-initializers)
endif()
if(WIN32)
  target_link_libraries(rtype_xr PRIVATE opengl32)
else()
  find_package(X11 REQUIRED)
  find_package(OpenGL REQUIRED COMPONENTS GLX)
  target_link_libraries(rtype_xr PRIVATE X11::X11 OpenGL::GLX)
endif()

# Its license goes with the client that links it (Apache 2.0), and the one of the jsoncpp it embeds
install(FILES ${OpenXR_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME openxr.txt COMPONENT client)
install(FILES ${OpenXR_SOURCE_DIR}/src/external/jsoncpp/LICENSE DESTINATION licenses
        RENAME openxr-jsoncpp.txt COMPONENT client)
message(STATUS "VR: OpenXR ${CPM_PACKAGE_OpenXR_VERSION}")
