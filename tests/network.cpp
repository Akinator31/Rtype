// Tests of the network library (libs/network): addresses, event loop, and sockets over the loopback.

#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace rtype::network;

namespace
{

// Polls the loop until the condition is true, or gives up after the timeout
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

// Polls the loop for a while, to let what should not happen happen
void pollFor(EventLoop &loop, std::chrono::milliseconds duration)
{
    pollUntil(loop, [] { return false; }, duration);
}

std::vector<std::byte> bytesOf(std::string_view text)
{
    std::vector<std::byte> bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
    return bytes;
}

std::string textOf(std::span<const std::byte> bytes)
{
    std::string text(bytes.size(), '\0');
    std::transform(bytes.begin(), bytes.end(), text.begin(),
                   [](std::byte b) { return static_cast<char>(b); });
    return text;
}

// Bytes 0, 1, ..., 255, 0, 1, ... so that a missing or reordered byte is detected
std::vector<std::byte> pattern(std::size_t size)
{
    std::vector<std::byte> bytes(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<std::byte>(i % 256);
    }
    return bytes;
}

// A connected pair of TCP connections on the loopback: one accepted by a listener, one opened with connect()
struct TcpPair {
    std::optional<TcpConnection> server;
    std::optional<TcpConnection> client;
};

// The listener's handler keeps a reference to the pair: it is filled in place, not returned
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

} // namespace

// --- Address ---

TEST_CASE("An IPv4 address is parsed and written back", "[network]")
{
    const auto address = Address::parse("192.168.1.10", 4242);

    REQUIRE(address);
    CHECK(address->isIpv4());
    CHECK(address->port() == 4242);
    CHECK(address->ip() == "192.168.1.10");
    CHECK(address->toString() == "192.168.1.10:4242");
    CHECK(address->bytes() == std::vector<std::uint8_t>{192, 168, 1, 10});
}

TEST_CASE("An IPv6 address is written with brackets around the IP", "[network]")
{
    const auto address = Address::parse("::1", 4242);

    REQUIRE(address);
    CHECK(address->isIpv6());
    CHECK(address->toString() == "[::1]:4242");
    CHECK(address->bytes().size() == 16);
}

TEST_CASE("Text that is not an IP address is refused", "[network]")
{
    CHECK_FALSE(Address::parse("localhost", 80));
    CHECK_FALSE(Address::parse("300.1.1.1", 80));
    CHECK_FALSE(Address::parse("", 80));
    CHECK_FALSE(Address::parse("1.2.3.4:80", 80));
}

TEST_CASE("Addresses compare by family, IP and port", "[network]")
{
    const Address a = Address::loopbackIpv4(4242);

    CHECK(a == *Address::parse("127.0.0.1", 4242));
    CHECK(a != a.withPort(4243));
    CHECK(a.withPort(4243).port() == 4243);
    CHECK(Address::anyIpv4() != Address::anyIpv6());
    CHECK(Address() == Address::anyIpv4(0));
}

TEST_CASE("Addresses can be keys of an unordered set and formatted", "[network]")
{
    const std::unordered_set<Address> set{Address::loopbackIpv4(1), Address::loopbackIpv4(2),
                                          Address::loopbackIpv4(1)};

    CHECK(set.size() == 2);
    CHECK(std::format("from {}", Address::loopbackIpv4(4242)) == "from 127.0.0.1:4242");
}

TEST_CASE("localhost resolves to a loopback address, IPv4 first", "[network]")
{
    const std::vector<Address> addresses = resolve("localhost", 4242);

    REQUIRE_FALSE(addresses.empty());
    CHECK(addresses.front().port() == 4242);
    const bool hasLoopback = std::ranges::any_of(
        addresses, [](const Address &a) { return a == Address::loopbackIpv4(4242) || a.ip() == "::1"; });
    CHECK(hasLoopback);
}

TEST_CASE("An IP address written as text resolves to itself", "[network]")
{
    CHECK(resolve("127.0.0.1", 80) == std::vector<Address>{Address::loopbackIpv4(80)});
}

TEST_CASE("A name that does not exist cannot be resolved", "[network]")
{
    // .invalid is reserved and never exists (RFC 6761)
    CHECK_THROWS_AS(resolve("rtype.invalid", 80), std::system_error);
}

// --- EventLoop ---

TEST_CASE("poll() returns at once when there is nothing to do", "[network]")
{
    EventLoop loop;

    CHECK(loop.poll() == 0);
}

TEST_CASE("post() runs a function on the thread that runs the loop", "[network]")
{
    EventLoop loop;
    std::atomic<bool> ran = false;
    std::thread::id runThread;
    std::thread::id postedThread;

    std::thread network([&] {
        runThread = std::this_thread::get_id();
        loop.run();
    });
    loop.post([&] {
        postedThread = std::this_thread::get_id();
        ran = true;
        loop.stop();
    });
    network.join();

    CHECK(ran);
    CHECK(postedThread == runThread);
    CHECK(loop.stopped());
}

TEST_CASE("run() waits for work until stop() is called from another thread", "[network]")
{
    EventLoop loop;
    std::atomic<bool> returned = false;

    std::thread network([&] {
        loop.run();
        returned = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK_FALSE(returned);
    loop.stop();
    network.join();

    CHECK(returned);
}

// --- UDP ---

TEST_CASE("A UDP socket bound to port 0 gets a port from the system", "[network]")
{
    EventLoop loop;
    const UdpSocket socket(loop, Address::loopbackIpv4(0));

    CHECK(socket.localAddress().port() != 0);
    CHECK(socket.localAddress().ip() == "127.0.0.1");
}

TEST_CASE("A datagram goes from one UDP socket to another", "[network]")
{
    EventLoop loop;
    UdpSocket sender(loop, Address::loopbackIpv4(0));
    UdpSocket receiver(loop, Address::loopbackIpv4(0));
    std::optional<std::string> received;
    std::optional<Address> from;

    receiver.startReceiving([&](const Address &source, std::span<const std::byte> datagram) {
        received = textOf(datagram);
        from = source;
    });
    sender.send(receiver.localAddress(), bytesOf("hello"));

    REQUIRE(pollUntil(loop, [&] { return received.has_value(); }));
    CHECK(*received == "hello");
    CHECK(*from == sender.localAddress());
}

TEST_CASE("A UDP socket answers the address a datagram came from", "[network]")
{
    EventLoop loop;
    UdpSocket client(loop, Address::loopbackIpv4(0));
    UdpSocket server(loop, Address::loopbackIpv4(0));
    std::optional<std::string> answer;

    server.startReceiving(
        [&](const Address &source, std::span<const std::byte> datagram) { server.send(source, datagram); });
    client.startReceiving(
        [&](const Address &, std::span<const std::byte> datagram) { answer = textOf(datagram); });
    client.send(server.localAddress(), bytesOf("ping"));

    REQUIRE(pollUntil(loop, [&] { return answer.has_value(); }));
    CHECK(*answer == "ping");
}

TEST_CASE("Opening a UDP socket on a port in use fails", "[network]")
{
    EventLoop loop;
    const UdpSocket first(loop, Address::loopbackIpv4(0));

    CHECK_THROWS_AS(UdpSocket(loop, first.localAddress()), std::system_error);
}

TEST_CASE("Datagrams can be sent from another thread", "[network]")
{
    constexpr int Count = 100;
    EventLoop loop;
    UdpSocket sender(loop, Address::loopbackIpv4(0));
    UdpSocket receiver(loop, Address::loopbackIpv4(0));
    int received = 0;

    receiver.startReceiving([&](const Address &, std::span<const std::byte>) { ++received; });
    std::thread game([&] {
        for (int i = 0; i < Count; ++i) {
            sender.send(receiver.localAddress(), bytesOf("tick"));
        }
    });
    game.join();

    CHECK(pollUntil(loop, [&] { return received == Count; }));
}

TEST_CASE("A closed UDP socket no longer calls its handler", "[network]")
{
    EventLoop loop;
    UdpSocket sender(loop, Address::loopbackIpv4(0));
    UdpSocket receiver(loop, Address::loopbackIpv4(0));
    int received = 0;

    receiver.startReceiving([&](const Address &, std::span<const std::byte>) { ++received; });
    receiver.close();
    sender.send(receiver.localAddress(), bytesOf("late"));
    pollFor(loop, std::chrono::milliseconds(50));

    CHECK(received == 0);
}

TEST_CASE("A UDP socket can be destroyed while it is receiving", "[network]")
{
    EventLoop loop;
    {
        UdpSocket socket(loop, Address::loopbackIpv4(0));
        socket.startReceiving([](const Address &, std::span<const std::byte>) {});
        loop.poll();
    }
    // The pending receive ends after the socket is gone, without touching it
    pollFor(loop, std::chrono::milliseconds(20));
    SUCCEED();
}

// --- TCP ---

TEST_CASE("A TCP listener bound to port 0 gets a port from the system", "[network]")
{
    EventLoop loop;
    const TcpListener listener(loop, Address::loopbackIpv4(0));

    CHECK(listener.localAddress().port() != 0);
}

TEST_CASE("Listening on a TCP port in use fails", "[network]")
{
    EventLoop loop;
    const TcpListener first(loop, Address::loopbackIpv4(0));

    CHECK_THROWS_AS(TcpListener(loop, first.localAddress()), std::system_error);
}

TEST_CASE("A client connects to a listener, and both know each other's address", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);

    CHECK(pair.client->remoteAddress() == listener.localAddress());
    CHECK(pair.server->remoteAddress() == pair.client->localAddress());
    CHECK(pair.server->localAddress() == listener.localAddress());
}

TEST_CASE("Bytes go both ways on a TCP connection", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    std::string serverReceived;
    std::string clientReceived;

    pair.server->start([&](std::span<const std::byte> bytes) { serverReceived += textOf(bytes); },
                       [](std::error_code) {});
    pair.client->start([&](std::span<const std::byte> bytes) { clientReceived += textOf(bytes); },
                       [](std::error_code) {});
    pair.client->send(bytesOf("HELLO"));
    pair.server->send(bytesOf("WELCOME"));

    REQUIRE(pollUntil(loop, [&] { return serverReceived.size() == 5 && clientReceived.size() == 7; }));
    CHECK(serverReceived == "HELLO");
    CHECK(clientReceived == "WELCOME");
}

TEST_CASE("A large TCP transfer arrives complete and in order", "[network]")
{
    constexpr std::size_t Size = 1024 * 1024;
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    const std::vector<std::byte> sent = pattern(Size);
    std::vector<std::byte> received;

    pair.server->start(
        [&](std::span<const std::byte> bytes) {
            received.insert(received.end(), bytes.begin(), bytes.end());
        },
        [](std::error_code) {});
    pair.client->start([](std::span<const std::byte>) {}, [](std::error_code) {});
    // Sent in uneven pieces: the receiver sees its own pieces, not these ones
    for (std::size_t offset = 0; offset < Size; offset += 1000) {
        const std::size_t length = std::min<std::size_t>(1000, Size - offset);
        pair.client->send(std::span(sent).subspan(offset, length));
    }

    REQUIRE(pollUntil(loop, [&] { return received.size() >= Size; }));
    CHECK(received == sent);
}

TEST_CASE("The peer closing a TCP connection calls the close handler without error", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    std::optional<std::error_code> reason;

    pair.server->start([](std::span<const std::byte>) {}, [&](std::error_code error) { reason = error; });
    pair.client->close();

    REQUIRE(pollUntil(loop, [&] { return reason.has_value(); }));
    CHECK_FALSE(*reason);
}

TEST_CASE("Destroying a TCP connection closes it for the peer", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    bool closed = false;

    pair.server->start([](std::span<const std::byte>) {}, [&](std::error_code) { closed = true; });
    pair.client.reset();

    CHECK(pollUntil(loop, [&] { return closed; }));
}

TEST_CASE("close() sends the pending bytes before closing, and does not call the close handler", "[network]")
{
    constexpr std::size_t Size = 256 * 1024;
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    const std::vector<std::byte> sent = pattern(Size);
    std::vector<std::byte> received;
    bool clientClosed = false;
    bool serverHandlerCalled = false;

    pair.client->start(
        [&](std::span<const std::byte> bytes) {
            received.insert(received.end(), bytes.begin(), bytes.end());
        },
        [&](std::error_code) { clientClosed = true; });
    pair.server->start([](std::span<const std::byte>) {},
                       [&](std::error_code) { serverHandlerCalled = true; });
    pair.server->send(sent);
    pair.server->close();

    REQUIRE(pollUntil(loop, [&] { return clientClosed; }));
    CHECK(received == sent);
    CHECK_FALSE(serverHandlerCalled);
}

TEST_CASE("Connecting to a port where nobody listens fails", "[network]")
{
    EventLoop loop;
    Address closedPort;
    {
        // A port that was free a moment ago, and nobody listens on any more
        TcpListener listener(loop, Address::loopbackIpv4(0));
        closedPort = listener.localAddress();
    }
    loop.poll();
    std::optional<std::error_code> result;
    bool gotConnection = false;

    TcpConnection::connect(loop, closedPort,
                           [&](std::error_code error, std::optional<TcpConnection> connection) {
                               result = error;
                               gotConnection = connection.has_value();
                           });

    REQUIRE(pollUntil(loop, [&] { return result.has_value(); }));
    CHECK(*result);
    CHECK_FALSE(gotConnection);
}

TEST_CASE("Connecting gives up after the timeout", "[network]")
{
    EventLoop loop;
    std::optional<std::error_code> result;

    // 192.0.2.0/24 is reserved for documentation (RFC 5737): nothing answers there. Depending on the network,
    // the attempt times out, or fails at once because the address is unreachable.
    TcpConnection::connect(
        loop, *Address::parse("192.0.2.1", 4242),
        [&](std::error_code error, std::optional<TcpConnection>) { result = error; },
        std::chrono::milliseconds(100));

    REQUIRE(pollUntil(loop, [&] { return result.has_value(); }, std::chrono::seconds(2)));
    CHECK(*result);
}
