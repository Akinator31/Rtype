#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace rtype::network
{

class TlsConnection;

/// SHA-256 of a public key, in its standard encoding (the DER SubjectPublicKeyInfo): what the client pins to
/// recognize the server (docs/decisions/0014-encryption.md).
///
/// Written as 64 lowercase hexadecimal digits, the form shown to people and logged:
/// @code
/// const auto key = rtype::network::ServerIdentity::load("server.key").fingerprint();
/// logger.info("Server key {}", key.toHex());
/// @endcode
class KeyFingerprint
{
public:
    static constexpr std::size_t Size = 32;
    using Bytes = std::array<std::byte, Size>;

private:
    Bytes _bytes;

public:
    explicit KeyFingerprint(const Bytes &bytes) : _bytes(bytes) {}

    /// Reads 64 hexadecimal digits, in either case. Nothing if the text is anything else.
    [[nodiscard]] static std::optional<KeyFingerprint> parse(std::string_view hex);

    /// The fingerprint of a public key in PEM ("-----BEGIN PUBLIC KEY-----").
    /// @throws std::invalid_argument if the text is not a public key.
    [[nodiscard]] static KeyFingerprint ofPublicKey(std::string_view pem);

    /// The fingerprint of the public key in a PEM file.
    /// @throws std::system_error if the file cannot be read.
    /// @throws std::invalid_argument if it does not hold a public key.
    [[nodiscard]] static KeyFingerprint loadPublicKey(const std::filesystem::path &path);

    [[nodiscard]] const Bytes &bytes() const { return _bytes; }
    /// 64 lowercase hexadecimal digits.
    [[nodiscard]] std::string toHex() const;

    friend bool operator==(const KeyFingerprint &, const KeyFingerprint &) = default;
};

/// What the server proves its identity with: its private key (ECDSA P-256), and the certificate it presents
/// in the TLS handshakes, made from that key when the identity is created.
///
/// The certificate is self-signed and only carries the public key: the client does not check it against a
/// certificate authority, it compares the key with the one it pins (KeyFingerprint).
///
/// @code
/// const auto identity = rtype::network::ServerIdentity::load("server.key"); // the --key option
/// logger.info("Server key {}", identity.fingerprint().toHex());
/// @endcode
///
/// Copying an identity is cheap: the copies share the same key, which can be used by many connections at
/// once.
class ServerIdentity
{
public:
    struct Impl;

private:
    friend class TlsConnection;

    std::shared_ptr<const Impl> _impl;

    explicit ServerIdentity(std::shared_ptr<const Impl> impl);

public:
    /// Draws a new key pair. For tests and for the key generation tool: a server loads its key.
    [[nodiscard]] static ServerIdentity generate();

    /// Reads a private key in PEM ("-----BEGIN EC PRIVATE KEY-----" or "-----BEGIN PRIVATE KEY-----").
    /// @throws std::invalid_argument if the text is not a private key, or not an ECDSA P-256 key.
    [[nodiscard]] static ServerIdentity fromPrivateKey(std::string_view pem);

    /// Reads a private key from a PEM file.
    /// @throws std::system_error if the file cannot be read.
    /// @throws std::invalid_argument if it does not hold an ECDSA P-256 private key.
    [[nodiscard]] static ServerIdentity load(const std::filesystem::path &path);

    /// The private key in PEM. Secret: it must only be written to a file only the server can read.
    [[nodiscard]] std::string privateKeyPem() const;
    /// The public key in PEM, the file given to the clients.
    [[nodiscard]] std::string publicKeyPem() const;
    /// The fingerprint of the public key, which the clients pin.
    [[nodiscard]] KeyFingerprint fingerprint() const;
};

} // namespace rtype::network
