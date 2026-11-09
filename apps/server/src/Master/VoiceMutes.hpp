#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace rtype::server
{

/// The longest mute of the voice of an account (docs/architecture/administration.md#voice-mutes): a mute is
/// for a time, as the bans of addresses; an account that must never talk is banned instead.
inline constexpr std::chrono::hours MaxVoiceMuteDuration{30 * 24};

/// An account whose voice the games drop until a date, in every game it plays (table `voice_mutes`).
struct VoiceMute {
    long long accountId = 0;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point expiresAt;
    std::string reason;
    /// Who muted it: `local` for the local access token, the id of the account of an administrator.
    std::string actor;

    friend bool operator==(const VoiceMute &, const VoiceMute &) = default;
};

/// The mutes of the voice of the accounts, in memory: what the games read when a player joins, and the panel
/// shows. The database keeps them across restarts: read at start (loadVoiceMutes()), written as they are
/// added. On the thread of the loop.
class VoiceMutes
{
    std::map<long long, VoiceMute> _mutes;

public:
    explicit VoiceMutes(const std::vector<VoiceMute> &mutes = {});

    /// The mute of an account, if one runs at `now`.
    [[nodiscard]] std::optional<VoiceMute>
    find(long long accountId,
         std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;
    /// Adds a mute, or replaces the one of its account.
    void add(VoiceMute mute);
    /// Lifts the mute of an account before it expires.
    /// @return The mute lifted; nothing if none ran at `now`.
    std::optional<VoiceMute>
    remove(long long accountId, std::chrono::system_clock::time_point now = std::chrono::system_clock::now());
    /// Forgets the mutes expired at `now`.
    /// @return How many.
    std::size_t purge(std::chrono::system_clock::time_point now);
    /// The mutes that run at `now`, the first to expire first.
    [[nodiscard]] std::vector<VoiceMute>
    list(std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;
};

/// The seconds left of a mute at `now`, rounded up so that it never ends early, for MUTE_VOICE and
/// ADD_PLAYER; 0 if it ended.
[[nodiscard]] std::uint32_t secondsLeft(const VoiceMute &mute, std::chrono::system_clock::time_point now);

} // namespace rtype::server
