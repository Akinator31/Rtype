#pragma once

#include "rtype/scripting/Lua.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace rtype::scripting
{

/// A Lua state with the sandbox of ADR 0018: only the base functions, `table`, `string` and `math`. There is
/// no `io`, `os`, `package`, `debug`, no `load`, `loadfile`, `dofile` nor `require`, and no `math.random` nor
/// `math.randomseed`: a script cannot touch the disk, the clock or an unseeded random value. A run that
/// executes more than its instruction limit is stopped with an error.
///
/// @code
/// rtype::scripting::State state;
/// const luabridge::LuaRef enemy = state.runFile("config/enemies/bydo_scout.lua", "enemies/bydo_scout.lua");
/// const double health = enemy.rawget("health").unsafe_cast<double>();
/// @endcode
///
/// Each run gets an environment table of its own, which reads the globals of the state: two scripts of the
/// same state do not see each other's globals. The values returned are references into the state: they are
/// valid while it lives.
class State
{
public:
    /// Instructions a run may execute: far more than a configuration file needs, few enough to stop an
    /// endless loop at once.
    static constexpr std::uint64_t DefaultInstructionLimit = 1'000'000;

private:
    lua_State *_lua;
    std::uint64_t _instructionLimit;
    /// Counted down by the hook during a run.
    std::uint64_t _instructionsLeft = 0;

    void openSandbox();
    luabridge::LuaRef runLoadedChunk(const std::string &name);
    /// Runs the function pushed with its `argumentCount` arguments, with the whole instruction limit, keeping
    /// `resultCount` results on the stack.
    void runPushedFunction(int argumentCount, int resultCount, const std::string &name);

public:
    explicit State(std::uint64_t instructionLimit = DefaultInstructionLimit);
    ~State();

    State(const State &) = delete;
    State &operator=(const State &) = delete;
    State(State &&) = delete;
    State &operator=(State &&) = delete;

    /// The state itself, for the bindings (luabridge::getGlobalNamespace()).
    [[nodiscard]] lua_State *getLuaState() const { return _lua; }

    /// Runs a Lua text file (never a precompiled chunk) and gives back its first returned value, nil if it
    /// returns nothing.
    /// @param name What the errors call the file, e.g. "enemies/bydo_scout.lua".
    /// @throws ScriptError if the file cannot be read, does not compile, fails, or reaches the instruction
    /// limit.
    luabridge::LuaRef runFile(const std::filesystem::path &path, const std::string &name);

    /// Same as runFile(), with the code itself.
    luabridge::LuaRef runCode(std::string_view code, const std::string &name);

    /// Calls a function of this state with its arguments, under a whole instruction limit of its own, and
    /// drops what it returns: a function called at every tick of a game.
    /// @param name What the errors call the function, e.g. "enemies/bydo_gunner.lua".
    /// @throws ScriptError if `function` cannot be called, fails, or reaches the instruction limit.
    void callFunction(const luabridge::LuaRef &function, std::span<const luabridge::LuaRef> arguments,
                      const std::string &name);
};

} // namespace rtype::scripting
