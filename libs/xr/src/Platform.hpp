#pragma once

// OpenXR with its OpenGL binding: WGL on Windows, Xlib (GLX) on Linux. The client runs on X11 on Linux,
// through XWayland under Wayland, since raylib builds GLFW without its Wayland backend (libs/CMakeLists.txt).
//
// rlgl.h first: windows.h and Xlib define macros and types (Rectangle, Font, near, far...) that clash with
// the headers of raylib, which is why no file of this library includes raylib.h.
#include "rlgl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define XR_USE_PLATFORM_WIN32
#else
#include <GL/glx.h>
#include <X11/Xlib.h>
#define XR_USE_PLATFORM_XLIB
#endif
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

// Xlib defines these as macros, which break any code below that uses them as names
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef Success
#undef Success
#endif
#ifdef Always
#undef Always
#endif
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
