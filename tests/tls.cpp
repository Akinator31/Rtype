// Tests of TLS in libs/network: key fingerprints, server identities, then TLS connections over the loopback
// (docs/decisions/0014-encryption.md).

#include "rtype/network/Tls.hpp"

#include "rtype/network/Errors.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/TlsConnection.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

using namespace rtype::network;

namespace
{

// A key made for these tests with OpenSSL, never used anywhere else, and its fingerprint computed by OpenSSL:
// openssl pkey -in test.key -pubout -outform DER | sha256sum
constexpr std::string_view TestPrivateKey =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEICyWj7QuVKZBzqnHRdHcCJTLkNFAzNy40t+xgIpenn9ooAoGCCqGSM49\n"
    "AwEHoUQDQgAE08vm7ZPrG3ddhUFar0QqEfppWvtf8EbTYa8jFEeqhYm1g0mHyZT9\n"
    "edZuPPtZEWBSIXkA9BfqBZuR/4GU3wq54w==\n"
    "-----END EC PRIVATE KEY-----\n";
constexpr std::string_view TestPublicKey =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE08vm7ZPrG3ddhUFar0QqEfppWvtf\n"
    "8EbTYa8jFEeqhYm1g0mHyZT9edZuPPtZEWBSIXkA9BfqBZuR/4GU3wq54w==\n"
    "-----END PUBLIC KEY-----\n";
constexpr std::string_view TestFingerprint =
    "099064f5b66e0c38f8ac3f38f97a500da9fbad765cd92c8c214f58ce7129a839";

// An ECDSA key on another curve (P-384), which a server must refuse
constexpr std::string_view P384PrivateKey =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MIGkAgEBBDA5R6hZ4vMIOhJzBrZ/ZffxoIuzjzumRuvRoqhbDNQXGZ+4BT5Pl/if\n"
    "28hG6iHdF8igBwYFK4EEACKhZANiAAQyajKRMmru7hedxbLJRrr22FzxUGErZh0V\n"
    "Cf6SQS3TYu/vnVgHF25HzgR5pCjN7wQ2cuiRVrndCQXI5TDpUGrkI4CKADUNv6w8\n"
    "HawZAUvsNHbtibpnvbDgwbqSXOwY2fE=\n"
    "-----END EC PRIVATE KEY-----\n";

std::vector<std::byte> bytesOf(std::string_view text)
{
    const auto *data = reinterpret_cast<const std::byte *>(text.data());
    return {data, data + text.size()};
}

// A payload whose bytes depend on their position, so that a byte lost or moved is seen
std::vector<std::byte> payloadOf(std::size_t size)
{
    std::vector<std::byte> payload(size);
    for (std::size_t i = 0; i < size; ++i) {
        payload[i] = static_cast<std::byte>((i * 7 + i / 251) % 256);
    }
    return payload;
}

bool contains(const std::vector<std::byte> &data, std::string_view text)
{
    const std::vector<std::byte> needle = bytesOf(text);
    return std::ranges::search(data, needle).begin() != data.end();
}

template <typename Condition>
bool pollUntil(EventLoop &loop, Condition condition,
               std::chrono::milliseconds timeout = std::chrono::seconds(5))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        if (loop.poll() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return true;
}

// A connection accepted by a listener, and the connection that opened it, both not started yet
struct TcpPair {
    std::optional<TcpConnection> server;
    std::optional<TcpConnection> client;
};

void connectPair(EventLoop &loop, TcpListener &listener, TcpPair &pair)
{
    listener.startAccepting(
        [&pair](TcpConnection connection) { pair.server.emplace(std::move(connection)); });
    TcpConnection::connect(loop, listener.localAddress(),
                           [&pair](std::error_code error, std::optional<TcpConnection> connection) {
                               REQUIRE_FALSE(error);
                               pair.client = std::move(connection);
                           });
    REQUIRE(pollUntil(loop, [&pair] { return pair.server && pair.client; }));
}

// What one side of a connection received
struct Side {
    std::vector<std::byte> received;
    std::optional<std::error_code> closed;

    TlsConnection::ReceiveHandler onReceive()
    {
        return [this](std::span<const std::byte> bytes) {
            received.insert(received.end(), bytes.begin(), bytes.end());
        };
    }

    TlsConnection::CloseHandler onClose()
    {
        return [this](std::error_code reason) { closed = reason; };
    }
};

// The client and the server of a TLS connection over the loopback, not started yet
struct TlsPair {
    std::optional<TlsConnection> server;
    std::optional<TlsConnection> client;
};

TlsPair makeTlsPair(EventLoop &loop, TcpListener &listener, const ServerIdentity &identity,
                    const KeyFingerprint &pinned)
{
    TcpPair tcp;
    connectPair(loop, listener, tcp);
    TlsPair pair;
    pair.server.emplace(TlsConnection::server(std::move(*tcp.server), identity));
    pair.client.emplace(TlsConnection::client(std::move(*tcp.client), pinned));
    return pair;
}

// Sits between a client and a server, like an attacker on the path: forwards the bytes of both directions,
// keeps a copy of what it sees, and can change the bytes from the client before forwarding them
struct Relay {
    TcpListener listener;
    std::optional<TcpConnection> toClient;
    std::optional<TcpConnection> toServer;
    std::vector<std::byte> seen;
    bool tamper = false;

    Relay(EventLoop &loop, const Address &server) : listener(loop, Address::loopbackIpv4(0))
    {
        listener.startAccepting([this, &loop, server](TcpConnection connection) {
            toClient.emplace(std::move(connection));
            TcpConnection::connect(loop, server,
                                   [this](std::error_code error, std::optional<TcpConnection> upstream) {
                                       REQUIRE_FALSE(error);
                                       toServer = std::move(upstream);
                                       toServer->start(
                                           [this](std::span<const std::byte> bytes) {
                                               seen.insert(seen.end(), bytes.begin(), bytes.end());
                                               toClient->send(bytes);
                                           },
                                           [this](std::error_code) { toClient->close(); });
                                       toClient->start(
                                           [this](std::span<const std::byte> bytes) {
                                               seen.insert(seen.end(), bytes.begin(), bytes.end());
                                               std::vector<std::byte> forwarded(bytes.begin(), bytes.end());
                                               if (tamper) {
                                                   // The last byte of a record is part of its authentication
                                                   // tag
                                                   forwarded.back() ^= std::byte{0x01};
                                               }
                                               toServer->send(std::move(forwarded));
                                           },
                                           [this](std::error_code) { toServer->close(); });
                                   });
        });
    }
};

std::filesystem::path writeTemporaryFile(std::string_view name, std::string_view content)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream(path, std::ios::binary) << content;
    return path;
}

} // namespace

// --- KeyFingerprint ---

TEST_CASE("A fingerprint survives a round trip through its hexadecimal form", "[network]")
{
    const auto fingerprint = KeyFingerprint::parse(TestFingerprint);
    REQUIRE(fingerprint);
    CHECK(fingerprint->toHex() == TestFingerprint);
    CHECK(fingerprint->bytes()[0] == std::byte{0x09});
    CHECK(fingerprint->bytes()[31] == std::byte{0x39});
}

TEST_CASE("A fingerprint is read in either case", "[network]")
{
    std::string upper(TestFingerprint);
    std::ranges::transform(upper, upper.begin(), [](char c) { return static_cast<char>(std::toupper(c)); });

    CHECK(KeyFingerprint::parse(upper) == KeyFingerprint::parse(TestFingerprint));
}

TEST_CASE("A fingerprint that is not 64 hexadecimal digits is refused", "[network]")
{
    CHECK_FALSE(KeyFingerprint::parse(""));
    CHECK_FALSE(KeyFingerprint::parse(TestFingerprint.substr(1)));
    CHECK_FALSE(KeyFingerprint::parse(std::string(TestFingerprint) + "0"));
    CHECK_FALSE(KeyFingerprint::parse(std::string(TestFingerprint.substr(1)) + "g"));
    CHECK_FALSE(KeyFingerprint::parse(std::string(TestFingerprint.substr(2)) + ":0"));
}

TEST_CASE("The fingerprint of a public key is the one OpenSSL computes", "[network]")
{
    CHECK(KeyFingerprint::ofPublicKey(TestPublicKey).toHex() == TestFingerprint);
}

TEST_CASE("Text that is not a public key has no fingerprint", "[network]")
{
    CHECK_THROWS_AS(KeyFingerprint::ofPublicKey(""), std::invalid_argument);
    CHECK_THROWS_AS(
        KeyFingerprint::ofPublicKey("-----BEGIN PUBLIC KEY-----\nAAAA\n-----END PUBLIC KEY-----\n"),
        std::invalid_argument);
}

// --- ServerIdentity ---

TEST_CASE("A server identity is read from a private key in PEM", "[network]")
{
    const ServerIdentity identity = ServerIdentity::fromPrivateKey(TestPrivateKey);

    CHECK(identity.fingerprint().toHex() == TestFingerprint);
    CHECK(identity.publicKeyPem() == TestPublicKey);
    CHECK(identity.privateKeyPem().starts_with("-----BEGIN EC PRIVATE KEY-----"));
}

TEST_CASE("A generated identity can be saved and read back", "[network]")
{
    const ServerIdentity identity = ServerIdentity::generate();
    const ServerIdentity reloaded = ServerIdentity::fromPrivateKey(identity.privateKeyPem());

    CHECK(reloaded.fingerprint() == identity.fingerprint());
    CHECK(KeyFingerprint::ofPublicKey(identity.publicKeyPem()) == identity.fingerprint());
}

TEST_CASE("Two generated identities have different keys", "[network]")
{
    CHECK(ServerIdentity::generate().fingerprint() != ServerIdentity::generate().fingerprint());
}

TEST_CASE("A server identity refuses anything but an ECDSA P-256 private key", "[network]")
{
    CHECK_THROWS_AS(ServerIdentity::fromPrivateKey(""), std::invalid_argument);
    CHECK_THROWS_AS(ServerIdentity::fromPrivateKey("not a key"), std::invalid_argument);
    CHECK_THROWS_AS(ServerIdentity::fromPrivateKey(TestPublicKey), std::invalid_argument);
    CHECK_THROWS_AS(ServerIdentity::fromPrivateKey(P384PrivateKey), std::invalid_argument);
}

TEST_CASE("Keys are read from files", "[network]")
{
    const auto privatePath = writeTemporaryFile("rtype_tests_tls.key", TestPrivateKey);
    const auto publicPath = writeTemporaryFile("rtype_tests_tls.pub", TestPublicKey);

    CHECK(ServerIdentity::load(privatePath).fingerprint().toHex() == TestFingerprint);
    CHECK(KeyFingerprint::loadPublicKey(publicPath).toHex() == TestFingerprint);

    std::filesystem::remove(privatePath);
    std::filesystem::remove(publicPath);
}

TEST_CASE("A missing key file throws a system error", "[network]")
{
    const auto missing = std::filesystem::temp_directory_path() / "rtype_tests_tls_missing.key";

    CHECK_THROWS_AS(ServerIdentity::load(missing), std::system_error);
    CHECK_THROWS_AS(KeyFingerprint::loadPublicKey(missing), std::system_error);
}

// --- TlsConnection ---

TEST_CASE("Bytes go both ways on a TLS connection, whole and in order", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, identity, identity.fingerprint());
    Side server;
    Side client;

    pair.server->start(server.onReceive(), server.onClose());
    pair.client->start(client.onReceive(), client.onClose());
    // Larger than a TLS record (16 KiB): sent in several records
    const std::vector<std::byte> large = payloadOf(100'000);
    pair.client->send(large);
    pair.server->send(bytesOf("WELCOME"));

    REQUIRE(
        pollUntil(loop, [&] { return server.received.size() == large.size() && !client.received.empty(); }));
    CHECK(server.received == large);
    CHECK(client.received == bytesOf("WELCOME"));
    CHECK_FALSE(server.closed);
    CHECK_FALSE(client.closed);
}

TEST_CASE("A TLS connection keeps the addresses of its TCP connection", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    const TlsPair pair = makeTlsPair(loop, listener, identity, identity.fingerprint());

    CHECK(pair.client->remoteAddress() == listener.localAddress());
    CHECK(pair.server->localAddress() == listener.localAddress());
}

TEST_CASE("A client pinning another key refuses the server", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    const ServerIdentity impostor = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, impostor, identity.fingerprint());
    Side server;
    Side client;

    pair.server->start(server.onReceive(), server.onClose());
    pair.client->start(client.onReceive(), client.onClose());
    pair.client->send(bytesOf("secret"));

    REQUIRE(pollUntil(loop, [&] { return client.closed && server.closed; }));
    CHECK(*client.closed == NetworkError::UntrustedServer);
    // The server learns that the handshake failed, from the alert of the client or from the connection
    // closing
    CHECK(*server.closed);
    CHECK(server.received.empty());
}

TEST_CASE("Nobody on the path reads the bytes, and a changed byte is detected", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    Relay relay(loop, listener.localAddress());
    const ServerIdentity identity = ServerIdentity::generate();
    std::optional<TlsConnection> server;
    listener.startAccepting([&](TcpConnection connection) {
        server.emplace(TlsConnection::server(std::move(connection), identity));
    });
    std::optional<TlsConnection> client;
    TcpConnection::connect(loop, relay.listener.localAddress(),
                           [&](std::error_code error, std::optional<TcpConnection> connection) {
                               REQUIRE_FALSE(error);
                               client.emplace(
                                   TlsConnection::client(std::move(*connection), identity.fingerprint()));
                           });
    REQUIRE(pollUntil(loop, [&] { return server && client; }));
    Side serverSide;
    Side clientSide;
    server->start(serverSide.onReceive(), serverSide.onClose());
    client->start(clientSide.onReceive(), clientSide.onClose());

    client->send(bytesOf("first secret"));
    REQUIRE(pollUntil(loop, [&] { return serverSide.received == bytesOf("first secret"); }));
    CHECK_FALSE(contains(relay.seen, "first secret"));

    relay.tamper = true;
    client->send(bytesOf("second secret"));
    REQUIRE(pollUntil(loop, [&] { return serverSide.closed.has_value(); }));
    CHECK(serverSide.closed->category() == tlsCategory());
    CHECK(serverSide.received == bytesOf("first secret"));
}

TEST_CASE("Bytes sent before the end of the handshake arrive after it", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, identity, identity.fingerprint());
    Side server;

    // Sent before start(), so before the handshake begins
    pair.client->send(bytesOf("HELLO"));
    pair.client->send(bytesOf(" again"));
    pair.client->start([](std::span<const std::byte>) {}, [](std::error_code) {});
    pair.server->start(server.onReceive(), server.onClose());

    REQUIRE(pollUntil(loop, [&] { return server.received.size() == 11; }));
    CHECK(server.received == bytesOf("HELLO again"));
}

TEST_CASE("Closing a TLS connection closes the peer without error", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, identity, identity.fingerprint());
    Side server;
    bool clientClosed = false;

    pair.server->start(server.onReceive(), server.onClose());
    pair.client->start([](std::span<const std::byte>) {}, [&](std::error_code) { clientClosed = true; });
    pair.client->send(bytesOf("BYE"));
    pair.client->close();

    REQUIRE(pollUntil(loop, [&] { return server.closed.has_value(); }));
    CHECK_FALSE(*server.closed);
    CHECK(server.received == bytesOf("BYE"));
    // close() calls no handler
    loop.poll();
    CHECK_FALSE(clientClosed);
}

TEST_CASE("A TLS client talking to a server that does not speak TLS fails", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair tcp;
    connectPair(loop, listener, tcp);
    TlsConnection client =
        TlsConnection::client(std::move(*tcp.client), ServerIdentity::generate().fingerprint());
    Side side;

    tcp.server->start(
        [&](std::span<const std::byte>) { tcp.server->send(bytesOf("220 FTP server ready\r\n")); },
        [](std::error_code) {});
    client.start(side.onReceive(), side.onClose());

    REQUIRE(pollUntil(loop, [&] { return side.closed.has_value(); }));
    CHECK(side.closed->category() == tlsCategory());
    CHECK_FALSE(side.closed->message().empty());
}

TEST_CASE("A TLS server receiving something else than TLS closes the connection", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair tcp;
    connectPair(loop, listener, tcp);
    TlsConnection server = TlsConnection::server(std::move(*tcp.server), ServerIdentity::generate());
    Side side;
    bool clientClosed = false;

    server.start(side.onReceive(), side.onClose());
    tcp.client->start([](std::span<const std::byte>) {}, [&](std::error_code) { clientClosed = true; });
    tcp.client->send(bytesOf("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"));

    REQUIRE(pollUntil(loop, [&] { return side.closed.has_value() && clientClosed; }));
    CHECK(side.closed->category() == tlsCategory());
    CHECK(side.received.empty());
}

TEST_CASE("A connection closed during the handshake reports HandshakeInterrupted", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair tcp;
    connectPair(loop, listener, tcp);
    TlsConnection client =
        TlsConnection::client(std::move(*tcp.client), ServerIdentity::generate().fingerprint());
    Side side;

    client.start(side.onReceive(), side.onClose());
    tcp.server->close();

    REQUIRE(pollUntil(loop, [&] { return side.closed.has_value(); }));
    CHECK(*side.closed == NetworkError::HandshakeInterrupted);
}

TEST_CASE("A connection reset during the handshake reports HandshakeInterrupted", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair tcp;
    connectPair(loop, listener, tcp);
    TlsConnection client =
        TlsConnection::client(std::move(*tcp.client), ServerIdentity::generate().fingerprint());
    Side side;

    client.start(side.onReceive(), side.onClose());
    // The server closes with the ClientHello still unread: the system resets the connection
    pollUntil(loop, [] { return false; }, std::chrono::milliseconds(100));
    tcp.server->close();

    REQUIRE(pollUntil(loop, [&] { return side.closed.has_value(); }));
    CHECK(*side.closed == NetworkError::HandshakeInterrupted);
}

TEST_CASE("Frames go both ways over TLS", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, identity, identity.fingerprint());
    FramedConnection server(std::move(*pair.server));
    FramedConnection client(std::move(*pair.client));
    std::vector<std::uint8_t> serverTypes;
    std::vector<std::byte> clientPayload;

    server.start([&](std::uint8_t type, std::span<const std::byte>) { serverTypes.push_back(type); },
                 [](std::error_code) {});
    client.start(
        [&](std::uint8_t, std::span<const std::byte> payload) {
            clientPayload.assign(payload.begin(), payload.end());
        },
        [](std::error_code) {});
    for (std::uint8_t type = 1; type <= 20; ++type) {
        client.send(type, payloadOf(type * 100));
    }
    server.send(0x80, payloadOf(1000));

    REQUIRE(pollUntil(loop, [&] { return serverTypes.size() == 20 && !clientPayload.empty(); }));
    for (std::uint8_t type = 1; type <= 20; ++type) {
        CHECK(serverTypes[type - 1] == type);
    }
    CHECK(clientPayload == payloadOf(1000));
}

TEST_CASE("Both sides learn once that the handshake ended, before the first bytes", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, identity, identity.fingerprint());
    std::vector<std::string> serverEvents;
    int clientSecured = 0;

    pair.server->start([&](std::span<const std::byte>) { serverEvents.emplace_back("bytes"); },
                       [](std::error_code) {}, [&] { serverEvents.emplace_back("secured"); });
    pair.client->start([](std::span<const std::byte>) {}, [](std::error_code) {}, [&] { ++clientSecured; });
    // Sent before the end of the handshake: it arrives with the last message of the client
    pair.client->send(bytesOf("hello"));

    REQUIRE(pollUntil(loop, [&] { return serverEvents.size() >= 2 && clientSecured == 1; }));
    CHECK(serverEvents.front() == "secured");
    CHECK(std::ranges::count(serverEvents, "secured") == 1);
    pair.client->send(bytesOf("again"));
    REQUIRE(pollUntil(loop, [&] { return serverEvents.size() >= 3; }));
    CHECK(std::ranges::count(serverEvents, "secured") == 1);
    CHECK(clientSecured == 1);
}

TEST_CASE("A handshake that fails is never reported as secured", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    const ServerIdentity impostor = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, impostor, identity.fingerprint());
    Side server;
    Side client;
    int secured = 0;

    pair.server->start(server.onReceive(), server.onClose(), [&] { ++secured; });
    pair.client->start(client.onReceive(), client.onClose(), [&] { ++secured; });

    REQUIRE(pollUntil(loop, [&] { return client.closed && server.closed; }));
    CHECK(secured == 0);
}

TEST_CASE("A framed connection over TLS reports the end of its handshake before its first frame", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, identity, identity.fingerprint());
    FramedConnection server(std::move(*pair.server));
    FramedConnection client(std::move(*pair.client));
    std::vector<std::string> events;

    server.start([&](std::uint8_t, std::span<const std::byte>) { events.emplace_back("frame"); },
                 [](std::error_code) {}, [&] { events.emplace_back("secured"); });
    client.start([](std::uint8_t, std::span<const std::byte>) {}, [](std::error_code) {});
    client.send(0x01, payloadOf(10));

    REQUIRE(pollUntil(loop, [&] { return events.size() == 2; }));
    CHECK(events == std::vector<std::string>{"secured", "frame"});
}

TEST_CASE("Identities and connections can be used from several threads", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    const ServerIdentity identity = ServerIdentity::generate();
    TlsPair pair = makeTlsPair(loop, listener, identity, identity.fingerprint());
    Side server;
    pair.server->start(server.onReceive(), server.onClose());
    pair.client->start([](std::span<const std::byte>) {}, [](std::error_code) {});

    // Another thread uses Mbed TLS while the loop's thread runs the handshake
    std::thread other([&] {
        for (int i = 0; i < 5; ++i) {
            CHECK(ServerIdentity::fromPrivateKey(TestPrivateKey).fingerprint().toHex() == TestFingerprint);
            pair.client->send(bytesOf("x"));
        }
    });
    const bool received = pollUntil(loop, [&] { return server.received.size() == 5; });
    other.join();
    CHECK(received);
}

TEST_CASE("TLS errors have a category and a message", "[network]")
{
    const std::error_code untrusted = NetworkError::UntrustedServer;
    CHECK(untrusted.category() == networkCategory());
    CHECK(untrusted.message() == "the server key is not the pinned one");
    CHECK(std::error_code(NetworkError::HandshakeInterrupted).message() ==
          "connection closed during the TLS handshake");
    CHECK(std::string(tlsCategory().name()) == "rtype.tls");
}
