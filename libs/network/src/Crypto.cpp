#include "rtype/network/Crypto.hpp"

#include "Tls.hpp"

#include <format>
#include <stdexcept>

namespace rtype::network
{

void randomBytes(std::span<std::byte> bytes)
{
    const auto lock = detail::lockCrypto();
    if (const psa_status_t status =
            psa_generate_random(reinterpret_cast<unsigned char *>(bytes.data()), bytes.size());
        status != PSA_SUCCESS) {
        throw std::runtime_error(std::format("cannot draw random bytes (status {})", status));
    }
}

Sha256 sha256(std::span<const std::byte> bytes)
{
    const auto lock = detail::lockCrypto();
    Sha256 hash{};
    std::size_t length = 0;
    const psa_status_t status =
        psa_hash_compute(PSA_ALG_SHA_256, reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(),
                         reinterpret_cast<unsigned char *>(hash.data()), hash.size(), &length);
    if (status != PSA_SUCCESS || length != hash.size()) {
        throw std::runtime_error(std::format("cannot compute a SHA-256 (status {})", status));
    }
    return hash;
}

Sha256 sha256(std::string_view text)
{
    return sha256(std::as_bytes(std::span(text.data(), text.size())));
}

} // namespace rtype::network
