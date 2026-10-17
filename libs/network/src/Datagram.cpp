#include "rtype/network/Datagram.hpp"

#include "NetworkLogger.hpp"
#include "Tls.hpp"

#include <mbedtls/platform_util.h>

#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace rtype::network
{

using detail::logger;

namespace
{

constexpr psa_algorithm_t Algorithm = PSA_ALG_CHACHA20_POLY1305;
constexpr std::size_t NonceSize = 12;

void writeHeader(const DatagramHeader &header, std::byte *out)
{
    const auto put = [&out](std::uint64_t value, int bytes) {
        for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8) {
            *out++ = static_cast<std::byte>((value >> shift) & 0xFF);
        }
    };
    put(DatagramMagic, 2);
    put(DatagramVersion, 1);
    put(0, 1);
    put(header.session, 4);
    put(header.packetNumber, 8);
}

std::uint64_t readBigEndian(std::span<const std::byte> bytes)
{
    std::uint64_t value = 0;
    for (const std::byte byte : bytes) {
        value = (value << 8) | std::to_integer<std::uint64_t>(byte);
    }
    return value;
}

/// 4 zero bytes, then the packet number in big-endian: unique as long as the number is.
std::array<unsigned char, NonceSize> makeNonce(std::uint64_t packetNumber)
{
    std::array<unsigned char, NonceSize> nonce{};
    for (std::size_t i = 0; i < 8; ++i) {
        nonce[NonceSize - 1 - i] = static_cast<unsigned char>((packetNumber >> (i * 8)) & 0xFF);
    }
    return nonce;
}

const unsigned char *asBytes(const std::byte *bytes)
{
    return reinterpret_cast<const unsigned char *>(bytes);
}

unsigned char *asBytes(std::byte *bytes)
{
    return reinterpret_cast<unsigned char *>(bytes);
}

std::optional<DatagramHeader> refuse(std::size_t size, std::string_view reason)
{
    if (logger().isEnabled(rtype::log::Level::Trace)) {
        logger().trace("Datagram of {} bytes dropped: {}", size, reason);
    }
    return std::nullopt;
}

} // namespace

// --- DatagramKey ---

struct DatagramKey::Impl {
    mbedtls_svc_key_id_t id = MBEDTLS_SVC_KEY_ID_INIT;
    /// Kept for bytes(): PSA keys cannot be read back unless made exportable.
    Bytes bytes{};

    Impl() = default;
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;

    ~Impl()
    {
        if (!mbedtls_svc_key_id_is_null(id)) {
            const auto lock = detail::lockCrypto();
            psa_destroy_key(id);
        }
        mbedtls_platform_zeroize(bytes.data(), bytes.size());
    }
};

DatagramKey::DatagramKey(const Bytes &bytes)
{
    auto impl = std::make_shared<Impl>();
    impl->bytes = bytes;

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_CHACHA20);
    psa_set_key_bits(&attributes, Size * 8);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, Algorithm);

    const auto lock = detail::lockCrypto();
    const psa_status_t status = psa_import_key(&attributes, asBytes(bytes.data()), bytes.size(), &impl->id);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS) {
        throw std::runtime_error(std::format("cannot load a datagram key (status {})", status));
    }
    _impl = std::move(impl);
}

DatagramKey DatagramKey::generate()
{
    Bytes bytes{};
    {
        const auto lock = detail::lockCrypto();
        if (const psa_status_t status = psa_generate_random(asBytes(bytes.data()), bytes.size());
            status != PSA_SUCCESS) {
            throw std::runtime_error(std::format("cannot draw a datagram key (status {})", status));
        }
    }
    DatagramKey key(bytes);
    mbedtls_platform_zeroize(bytes.data(), bytes.size());
    return key;
}

DatagramKey::Bytes DatagramKey::bytes() const
{
    return _impl->bytes;
}

// --- Datagrams ---

std::optional<DatagramHeader> parseDatagramHeader(std::span<const std::byte> datagram)
{
    if (datagram.size() < MinSealedDatagramSize || datagram.size() > MaxSealedDatagramSize) {
        return refuse(datagram.size(), "wrong size");
    }
    if (readBigEndian(datagram.subspan(0, 2)) != DatagramMagic) {
        return refuse(datagram.size(), "wrong magic");
    }
    if (std::to_integer<std::uint8_t>(datagram[2]) != DatagramVersion) {
        return refuse(datagram.size(), "unknown version");
    }
    if (datagram[3] != std::byte{0}) {
        return refuse(datagram.size(), "reserved byte set");
    }
    DatagramHeader header;
    header.session = static_cast<std::uint32_t>(readBigEndian(datagram.subspan(4, 4)));
    header.packetNumber = readBigEndian(datagram.subspan(8, 8));
    if (header.session == 0 || header.packetNumber == 0) {
        return refuse(datagram.size(), "session or packet number 0");
    }
    return header;
}

std::vector<std::byte> sealDatagram(const DatagramKey &key, const DatagramHeader &header,
                                    std::span<const std::byte> body)
{
    if (body.size() > MaxDatagramBodySize) {
        throw std::length_error(
            std::format("a datagram body is at most {} bytes, not {}", MaxDatagramBodySize, body.size()));
    }
    if (header.session == 0 || header.packetNumber == 0) {
        throw std::invalid_argument("the session and the packet number of a datagram are never 0");
    }

    std::vector<std::byte> datagram(DatagramHeaderSize + body.size() + DatagramTagSize);
    writeHeader(header, datagram.data());
    const auto nonce = makeNonce(header.packetNumber);
    // PSA wants a valid pointer even for an empty body
    const std::byte empty{};
    const std::byte *plaintext = body.empty() ? &empty : body.data();

    std::size_t length = 0;
    psa_status_t status = PSA_SUCCESS;
    {
        const auto lock = detail::lockCrypto();
        status = psa_aead_encrypt(key._impl->id, Algorithm, nonce.data(), nonce.size(),
                                  asBytes(datagram.data()), DatagramHeaderSize, asBytes(plaintext),
                                  body.size(), asBytes(datagram.data() + DatagramHeaderSize),
                                  datagram.size() - DatagramHeaderSize, &length);
    }
    if (status != PSA_SUCCESS || length != body.size() + DatagramTagSize) {
        throw std::runtime_error(std::format("cannot encrypt a datagram (status {})", status));
    }
    return datagram;
}

std::optional<std::vector<std::byte>> openDatagram(const DatagramKey &key,
                                                   std::span<const std::byte> datagram)
{
    const auto header = parseDatagramHeader(datagram);
    if (!header) {
        return std::nullopt;
    }
    const auto nonce = makeNonce(header->packetNumber);
    const std::span<const std::byte> sealed = datagram.subspan(DatagramHeaderSize);
    std::vector<std::byte> body(sealed.size() - DatagramTagSize);
    std::byte empty{};
    std::byte *plaintext = body.empty() ? &empty : body.data();

    std::size_t length = 0;
    psa_status_t status = PSA_SUCCESS;
    {
        const auto lock = detail::lockCrypto();
        status = psa_aead_decrypt(key._impl->id, Algorithm, nonce.data(), nonce.size(),
                                  asBytes(datagram.data()), DatagramHeaderSize, asBytes(sealed.data()),
                                  sealed.size(), asBytes(plaintext), body.size(), &length);
    }
    if (status != PSA_SUCCESS || length != body.size()) {
        if (logger().isEnabled(rtype::log::Level::Trace)) {
            logger().trace("Datagram {} of session {} dropped: not authentic", header->packetNumber,
                           header->session);
        }
        return std::nullopt;
    }
    return body;
}

// --- ReplayWindow ---

bool ReplayWindow::accepts(std::uint64_t packetNumber) const
{
    if (packetNumber == 0) {
        return false;
    }
    if (packetNumber > _highest) {
        return true;
    }
    const std::uint64_t distance = _highest - packetNumber;
    if (distance == 0 || distance > Size) {
        return false;
    }
    return !_seen.test(static_cast<std::size_t>(distance - 1));
}

void ReplayWindow::mark(std::uint64_t packetNumber)
{
    if (!accepts(packetNumber)) {
        return;
    }
    if (packetNumber < _highest) {
        _seen.set(static_cast<std::size_t>(_highest - packetNumber - 1));
        return;
    }
    const std::uint64_t shift = packetNumber - _highest;
    if (shift > Size) {
        _seen.reset();
    } else {
        _seen <<= static_cast<std::size_t>(shift);
        // The old highest number is now `shift` numbers below the new one
        if (_highest != 0) {
            _seen.set(static_cast<std::size_t>(shift - 1));
        }
    }
    _highest = packetNumber;
}

std::uint32_t ReplayWindow::ackBits() const
{
    std::uint32_t bits = 0;
    for (std::size_t i = 0; i < 32; ++i) {
        if (_seen.test(i)) {
            bits |= std::uint32_t{1} << i;
        }
    }
    return bits;
}

// --- PacketCounter ---

std::optional<std::uint64_t> PacketCounter::next()
{
    if (_last == std::numeric_limits<std::uint64_t>::max()) {
        return std::nullopt;
    }
    return ++_last;
}

} // namespace rtype::network
