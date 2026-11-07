// Tests of the reliability layer of libs/network over UDP: the body of the datagrams, then a MessageChannel
// alone, then two channels talking through a fake network that loses, duplicates, delays and reorders the
// datagrams (docs/protocol/game.md, sections 5 and 6).

#include "rtype/network/DatagramBody.hpp"
#include "rtype/network/MessageChannel.hpp"
#include "rtype/network/Writer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace rtype::network;
using namespace std::chrono_literals;
using Clock = MessageChannel::Clock;

namespace
{

std::vector<std::byte> bytesOf(std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

std::vector<std::byte> fromHex(std::string_view hex)
{
    std::vector<std::byte> bytes;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::byte>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16)));
    }
    return bytes;
}

std::vector<std::byte> numbered(std::uint32_t value)
{
    Writer writer;
    writer.writeU32(value);
    return writer.take();
}

std::uint32_t numberOf(const std::vector<std::byte> &payload)
{
    std::uint32_t value = 0;
    for (const std::byte byte : payload) {
        value = (value << 8) | std::to_integer<std::uint32_t>(byte);
    }
    return value;
}

// Sends what `from` has to send straight to `to`, with no loss
std::vector<ReceivedMessage> transfer(MessageChannel &from, MessageChannel &to, Clock::time_point now)
{
    const auto datagram = from.next(now);
    REQUIRE(datagram);
    const auto received = to.receive(datagram->packetNumber, datagram->body, now);
    REQUIRE(received);
    return *received;
}

// One direction of a fake network: each datagram is lost, duplicated, and delayed at random, so that they
// arrive out of order
class FakeLink
{
private:
    std::mt19937 _random;
    double _loss;
    double _duplication;
    Clock::duration _minDelay;
    Clock::duration _maxDelay;
    std::multimap<Clock::time_point, OutgoingDatagram> _inFlight;

    Clock::duration delay()
    {
        std::uniform_int_distribution<Clock::rep> range(_minDelay.count(), _maxDelay.count());
        return Clock::duration(range(_random));
    }

public:
    FakeLink(unsigned seed, double loss, double duplication, Clock::duration minDelay,
             Clock::duration maxDelay)
        : _random(seed), _loss(loss), _duplication(duplication), _minDelay(minDelay), _maxDelay(maxDelay)
    {
    }

    void setLoss(double loss) { _loss = loss; }

    void send(const OutgoingDatagram &datagram, Clock::time_point now)
    {
        std::uniform_real_distribution<double> chance(0.0, 1.0);
        if (chance(_random) < _loss) {
            return;
        }
        _inFlight.emplace(now + delay(), datagram);
        if (chance(_random) < _duplication) {
            _inFlight.emplace(now + delay(), datagram);
        }
    }

    // The datagrams arrived by `now`
    std::vector<OutgoingDatagram> arrived(Clock::time_point now)
    {
        std::vector<OutgoingDatagram> result;
        while (!_inFlight.empty() && _inFlight.begin()->first <= now) {
            result.push_back(_inFlight.begin()->second);
            _inFlight.erase(_inFlight.begin());
        }
        return result;
    }
};

// One side of a game: sends a datagram every tick (16 ms), with everything it has to send
struct Peer {
    MessageChannel channel;
    std::vector<ReceivedMessage> received;
    std::size_t datagramsDropped = 0;

    void sendAll(FakeLink &link, Clock::time_point now)
    {
        do {
            const auto datagram = channel.next(now);
            REQUIRE(datagram);
            link.send(*datagram, now);
        } while (channel.hasMessagesToSend(now));
    }

    void receiveAll(FakeLink &link, Clock::time_point now)
    {
        for (const OutgoingDatagram &datagram : link.arrived(now)) {
            if (const auto messages = channel.receive(datagram.packetNumber, datagram.body, now)) {
                received.insert(received.end(), messages->begin(), messages->end());
            } else {
                ++datagramsDropped;
            }
        }
    }
};

constexpr std::uint8_t ReliableType = 0x81;
constexpr std::uint8_t UnreliableType = 0x80;

} // namespace

// --- Body ---

TEST_CASE("A body is the one the specification describes", "[network]")
{
    const std::vector<std::byte> fire = bytesOf("fire");
    const std::vector<std::byte> move = bytesOf("up");
    const DatagramBody body{41, 0xFFFFFFFF, {{0x40, false, 0, fire}, {0x81, true, 0x1234, move}}, 250};

    // ack 41, all 32 before, acknowledged 25 ms after it came, 2 messages; 0x40 unreliable "fire"; 0x81
    // reliable id 0x1234 "up"
    CHECK(encodeBody(body) == fromHex("0000000000000029ffffffff00fa02"
                                      "40000004"
                                      "66697265"
                                      "810112340002"
                                      "7570"));
}

TEST_CASE("A body survives a round trip", "[network]")
{
    const std::vector<std::byte> payload = bytesOf("snapshot");
    const DatagramBody body{7, 0x5, {{0x80, false, 0, payload}, {0x02, true, 65535, {}}}, MaxAckDelay};

    const std::vector<std::byte> bytes = encodeBody(body);
    const auto decoded = decodeBody(bytes);

    REQUIRE(decoded);
    CHECK(decoded->ack == 7);
    CHECK(decoded->ackBits == 0x5);
    CHECK(decoded->ackDelay == MaxAckDelay);
    REQUIRE(decoded->messages.size() == 2);
    CHECK(decoded->messages[0].type == 0x80);
    CHECK_FALSE(decoded->messages[0].reliable);
    CHECK(std::vector(decoded->messages[0].payload.begin(), decoded->messages[0].payload.end()) == payload);
    CHECK(decoded->messages[1].reliable);
    CHECK(decoded->messages[1].id == 65535);
    CHECK(decoded->messages[1].payload.empty());
}

TEST_CASE("A body without messages is a heartbeat", "[network]")
{
    const std::vector<std::byte> bytes = encodeBody({3, 0, {}});

    CHECK(bytes.size() == BodyHeaderSize);
    const auto decoded = decodeBody(bytes);
    REQUIRE(decoded);
    CHECK(decoded->messages.empty());
}

TEST_CASE("A malformed body is refused", "[network]")
{
    const std::vector<std::byte> valid = encodeBody({1, 0, {{0x40, false, 0, bytesOf("abc")}}});
    REQUIRE(decodeBody(valid));

    std::vector<std::byte> trailing = valid;
    trailing.push_back(std::byte{0});
    const std::vector<std::byte> truncated(valid.begin(), valid.end() - 1);
    std::vector<std::byte> moreMessages = valid;
    moreMessages[14] = std::byte{2};
    std::vector<std::byte> reservedFlag = valid;
    reservedFlag[16] = std::byte{0x02};
    std::vector<std::byte> sizeTooLarge = valid;
    sizeTooLarge[18] = std::byte{4};

    CHECK_FALSE(decodeBody(trailing));
    CHECK_FALSE(decodeBody(truncated));
    CHECK_FALSE(decodeBody(moreMessages));
    CHECK_FALSE(decodeBody(reservedFlag));
    CHECK_FALSE(decodeBody(sizeTooLarge));
    CHECK_FALSE(decodeBody({}));
}

TEST_CASE("A body larger than a datagram cannot be written", "[network]")
{
    const std::vector<std::byte> largest(MaxUnreliablePayloadSize);
    const std::vector<std::byte> tooLarge(MaxUnreliablePayloadSize + 1);

    CHECK(encodeBody({0, 0, {{0x80, false, 0, largest}}}).size() == MaxDatagramBodySize);
    CHECK_THROWS_AS(encodeBody({0, 0, {{0x80, false, 0, tooLarge}}}), std::length_error);
    CHECK_THROWS_AS(encodeBody({0, 0, std::vector<BodyMessage>(MaxBodyMessages + 1)}), std::length_error);
}

TEST_CASE("Random bodies never crash the decoder", "[network]")
{
    std::mt19937 random(99);
    std::uniform_int_distribution<int> byte(0, 255);
    std::uniform_int_distribution<std::size_t> size(0, MaxDatagramBodySize);

    for (int i = 0; i < 5000; ++i) {
        std::vector<std::byte> body(size(random));
        for (auto &value : body) {
            value = static_cast<std::byte>(byte(random));
        }
        // Small counts half of the time, to reach the messages
        if (body.size() > 12 && i % 2 == 0) {
            body[12] = static_cast<std::byte>(byte(random) % 4);
        }
        (void)decodeBody(body);
    }
}

// --- MessageChannel ---

TEST_CASE("Packets are numbered from 1 and acknowledged in the next datagram", "[network]")
{
    MessageChannel a;
    MessageChannel b;
    const Clock::time_point now{};

    const auto first = a.next(now);
    REQUIRE(first);
    CHECK(first->packetNumber == 1);
    REQUIRE(b.receive(first->packetNumber, first->body, now));

    const auto answer = b.next(now);
    REQUIRE(answer);
    const auto decoded = decodeBody(answer->body);
    REQUIRE(decoded);
    CHECK(decoded->ack == 1);
    CHECK(decoded->ackBits == 0);
}

TEST_CASE("Messages are delivered with their type and payload", "[network]")
{
    MessageChannel a;
    MessageChannel b;
    const Clock::time_point now{};

    a.send(UnreliableType, bytesOf("snapshot"), Delivery::Unreliable);
    a.send(ReliableType, bytesOf("destroyed"), Delivery::Reliable);

    const std::vector<ReceivedMessage> received = transfer(a, b, now);
    REQUIRE(received.size() == 2);
    CHECK(received[0] == ReceivedMessage{ReliableType, bytesOf("destroyed")});
    CHECK(received[1] == ReceivedMessage{UnreliableType, bytesOf("snapshot")});
}

TEST_CASE("A reliable message is sent again until acknowledged, and then forgotten", "[network]")
{
    MessageChannel a;
    MessageChannel b;
    Clock::time_point now{};
    a.send(ReliableType, bytesOf("game over"), Delivery::Reliable);

    // Lost
    REQUIRE(a.next(now));
    CHECK(a.pendingReliable() == 1);

    // Not sent again before the resend delay
    now += a.resendDelay() - 1ms;
    CHECK_FALSE(a.hasMessagesToSend(now));
    const auto early = a.next(now);
    REQUIRE(early);
    CHECK(decodeBody(early->body)->messages.empty());

    // Sent again once due, and received
    now += 1ms;
    CHECK(a.hasMessagesToSend(now));
    const std::vector<ReceivedMessage> received = transfer(a, b, now);
    REQUIRE(received.size() == 1);
    CHECK(received[0].payload == bytesOf("game over"));

    // Acknowledged by the next datagram of b, then never sent again
    CHECK(transfer(b, a, now).empty());
    CHECK(a.pendingReliable() == 0);
    now += 10s;
    CHECK_FALSE(a.hasMessagesToSend(now));
}

TEST_CASE("Reliable messages are delivered once and in order, whatever the order of the datagrams",
          "[network]")
{
    MessageChannel a;
    MessageChannel b;
    Clock::time_point now{};
    std::vector<OutgoingDatagram> datagrams;
    for (std::uint32_t i = 0; i < 5; ++i) {
        a.send(ReliableType, numbered(i), Delivery::Reliable);
        const auto datagram = a.next(now);
        REQUIRE(datagram);
        datagrams.push_back(*datagram);
    }

    std::vector<std::uint32_t> order;
    for (const std::size_t index : {4, 2, 0, 3, 1}) {
        const auto received = b.receive(datagrams[index].packetNumber, datagrams[index].body, now);
        REQUIRE(received);
        for (const ReceivedMessage &message : *received) {
            order.push_back(numberOf(message.payload));
        }
    }
    CHECK(order == std::vector<std::uint32_t>{0, 1, 2, 3, 4});

    // The same datagram again is refused, and a resent copy in a new datagram is not delivered twice
    CHECK_FALSE(b.receive(datagrams[0].packetNumber, datagrams[0].body, now));
    now += a.resendDelay();
    const auto resent = a.next(now);
    REQUIRE(resent);
    REQUIRE(decodeBody(resent->body)->messages.size() == 5);
    const auto again = b.receive(resent->packetNumber, resent->body, now);
    REQUIRE(again);
    CHECK(again->empty());
}

TEST_CASE("Reliable message ids wrap from 65535 to 0", "[network]")
{
    MessageChannel a;
    MessageChannel b;
    const Clock::time_point now{};

    std::uint32_t delivered = 0;
    for (std::uint32_t i = 0; i < 65536 + 10; ++i) {
        REQUIRE(a.send(ReliableType, numbered(i), Delivery::Reliable));
        for (const ReceivedMessage &message : transfer(a, b, now)) {
            CHECK(numberOf(message.payload) == delivered);
            ++delivered;
        }
        // Acknowledged at once, so that they do not pile up
        (void)transfer(b, a, now);
    }
    CHECK(delivered == 65536 + 10);
}

TEST_CASE("A channel fails when the peer stops acknowledging", "[network]")
{
    MessageChannel a;
    const Clock::time_point now{};

    for (std::size_t i = 0; i < MessageChannel::MaxPendingReliable; ++i) {
        REQUIRE(a.send(ReliableType, numbered(static_cast<std::uint32_t>(i)), Delivery::Reliable));
    }
    CHECK_FALSE(a.failed());
    CHECK_FALSE(a.send(ReliableType, numbered(0), Delivery::Reliable));

    CHECK(a.failed());
    CHECK_FALSE(a.next(now));
    CHECK_FALSE(a.hasMessagesToSend(now));
    CHECK_FALSE(a.accepts(1));
    CHECK_FALSE(a.send(UnreliableType, numbered(0), Delivery::Unreliable));
}

TEST_CASE("Messages too large for a datagram are refused, and large ones go in datagrams of their own",
          "[network]")
{
    MessageChannel a;
    MessageChannel b;
    const Clock::time_point now{};

    CHECK_THROWS_AS(
        a.send(UnreliableType, std::vector<std::byte>(MaxUnreliablePayloadSize + 1), Delivery::Unreliable),
        std::length_error);
    CHECK_THROWS_AS(
        a.send(ReliableType, std::vector<std::byte>(MaxReliablePayloadSize + 1), Delivery::Reliable),
        std::length_error);

    // Two snapshot parts that fill a datagram each, and a small reliable message
    a.send(UnreliableType, std::vector<std::byte>(MaxUnreliablePayloadSize), Delivery::Unreliable);
    a.send(UnreliableType, std::vector<std::byte>(MaxUnreliablePayloadSize), Delivery::Unreliable);
    a.send(ReliableType, bytesOf("wave 2"), Delivery::Reliable);

    std::size_t datagrams = 0;
    std::size_t messages = 0;
    do {
        const auto datagram = a.next(now);
        REQUIRE(datagram);
        CHECK(datagram->body.size() <= MaxDatagramBodySize);
        messages += b.receive(datagram->packetNumber, datagram->body, now)->size();
        ++datagrams;
    } while (a.hasMessagesToSend(now));
    CHECK(datagrams == 3);
    CHECK(messages == 3);
}

TEST_CASE("A reliable message received is acknowledged by the next datagram, without waiting", "[network]")
{
    MessageChannel a;
    MessageChannel b;
    const Clock::time_point now{};

    a.send(UnreliableType, bytesOf("snapshot"), Delivery::Unreliable);
    (void)transfer(a, b, now);
    CHECK_FALSE(b.hasMessagesToSend(now));

    a.send(ReliableType, bytesOf("destroyed"), Delivery::Reliable);
    (void)transfer(a, b, now);
    CHECK(b.hasMessagesToSend(now));
    (void)transfer(b, a, now);
    CHECK_FALSE(b.hasMessagesToSend(now));
    CHECK(a.pendingReliable() == 0);
}

TEST_CASE("Last words are sent even by a failed channel, and leave the queue alone", "[network]")
{
    MessageChannel a;
    MessageChannel b;
    const Clock::time_point now{};
    for (std::size_t i = 0; i <= MessageChannel::MaxPendingReliable; ++i) {
        (void)a.send(ReliableType, numbered(0), Delivery::Reliable);
    }
    REQUIRE(a.failed());

    const auto farewell = a.farewell(0x03, bytesOf("bye"));

    REQUIRE(farewell);
    const auto received = b.receive(farewell->packetNumber, farewell->body, now);
    REQUIRE(received);
    CHECK(*received == std::vector<ReceivedMessage>{{0x03, bytesOf("bye")}});
    CHECK(a.pendingReliable() == MessageChannel::MaxPendingReliable);
    CHECK_THROWS_AS(a.farewell(0x03, std::vector<std::byte>(MaxUnreliablePayloadSize + 1)),
                    std::length_error);
}

TEST_CASE("A malformed body does not count as received", "[network]")
{
    MessageChannel a;
    const Clock::time_point now{};

    CHECK_FALSE(a.receive(1, bytesOf("garbage"), now));
    CHECK(a.accepts(1));
    const auto datagram = a.next(now);
    REQUIRE(datagram);
    CHECK(decodeBody(datagram->body)->ack == 0);
}

TEST_CASE("The round-trip time is measured from the acknowledgements", "[network]")
{
    MessageChannel a;
    MessageChannel b;
    Clock::time_point now{};
    CHECK_FALSE(a.rtt());
    CHECK(a.resendDelay() == 150ms);

    for (int i = 0; i < 50; ++i) {
        const auto sent = a.next(now);
        REQUIRE(sent);
        now += 20ms;
        REQUIRE(b.receive(sent->packetNumber, sent->body, now));
        const auto answer = b.next(now);
        now += 20ms;
        REQUIRE(a.receive(answer->packetNumber, answer->body, now));
    }

    REQUIRE(a.rtt());
    CHECK(*a.rtt() == 40ms);
    // max(1.5 × 40 ms, 100 ms)
    CHECK(a.resendDelay() == MessageChannel::MinResendDelay);
}

TEST_CASE("The round-trip time leaves out the time the peer waited to acknowledge", "[network]")
{
    // b answers 30 ms after it receives, as a game that acknowledges only with its next snapshot
    MessageChannel a;
    MessageChannel b;
    Clock::time_point now{};
    for (int i = 0; i < 50; ++i) {
        const auto sent = a.next(now);
        REQUIRE(sent);
        now += 20ms;
        REQUIRE(b.receive(sent->packetNumber, sent->body, now));
        now += 30ms;
        const auto answer = b.next(now);
        CHECK(decodeBody(answer->body)->ackDelay == 300);
        now += 20ms;
        REQUIRE(a.receive(answer->packetNumber, answer->body, now));
    }
    REQUIRE(a.rtt());
    CHECK(*a.rtt() == 40ms);
}

TEST_CASE("Only the packet acknowledged directly gives a sample, and never a saturated delay", "[network]")
{
    MessageChannel a;
    MessageChannel b;
    Clock::time_point now{};
    // Two packets: the first lost on the way, acknowledged later by the bits only
    const auto first = a.next(now);
    const auto second = a.next(now);
    REQUIRE(first);
    REQUIRE(second);
    now += 20ms;
    REQUIRE(b.receive(second->packetNumber, second->body, now));
    REQUIRE(b.receive(first->packetNumber, first->body, now + 500ms));
    now += 520ms;
    const auto answer = b.next(now);
    REQUIRE(answer);
    // The delay is of the second, received 520 ms ago: 20 + 520 + 20 − 520
    now += 20ms;
    REQUIRE(a.receive(answer->packetNumber, answer->body, now));
    REQUIRE(a.rtt());
    CHECK(*a.rtt() == 40ms);

    // A peer silent for more than 6.5 s: no sample
    MessageChannel c;
    MessageChannel d;
    Clock::time_point later{};
    const auto lone = c.next(later);
    REQUIRE(d.receive(lone->packetNumber, lone->body, later));
    later += 10s;
    const auto late = d.next(later);
    CHECK(decodeBody(late->body)->ackDelay == MaxAckDelay);
    REQUIRE(c.receive(late->packetNumber, late->body, later));
    CHECK_FALSE(c.rtt());
}

// --- Over a fake network ---

TEST_CASE("Over a bad network, every reliable message arrives once and in order", "[network]")
{
    for (const unsigned seed : {1U, 2U, 3U}) {
        CAPTURE(seed);
        // 30 % lost, 10 % duplicated, 20 to 120 ms: reordered
        FakeLink toB(seed, 0.3, 0.1, 20ms, 120ms);
        FakeLink toA(seed + 100, 0.3, 0.1, 20ms, 120ms);
        Peer a;
        Peer b;
        Clock::time_point now{};
        std::uint32_t sentReliable = 0;
        std::uint32_t sentUnreliable = 0;

        // 10 seconds of a game: 60 ticks per second, a reliable message every 5 ticks, snapshots every 2
        for (int tick = 0; tick < 600; ++tick) {
            if (tick % 5 == 0) {
                REQUIRE(a.channel.send(ReliableType, numbered(sentReliable++), Delivery::Reliable));
            }
            if (tick % 2 == 0) {
                a.channel.send(UnreliableType, numbered(sentUnreliable++), Delivery::Unreliable);
            }
            a.sendAll(toB, now);
            b.sendAll(toA, now);
            now += 16ms;
            a.receiveAll(toA, now);
            b.receiveAll(toB, now);
        }
        // Then a good network, until everything arrived
        toA.setLoss(0.0);
        toB.setLoss(0.0);
        for (int tick = 0; tick < 120; ++tick) {
            a.sendAll(toB, now);
            b.sendAll(toA, now);
            now += 16ms;
            a.receiveAll(toA, now);
            b.receiveAll(toB, now);
        }

        std::vector<std::uint32_t> reliable;
        std::map<std::uint32_t, int> unreliable;
        for (const ReceivedMessage &message : b.received) {
            if (message.type == ReliableType) {
                reliable.push_back(numberOf(message.payload));
            } else {
                ++unreliable[numberOf(message.payload)];
            }
        }
        std::vector<std::uint32_t> expected(sentReliable);
        for (std::uint32_t i = 0; i < sentReliable; ++i) {
            expected[i] = i;
        }
        CHECK(reliable == expected);
        CHECK(a.channel.pendingReliable() == 0);
        CHECK_FALSE(a.channel.failed());
        CHECK_FALSE(b.channel.failed());
        // Unreliable messages: some lost, none twice (the duplicated datagrams were dropped)
        CHECK(unreliable.size() < sentUnreliable);
        CHECK(unreliable.size() > sentUnreliable / 2);
        for (const auto &[number, count] : unreliable) {
            CHECK(count == 1);
        }
        CHECK(b.datagramsDropped > 0);
        // 20 to 120 ms each way
        REQUIRE(a.channel.rtt());
        CHECK(*a.channel.rtt() >= 40ms);
        CHECK(*a.channel.rtt() <= 240ms);
    }
}

TEST_CASE("A peer that never answers makes the channel fail", "[network]")
{
    FakeLink toB(7, 1.0, 0.0, 10ms, 10ms);
    Peer a;
    Clock::time_point now{};

    bool failed = false;
    for (int tick = 0; tick < 2000 && !failed; ++tick) {
        failed =
            !a.channel.send(ReliableType, numbered(static_cast<std::uint32_t>(tick)), Delivery::Reliable);
        if (!failed) {
            a.sendAll(toB, now);
        }
        now += 16ms;
    }
    CHECK(failed);
    CHECK(a.channel.failed());
}
