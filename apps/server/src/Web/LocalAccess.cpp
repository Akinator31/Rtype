#include "Web/LocalAccess.hpp"

#include "Master/ServerKey.hpp"
#include "rtype/network/Datagram.hpp"

#include <format>

namespace rtype::server
{

Secret newAccessToken()
{
    // 32 bytes from the random generator of the cryptography: a key is exactly that
    return rtype::network::DatagramKey::generate().bytes();
}

std::string panelAddress(const rtype::network::Address &listening, std::string_view publicUrl)
{
    if (!publicUrl.empty()) {
        return std::string(publicUrl);
    }
    rtype::network::Address address = listening;
    if (address == rtype::network::Address::anyIpv4(listening.port())) {
        address = rtype::network::Address::loopbackIpv4(listening.port());
    } else if (address == rtype::network::Address::anyIpv6(listening.port())) {
        address = *rtype::network::Address::parse("::1", listening.port());
    }
    return std::format("http://{}", address);
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
