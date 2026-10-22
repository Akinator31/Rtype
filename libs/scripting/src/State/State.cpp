#include "rtype/scripting/State/State.hpp"

#include "rtype/scripting/State/ScriptError.hpp"

#include <fstream>
#include <iterator>
#include <new>
#include <optional>

namespace rtype::scripting
{

namespace
{

/// The hook runs every HookInterval instructions: the limit is checked to this precision.
constexpr int HookInterval = 1000;

/// The counter of the State that owns `lua`, kept in the extra space of the state.
std::uint64_t &getInstructionsLeft(lua_State *lua)
{
    return **static_cast<std::uint64_t **>(lua_getextraspace(lua));
}

void countInstructions(lua_State *lua, lua_Debug * /*debug*/)
{
    std::uint64_t &left = getInstructionsLeft(lua);
    if (left <= HookInterval) {
        left = 0;
        luaL_error(lua, "instruction limit reached");
        return;
    }
    left -= HookInterval;
}

void openLibrary(lua_State *lua, const char *name, lua_CFunction open)
{
    luaL_requiref(lua, name, open, 1);
    lua_pop(lua, 1);
}

void removeGlobal(lua_State *lua, const char *name)
{
    lua_pushnil(lua);
    lua_setglobal(lua, name);
}

void removeField(lua_State *lua, const char *table, const char *field)
{
    lua_getglobal(lua, table);
    lua_pushnil(lua);
    lua_setfield(lua, -2, field);
    lua_pop(lua, 1);
}

/// The message of the error on the top of the stack, which is popped.
std::string popError(lua_State *lua)
{
    const char *message = lua_tostring(lua, -1);
    std::string text = message != nullptr ? message : "error object is not a string";
    lua_pop(lua, 1);
    return text;
}

/// Gives the chunk on the top of the stack a new table as its _ENV: its globals go to that table, what it
/// does not find there is read from the globals of the state, and its _G is the table itself.
void giveOwnEnvironment(lua_State *lua)
{
    lua_newtable(lua);
    lua_newtable(lua);
    lua_pushglobaltable(lua);
    lua_setfield(lua, -2, "__index");
    lua_setmetatable(lua, -2);
    lua_pushvalue(lua, -1);
    lua_setfield(lua, -2, "_G");
    lua_setupvalue(lua, -2, 1);
}

std::optional<std::string> readFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

} // namespace

State::State(std::uint64_t instructionLimit) : _lua(luaL_newstate()), _instructionLimit(instructionLimit)
{
    if (_lua == nullptr) {
        throw std::bad_alloc();
    }
    *static_cast<std::uint64_t **>(lua_getextraspace(_lua)) = &_instructionsLeft;
    openSandbox();
    lua_sethook(_lua, countInstructions, LUA_MASKCOUNT, HookInterval);
}

State::~State()
{
    lua_close(_lua);
}

void State::openSandbox()
{
    openLibrary(_lua, LUA_GNAME, luaopen_base);
    openLibrary(_lua, LUA_TABLIBNAME, luaopen_table);
    openLibrary(_lua, LUA_STRLIBNAME, luaopen_string);
    openLibrary(_lua, LUA_MATHLIBNAME, luaopen_math);
    removeGlobal(_lua, "dofile");
    removeGlobal(_lua, "load");
    removeGlobal(_lua, "loadfile");
    removeField(_lua, LUA_MATHLIBNAME, "random");
    removeField(_lua, LUA_MATHLIBNAME, "randomseed");
}

luabridge::LuaRef State::runFile(const std::filesystem::path &path, const std::string &name)
{
    const std::optional<std::string> code = readFile(path);
    if (!code) {
        throw ScriptError(name, "cannot be read");
    }
    return runCode(*code, name);
}

luabridge::LuaRef State::runCode(std::string_view code, const std::string &name)
{
    const std::string chunkName = "@" + name;
    if (luaL_loadbufferx(_lua, code.data(), code.size(), chunkName.c_str(), "t") != LUA_OK) {
        throw ScriptError(name, popError(_lua));
    }
    return runLoadedChunk(name);
}

luabridge::LuaRef State::runLoadedChunk(const std::string &name)
{
    giveOwnEnvironment(_lua);
    runPushedFunction(0, 1, name);
    return luabridge::LuaRef::fromStack(_lua);
}

void State::callFunction(const luabridge::LuaRef &function, std::span<const luabridge::LuaRef> arguments,
                         const std::string &name)
{
    if (!function.isCallable()) {
        throw ScriptError(name, "expected a function");
    }
    function.push(_lua);
    for (const luabridge::LuaRef &argument : arguments) {
        argument.push(_lua);
    }
    runPushedFunction(static_cast<int>(arguments.size()), 0, name);
}

void State::runPushedFunction(int argumentCount, int resultCount, const std::string &name)
{
    _instructionsLeft = _instructionLimit;
    if (lua_pcall(_lua, argumentCount, resultCount, 0) != LUA_OK) {
        throw ScriptError(name, popError(_lua));
    }
}

} // namespace rtype::scripting
