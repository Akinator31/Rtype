#pragma once

#include "rtype/network/Tls.hpp"
#include "rtype/save/Save/Save.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// The address of a server, as its keys are known by: "host:port", the host in lowercase, an IPv6 address in
/// brackets ("[::1]:4242").
[[nodiscard]] std::string serverAddress(const std::string &host, std::uint16_t port);

/// The key the client trusts for each server
/// (docs/decisions/0021-server-generated-key-trust-on-first-use.md): the one the player trusted at its first
/// connection, kept in the save under `servers.known.<address>`.
///
/// @code
/// KnownServers known(engine.getSave());
/// LobbyClient lobby(loop, server, known.trusted(address), name, {
///     .onUntrusted = [&](const network::KeyFingerprint &presented) {
///         if (known.judge(address) == KnownServers::Verdict::FirstUse) {
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
    };

    /// Where the keys of the servers trusted by the player are kept in the save.
    static constexpr std::string_view SaveKeyPrefix = "servers.known.";

private:
    save::Save &_save;

public:
    explicit KnownServers(save::Save &save);

    /// The keys the server at this address may present, to give to the LobbyClient: the one remembered, or
    /// none for a server never met.
    [[nodiscard]] std::vector<network::KeyFingerprint> trusted(const std::string &address) const;
    /// What a key this server presented, and that is not trusted, means.
    [[nodiscard]] Verdict judge(const std::string &address) const;

    /// The key the player trusted for this server, if any.
    [[nodiscard]] std::optional<network::KeyFingerprint> remembered(const std::string &address) const;

    /// Trusts this key for this server from now on, and writes the save.
    /// @return False if the save could not be written: the key is then only trusted until the client closes.
    bool trust(const std::string &address, const network::KeyFingerprint &key);
    /// Forgets the key trusted for this server, and writes the save: its next connection is a first use.
    /// @return A key was forgotten.
    bool forget(const std::string &address);
};

} // namespace rtype::client
