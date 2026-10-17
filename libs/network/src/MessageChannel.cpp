#include "rtype/network/MessageChannel.hpp"

#include "NetworkLogger.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

namespace rtype::network
{

using detail::logger;

namespace
{

/// How many packets before the highest acknowledgement can still be acknowledged: the `Ack bits`.
constexpr std::uint64_t AckRange = 32;
/// Most packets remembered for their acknowledgements, if the peer never answers.
constexpr std::size_t MaxSentPackets = 1024;

} // namespace

bool MessageChannel::send(std::uint8_t type, std::span<const std::byte> payload, Delivery delivery)
{
    const bool reliable = delivery == Delivery::Reliable;
    const std::size_t maxSize = reliable ? MaxReliablePayloadSize : MaxUnreliablePayloadSize;
    if (payload.size() > maxSize) {
        throw std::length_error(std::format("a {} message holds at most {} bytes, not {}",
                                            reliable ? "reliable" : "unreliable", maxSize, payload.size()));
    }
    if (_failed) {
        return false;
    }
    std::vector<std::byte> copy(payload.begin(), payload.end());
    if (!reliable) {
        if (_unreliable.size() == MaxQueuedUnreliable) {
            _unreliable.pop_front();
            logger().debug("Unreliable message dropped: {} already wait to be sent", MaxQueuedUnreliable);
        }
        _unreliable.push_back({type, std::move(copy)});
        return true;
    }
    if (_pending.size() == MaxPendingReliable) {
        fail("too many reliable messages wait for an acknowledgement");
        return false;
    }
    _pending.push_back({_nextId, type, std::move(copy), std::nullopt});
    ++_nextId; // Wraps from 65535 to 0
    return true;
}

std::optional<OutgoingDatagram> MessageChannel::next(Clock::time_point now)
{
    if (_failed) {
        return std::nullopt;
    }
    const std::optional<std::uint64_t> number = _packets.next();
    if (!number) {
        fail("every packet number was used");
        return std::nullopt;
    }

    DatagramBody body{_window.highest(), _window.ackBits(), {}};
    SentPacket sent{*number, now, {}, false};
    std::size_t size = BodyHeaderSize;
    const Clock::duration delay = resendDelay();

    // Reliable messages first: they wait the longest
    for (PendingMessage &message : _pending) {
        if (body.messages.size() == MaxBodyMessages) {
            break;
        }
        const bool due = !message.lastSent || now - *message.lastSent >= delay;
        const std::size_t messageSize = encodedMessageSize(true, message.payload.size());
        if (!due || size + messageSize > MaxDatagramBodySize) {
            continue;
        }
        if (message.lastSent && logger().isEnabled(rtype::log::Level::Trace)) {
            logger().trace("Reliable message {} sent again in packet {}", message.id, *number);
        }
        body.messages.push_back({message.type, true, message.id, message.payload});
        message.lastSent = now;
        sent.reliable.push_back(message.id);
        size += messageSize;
    }

    // Then the unreliable ones, in order, as long as they fit. Moved out first: the body points to them
    std::vector<QueuedMessage> unreliable;
    while (!_unreliable.empty() && body.messages.size() + unreliable.size() < MaxBodyMessages) {
        const std::size_t messageSize = encodedMessageSize(false, _unreliable.front().payload.size());
        if (size + messageSize > MaxDatagramBodySize) {
            break;
        }
        unreliable.push_back(std::move(_unreliable.front()));
        _unreliable.pop_front();
        size += messageSize;
    }
    for (const QueuedMessage &message : unreliable) {
        body.messages.push_back({message.type, false, 0, message.payload});
    }

    std::vector<std::byte> bytes = encodeBody(body);
    _sent.push_back(std::move(sent));
    if (_sent.size() > MaxSentPackets) {
        _sent.pop_front();
    }
    return OutgoingDatagram{*number, std::move(bytes)};
}

bool MessageChannel::hasMessagesToSend(Clock::time_point now) const
{
    if (_failed) {
        return false;
    }
    if (!_unreliable.empty()) {
        return true;
    }
    const Clock::duration delay = resendDelay();
    return std::ranges::any_of(_pending, [&](const PendingMessage &message) {
        return !message.lastSent || now - *message.lastSent >= delay;
    });
}

bool MessageChannel::accepts(std::uint64_t packetNumber) const
{
    return !_failed && _window.accepts(packetNumber);
}

std::optional<std::vector<ReceivedMessage>>
MessageChannel::receive(std::uint64_t packetNumber, std::span<const std::byte> body, Clock::time_point now)
{
    if (!accepts(packetNumber)) {
        return std::nullopt;
    }
    const std::optional<DatagramBody> decoded = decodeBody(body);
    if (!decoded) {
        return std::nullopt;
    }
    // Only now: a malformed body must not count as received
    _window.mark(packetNumber);
    acknowledge(decoded->ack, decoded->ackBits, now);

    std::vector<ReceivedMessage> delivered;
    for (const BodyMessage &message : decoded->messages) {
        deliver(message, delivered);
    }
    return delivered;
}

MessageChannel::Clock::duration MessageChannel::resendDelay() const
{
    const Clock::duration rtt = _rtt.value_or(InitialRtt);
    return std::max<Clock::duration>(rtt + rtt / 2, MinResendDelay);
}

void MessageChannel::acknowledge(std::uint64_t ack, std::uint32_t ackBits, Clock::time_point now)
{
    if (ack == 0) {
        return;
    }
    for (SentPacket &packet : _sent) {
        if (packet.acknowledged || packet.number > ack) {
            continue;
        }
        const std::uint64_t distance = ack - packet.number;
        if (distance == 0 || (distance <= AckRange && ((ackBits >> (distance - 1)) & 1U) != 0)) {
            onAcknowledged(packet, now);
        }
    }
    // Packets further than the acknowledgement bits can no longer be acknowledged
    _highestAck = std::max(_highestAck, ack);
    while (!_sent.empty() && _sent.front().number + AckRange < _highestAck) {
        _sent.pop_front();
    }
}

void MessageChannel::onAcknowledged(SentPacket &packet, Clock::time_point now)
{
    packet.acknowledged = true;
    const Clock::duration sample = now - packet.time;
    _rtt = _rtt ? (*_rtt * 9 + sample) / 10 : sample;
    for (const std::uint16_t id : packet.reliable) {
        // Already gone if another packet that carried it was acknowledged first
        if (const auto found = std::ranges::find(_pending, id, &PendingMessage::id);
            found != _pending.end()) {
            _pending.erase(found);
        }
    }
}

void MessageChannel::deliver(const BodyMessage &message, std::vector<ReceivedMessage> &delivered)
{
    ReceivedMessage received{message.type, {message.payload.begin(), message.payload.end()}};
    if (!message.reliable) {
        delivered.push_back(std::move(received));
        return;
    }
    // Distance from the next expected id, modulo 65536
    const auto distance = static_cast<std::uint16_t>(message.id - _expected);
    if (distance == 0) {
        delivered.push_back(std::move(received));
        ++_expected;
        // Then the messages received before, which were waiting for this one
        for (auto found = _buffered.find(_expected); found != _buffered.end();
             found = _buffered.find(_expected)) {
            delivered.push_back(std::move(found->second));
            _buffered.erase(found);
            ++_expected;
        }
    } else if (distance < ReceiveWindow) {
        _buffered.try_emplace(message.id, std::move(received));
    }
    // Otherwise delivered already, or too far ahead: ignored
}

void MessageChannel::fail(const char *reason)
{
    if (!_failed) {
        _failed = true;
        logger().debug("Message channel failed: {}", reason);
    }
}

} // namespace rtype::network
