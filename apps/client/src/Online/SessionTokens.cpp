#include "Online/SessionTokens.hpp"

#include "rtype/log/Logger.hpp"

#include <exception>

namespace rtype::client
{

namespace
{

constexpr std::string_view HexDigits = "0123456789abcdef";

const log::Logger &logger()
{
    static const log::Logger instance("online");
    return instance;
}

std::string settingsKey(const std::string &address)
{
    return std::string(SessionTokens::SettingsKeyPrefix).append(address);
}

std::string discordKey(const std::string &address)
{
    return std::string(SessionTokens::DiscordSettingsKeyPrefix).append(address);
}

std::optional<unsigned> hexValue(char digit)
{
    if (digit >= '0' && digit <= '9') {
        return static_cast<unsigned>(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f') {
        return static_cast<unsigned>(digit - 'a' + 10);
    }
    if (digit >= 'A' && digit <= 'F') {
        return static_cast<unsigned>(digit - 'A' + 10);
    }
    return std::nullopt;
}

void write(engine::Settings &settings)
{
    try {
        settings.writeFile();
    } catch (const std::exception &error) {
        logger().error("Cannot write the settings {}: {}", settings.getPath().string(), error.what());
        throw;
    }
}

} // namespace

SessionTokens::SessionTokens(engine::Settings &settings) : _settings(settings) {}

std::optional<lobby::SessionToken> SessionTokens::token(const std::string &address) const
{
    const std::optional<std::string> hex = _settings.getString(settingsKey(address));
    lobby::SessionToken token{};
    if (!hex || hex->size() != token.size() * 2) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < token.size(); ++i) {
        const std::optional<unsigned> high = hexValue((*hex)[i * 2]);
        const std::optional<unsigned> low = hexValue((*hex)[(i * 2) + 1]);
        if (!high || !low) {
            return std::nullopt;
        }
        token[i] = static_cast<std::byte>((*high << 4U) | *low);
    }
    return token;
}

bool SessionTokens::keep(const std::string &address, const lobby::SessionToken &token)
{
    std::string hex;
    hex.reserve(token.size() * 2);
    for (const std::byte byte : token) {
        hex += HexDigits[std::to_integer<unsigned>(byte) >> 4U];
        hex += HexDigits[std::to_integer<unsigned>(byte) & 0x0FU];
    }
    _settings.setString(settingsKey(address), hex);
    // The token itself is a secret: only the server is logged
    logger().info("Session kept for {}", address);
    try {
        write(_settings);
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

bool SessionTokens::forget(const std::string &address)
{
    const bool discord = _settings.hasKey(discordKey(address));
    _settings.removeKey(discordKey(address));
    if (!_settings.hasKey(settingsKey(address))) {
        if (discord) {
            try {
                write(_settings);
            } catch (const std::exception &) {
                // Forgotten in memory all the same
            }
        }
        return false;
    }
    _settings.removeKey(settingsKey(address));
    logger().info("Session of {} forgotten", address);
    try {
        write(_settings);
    } catch (const std::exception &) {
        // Forgotten in memory all the same: the next connection is a guest
    }
    return true;
}

std::optional<std::string> SessionTokens::discordRefreshToken(const std::string &address) const
{
    std::optional<std::string> token = _settings.getString(discordKey(address));
    if (!token || !lobby::isValidDiscordToken(*token)) {
        return std::nullopt;
    }
    return token;
}

bool SessionTokens::keepDiscord(const std::string &address, const std::string &refreshToken)
{
    if (!lobby::isValidDiscordToken(refreshToken)) {
        return false;
    }
    _settings.setString(discordKey(address), refreshToken);
    // A secret: only the server is logged
    logger().info("Discord token kept for {}", address);
    try {
        write(_settings);
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

void SessionTokens::forgetDiscord(const std::string &address)
{
    if (!_settings.hasKey(discordKey(address))) {
        return;
    }
    _settings.removeKey(discordKey(address));
    logger().info("Discord token of {} forgotten", address);
    try {
        write(_settings);
    } catch (const std::exception &) {
        // Forgotten in memory all the same
    }
}

} // namespace rtype::client
