#pragma once

#include "rtype/network/Datagram.hpp"
#include "rtype/network/DatagramBody.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace rtype::network
{

/// How a message is sent over a MessageChannel.
enum class Delivery {
    /// Sent once: it may be lost, and arrive in any order. For what the next message replaces (a snapshot).
    Unreliable,
    /// Sent again until the peer acknowledges it, and delivered exactly once and in order. For what must
    /// arrive
    /// (a destruction, the end of the game).
    Reliable,
};

/// A message received, delivered by MessageChannel::receive().
struct ReceivedMessage {
    std::uint8_t type = 0;
    std::vector<std::byte> payload;

    friend bool operator==(const ReceivedMessage &, const ReceivedMessage &) = default;
};

/// The body of the next datagram to send, and the packet number to seal it with.
struct OutgoingDatagram {
    std::uint64_t packetNumber = 0;
    std::vector<std::byte> body;
};

/// The reliability layer of one session over UDP (docs/protocol/game.md, sections 5 and 6): numbers the
/// datagrams, acknowledges those received, sends reliable messages again until they are acknowledged,
/// delivers them once and in order, and measures the round-trip time.
///
/// It works on the bodies of the datagrams only, before encryption and after decryption, and is given the
/// time: no socket, no clock of its own, so that it can be tested with a fake network. The session using it
/// does:
/// @code
/// // Sending, at least once per second (a heartbeat otherwise), and as long as there is something to send
/// channel.send(Snapshot::Type, snapshot, rtype::network::Delivery::Unreliable);
/// do {
///     const auto datagram = channel.next(now);
///     if (!datagram) { /* the channel failed: close the session */ }
///     socket.send(peer, sealDatagram(ownKey, {sessionId, datagram->packetNumber}, datagram->body));
/// } while (channel.hasMessagesToSend(now));
///
/// // Receiving
/// const auto header = parseDatagramHeader(datagram);
/// if (!header || header->session != sessionId || !channel.accepts(header->packetNumber)) return;
/// const auto body = openDatagram(peerKey, datagram);
/// if (!body) return;
/// if (const auto messages = channel.receive(header->packetNumber, *body, now)) {
///     for (const auto &message : *messages) { handle(message.type, message.payload); }
/// }
/// @endcode
///
/// A channel **fails** when the peer stops acknowledging (more than MaxPendingReliable reliable messages
/// waiting), or once every packet number was used. It then sends and receives nothing: the session must
/// close.
class MessageChannel
{
public:
    using Clock = std::chrono::steady_clock;

    /// Most reliable messages waiting for an acknowledgement before the channel fails.
    static constexpr std::size_t MaxPendingReliable = 256;
    /// Most unreliable messages waiting to be sent: beyond, the oldest ones are dropped.
    static constexpr std::size_t MaxQueuedUnreliable = 256;
    /// How far ahead of the next expected reliable message one is kept, waiting for those before it.
    static constexpr std::uint16_t ReceiveWindow = 256;
    /// Shortest delay before a reliable message is sent again.
    static constexpr Clock::duration MinResendDelay = std::chrono::milliseconds(100);
    /// The round-trip time assumed until it is measured.
    static constexpr Clock::duration InitialRtt = std::chrono::milliseconds(100);

private:
    struct PendingMessage {
        std::uint16_t id = 0;
        std::uint8_t type = 0;
        std::vector<std::byte> payload;
        /// Not sent yet if empty.
        std::optional<Clock::time_point> lastSent;
    };
    struct QueuedMessage {
        std::uint8_t type = 0;
        std::vector<std::byte> payload;
    };
    struct SentPacket {
        std::uint64_t number = 0;
        Clock::time_point time;
        /// Ids of the reliable messages it carried.
        std::vector<std::uint16_t> reliable;
        bool acknowledged = false;
    };

    PacketCounter _packets;
    ReplayWindow _window;

    /// Reliable messages sent and not acknowledged, oldest first.
    std::deque<PendingMessage> _pending;
    std::deque<QueuedMessage> _unreliable;
    /// Packets sent that the peer may still acknowledge, in increasing numbers.
    std::deque<SentPacket> _sent;
    std::uint16_t _nextId = 0;
    /// Highest acknowledgement received.
    std::uint64_t _highestAck = 0;

    /// Id of the next reliable message to deliver.
    std::uint16_t _expected = 0;
    /// Reliable messages received ahead of _expected.
    std::map<std::uint16_t, ReceivedMessage> _buffered;

    std::optional<Clock::duration> _rtt;
    bool _failed = false;

    void acknowledge(std::uint64_t ack, std::uint32_t ackBits, Clock::time_point now);
    void onAcknowledged(SentPacket &packet, Clock::time_point now);
    void deliver(const BodyMessage &message, std::vector<ReceivedMessage> &delivered);
    void fail(const char *reason);

public:
    /// Queues a message for the next datagrams.
    /// @return false if the channel failed, or fails now because too many reliable messages wait for an
    /// acknowledgement.
    /// @throws std::length_error if the payload is larger than MaxReliablePayloadSize or
    /// MaxUnreliablePayloadSize: it would not fit in a datagram.
    bool send(std::uint8_t type, std::span<const std::byte> payload, Delivery delivery);

    /// Builds the body of the next datagram: the acknowledgements, the reliable messages never sent or due
    /// for a resend, then the unreliable messages, as many as fit. With nothing to send, the body is a
    /// heartbeat.
    /// @return Nothing if the channel failed.
    [[nodiscard]] std::optional<OutgoingDatagram> next(Clock::time_point now);

    /// Tells whether messages are waiting for next(): queued, or due for a resend.
    [[nodiscard]] bool hasMessagesToSend(Clock::time_point now) const;

    /// Tells whether a datagram with this packet number may be new, before decrypting it.
    [[nodiscard]] bool accepts(std::uint64_t packetNumber) const;

    /// Handles the decrypted body of a datagram: records its packet number for the acknowledgements, applies
    /// the acknowledgements it carries, and delivers its messages.
    /// @return The messages to handle, in order: the unreliable ones, and the reliable ones that are next in
    /// order (possibly with others received before and kept). Nothing if the datagram is dropped: the packet
    /// number was received already or is too old, the body is malformed, or the channel failed.
    [[nodiscard]] std::optional<std::vector<ReceivedMessage>>
    receive(std::uint64_t packetNumber, std::span<const std::byte> body, Clock::time_point now);

    /// The smoothed round-trip time, once a packet was acknowledged: the ping shown to the player.
    [[nodiscard]] std::optional<Clock::duration> rtt() const { return _rtt; }
    /// How long a reliable message waits for an acknowledgement before being sent again:
    /// `max(1.5 × RTT, 100 ms)`.
    [[nodiscard]] Clock::duration resendDelay() const;
    /// Number of reliable messages waiting for an acknowledgement.
    [[nodiscard]] std::size_t pendingReliable() const { return _pending.size(); }
    /// The channel failed: the session must close.
    [[nodiscard]] bool failed() const { return _failed; }
};

} // namespace rtype::network
