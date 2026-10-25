#pragma once

#include "Online/OfficialServers.hpp"
#include "rtype/keys/DevelopmentKey.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/save/Save/Save.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rtype::client
{

/// The address of a server, as its keys are known by: "host:port", the host in lowercase, an IPv6 address in
/// brackets ("[::1]:4242").
[[nodiscard]] std::string serverAddress(const std::string &host, std::uint16_t port);

/// The keys the client trusts for each server (docs/decisions/0020-server-key-distribution.md):
///
/// - the keys of the official servers, built into the client;
/// - the key of each other server the player trusted at its first connection, kept in the save under
///   `servers.known.<address>`;
/// - the development key, for every server, in Debug builds only.
///
/// @code
/// KnownServers known(engine.getSave());
/// LobbyClient lobby(loop, server, known.trusted(address), name, {
///     .onUntrusted = [&](const network::KeyFingerprint &presented) {
///         if (known.judge(address, presented) == KnownServers::Verdict::FirstUse) {
///             // show the key, and if the player trusts it: known.trust(address, presented), then connect
///             again
///         }
///     }});
/// @endcode
class KnownServers
{
public:
    /// What a key a server presented, and that the client does not trust, means.
    enum class Verdict {
        /// A server never met: the player may trust its key.
        FirstUse,
        /// A server known with another key: a new key, or an attack. Refused, until the player forgets the
        /// server.
        Changed,
        /// The development key, in a Release build: refused, since its private key is public.
        DevelopmentKey,
    };

    /// Where the keys of the servers trusted by the player are kept in the save.
    static constexpr std::string_view SaveKeyPrefix = "servers.known.";

private:
    struct Official {
        std::string address;
        std::vector<network::KeyFingerprint> keys;
    };

    save::Save &_save;
    std::vector<Official> _officials;
    bool _trustDevelopmentKey;

    [[nodiscard]] const Official *official(const std::string &address) const;

public:
    /// @param officials The official servers: those built into the client, unless for the tests. A
    /// fingerprint that is not 64 hexadecimal digits is skipped, and logged.
    /// @param trustDevelopmentKey Trust the development key: Debug builds only.
    explicit KnownServers(save::Save &save, std::span<const OfficialServer> officials = OfficialServers,
                          bool trustDevelopmentKey = keys::DevelopmentKeyTrusted);

    /// The keys the server at this address may present, to give to the LobbyClient. None for a server never
    /// met, in a Release build.
    [[nodiscard]] std::vector<network::KeyFingerprint> trusted(const std::string &address) const;
    /// What a key this server presented and that is not trusted means.
    [[nodiscard]] Verdict judge(const std::string &address, const network::KeyFingerprint &presented) const;

    /// This address is the one of an official server.
    [[nodiscard]] bool isOfficial(const std::string &address) const;
    /// The key the player trusted for this server, if any. Never one for an official server.
    [[nodiscard]] std::optional<network::KeyFingerprint> remembered(const std::string &address) const;

    /// Trusts this key for this server from now on, and writes the save.
    /// @return The save could not be written: the key is only trusted until the client closes.
    bool trust(const std::string &address, const network::KeyFingerprint &key);
    /// Forgets the key trusted for this server, and writes the save: its next connection is a first use.
    /// @return A key was forgotten. An official server cannot be.
    bool forget(const std::string &address);
};

} // namespace rtype::client
