#include "Master/VoiceMutes.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace rtype::server
{

VoiceMutes::VoiceMutes(const std::vector<VoiceMute> &mutes)
{
    for (const VoiceMute &mute : mutes) {
        add(mute);
    }
}

std::optional<VoiceMute> VoiceMutes::find(long long accountId,
                                          std::chrono::system_clock::time_point now) const
{
    const auto found = _mutes.find(accountId);
    if (found == _mutes.end() || found->second.expiresAt <= now) {
        return std::nullopt;
    }
    return found->second;
}

void VoiceMutes::add(VoiceMute mute)
{
    const long long key = mute.accountId;
    _mutes.insert_or_assign(key, std::move(mute));
}

std::optional<VoiceMute> VoiceMutes::remove(long long accountId, std::chrono::system_clock::time_point now)
{
    const auto found = _mutes.find(accountId);
    if (found == _mutes.end()) {
        return std::nullopt;
    }
    VoiceMute lifted = std::move(found->second);
    _mutes.erase(found);
    if (lifted.expiresAt <= now) {
        return std::nullopt;
    }
    return lifted;
}

std::size_t VoiceMutes::purge(std::chrono::system_clock::time_point now)
{
    return std::erase_if(_mutes, [now](const auto &entry) { return entry.second.expiresAt <= now; });
}

std::vector<VoiceMute> VoiceMutes::list(std::chrono::system_clock::time_point now) const
{
    std::vector<VoiceMute> mutes;
    for (const auto &[account, mute] : _mutes) {
        if (mute.expiresAt > now) {
            mutes.push_back(mute);
        }
    }
    std::ranges::sort(mutes, [](const VoiceMute &first, const VoiceMute &second) {
        return first.expiresAt != second.expiresAt ? first.expiresAt < second.expiresAt
                                                   : first.accountId < second.accountId;
    });
    return mutes;
}

std::uint32_t secondsLeft(const VoiceMute &mute, std::chrono::system_clock::time_point now)
{
    if (mute.expiresAt <= now) {
        return 0;
    }
    const auto left = std::chrono::ceil<std::chrono::seconds>(mute.expiresAt - now).count();
    // Below the value that means "until the end of the game": a mute is never that long
    return static_cast<std::uint32_t>(
        std::clamp<long long>(left, 1, std::numeric_limits<std::uint32_t>::max() - 1));
}

} // namespace rtype::server
