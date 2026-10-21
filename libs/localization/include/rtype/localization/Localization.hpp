#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rtype::localization
{

class Localization
{
    using Catalog = std::unordered_map<std::string, std::string>;

    std::unordered_map<std::string, Catalog> _catalogs;
    std::string _defaultLanguage;
    std::string _language;

public:
    explicit Localization(std::string defaultLanguage);

    void load(std::string language, const std::filesystem::path &path);
    bool setLanguage(std::string_view language);

    [[nodiscard]] const std::string &language() const noexcept { return _language; }
    [[nodiscard]] const std::string &defaultLanguage() const noexcept { return _defaultLanguage; }
    [[nodiscard]] std::string text(std::string_view key) const;
};

}
