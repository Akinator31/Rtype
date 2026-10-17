#pragma once

// Private to libs/network: the only place where Mbed TLS is included, with the TLS state of a connection.

#include "rtype/network/Tls.hpp"

#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include <cstddef>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace rtype::network
{

namespace detail
{

/// Mbed TLS is built without its own locks, so its global state (the PSA key store and random generator) must
/// only be used by one thread at a time: every call into it holds this lock. The first lock also initializes
/// PSA Crypto.
/// @throws std::runtime_error if PSA Crypto cannot be initialized.
[[nodiscard]] std::unique_lock<std::recursive_mutex> lockCrypto();

/// An error code of Mbed TLS (negative) as a std::error_code of tlsCategory().
[[nodiscard]] std::error_code tlsError(int code);

/// The fingerprint of a public key in DER (SubjectPublicKeyInfo). Holds the lock of lockCrypto().
[[nodiscard]] KeyFingerprint fingerprintOf(std::span<const unsigned char> der);

} // namespace detail

struct ServerIdentity::Impl {
    mbedtls_pk_context key;
    /// The self-signed certificate presented in the handshakes.
    mbedtls_x509_crt certificate;
    std::string privatePem;
    std::string publicPem;
    std::optional<KeyFingerprint> fingerprint;

    Impl();
    ~Impl();
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
};

namespace detail
{

/// The TLS state of one connection, without any socket: encrypted bytes go in with receive() and out with
/// takeOutput(), decrypted bytes come out of receive(). Used on the loop's thread only.
class TlsSession
{
private:
    mbedtls_ssl_config _config;
    mbedtls_ssl_context _ssl;
    /// Kept alive while the configuration points to its key and certificate (server).
    std::shared_ptr<const ServerIdentity::Impl> _identity;
    /// The key the server must present (client).
    std::optional<KeyFingerprint> _pinned;
    /// The key the server presented (client), for the logs.
    std::optional<KeyFingerprint> _presented;
    bool _pinRejected = false;

    /// Encrypted bytes received, not read by Mbed TLS yet.
    std::vector<unsigned char> _input;
    std::size_t _inputStart = 0;
    /// Encrypted bytes to send.
    std::vector<std::byte> _output;
    /// Bytes given to write() before the end of the handshake.
    std::vector<std::byte> _pending;

    bool _secured = false;
    bool _peerClosed = false;
    std::error_code _error;

    static int sendCallback(void *self, const unsigned char *buffer, std::size_t length);
    static int receiveCallback(void *self, unsigned char *buffer, std::size_t length);
    static int verifyCallback(void *self, mbedtls_x509_crt *certificate, int depth, std::uint32_t *flags);

    void setup(int endpoint);
    void handshake();
    void encrypt(std::span<const std::byte> bytes);
    void fail(int code);

public:
    /// The client side: the server must present the key of this fingerprint.
    explicit TlsSession(const KeyFingerprint &pinned);
    /// The server side.
    explicit TlsSession(std::shared_ptr<const ServerIdentity::Impl> identity);
    ~TlsSession();

    TlsSession(const TlsSession &) = delete;
    TlsSession &operator=(const TlsSession &) = delete;

    /// Starts the handshake: on the client, the first message is then waiting in the output.
    void start();
    /// Gives encrypted bytes received, and returns the bytes they decrypt to.
    [[nodiscard]] std::vector<std::byte> receive(std::span<const std::byte> bytes);
    /// Encrypts bytes to send, or keeps them until the end of the handshake.
    void write(std::span<const std::byte> bytes);
    /// Tells the peer that the connection ends.
    void closeNotify();
    /// The encrypted bytes to send, which the session forgets.
    [[nodiscard]] std::vector<std::byte> takeOutput();

    /// The handshake is done: the peer is authenticated and the bytes are encrypted.
    [[nodiscard]] bool secured() const { return _secured; }
    /// Bytes given to write() wait for the end of the handshake.
    [[nodiscard]] bool hasPending() const { return !_pending.empty(); }
    /// The peer said the connection ends.
    [[nodiscard]] bool peerClosed() const { return _peerClosed; }
    /// The session failed and can no longer be used: why.
    [[nodiscard]] std::error_code error() const { return _error; }
    /// The key the server presented, once received (client).
    [[nodiscard]] const std::optional<KeyFingerprint> &presentedKey() const { return _presented; }
    /// The cipher suite agreed on, once secured.
    [[nodiscard]] std::string cipherSuite() const;
};

} // namespace detail

} // namespace rtype::network
