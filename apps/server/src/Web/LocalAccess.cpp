#include "Web/LocalAccess.hpp"

#include "Master/ServerKey.hpp"
#include "rtype/network/Crypto.hpp"

#include <format>

namespace rtype::server
{

Secret newAccessToken()
{
    return rtype::network::randomBytes<SecretSize>();
}

rtype::network::Address reachableAddress(const rtype::network::Address &listening)
{
    if (listening == rtype::network::Address::anyIpv4(listening.port())) {
        return rtype::network::Address::loopbackIpv4(listening.port());
    }
    if (listening == rtype::network::Address::anyIpv6(listening.port())) {
        return *rtype::network::Address::parse("::1", listening.port());
    }
    return listening;
}

std::string panelAddress(const rtype::network::Address &listening, std::string_view publicUrl)
{
    if (!publicUrl.empty()) {
        return std::string(publicUrl);
    }
    return std::format("http://{}", reachableAddress(listening));
}

std::string accessUrl(std::string_view panel, const Secret &token)
{
    return std::format("{}/?token={}", panel, formatSecret(token));
}

void writeAccessUrl(const std::filesystem::path &file, std::string_view url)
{
    // Removed first: the new file is created readable by its owner only, whatever the old one was
    std::filesystem::remove(file);
    writeNewSecretFile(file, std::string(url) + "\n");
}

} // namespace rtype::server
