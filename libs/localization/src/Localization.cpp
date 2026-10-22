#include "rtype/localization/Localization.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace rtype::localization
{
namespace
{

std::string trim(std::string_view value)
{
    const auto start = value.find_first_not_of(" \t\r");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r");
    return std::string(value.substr(start, end - start + 1));
}

}

Localization::Localization(std::string defaultLanguage)
    : _defaultLanguage(std::move(defaultLanguage)), _language(_defaultLanguage)
{
    if (_defaultLanguage.empty()) {
        throw std::invalid_argument("Default language cannot be empty");
    }
}

void Localization::load(std::string language, const std::filesystem::path &path)
{
    if (language.empty()) {
        throw std::invalid_argument("Language cannot be empty");
    }
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open translation file: " + path.string());
    }
    Catalog catalog;
    std::string line;
    std::size_t number = 0;
    while (std::getline(file, line)) {
        ++number;
        const std::string_view content(line);
        if (content.empty() || content.front() == '#') {
            continue;
        }
        const auto separator = content.find('=');
        const std::string key = trim(content.substr(0, separator));
        if (separator == std::string_view::npos || key.empty()) {
            throw std::runtime_error("Invalid translation at " + path.string() + ":" +
                                     std::to_string(number));
        }
        if (!catalog.emplace(key, trim(content.substr(separator + 1))).second) {
            throw std::runtime_error("Duplicate translation key at " + path.string() + ":" +
                                     std::to_string(number));
        }
    }
    if (file.bad()) {
        throw std::runtime_error("Cannot read translation file: " + path.string());
    }
    _catalogs.insert_or_assign(std::move(language), std::move(catalog));
}

bool Localization::setLanguage(std::string_view language)
{
    const auto entry = _catalogs.find(std::string(language));
    if (entry == _catalogs.end()) {
        return false;
    }
    _language = entry->first;
    return true;
}

std::string Localization::text(std::string_view key) const
{
    const auto lookup = [&](std::string_view language) -> const std::string * {
        const auto catalog = _catalogs.find(std::string(language));
        if (catalog == _catalogs.end()) {
            return nullptr;
        }
        const auto value = catalog->second.find(std::string(key));
        return value == catalog->second.end() ? nullptr : &value->second;
    };
    if (const auto *value = lookup(_language)) {
        return *value;
    }
    if (const auto *value = lookup(_defaultLanguage)) {
        return *value;
    }
    return std::string(key);
}

}
