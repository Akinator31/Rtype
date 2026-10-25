#include "Online/KnownServers.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <format>

namespace rtype::client
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("online");
    return instance;
}

std::string saveKey(const std::string &address)
{
    return std::string(KnownServers::SaveKeyPrefix).append(address);
}

} // namespace

std::string serverAddress(const std::string &host, std::uint16_t port)
{
    std::string lower = host;
    std::ranges::transform(lower, lower.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    if (lower.find(':') != std::string::npos) {
        return std::format("[{}]:{}", lower, port);
    }
    return std::format("{}:{}", lower, port);
}

KnownServers::KnownServers(save::Save &save, std::span<const OfficialServer> officials,
                           bool trustDevelopmentKey)
    : _save(save), _trustDevelopmentKey(trustDevelopmentKey)
{
    for (const OfficialServer &server : officials) {
        Official official{serverAddress(std::string(server.host), server.port), {}};
        for (const std::string_view key : server.keys) {
            if (const auto fingerprint = network::KeyFingerprint::parse(key)) {
                official.keys.push_back(*fingerprint);
            } else {
                logger().error("The official server {} has an invalid key fingerprint: {}", official.address,
                               key);
            }
        }
        _officials.push_back(std::move(official));
    }
}

const KnownServers::Official *KnownServers::official(const std::string &address) const
{
    const auto found = std::ranges::find(_officials, address, &Official::address);
    return found != _officials.end() ? &*found : nullptr;
}

bool KnownServers::isOfficial(const std::string &address) const
{
    return official(address) != nullptr;
}

std::optional<network::KeyFingerprint> KnownServers::remembered(const std::string &address) const
{
    if (isOfficial(address)) {
        return std::nullopt;
    }
    const std::optional<std::string> hex = _save.getString(saveKey(address));
    return hex ? network::KeyFingerprint::parse(*hex) : std::nullopt;
}

std::vector<network::KeyFingerprint> KnownServers::trusted(const std::string &address) const
{
    std::vector<network::KeyFingerprint> result;
    if (const Official *server = official(address)) {
        result = server->keys;
    } else if (const auto key = remembered(address)) {
        result.push_back(*key);
    }
    if (_trustDevelopmentKey) {
        result.push_back(keys::developmentFingerprint());
    }
    return result;
}

KnownServers::Verdict KnownServers::judge(const std::string &address,
                                          const network::KeyFingerprint &presented) const
{
    if (!_trustDevelopmentKey && presented == keys::developmentFingerprint()) {
        return Verdict::DevelopmentKey;
    }
    if (isOfficial(address) || remembered(address)) {
        return Verdict::Changed;
    }
    return Verdict::FirstUse;
}

bool KnownServers::trust(const std::string &address, const network::KeyFingerprint &key)
{
    _save.setString(saveKey(address), key.toHex());
    logger().info("Key {} trusted for {}", key.toHex(), address);
    try {
        _save.writeFile();
    } catch (const std::exception &error) {
        logger().error("Cannot write the save {}: {}", _save.getPath().string(), error.what());
        return false;
    }
    return true;
}

bool KnownServers::forget(const std::string &address)
{
    if (isOfficial(address) || !_save.hasKey(saveKey(address))) {
        return false;
    }
    _save.removeKey(saveKey(address));
    logger().info("Key of {} forgotten", address);
    try {
        _save.writeFile();
    } catch (const std::exception &error) {
        logger().error("Cannot write the save {}: {}", _save.getPath().string(), error.what());
    }
    return true;
}

} // namespace rtype::client
