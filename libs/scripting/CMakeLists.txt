# Lua for the game, without any game notion (ADR 0019): the build of Lua 5.4, LuaBridge3, and a sandboxed State.
# No raylib, no ECS: the server links it through libs/game.

# The sources of Lua, compiled here as a static C library. Only what the sandbox opens (ADR 0018) is built:
# no lua.c and luac.c (the programs), no io, os, package, debug, coroutine nor utf8 library
CPMAddPackage(
    NAME lua
    VERSION 5.4.8
    URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
    URL_HASH SHA256=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
    DOWNLOAD_ONLY YES
)
add_library(rtype_lua STATIC
    ${lua_SOURCE_DIR}/src/lapi.c
    ${lua_SOURCE_DIR}/src/lauxlib.c
    ${lua_SOURCE_DIR}/src/lbaselib.c
    ${lua_SOURCE_DIR}/src/lcode.c
    ${lua_SOURCE_DIR}/src/lctype.c
    ${lua_SOURCE_DIR}/src/ldebug.c
    ${lua_SOURCE_DIR}/src/ldo.c
    ${lua_SOURCE_DIR}/src/ldump.c
    ${lua_SOURCE_DIR}/src/lfunc.c
    ${lua_SOURCE_DIR}/src/lgc.c
    ${lua_SOURCE_DIR}/src/llex.c
    ${lua_SOURCE_DIR}/src/lmathlib.c
    ${lua_SOURCE_DIR}/src/lmem.c
    ${lua_SOURCE_DIR}/src/lobject.c
    ${lua_SOURCE_DIR}/src/lopcodes.c
    ${lua_SOURCE_DIR}/src/lparser.c
    ${lua_SOURCE_DIR}/src/lstate.c
    ${lua_SOURCE_DIR}/src/lstring.c
    ${lua_SOURCE_DIR}/src/lstrlib.c
    ${lua_SOURCE_DIR}/src/ltable.c
    ${lua_SOURCE_DIR}/src/ltablib.c
    ${lua_SOURCE_DIR}/src/ltm.c
    ${lua_SOURCE_DIR}/src/lundump.c
    ${lua_SOURCE_DIR}/src/lvm.c
    ${lua_SOURCE_DIR}/src/lzio.c
)
target_include_directories(rtype_lua SYSTEM PUBLIC ${lua_SOURCE_DIR}/src)
# Lua seeds its string hashes with the time and the addresses, which changes the order of pairs() from one run
# to the next. lstate.c calls luai_makeseed(L): this turns it into 0U * sizeof(L), a fixed seed, so that the
# same scripts run the same everywhere
target_compile_definitions(rtype_lua PRIVATE "luai_makeseed=0U*sizeof")
# As Lua's own makefile does on Linux and macOS: its errors jump with _setjmp/_longjmp, without the signal mask.
# ThreadSanitizer does not see the setjmp of recent glibc versions (2.44): with it, a Lua error stops the program
if(NOT WIN32)
  target_compile_definitions(rtype_lua PRIVATE LUA_USE_POSIX)
endif()

# LuaBridge3, header-only: the bindings between C++ and Lua
CPMAddPackage(
    NAME LuaBridge3
    VERSION 3.0-rc12
    URL https://github.com/kunitoki/LuaBridge3/archive/refs/tags/3.0-rc12.tar.gz
    URL_HASH SHA256=0c15204d5ca2eefa760aa2a8b6543e6ec47231723ee9ed2819c9c9a9cb319e69
    DOWNLOAD_ONLY YES
)
add_library(rtype_luabridge INTERFACE)
target_include_directories(rtype_luabridge SYSTEM INTERFACE ${LuaBridge3_SOURCE_DIR}/Source)
target_link_libraries(rtype_luabridge INTERFACE rtype_lua)

add_library(rtype_scripting
    src/State/ScriptError.cpp
    src/State/State.cpp
)
add_library(rtype::scripting ALIAS rtype_scripting)
target_include_directories(rtype_scripting PUBLIC include)
target_link_libraries(rtype_scripting PUBLIC rtype_luabridge PRIVATE rtype_options)
