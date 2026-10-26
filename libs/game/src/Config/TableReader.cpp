#include "Config/TableReader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

namespace rtype::game
{

namespace
{

/// "\"bullet\", \"missile\" or \"laser\"".
std::string listChoices(std::span<const std::string_view> choices)
{
    std::string text;
    for (std::size_t i = 0; i < choices.size(); ++i) {
        if (i > 0) {
            text += i + 1 == choices.size() ? " or " : ", ";
        }
        text += std::format("\"{}\"", choices[i]);
    }
    return text;
}

bool acceptsAny(double /*value*/)
{
    return true;
}

bool isAboveZero(double value)
{
    return value > 0.0;
}

bool isZeroOrMore(double value)
{
    return value >= 0.0;
}

/// What a NumberRange accepts, and how an error names it.
struct RangeRule {
    std::string_view description;
    bool (*accepts)(double value);
};

/// In the order of the values of NumberRange.
constexpr std::array<RangeRule, 3> RangeRules{{
    {"a number", acceptsAny},
    {"a number above 0", isAboveZero},
    {"a number of 0 or more", isZeroOrMore},
}};

std::string_view describeRange(NumberRange range)
{
    return RangeRules[static_cast<std::size_t>(range)].description;
}

bool isInRange(double value, NumberRange range)
{
    return RangeRules[static_cast<std::size_t>(range)].accepts(value);
}

/// The length of a table without its __len metamethod.
std::size_t getRawLength(const luabridge::LuaRef &table)
{
    lua_State *lua = table.state();
    table.push(lua);
    const std::size_t length = lua_rawlen(lua, -1);
    lua_pop(lua, 1);
    return length;
}

std::size_t countEntries(const luabridge::LuaRef &table)
{
    std::size_t count = 0;
    for (const auto &entry : luabridge::pairs(table)) {
        static_cast<void>(entry);
        ++count;
    }
    return count;
}

} // namespace

TableReader::TableReader(luabridge::LuaRef table, std::string path)
    : _table(std::move(table)), _path(std::move(path))
{
}

luabridge::LuaRef TableReader::getField(const std::string &name)
{
    _read.push_back(name);
    return _table.rawget(name);
}

void TableReader::fail(const std::string &name, std::string_view problem) const
{
    throw ConfigError(std::format("{}{}: {}", _path, name, problem));
}

std::string TableReader::toString(const std::string &name, const luabridge::LuaRef &value) const
{
    if (!value.isString()) {
        fail(name, "expected a string");
    }
    return value.unsafe_cast<std::string>();
}

float TableReader::toNumber(const std::string &name, const luabridge::LuaRef &value, NumberRange range) const
{
    if (!value.isNumber()) {
        fail(name, std::format("expected {}", describeRange(range)));
    }
    const double number = value.unsafe_cast<double>();
    if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max() ||
        !isInRange(number, range)) {
        fail(name, std::format("expected {}", describeRange(range)));
    }
    return static_cast<float>(number);
}

std::uint64_t TableReader::toInteger(const std::string &name, const luabridge::LuaRef &value,
                                     std::uint64_t min, std::uint64_t max) const
{
    const std::string expected = std::format("expected an integer from {} to {}", min, max);
    if (!value.isNumber()) {
        fail(name, expected);
    }
    lua_State *lua = value.state();
    value.push(lua);
    int isInteger = 0;
    const lua_Integer integer = lua_tointegerx(lua, -1, &isInteger);
    lua_pop(lua, 1);
    if (isInteger == 0 || integer < 0 || static_cast<std::uint64_t>(integer) < min ||
        static_cast<std::uint64_t>(integer) > max) {
        fail(name, expected);
    }
    return static_cast<std::uint64_t>(integer);
}

std::string TableReader::readString(const std::string &name)
{
    const luabridge::LuaRef value = getField(name);
    if (value.isNil()) {
        fail(name, "missing");
    }
    std::string text = toString(name, value);
    if (text.empty()) {
        fail(name, "expected a non-empty string");
    }
    return text;
}

std::string TableReader::readString(const std::string &name, const std::string &fallback)
{
    const luabridge::LuaRef value = getField(name);
    return value.isNil() ? fallback : toString(name, value);
}

float TableReader::readNumber(const std::string &name, NumberRange range)
{
    const luabridge::LuaRef value = getField(name);
    if (value.isNil()) {
        fail(name, "missing");
    }
    return toNumber(name, value, range);
}

float TableReader::readNumber(const std::string &name, NumberRange range, float fallback)
{
    const luabridge::LuaRef value = getField(name);
    return value.isNil() ? fallback : toNumber(name, value, range);
}

std::uint64_t TableReader::readInteger(const std::string &name, std::uint64_t min, std::uint64_t max)
{
    const luabridge::LuaRef value = getField(name);
    if (value.isNil()) {
        fail(name, "missing");
    }
    return toInteger(name, value, min, max);
}

std::uint64_t TableReader::readInteger(const std::string &name, std::uint64_t min, std::uint64_t max,
                                       std::uint64_t fallback)
{
    const luabridge::LuaRef value = getField(name);
    return value.isNil() ? fallback : toInteger(name, value, min, max);
}

bool TableReader::readBoolean(const std::string &name, bool fallback)
{
    const luabridge::LuaRef value = getField(name);
    if (value.isNil()) {
        return fallback;
    }
    if (!value.isBool()) {
        fail(name, "expected true or false");
    }
    return value.unsafe_cast<bool>();
}

bool TableReader::readFunction(const std::string &name)
{
    const luabridge::LuaRef value = getField(name);
    if (value.isNil()) {
        return false;
    }
    if (!value.isFunction()) {
        fail(name, "expected a function");
    }
    return true;
}

std::size_t TableReader::readChoice(const std::string &name, std::span<const std::string_view> choices,
                                    std::size_t fallback)
{
    if (!hasField(name)) {
        _read.push_back(name);
        return fallback;
    }
    return readChoice(name, choices);
}

std::size_t TableReader::readChoice(const std::string &name, std::span<const std::string_view> choices)
{
    const luabridge::LuaRef value = getField(name);
    if (value.isNil()) {
        fail(name, std::format("missing: expected {}", listChoices(choices)));
    }
    const std::string text = value.isString() ? value.unsafe_cast<std::string>() : std::string();
    const auto found = std::ranges::find(choices, text);
    if (!value.isString() || found == choices.end()) {
        fail(name, std::format("expected {}", listChoices(choices)));
    }
    return static_cast<std::size_t>(found - choices.begin());
}

bool TableReader::hasField(const std::string &name) const
{
    return !_table.rawget(name).isNil();
}

TableReader TableReader::readTable(const std::string &name)
{
    const luabridge::LuaRef value = getField(name);
    if (value.isNil()) {
        fail(name, "missing");
    }
    if (!value.isTable()) {
        fail(name, "expected a table");
    }
    return {value, std::format("{}{}.", _path, name)};
}

std::vector<TableReader> TableReader::readList(const std::string &name)
{
    const luabridge::LuaRef value = getField(name);
    if (value.isNil()) {
        return {};
    }
    const std::size_t length = value.isTable() ? getRawLength(value) : 0;
    if (!value.isTable() || countEntries(value) != length) {
        fail(name, "expected a list of tables");
    }
    std::vector<TableReader> items;
    for (std::size_t i = 1; i <= length; ++i) {
        const std::string item = std::format("{}[{}]", name, i);
        const luabridge::LuaRef element = value.rawget(static_cast<lua_Integer>(i));
        if (!element.isTable()) {
            fail(item, "expected a table");
        }
        items.emplace_back(element, std::format("{}{}.", _path, item));
    }
    return items;
}

void TableReader::checkAllFieldsRead() const
{
    for (const auto &[key, value] : luabridge::pairs(_table)) {
        static_cast<void>(value);
        if (!key.isString()) {
            throw ConfigError(std::format("{}fields must have names: no list here", _path));
        }
        const std::string name = key.unsafe_cast<std::string>();
        if (std::ranges::find(_read, name) == _read.end()) {
            fail(name, "unknown field");
        }
    }
}

} // namespace rtype::game
