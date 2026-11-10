#include "Voice/VoiceDevices.hpp"

#include <format>

namespace rtype::client
{

std::string playerVoiceKey(std::string_view server, std::uint64_t accountId, std::string_view name)
{
    if (accountId != 0) {
        return std::format("{}/{}", server, accountId);
    }
    return std::format("{}/guest/{}", server, name);
}

} // namespace rtype::client
