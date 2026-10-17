#include "Tls.hpp"

#include "rtype/network/Errors.hpp"

#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace rtype::network
{

namespace
{

class TlsCategory : public std::error_category
{
public:
    [[nodiscard]] const char *name() const noexcept override { return "rtype.tls"; }

    [[nodiscard]] std::string message(int value) const override
    {
        std::array<char, 128> text{};
        mbedtls_strerror(-value, text.data(), text.size());
        return text.data();
    }
};

// The algorithms of docs/protocol/lobby.md, section 2, in order of preference. Mbed TLS keeps the pointers.
constexpr std::array<int, 3> CipherSuites = {MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
                                             MBEDTLS_TLS1_3_AES_128_GCM_SHA256, 0};
constexpr std::array<std::uint16_t, 3> Groups = {
    MBEDTLS_SSL_IANA_TLS_GROUP_X25519, MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1, MBEDTLS_SSL_IANA_TLS_GROUP_NONE};
constexpr std::array<std::uint16_t, 2> SignatureAlgorithms = {MBEDTLS_TLS1_3_SIG_ECDSA_SECP256R1_SHA256,
                                                              MBEDTLS_TLS1_3_SIG_NONE};

/// Largest DER or PEM encoding of an ECDSA P-256 key or of our certificate, with a wide margin.
constexpr std::size_t EncodingBufferSize = 2048;

std::string readFile(const std::filesystem::path &path)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        throw std::system_error(error ? error : std::make_error_code(std::errc::no_such_file_or_directory),
                                path.string());
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::system_error(std::make_error_code(std::errc::permission_denied), path.string());
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// The DER encoding of a public key. Under the lock of lockCrypto().
std::vector<unsigned char> publicKeyDer(const mbedtls_pk_context &key)
{
    std::vector<unsigned char> buffer(EncodingBufferSize);
    // Written at the end of the buffer
    const int length = mbedtls_pk_write_pubkey_der(&key, buffer.data(), buffer.size());
    if (length < 0) {
        throw std::runtime_error(
            std::format("cannot encode a public key: {}", detail::tlsError(length).message()));
    }
    buffer.erase(buffer.begin(), buffer.end() - length);
    return buffer;
}

/// Writes a key in PEM with an encoding function of Mbed TLS. Under the lock of lockCrypto().
std::string writePem(const mbedtls_pk_context &key,
                     int (*write)(const mbedtls_pk_context *, unsigned char *, size_t))
{
    std::vector<unsigned char> buffer(EncodingBufferSize);
    if (const int result = write(&key, buffer.data(), buffer.size()); result != 0) {
        throw std::runtime_error(std::format("cannot encode a key: {}", detail::tlsError(result).message()));
    }
    // A text ended by a null character
    return reinterpret_cast<const char *>(buffer.data());
}

/// Makes the self-signed certificate presented in the handshakes. Under the lock of lockCrypto().
void makeCertificate(ServerIdentity::Impl &identity)
{
    mbedtls_x509write_cert writer;
    mbedtls_x509write_crt_init(&writer);
    std::array<unsigned char, 16> serial{};
    psa_generate_random(serial.data(), serial.size());
    serial[0] &= 0x7F; // a positive number
    mbedtls_x509write_crt_set_version(&writer, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&writer, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&writer, &identity.key);
    mbedtls_x509write_crt_set_issuer_key(&writer, &identity.key);
    // The client only checks the key: the names and dates are there because a certificate must have them
    int result = mbedtls_x509write_crt_set_serial_raw(&writer, serial.data(), serial.size());
    if (result == 0) {
        result = mbedtls_x509write_crt_set_subject_name(&writer, "CN=R-Type server");
    }
    if (result == 0) {
        result = mbedtls_x509write_crt_set_issuer_name(&writer, "CN=R-Type server");
    }
    if (result == 0) {
        result = mbedtls_x509write_crt_set_validity(&writer, "20000101000000", "20991231235959");
    }
    std::vector<unsigned char> buffer(EncodingBufferSize);
    int length = result;
    if (result == 0) {
        // Written at the end of the buffer
        length = mbedtls_x509write_crt_der(&writer, buffer.data(), buffer.size());
    }
    mbedtls_x509write_crt_free(&writer);
    if (length >= 0) {
        length = mbedtls_x509_crt_parse_der(&identity.certificate, buffer.data() + buffer.size() - length,
                                            static_cast<std::size_t>(length));
    }
    if (length < 0) {
        throw std::runtime_error(
            std::format("cannot make the server certificate: {}", detail::tlsError(length).message()));
    }
}

} // namespace

const std::error_category &tlsCategory()
{
    static const TlsCategory category;
    return category;
}

namespace detail
{

std::unique_lock<std::recursive_mutex> lockCrypto()
{
    static std::recursive_mutex mutex;
    static bool initialized = false;
    std::unique_lock lock(mutex);
    if (!initialized) {
        if (const psa_status_t status = psa_crypto_init(); status != PSA_SUCCESS) {
            throw std::runtime_error(std::format("cannot initialize PSA Crypto (status {})", status));
        }
        initialized = true;
    }
    return lock;
}

std::error_code tlsError(int code)
{
    return {-code, tlsCategory()};
}

KeyFingerprint fingerprintOf(std::span<const unsigned char> der)
{
    const auto lock = lockCrypto();
    std::array<unsigned char, KeyFingerprint::Size> hash{};
    std::size_t length = 0;
    const psa_status_t status =
        psa_hash_compute(PSA_ALG_SHA_256, der.data(), der.size(), hash.data(), hash.size(), &length);
    if (status != PSA_SUCCESS || length != hash.size()) {
        throw std::runtime_error(std::format("cannot compute a SHA-256 (status {})", status));
    }
    KeyFingerprint::Bytes bytes{};
    std::ranges::transform(hash, bytes.begin(),
                           [](unsigned char byte) { return static_cast<std::byte>(byte); });
    return KeyFingerprint(bytes);
}

} // namespace detail

std::optional<KeyFingerprint> KeyFingerprint::parse(std::string_view hex)
{
    if (hex.size() != Size * 2) {
        return std::nullopt;
    }
    const auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    Bytes bytes{};
    for (std::size_t i = 0; i < Size; ++i) {
        const int high = digit(hex[i * 2]);
        const int low = digit(hex[(i * 2) + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        bytes[i] = static_cast<std::byte>((high << 4) | low);
    }
    return KeyFingerprint(bytes);
}

KeyFingerprint KeyFingerprint::ofPublicKey(std::string_view pem)
{
    const auto lock = detail::lockCrypto();
    // Mbed TLS reads PEM from a text ended by a null character, counted in the length
    const std::string text(pem);
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    const int result = mbedtls_pk_parse_public_key(
        &key, reinterpret_cast<const unsigned char *>(text.c_str()), text.size() + 1);
    if (result != 0) {
        mbedtls_pk_free(&key);
        throw std::invalid_argument(std::format("not a public key: {}", detail::tlsError(result).message()));
    }
    std::vector<unsigned char> der;
    try {
        der = publicKeyDer(key);
    } catch (...) {
        mbedtls_pk_free(&key);
        throw;
    }
    mbedtls_pk_free(&key);
    return detail::fingerprintOf(der);
}

KeyFingerprint KeyFingerprint::loadPublicKey(const std::filesystem::path &path)
{
    return ofPublicKey(readFile(path));
}

std::string KeyFingerprint::toHex() const
{
    static constexpr std::string_view Digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(Size * 2);
    for (const std::byte byte : _bytes) {
        hex.push_back(Digits[std::to_integer<unsigned>(byte) >> 4]);
        hex.push_back(Digits[std::to_integer<unsigned>(byte) & 0x0F]);
    }
    return hex;
}

ServerIdentity::Impl::Impl()
{
    mbedtls_pk_init(&key);
    mbedtls_x509_crt_init(&certificate);
}

ServerIdentity::Impl::~Impl()
{
    const auto lock = detail::lockCrypto();
    mbedtls_x509_crt_free(&certificate);
    mbedtls_pk_free(&key);
}

ServerIdentity::ServerIdentity(std::shared_ptr<const Impl> impl) : _impl(std::move(impl)) {}

ServerIdentity ServerIdentity::generate()
{
    const auto lock = detail::lockCrypto();
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, 256);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
    mbedtls_svc_key_id_t id = MBEDTLS_SVC_KEY_ID_INIT;
    if (const psa_status_t status = psa_generate_key(&attributes, &id); status != PSA_SUCCESS) {
        throw std::runtime_error(std::format("cannot generate a key pair (status {})", status));
    }
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    const int result = mbedtls_pk_copy_from_psa(id, &key);
    psa_destroy_key(id);
    std::string pem;
    try {
        if (result != 0) {
            throw std::runtime_error(
                std::format("cannot read the generated key: {}", detail::tlsError(result).message()));
        }
        pem = writePem(key, mbedtls_pk_write_key_pem);
    } catch (...) {
        mbedtls_pk_free(&key);
        throw;
    }
    mbedtls_pk_free(&key);
    // Read back like a key from a file, so that both go through the same checks
    return fromPrivateKey(pem);
}

ServerIdentity ServerIdentity::fromPrivateKey(std::string_view pem)
{
    const auto lock = detail::lockCrypto();
    auto impl = std::make_shared<Impl>();
    // Mbed TLS reads PEM from a text ended by a null character, counted in the length
    const std::string text(pem);
    const int result = mbedtls_pk_parse_key(&impl->key, reinterpret_cast<const unsigned char *>(text.c_str()),
                                            text.size() + 1, nullptr, 0);
    if (result != 0) {
        throw std::invalid_argument(std::format("not a private key: {}", detail::tlsError(result).message()));
    }
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    const bool p256 = mbedtls_pk_get_psa_attributes(&impl->key, PSA_KEY_USAGE_SIGN_HASH, &attributes) == 0 &&
                      psa_get_key_type(&attributes) == PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1) &&
                      psa_get_key_bits(&attributes) == 256;
    psa_reset_key_attributes(&attributes);
    if (!p256) {
        throw std::invalid_argument("the server key must be an ECDSA P-256 private key");
    }
    impl->privatePem = writePem(impl->key, mbedtls_pk_write_key_pem);
    impl->publicPem = writePem(impl->key, mbedtls_pk_write_pubkey_pem);
    impl->fingerprint = detail::fingerprintOf(publicKeyDer(impl->key));
    makeCertificate(*impl);
    return ServerIdentity(std::move(impl));
}

ServerIdentity ServerIdentity::load(const std::filesystem::path &path)
{
    return fromPrivateKey(readFile(path));
}

std::string ServerIdentity::privateKeyPem() const
{
    return _impl->privatePem;
}

std::string ServerIdentity::publicKeyPem() const
{
    return _impl->publicPem;
}

KeyFingerprint ServerIdentity::fingerprint() const
{
    return *_impl->fingerprint;
}

namespace detail
{

TlsSession::TlsSession(const KeyFingerprint &pinned) : _pinned(pinned)
{
    setup(MBEDTLS_SSL_IS_CLIENT);
}

TlsSession::TlsSession(std::shared_ptr<const ServerIdentity::Impl> identity) : _identity(std::move(identity))
{
    setup(MBEDTLS_SSL_IS_SERVER);
}

TlsSession::~TlsSession()
{
    const auto lock = lockCrypto();
    mbedtls_ssl_free(&_ssl);
    mbedtls_ssl_config_free(&_config);
}

void TlsSession::setup(int endpoint)
{
    const auto lock = lockCrypto();
    mbedtls_ssl_config_init(&_config);
    mbedtls_ssl_init(&_ssl);
    int result = mbedtls_ssl_config_defaults(&_config, endpoint, MBEDTLS_SSL_TRANSPORT_STREAM,
                                             MBEDTLS_SSL_PRESET_DEFAULT);
    if (result == 0) {
        // TLS 1.3 only, and only the key exchanges with fresh keys: no resumption from a previous session
        mbedtls_ssl_conf_min_tls_version(&_config, MBEDTLS_SSL_VERSION_TLS1_3);
        mbedtls_ssl_conf_max_tls_version(&_config, MBEDTLS_SSL_VERSION_TLS1_3);
        mbedtls_ssl_conf_tls13_key_exchange_modes(&_config, MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL);
        mbedtls_ssl_conf_ciphersuites(&_config, CipherSuites.data());
        mbedtls_ssl_conf_groups(&_config, Groups.data());
        mbedtls_ssl_conf_sig_algs(&_config, SignatureAlgorithms.data());
        if (endpoint == MBEDTLS_SSL_IS_CLIENT) {
            // There is no certificate authority: the certificate is accepted by verifyCallback() if it holds
            // the pinned key, and the handshake fails otherwise
            mbedtls_ssl_conf_authmode(&_config, MBEDTLS_SSL_VERIFY_OPTIONAL);
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
            mbedtls_ssl_conf_session_tickets(&_config, MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
#endif
        } else {
            // The clients have no certificate: who the player is comes later, with accounts
            mbedtls_ssl_conf_authmode(&_config, MBEDTLS_SSL_VERIFY_NONE);
            // Mbed TLS takes non-const pointers but only reads them, under the lock of lockCrypto()
            auto &identity = const_cast<ServerIdentity::Impl &>(*_identity);
            result = mbedtls_ssl_conf_own_cert(&_config, &identity.certificate, &identity.key);
        }
    }
    if (result == 0) {
        result = mbedtls_ssl_setup(&_ssl, &_config);
    }
    if (result != 0) {
        mbedtls_ssl_free(&_ssl);
        mbedtls_ssl_config_free(&_config);
        throw std::runtime_error(std::format("cannot set up TLS: {}", tlsError(result).message()));
    }
    mbedtls_ssl_set_bio(&_ssl, this, sendCallback, receiveCallback, nullptr);
    if (endpoint == MBEDTLS_SSL_IS_CLIENT) {
        mbedtls_ssl_set_verify(&_ssl, verifyCallback, this);
    }
}

int TlsSession::sendCallback(void *self, const unsigned char *buffer, std::size_t length)
{
    auto &session = *static_cast<TlsSession *>(self);
    const auto *bytes = reinterpret_cast<const std::byte *>(buffer);
    session._output.insert(session._output.end(), bytes, bytes + length);
    return static_cast<int>(length);
}

int TlsSession::receiveCallback(void *self, unsigned char *buffer, std::size_t length)
{
    auto &session = *static_cast<TlsSession *>(self);
    const std::size_t available = session._input.size() - session._inputStart;
    if (available == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    const std::size_t count = std::min(length, available);
    std::copy_n(session._input.begin() + static_cast<std::ptrdiff_t>(session._inputStart), count, buffer);
    session._inputStart += count;
    return static_cast<int>(count);
}

int TlsSession::verifyCallback(void *self, mbedtls_x509_crt *certificate, int depth, std::uint32_t *flags)
{
    auto &session = *static_cast<TlsSession *>(self);
    // Called for each certificate of the chain, the server's own last (depth 0): only its key matters
    if (depth > 0) {
        *flags = 0;
        return 0;
    }
    session._presented = fingerprintOf({certificate->pk_raw.p, certificate->pk_raw.len});
    if (session._presented == session._pinned) {
        // Trusted because of its key, whatever its issuer and its dates
        *flags = 0;
        return 0;
    }
    session._pinRejected = true;
    *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    // A fatal error, which ends the handshake even in the optional mode
    return MBEDTLS_ERR_X509_FATAL_ERROR;
}

void TlsSession::start()
{
    const auto lock = lockCrypto();
    handshake();
}

void TlsSession::handshake()
{
    if (_secured || _error) {
        return;
    }
    const int result = mbedtls_ssl_handshake(&_ssl);
    if (result == MBEDTLS_ERR_SSL_WANT_READ) {
        return;
    }
    if (result != 0) {
        fail(result);
        return;
    }
    if (_pinned && _presented != _pinned) {
        // Cannot happen if Mbed TLS called verifyCallback(): checked again, as it is the whole point
        _pinRejected = true;
        fail(MBEDTLS_ERR_X509_FATAL_ERROR);
        return;
    }
    _secured = true;
    encrypt(_pending);
    _pending.clear();
    _pending.shrink_to_fit();
}

std::vector<std::byte> TlsSession::receive(std::span<const std::byte> bytes)
{
    const auto lock = lockCrypto();
    std::vector<std::byte> decrypted;
    if (_error || _peerClosed) {
        return decrypted;
    }
    // Forgets the bytes already read by Mbed TLS, which keeps a whole record itself
    _input.erase(_input.begin(), _input.begin() + static_cast<std::ptrdiff_t>(_inputStart));
    _inputStart = 0;
    std::ranges::transform(bytes, std::back_inserter(_input),
                           [](std::byte byte) { return static_cast<unsigned char>(byte); });
    handshake();
    while (_secured && !_error) {
        static constexpr std::size_t ChunkSize = MBEDTLS_SSL_IN_CONTENT_LEN;
        const std::size_t size = decrypted.size();
        const std::size_t consumed = _inputStart;
        decrypted.resize(size + ChunkSize);
        const int result =
            mbedtls_ssl_read(&_ssl, reinterpret_cast<unsigned char *>(decrypted.data() + size), ChunkSize);
        decrypted.resize(size + static_cast<std::size_t>(std::max(result, 0)));
        if (result > 0 || result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
            continue;
        }
        if (result == MBEDTLS_ERR_SSL_WANT_READ) {
            // Also returned after a message that carries no data, such as a session ticket, even when more
            // records follow: read on while Mbed TLS takes bytes
            if (_inputStart != consumed && _inputStart < _input.size()) {
                continue;
            }
            break;
        }
        if (result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || result == 0) {
            _peerClosed = true;
            break;
        }
        fail(result);
    }
    return decrypted;
}

void TlsSession::write(std::span<const std::byte> bytes)
{
    const auto lock = lockCrypto();
    if (_error || _peerClosed) {
        return;
    }
    if (!_secured) {
        _pending.insert(_pending.end(), bytes.begin(), bytes.end());
        return;
    }
    encrypt(bytes);
}

void TlsSession::encrypt(std::span<const std::byte> bytes)
{
    std::size_t written = 0;
    while (written < bytes.size() && !_error) {
        // Writes at most one record at a time
        const int result = mbedtls_ssl_write(
            &_ssl, reinterpret_cast<const unsigned char *>(bytes.data() + written), bytes.size() - written);
        if (result < 0) {
            fail(result);
            return;
        }
        written += static_cast<std::size_t>(result);
    }
}

void TlsSession::closeNotify()
{
    const auto lock = lockCrypto();
    if (_secured && !_error) {
        mbedtls_ssl_close_notify(&_ssl);
    }
}

std::vector<std::byte> TlsSession::takeOutput()
{
    return std::exchange(_output, {});
}

std::string TlsSession::cipherSuite() const
{
    const auto lock = lockCrypto();
    const char *name = mbedtls_ssl_get_ciphersuite(&_ssl);
    return name != nullptr ? name : "none";
}

void TlsSession::fail(int code)
{
    if (_error) {
        return;
    }
    _error = _pinRejected ? make_error_code(NetworkError::UntrustedServer) : tlsError(code);
    _pending.clear();
}

} // namespace detail

} // namespace rtype::network
