#pragma once

#include "rtype/scripting/Lua.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rtype::game
{

/// A field of a configuration file that is missing, of the wrong type or out of range. what() names the field
/// and the problem, "health: expected an integer from 1 to 255"; the loader adds the file.
class ConfigError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/// What a number field accepts, besides being finite.
enum class NumberRange : std::uint8_t {
    Any,
    AboveZero,
    ZeroOrMore,
};

/// Reads the fields of a Lua table into C++ values, checking their types and ranges: each read throws a
/// ConfigError naming the field. The reads are raw (no metamethod runs). Every field read is remembered, so
/// that checkAllFieldsRead() refuses the others: a misspelt optional field is an error, not a default.
class TableReader
{
    luabridge::LuaRef _table;
    /// Prefix of the field names in the errors: "" for the table of the file, "collider." or "spawns[2]." for
    /// a table in it.
    std::string _path;
    std::vector<std::string> _read;

    /// The value of a field, nil if absent, and remembers it was read.
    luabridge::LuaRef getField(const std::string &name);
    [[noreturn]] void fail(const std::string &name, std::string_view problem) const;

    std::string toString(const std::string &name, const luabridge::LuaRef &value) const;
    float toNumber(const std::string &name, const luabridge::LuaRef &value, NumberRange range) const;
    std::uint64_t toInteger(const std::string &name, const luabridge::LuaRef &value, std::uint64_t min,
                            std::uint64_t max) const;

public:
    /// @param table A Lua table.
    TableReader(luabridge::LuaRef table, std::string path);

    /// A required string, not empty.
    [[nodiscard]] std::string readString(const std::string &name);
    /// An optional string, `fallback` if absent.
    [[nodiscard]] std::string readString(const std::string &name, const std::string &fallback);

    /// A required finite number.
    [[nodiscard]] float readNumber(const std::string &name, NumberRange range);
    [[nodiscard]] float readNumber(const std::string &name, NumberRange range, float fallback);

    /// A required whole number from `min` to `max`; 3.0 is accepted, 3.5 is not.
    [[nodiscard]] std::uint64_t readInteger(const std::string &name, std::uint64_t min, std::uint64_t max);
    [[nodiscard]] std::uint64_t readInteger(const std::string &name, std::uint64_t min, std::uint64_t max,
                                            std::uint64_t fallback);

    [[nodiscard]] bool readBoolean(const std::string &name, bool fallback);

    /// An optional function: whether the table has it. Its code is not run.
    [[nodiscard]] bool readFunction(const std::string &name);

    /// An optional string among `choices`: gives its index in them, `fallback` if absent.
    [[nodiscard]] std::size_t readChoice(const std::string &name, std::span<const std::string_view> choices,
                                         std::size_t fallback);
    /// A required string among `choices`.
    [[nodiscard]] std::size_t readChoice(const std::string &name, std::span<const std::string_view> choices);

    [[nodiscard]] bool hasField(const std::string &name) const;

    /// A required table.
    [[nodiscard]] TableReader readTable(const std::string &name);
    /// An optional list of tables, `{ {...}, {...} }`, empty if absent.
    [[nodiscard]] std::vector<TableReader> readList(const std::string &name);

    /// An optional list of non-empty strings, `{ "a", "b" }`, empty if absent.
    [[nodiscard]] std::vector<std::string> readStrings(const std::string &name);

    /// Throws a ConfigError for the first field that was not read: unknown to the definition.
    void checkAllFieldsRead() const;
};

} // namespace rtype::game
