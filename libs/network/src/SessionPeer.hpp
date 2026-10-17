#pragma once

// Private to libs/network: what the client and the game sides of a session share.

#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/MessageChannel.hpp"
#include "rtype/network/Session.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype::network::detail
{

/// One end of a session: its keys, its MessageChannel, and the times that drive the heartbeat and the
/// timeouts.
class SessionPeer
{
public:
    using Clock = MessageChannel::Clock;

    std::uint32_t id;
    /// The key of the datagrams this side sends.
    DatagramKey ownKey;
    /// The key of the datagrams the peer sends.
    DatagramKey peerKey;
    MessageChannel channel;
    Clock::time_point created;
    Clock::time_point lastReceived;
    Clock::time_point lastSent;
    /// Game side: the client sent `CONNECT`.
    bool connected = false;
    /// Game side: where the client sends from.
    std::optional<Address> address;

    SessionPeer(std::uint32_t sessionId, const DatagramKey &own, const DatagramKey &peer,
                Clock::time_point now);

    /// Checks, decrypts and reads a datagram whose header was parsed and whose session is this one.
    /// @return The messages it delivers, or nothing if it is dropped.
    [[nodiscard]] std::optional<std::vector<ReceivedMessage>>
    receive(const DatagramHeader &header, std::span<const std::byte> datagram, Clock::time_point now);

    /// The sealed datagrams to send now: the messages waiting, the acknowledgements owed, or a heartbeat if
    /// nothing was sent for HeartbeatInterval. Empty if there is nothing to send, or if the channel failed.
    [[nodiscard]] std::vector<std::vector<std::byte>> flush(Clock::time_point now);

    /// The sealed datagrams of a `DISCONNECT`, DisconnectRepeats of them, sent even if the channel failed.
    [[nodiscard]] std::vector<std::vector<std::byte>> farewell(DisconnectReason reason);
};

/// The reason of a `DISCONNECT` payload: unknown reasons and invalid payloads are Quit.
[[nodiscard]] DisconnectReason disconnectReasonOf(std::span<const std::byte> payload);

} // namespace rtype::network::detail
