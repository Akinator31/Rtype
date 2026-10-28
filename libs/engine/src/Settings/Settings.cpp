#include "rtype/engine/Settings/Settings.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace rtype::engine
{
namespace
{

/// Escapes what would break a `key=value` line: the backslash, '=' and the line breaks.
std::string escape(std::string_view text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (const char character : text) {
        if (character == '\\' || character == '=') {
            escaped += '\\';
            escaped += character;
        } else if (character == '\n') {
            escaped += "\\n";
        } else if (character == '\r') {
            escaped += "\\r";
        } else {
            escaped += character;
        }
    }
    return escaped;
}

char unescape(char character)
{
    if (character == 'n') {
        return '\n';
    }
    if (character == 'r') {
        return '\r';
    }
    return character;
}

/// Splits a line at its first unescaped '='. std::nullopt when it has none.
std::optional<std::pair<std::string, std::string>> parseLine(std::string_view line)
{
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    std::string key;
    std::string value;
    std::string *current = &key;
    for (std::size_t index = 0; index < line.size(); ++index) {
        if (line[index] == '\\' && index + 1 < line.size()) {
            *current += unescape(line[++index]);
        } else if (line[index] == '=' && current == &key) {
            current = &value;
        } else {
            *current += line[index];
        }
    }
    if (current == &key) {
        return std::nullopt;
    }
    return std::pair{std::move(key), std::move(value)};
}

/// Reads the whole text as a number, std::nullopt if anything else is left.
template <typename Number>
std::optional<Number> parseNumber(std::string_view text)
{
    Number number{};
    const char *end = text.data() + text.size();
    const auto [last, error] = std::from_chars(text.data(), end, number);
    if (error != std::errc{} || last != end) {
        return std::nullopt;
    }
    return number;
}

} // namespace

Settings::Settings(std::filesystem::path path) : _path(std::move(path)) {}

void Settings::setPath(std::filesystem::path path)
{
    _path = std::move(path);
}

const std::filesystem::path &Settings::getPath() const
{
    return _path;
}

void Settings::readFile()
{
    _values.clear();
    if (!std::filesystem::exists(_path)) {
        return;
    }
    std::ifstream file(_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open the settings file: " + _path.string());
    }
    std::string line;
    while (std::getline(file, line)) {
        if (auto entry = parseLine(line)) {
            _values.insert_or_assign(std::move(entry->first), std::move(entry->second));
        }
    }
    if (file.bad()) {
        throw std::runtime_error("Cannot read the settings file: " + _path.string());
    }
}

void Settings::writeFile() const
{
    if (_path.has_parent_path()) {
        std::filesystem::create_directories(_path.parent_path());
    }
    std::filesystem::path temporary = _path;
    temporary += ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    for (const auto &[key, value] : _values) {
        file << escape(key) << '=' << escape(value) << '\n';
    }
    file.close();
    if (!file) {
        throw std::runtime_error("Cannot write the settings file: " + temporary.string());
    }
    std::filesystem::rename(temporary, _path);
}

bool Settings::hasKey(std::string_view key) const
{
    return _values.contains(key);
}

void Settings::removeKey(std::string_view key)
{
    const auto entry = _values.find(key);
    if (entry != _values.end()) {
        _values.erase(entry);
    }
}

void Settings::setString(std::string_view key, std::string_view value)
{
    _values.insert_or_assign(std::string(key), std::string(value));
}

void Settings::setInt(std::string_view key, int value)
{
    setString(key, std::to_string(value));
}

void Settings::setFloat(std::string_view key, float value)
{
    std::array<char, 32> buffer{};
    const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    setString(key, std::string_view(buffer.data(), end));
}

void Settings::setBool(std::string_view key, bool value)
{
    setString(key, value ? "true" : "false");
}

std::optional<std::string> Settings::getString(std::string_view key) const
{
    const auto entry = _values.find(key);
    if (entry == _values.end()) {
        return std::nullopt;
    }
    return entry->second;
}

std::optional<int> Settings::getInt(std::string_view key) const
{
    const auto entry = _values.find(key);
    if (entry == _values.end()) {
        return std::nullopt;
    }
    return parseNumber<int>(entry->second);
}

std::optional<float> Settings::getFloat(std::string_view key) const
{
    const auto entry = _values.find(key);
    if (entry == _values.end()) {
        return std::nullopt;
    }
    return parseNumber<float>(entry->second);
}

std::optional<bool> Settings::getBool(std::string_view key) const
{
    const auto entry = _values.find(key);
    if (entry == _values.end()) {
        return std::nullopt;
    }
    if (entry->second == "true") {
        return true;
    }
    if (entry->second == "false") {
        return false;
    }
    return std::nullopt;
}

} // namespace rtype::engine
