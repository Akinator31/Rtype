#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace rtype::save
{

/// Values kept from one run of the client to the next, each under a key: the settings, and more later.
///
/// The values live in memory: readFile() replaces them with the content of the file, writeFile() writes them
/// to it. The file is text, one `key=value` line per value, sorted by key. Typed values are stored as text: a
/// typed getter returns std::nullopt when the key is missing or when its value is not of that type.
/// @code
/// Save save("save.txt");
/// save.readFile();
/// save.setInt("audio.volume", 80);
/// const int volume = save.getInt("audio.volume").value_or(100);
/// save.writeFile();
/// @endcode
class Save
{
    std::filesystem::path _path;
    std::map<std::string, std::string, std::less<>> _values;

public:
    /// @param path The file read by readFile() and written by writeFile(). Nothing is read yet.
    explicit Save(std::filesystem::path path = {});

    void setPath(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path &getPath() const;

    /// Replaces every value with the content of the file. A missing file leaves no value, as on a first run;
    /// a line without '=' is ignored. Throws std::runtime_error if the file exists but cannot be read.
    void readFile();

    /// Writes every value to the file, creating its directories. The file is written to a temporary file
    /// first, then renamed, so that a crash while writing keeps the previous save. Throws
    /// std::runtime_error or std::filesystem::filesystem_error if the file cannot be written.
    void writeFile() const;

    [[nodiscard]] bool hasKey(std::string_view key) const;
    void removeKey(std::string_view key);

    void setString(std::string_view key, std::string_view value);
    void setInt(std::string_view key, int value);
    void setFloat(std::string_view key, float value);
    void setBool(std::string_view key, bool value);

    [[nodiscard]] std::optional<std::string> getString(std::string_view key) const;
    [[nodiscard]] std::optional<int> getInt(std::string_view key) const;
    [[nodiscard]] std::optional<float> getFloat(std::string_view key) const;
    [[nodiscard]] std::optional<bool> getBool(std::string_view key) const;
};

} // namespace rtype::save
