// Tests of the TCP framing of libs/network: the frame decoder alone, then framed connections over the
// loopback (docs/protocol/lobby.md, section 3).

#include "rtype/network/Errors.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Frame.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/TcpListener.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using namespace rtype::network;

namespace
{

std::vector<std::byte> bytes(std::initializer_list<int> values)
{
    std::vector<std::byte> result;
    for (const int value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

std::vector<std::byte> copy(std::span<const std::byte> data)
{
    return {data.begin(), data.end()};
}

// A payload whose bytes depend on its index, so that two frames never look alike
std::vector<std::byte> payloadOf(std::size_t index, std::size_t size)
{
    std::vector<std::byte> payload(size);
    for (std::size_t i = 0; i < size; ++i) {
        payload[i] = static_cast<std::byte>((index * 31 + i) % 256);
    }
    return payload;
}

struct Received {
    std::uint8_t type;
    std::vector<std::byte> payload;
};

// Feeds bytes to a decoder and collects the frames it gives
std::vector<Received> decodeAll(FrameDecoder &decoder, std::span<const std::byte> data)
{
    decoder.feed(data);
    std::vector<Received> frames;
    while (const auto frame = decoder.next()) {
        frames.push_back({frame->type, copy(frame->payload)});
    }
    return frames;
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

} // namespace

// --- makeFrame ---

TEST_CASE("makeFrame writes the HELLO frame of the specification", "[network]")
{
    // docs/protocol/lobby.md, section 3: HELLO with version 1 and the name "Pavel"
    const std::vector<std::byte> payload = bytes({0x00, 0x01, 0x05, 0x50, 0x61, 0x76, 0x65, 0x6C});

    CHECK(makeFrame(0x01, payload) ==
          bytes({0x00, 0x09, 0x01, 0x00, 0x01, 0x05, 0x50, 0x61, 0x76, 0x65, 0x6C}));
}

TEST_CASE("makeFrame accepts an empty payload and refuses one above the maximum", "[network]")
{
    CHECK(makeFrame(0x06, {}) == bytes({0x00, 0x01, 0x06}));
    CHECK(makeFrame(0x01, std::vector<std::byte>(MaxFramePayloadSize)).size() == MaxFrameSize);
    CHECK_THROWS_AS(makeFrame(0x01, std::vector<std::byte>(MaxFramePayloadSize + 1)), std::length_error);
}

// --- FrameDecoder ---

TEST_CASE("A whole frame is decoded with its type and payload", "[network]")
{
    FrameDecoder decoder;
    const auto frames = decodeAll(decoder, makeFrame(0x82, bytes({1, 2, 3})));

    REQUIRE(frames.size() == 1);
    CHECK(frames[0].type == 0x82);
    CHECK(frames[0].payload == bytes({1, 2, 3}));
    CHECK(decoder.buffered() == 0);
    CHECK_FALSE(decoder.failed());
}

TEST_CASE("A frame arriving byte by byte is given only once complete", "[network]")
{
    FrameDecoder decoder;
    const std::vector<std::byte> frame = makeFrame(0x01, bytes({10, 20, 30}));

    for (std::size_t i = 0; i + 1 < frame.size(); ++i) {
        CHECK(decodeAll(decoder, std::span(frame).subspan(i, 1)).empty());
    }
    const auto frames = decodeAll(decoder, std::span(frame).last(1));

    REQUIRE(frames.size() == 1);
    CHECK(frames[0].payload == bytes({10, 20, 30}));
}

TEST_CASE("Several frames in one piece are all decoded, in order", "[network]")
{
    std::vector<std::byte> stream;
    for (std::uint8_t type = 1; type <= 3; ++type) {
        const auto frame = makeFrame(type, payloadOf(type, type));
        stream.insert(stream.end(), frame.begin(), frame.end());
    }
    FrameDecoder decoder;
    const auto frames = decodeAll(decoder, stream);

    REQUIRE(frames.size() == 3);
    for (std::uint8_t type = 1; type <= 3; ++type) {
        CHECK(frames[type - 1].type == type);
        CHECK(frames[type - 1].payload == payloadOf(type, type));
    }
}

TEST_CASE("A frame of the maximum size is accepted", "[network]")
{
    FrameDecoder decoder;
    const auto frames = decodeAll(decoder, makeFrame(0x82, payloadOf(0, MaxFramePayloadSize)));

    REQUIRE(frames.size() == 1);
    CHECK(frames[0].payload.size() == MaxFramePayloadSize);
}

TEST_CASE("A size of 0 makes the decoder fail", "[network]")
{
    FrameDecoder decoder;

    CHECK(decodeAll(decoder, bytes({0x00, 0x00, 0x01})).empty());
    CHECK(decoder.failed());
    CHECK(decoder.error() == NetworkError::EmptyFrame);
}

TEST_CASE("A size above the maximum fails as soon as the size is read", "[network]")
{
    FrameDecoder decoder;

    // 16383 bytes announced: one more than the maximum. Only the size is sent.
    CHECK(decodeAll(decoder, bytes({0x3F, 0xFF})).empty());
    CHECK(decoder.failed());
    CHECK(decoder.error() == NetworkError::FrameTooLarge);
    CHECK(decoder.buffered() == 0);
}

TEST_CASE("A failed decoder ignores everything after", "[network]")
{
    FrameDecoder decoder;
    (void)decodeAll(decoder, bytes({0xFF, 0xFF}));

    CHECK(decodeAll(decoder, makeFrame(0x01, bytes({1}))).empty());
    CHECK(decoder.buffered() == 0);
    CHECK(decoder.error() == NetworkError::FrameTooLarge);
}

TEST_CASE("A valid frame before an invalid one is still decoded", "[network]")
{
    std::vector<std::byte> stream = makeFrame(0x02, {});
    stream.push_back(std::byte{0x00});
    stream.push_back(std::byte{0x00});
    FrameDecoder decoder;
    const auto frames = decodeAll(decoder, stream);

    REQUIRE(frames.size() == 1);
    CHECK(frames[0].type == 0x02);
    CHECK(decoder.failed());
}

TEST_CASE("Frames cut at random places are all decoded, and the buffer stays bounded", "[network]")
{
    std::vector<std::vector<std::byte>> payloads;
    std::vector<std::byte> stream;
    std::mt19937 random(42); // fixed seed: the same pieces at every run
    for (std::size_t i = 0; i < 300; ++i) {
        payloads.push_back(payloadOf(i, std::uniform_int_distribution<std::size_t>(0, 3000)(random)));
        const auto frame = makeFrame(static_cast<std::uint8_t>(i % 256), payloads.back());
        stream.insert(stream.end(), frame.begin(), frame.end());
    }

    FrameDecoder decoder;
    std::vector<Received> frames;
    std::size_t largestBuffer = 0;
    for (std::size_t offset = 0; offset < stream.size();) {
        const std::size_t length =
            std::min(std::uniform_int_distribution<std::size_t>(1, 5000)(random), stream.size() - offset);
        for (auto &frame : decodeAll(decoder, std::span(stream).subspan(offset, length))) {
            frames.push_back(std::move(frame));
        }
        largestBuffer = std::max(largestBuffer, decoder.buffered());
        offset += length;
    }

    REQUIRE(frames.size() == payloads.size());
    for (std::size_t i = 0; i < payloads.size(); ++i) {
        CHECK(frames[i].type == i % 256);
        CHECK(frames[i].payload == payloads[i]);
    }
    CHECK(largestBuffer < MaxFrameSize);
    CHECK_FALSE(decoder.failed());
}

// --- FramedConnection ---

TEST_CASE("Frames go both ways on a framed connection, whole and in order", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    FramedConnection server(std::move(*pair.server));
    FramedConnection client(std::move(*pair.client));
    std::vector<Received> serverReceived;
    std::vector<Received> clientReceived;

    server.start([&](std::uint8_t type,
                     std::span<const std::byte> payload) { serverReceived.push_back({type, copy(payload)}); },
                 [](std::error_code) {});
    client.start([&](std::uint8_t type,
                     std::span<const std::byte> payload) { clientReceived.push_back({type, copy(payload)}); },
                 [](std::error_code) {});
    for (std::uint8_t i = 0; i < 50; ++i) {
        client.send(i, payloadOf(i, i * 10));
    }
    server.send(0x81, payloadOf(7, MaxFramePayloadSize));

    REQUIRE(pollUntil(loop, [&] { return serverReceived.size() == 50 && clientReceived.size() == 1; }));
    for (std::uint8_t i = 0; i < 50; ++i) {
        CHECK(serverReceived[i].type == i);
        CHECK(serverReceived[i].payload == payloadOf(i, i * 10));
    }
    CHECK(clientReceived[0].type == 0x81);
    CHECK(clientReceived[0].payload == payloadOf(7, MaxFramePayloadSize));
}

TEST_CASE("A framed connection keeps the addresses of its TCP connection", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    const Address remote = pair.client->remoteAddress();
    const FramedConnection client(std::move(*pair.client));

    CHECK(client.remoteAddress() == remote);
    CHECK(client.remoteAddress() == listener.localAddress());
}

TEST_CASE("A peer sending a frame too large is disconnected with FrameTooLarge", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    FramedConnection server(std::move(*pair.server));
    std::optional<std::error_code> reason;
    bool peerClosed = false;

    server.start([](std::uint8_t, std::span<const std::byte>) {},
                 [&](std::error_code error) { reason = error; });
    pair.client->start([](std::span<const std::byte>) {}, [&](std::error_code) { peerClosed = true; });
    pair.client->send(bytes({0xFF, 0xFF, 0x01}));

    REQUIRE(pollUntil(loop, [&] { return reason.has_value() && peerClosed; }));
    CHECK(*reason == NetworkError::FrameTooLarge);
}

TEST_CASE("Frames received before an invalid one are still handled", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    FramedConnection server(std::move(*pair.server));
    std::vector<std::uint8_t> types;
    std::optional<std::error_code> reason;

    server.start([&](std::uint8_t type, std::span<const std::byte>) { types.push_back(type); },
                 [&](std::error_code error) { reason = error; });
    std::vector<std::byte> stream = makeFrame(0x01, {});
    stream.push_back(std::byte{0x00});
    stream.push_back(std::byte{0x00});
    pair.client->send(std::move(stream));

    REQUIRE(pollUntil(loop, [&] { return reason.has_value(); }));
    CHECK(types == std::vector<std::uint8_t>{0x01});
    CHECK(*reason == NetworkError::EmptyFrame);
}

TEST_CASE("close() from a frame handler stops the frames that follow", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    FramedConnection server(std::move(*pair.server));
    int handled = 0;
    bool closeHandlerCalled = false;
    bool peerClosed = false;

    server.start(
        [&](std::uint8_t, std::span<const std::byte>) {
            ++handled;
            server.close();
        },
        [&](std::error_code) { closeHandlerCalled = true; });
    pair.client->start([](std::span<const std::byte>) {}, [&](std::error_code) { peerClosed = true; });
    std::vector<std::byte> stream;
    for (int i = 0; i < 5; ++i) {
        const auto frame = makeFrame(0x01, {});
        stream.insert(stream.end(), frame.begin(), frame.end());
    }
    pair.client->send(std::move(stream)); // five frames, in one piece

    REQUIRE(pollUntil(loop, [&] { return peerClosed; }));
    CHECK(handled == 1);
    CHECK_FALSE(closeHandlerCalled);
}

TEST_CASE("The peer closing a framed connection calls the close handler without error", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    FramedConnection server(std::move(*pair.server));
    FramedConnection client(std::move(*pair.client));
    std::optional<std::error_code> reason;
    std::vector<std::uint8_t> types;

    server.start([&](std::uint8_t type, std::span<const std::byte>) { types.push_back(type); },
                 [&](std::error_code error) { reason = error; });
    client.start([](std::uint8_t, std::span<const std::byte>) {}, [](std::error_code) {});
    client.send(0x05, {});
    client.close(); // after the frame, which is sent first

    REQUIRE(pollUntil(loop, [&] { return reason.has_value(); }));
    CHECK(types == std::vector<std::uint8_t>{0x05});
    CHECK_FALSE(*reason);
}

TEST_CASE("Sending a payload above the maximum throws at once", "[network]")
{
    EventLoop loop;
    TcpListener listener(loop, Address::loopbackIpv4(0));
    TcpPair pair;
    connectPair(loop, listener, pair);
    FramedConnection client(std::move(*pair.client));

    CHECK_THROWS_AS(client.send(0x01, std::vector<std::byte>(MaxFramePayloadSize + 1)), std::length_error);
}

TEST_CASE("Network errors have a category and a message", "[network]")
{
    const std::error_code error = NetworkError::FrameTooLarge;

    CHECK(error.category() == networkCategory());
    CHECK(std::string(error.category().name()) == "rtype.network");
    CHECK(error.message() == "frame larger than the maximum");
    CHECK(std::error_code(NetworkError::EmptyFrame).message() == "empty frame");
}
