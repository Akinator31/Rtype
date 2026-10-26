#pragma once

/// Lua and LuaBridge3, in the order LuaBridge3 needs: the Lua headers first. Include this one rather than
/// either of them. Only the sources that write bindings include it, never a header of the game (ADR 0019).
// clang-format off: sorting the includes would put LuaBridge3 first
#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>
// clang-format on
