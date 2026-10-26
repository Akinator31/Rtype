#include "rtype/scripting/State/ScriptError.hpp"
#include "rtype/scripting/State/State.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;
using rtype::scripting::ScriptError;
using rtype::scripting::State;

namespace
{

/// The message of the ScriptError thrown by running `code`, empty if none is thrown.
std::string runError(State &state, const std::string &code)
{
    try {
        state.runCode(code, "test.lua");
    } catch (const ScriptError &error) {
        return error.what();
    }
    return {};
}

} // namespace

TEST_CASE("A state runs a chunk and gives back the table it returns", "[scripting]")
{
    State state;
    const luabridge::LuaRef table =
        state.runCode("return { name = 'scout', health = 30, speed = 2.5 * 2 }", "scout.lua");

    REQUIRE(table.isTable());
    CHECK(table.rawget("name").unsafe_cast<std::string>() == "scout");
    CHECK(table.rawget("health").unsafe_cast<int>() == 30);
    CHECK(table.rawget("speed").unsafe_cast<double>() == 5.0);
}

TEST_CASE("A chunk that returns nothing gives nil", "[scripting]")
{
    State state;

    CHECK(state.runCode("local x = 1", "empty.lua").isNil());
}

TEST_CASE("The sandbox has the base functions, table, string and math, and nothing else", "[scripting]")
{
    State state;
    const luabridge::LuaRef result = state.runCode(R"(
        return {
            io = io, os = os, package = package, debug = debug, coroutine = coroutine, utf8 = utf8,
            require = require, load = load, loadfile = loadfile, dofile = dofile,
            random = math.random, randomseed = math.randomseed,
            floor = math.floor(2.5), concat = table.concat({ 'a', 'b' }), format = string.format('%d', 7),
            pairs = pairs ~= nil, pcall = pcall ~= nil,
        }
    )",
                                                   "sandbox.lua");

    for (const char *name : {"io", "os", "package", "debug", "coroutine", "utf8", "require", "load",
                             "loadfile", "dofile", "random", "randomseed"}) {
        INFO(name);
        CHECK(result.rawget(name).isNil());
    }
    CHECK(result.rawget("floor").unsafe_cast<int>() == 2);
    CHECK(result.rawget("concat").unsafe_cast<std::string>() == "ab");
    CHECK(result.rawget("format").unsafe_cast<std::string>() == "7");
    CHECK(result.rawget("pairs").unsafe_cast<bool>());
    CHECK(result.rawget("pcall").unsafe_cast<bool>());
}

TEST_CASE("Two chunks of the same state do not share their globals", "[scripting]")
{
    State state;
    state.runCode("shared = 1; _G.through_g = 2", "first.lua");

    const luabridge::LuaRef second =
        state.runCode("return { shared = shared, through_g = through_g }", "second.lua");

    CHECK(second.rawget("shared").isNil());
    CHECK(second.rawget("through_g").isNil());
}

TEST_CASE("An endless loop is stopped by the instruction limit", "[scripting]")
{
    State state(10'000);

    CHECK_THAT(runError(state, "while true do end"),
               StartsWith("test.lua:") && ContainsSubstring("instruction limit"));
    // The limit is per run: the state still works after
    CHECK(state.runCode("return 1", "after.lua").unsafe_cast<int>() == 1);
}

TEST_CASE("A function of the state is called with its arguments, each call under a limit of its own",
          "[scripting]")
{
    State state(10'000);
    const luabridge::LuaRef chunk =
        state.runCode("local total = 0\n"
                      "return {\n"
                      "    add = function(a, b) total = total + a + b; return total end,\n"
                      "    total = function(out) out.value = total end,\n"
                      "    spin = function(n) for i = 1, n do end end,\n"
                      "}",
                      "calls.lua");
    lua_State *lua = state.getLuaState();
    const std::vector<luabridge::LuaRef> numbers{luabridge::LuaRef(lua, 2), luabridge::LuaRef(lua, 3)};
    state.callFunction(chunk.rawget("add"), numbers, "calls.lua");
    state.callFunction(chunk.rawget("add"), numbers, "calls.lua");
    const std::vector<luabridge::LuaRef> out{luabridge::newTable(lua)};
    state.callFunction(chunk.rawget("total"), out, "calls.lua");
    CHECK(out[0].rawget("value").unsafe_cast<int>() == 10);

    // A run of a few thousand instructions each time, which would reach the limit if it were shared
    const std::vector<luabridge::LuaRef> loops{luabridge::LuaRef(lua, 1000)};
    for (int call = 0; call < 10; ++call) {
        state.callFunction(chunk.rawget("spin"), loops, "calls.lua");
    }
    const std::vector<luabridge::LuaRef> endless{luabridge::LuaRef(lua, 1'000'000)};
    CHECK_THROWS_AS(state.callFunction(chunk.rawget("spin"), endless, "calls.lua"), ScriptError);
    try {
        state.callFunction(chunk.rawget("nope"), {}, "calls.lua");
        FAIL("calling nil must throw");
    } catch (const ScriptError &error) {
        CHECK_THAT(error.what(), StartsWith("calls.lua: ") && ContainsSubstring("expected a function"));
    }
}

TEST_CASE("The errors name the file: syntax, runtime, error objects", "[scripting]")
{
    State state;

    CHECK_THAT(runError(state, "return {"), StartsWith("test.lua:1:"));
    CHECK_THAT(runError(state, "local x = nil\nreturn x.field"), StartsWith("test.lua:2:"));
    CHECK_THAT(runError(state, "error({})"), StartsWith("test.lua: ") && ContainsSubstring("not a string"));
    CHECK(runError(state, "return 1").empty());
}

TEST_CASE("A precompiled chunk is refused", "[scripting]")
{
    State state;

    CHECK_THAT(runError(state, "\x1bLua"), StartsWith("test.lua:") && ContainsSubstring("binary"));
}

TEST_CASE("A file is run under the name it is given, and a missing one is an error naming it", "[scripting]")
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "rtype_tests_scripting.lua";
    {
        std::ofstream file(path);
        file << "return { value = 42 }\n";
    }
    State state;

    CHECK(state.runFile(path, "value.lua").rawget("value").unsafe_cast<int>() == 42);
    std::filesystem::remove(path);
    try {
        state.runFile(path, "value.lua");
        FAIL("a missing file must throw");
    } catch (const ScriptError &error) {
        CHECK(error.getFile() == "value.lua");
        CHECK_THAT(error.what(), StartsWith("value.lua: ") && ContainsSubstring("cannot be read"));
    }
}
