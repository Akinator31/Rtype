#include "Online/KnownProfiles.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <exception>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace rtype::client
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("online");
    return instance;
}

std::string settingsKey(const std::string &address)
{
    return std::string(KnownProfiles::SettingsKeyPrefix).append(address);
}

bool write(engine::Settings &settings)
{
    try {
        settings.writeFile();
        return true;
    } catch (const std::exception &error) {
        logger().error("Cannot write the settings {}: {}", settings.getPath().string(), error.what());
        return false;
    }
}

} // namespace

bool isPictureVersion(std::string_view hash)
{
    return !hash.empty() && hash.size() <= 64 && std::ranges::all_of(hash, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    });
}

KnownProfiles::KnownProfiles(engine::Settings &settings, std::filesystem::path pictures)
    : _settings(settings), _pictures(std::move(pictures))
{
}

// Kept as "<discord: 0 or 1>:<version of the picture>:<username>": a username has no ':', a version neither
std::optional<RememberedProfile> KnownProfiles::profile(const std::string &address) const
{
    const std::optional<std::string> value = _settings.getString(settingsKey(address));
    if (!value || value->size() < 4 || ((*value)[0] != '0' && (*value)[0] != '1') || (*value)[1] != ':') {
        return std::nullopt;
    }
    const std::size_t separator = value->find(':', 2);
    if (separator == std::string::npos || separator + 1 >= value->size()) {
        return std::nullopt;
    }
    RememberedProfile profile{.username = value->substr(separator + 1),
                              .discord = (*value)[0] == '1',
                              .avatarHash = value->substr(2, separator - 2)};
    if (!profile.avatarHash.empty() && !isPictureVersion(profile.avatarHash)) {
        profile.avatarHash.clear();
    }
    return profile;
}

bool KnownProfiles::remember(const std::string &address, const RememberedProfile &profile)
{
    const std::string hash = isPictureVersion(profile.avatarHash) ? profile.avatarHash : std::string();
    _settings.setString(
        settingsKey(address),
        std::string(profile.discord ? "1:" : "0:").append(hash).append(":").append(profile.username));
    return write(_settings);
}

bool KnownProfiles::forget(const std::string &address)
{
    if (!_settings.hasKey(settingsKey(address))) {
        return false;
    }
    _settings.removeKey(settingsKey(address));
    write(_settings);
    return true;
}

std::optional<std::vector<unsigned char>> KnownProfiles::picture(const std::string &hash) const
{
    if (!isPictureVersion(hash)) {
        return std::nullopt;
    }
    std::ifstream file(_pictures / (hash + ".png"), std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (bytes.empty()) {
        return std::nullopt;
    }
    return bytes;
}

bool KnownProfiles::keepPicture(const std::string &hash, std::span<const std::byte> png)
{
    if (!isPictureVersion(hash)) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(_pictures, error);
    std::ofstream file(_pictures / (hash + ".png"), std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
    if (!file) {
        logger().warning("Cannot keep the picture {} in {}", hash, _pictures.string());
        return false;
    }
    return true;
}

} // namespace rtype::client
