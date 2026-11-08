// Tests of the instance mode of r-type_server: the messages between the master and its instances, the
// simulation thread, the link to the master, and whole instances run on a thread of the test, with a fake
// master over TCP and real clients over UDP (docs/protocol/instances.md, docs/architecture/server.md).

#include "Common/InstanceMessages.hpp"
#include "Common/Options.hpp"
#include "Common/RelayFrame.hpp"
#include "Common/Secret.hpp"
#include "Instance/DirectTransport.hpp"
#include "Instance/GameHost.hpp"
#include "Instance/GameThread.hpp"
#include "Instance/GameTransport.hpp"
#include "Instance/Instance.hpp"
#include "Instance/MasterLink.hpp"
#include "Instance/RelayedTransport.hpp"
#include "Instance/VoiceRate.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/game/Standalone.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using rtype::network::Address;
using rtype::network::ClientSession;
using rtype::network::DatagramKey;
using rtype::network::Delivery;
using rtype::network::DisconnectReason;
using rtype::network::EventLoop;
using rtype::network::FramedConnection;
using rtype::network::SessionEnd;
using rtype::network::TcpConnection;
using rtype::network::TcpListener;
using rtype::network::UdpSocket;
using Clock = std::chrono::steady_clock;
using Bytes = std::vector<std::byte>;

namespace
{

/// Long enough for any machine of the CI, short enough for a test that fails.
constexpr auto Patience = 10s;

Bytes copy(std::span<const std::byte> bytes)
{
    return {bytes.begin(), bytes.end()};
}

Secret testSecret()
{
    Secret secret{};
    for (std::size_t i = 0; i < secret.size(); ++i) {
        secret[i] = static_cast<std::byte>(i * 7);
    }
    return secret;
}

std::array<std::byte, 32> key(int value)
{
    std::array<std::byte, 32> bytes{};
    bytes.fill(static_cast<std::byte>(value));
    return bytes;
}

AddPlayerMessage player(std::uint32_t playerId, std::uint32_t session, std::string name)
{
    return {playerId, session, key(static_cast<int>(session)), key(static_cast<int>(session) + 100),
            std::move(name)};
}

/// Polls the loop of the test until the condition holds, or `limit`.
bool waitUntil(EventLoop &loop, const std::function<bool()> &condition,
               const std::function<void()> &eachTurn = {}, std::chrono::milliseconds limit = Patience)
{
    const auto deadline = Clock::now() + limit;
    while (!condition()) {
        if (Clock::now() > deadline) {
            return false;
        }
        loop.poll();
        if (eachTurn) {
            eachTurn();
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

/// The master side of the local connection, in the test.
struct FakeMaster {
    TcpListener listener;
    std::optional<FramedConnection> connection;
    std::vector<std::pair<std::uint8_t, Bytes>> received;
    bool closed = false;

    explicit FakeMaster(EventLoop &loop) : listener(loop, Address::loopbackIpv4(0))
    {
        listener.startAccepting([this](TcpConnection accepted) {
            connection.emplace(std::move(accepted));
            connection->start(
                [this](std::uint8_t type, std::span<const std::byte> payload) {
                    received.emplace_back(type, copy(payload));
                },
                [this](std::error_code) { closed = true; });
        });
    }

    [[nodiscard]] std::uint16_t port() const { return listener.localAddress().port(); }

    template <typename Message>
    void send(std::uint8_t type, const Message &message)
    {
        connection->send(type, encode(message));
    }

    /// The first message of this type received, taken out.
    std::optional<Bytes> take(std::uint8_t type)
    {
        const auto found = std::ranges::find(received, type, &std::pair<std::uint8_t, Bytes>::first);
        if (found == received.end()) {
            return std::nullopt;
        }
        Bytes payload = std::move(found->second);
        received.erase(found);
        return payload;
    }

    [[nodiscard]] bool has(std::uint8_t type) const
    {
        return std::ranges::find(received, type, &std::pair<std::uint8_t, Bytes>::first) != received.end();
    }
};

/// A client of the game, over UDP on the loopback, with the loop of the test.
struct TestClient {
    UdpSocket socket;
    Address game;
    ClientSession session;
    std::optional<rtype::game::AcceptedMessage> accepted;
    std::vector<std::pair<std::uint8_t, Bytes>> messages;
    std::optional<SessionEnd> end;

    TestClient(EventLoop &loop, std::uint16_t port, std::uint32_t sessionId,
               const std::array<std::byte, 32> &clientKey, const std::array<std::byte, 32> &serverKey)
        : socket(loop, Address::loopbackIpv4(0)), game(Address::loopbackIpv4(port)),
          session(sessionId, DatagramKey(clientKey), DatagramKey(serverKey),
                  [this](std::span<const std::byte> datagram) { socket.send(game, datagram); })
    {
        socket.startReceiving([this](const Address &from, std::span<const std::byte> datagram) {
            if (from == game) {
                session.receive(datagram, Clock::now());
            }
        });
        session.start(
            [this](std::span<const std::byte> payload) { accepted = rtype::game::decodeAccepted(payload); },
            [this](std::uint8_t type, std::span<const std::byte> payload) {
                messages.emplace_back(type, copy(payload));
            },
            [this](SessionEnd ended) { end = ended; }, Clock::now());
    }

    TestClient(EventLoop &loop, std::uint16_t port, const AddPlayerMessage &added)
        : TestClient(loop, port, added.sessionId, added.clientKey, added.serverKey)
    {
    }

    void update() { session.update(Clock::now()); }

    void sendInput(std::uint32_t tick, rtype::game::PlayerInput input)
    {
        session.send(rtype::game::message_type::Input,
                     rtype::game::encode(rtype::game::InputMessage{tick, {input}}), Delivery::Unreliable);
    }

    [[nodiscard]] std::vector<rtype::game::PlayerJoinedMessage> joined() const
    {
        std::vector<rtype::game::PlayerJoinedMessage> players;
        for (const auto &[type, payload] : messages) {
            if (type == rtype::game::message_type::PlayerJoined) {
                players.push_back(*rtype::game::decodePlayerJoined(payload));
            }
        }
        return players;
    }

    [[nodiscard]] std::vector<rtype::game::PlayerLeftMessage> left() const
    {
        std::vector<rtype::game::PlayerLeftMessage> players;
        for (const auto &[type, payload] : messages) {
            if (type == rtype::game::message_type::PlayerLeft) {
                players.push_back(*rtype::game::decodePlayerLeft(payload));
            }
        }
        return players;
    }

    void sendVoiceState(rtype::voice::Backend backend)
    {
        session.send(rtype::game::message_type::VoiceState,
                     rtype::game::encode(rtype::game::VoiceStateMessage{backend}), Delivery::Reliable);
    }

    void sendVoiceStatus(rtype::voice::Status status)
    {
        session.send(rtype::game::message_type::VoiceStatus,
                     rtype::game::encode(rtype::game::VoiceStatusMessage{status}), Delivery::Reliable);
    }

    /// The latest VOICE_STATUSES received.
    [[nodiscard]] std::optional<rtype::game::VoiceStatusesMessage> lastStatuses() const
    {
        for (auto message = messages.rbegin(); message != messages.rend(); ++message) {
            if (message->first == rtype::game::message_type::VoiceStatuses) {
                return rtype::game::decodeVoiceStatuses(message->second);
            }
        }
        return std::nullopt;
    }

    void sendVoice(std::uint16_t sequence)
    {
        session.send(rtype::game::message_type::Voice,
                     rtype::game::encode(rtype::game::VoiceMessage{sequence, false, {std::byte{0xF8}}}),
                     Delivery::Unreliable);
    }

    /// The slots whose voice the game forwarded to this client, in order.
    [[nodiscard]] std::vector<std::uint8_t> voicesFrom() const
    {
        std::vector<std::uint8_t> slots;
        for (const auto &[type, payload] : messages) {
            if (type == rtype::game::message_type::VoiceFrame) {
                slots.push_back(rtype::game::decodeVoiceFrame(payload)->slot);
            }
        }
        return slots;
    }

    /// The latest VOICE_ROSTER received.
    [[nodiscard]] std::optional<rtype::game::VoiceRosterMessage> lastRoster() const
    {
        for (auto message = messages.rbegin(); message != messages.rend(); ++message) {
            if (message->first == rtype::game::message_type::VoiceRoster) {
                return rtype::game::decodeVoiceRoster(message->second);
            }
        }
        return std::nullopt;
    }

    /// The latest snapshot part received.
    [[nodiscard]] std::optional<rtype::game::SnapshotPart> lastSnapshot() const
    {
        for (auto message = messages.rbegin(); message != messages.rend(); ++message) {
            if (message->first == rtype::game::message_type::Snapshot) {
                return rtype::game::decodeSnapshotPart(message->second);
            }
        }
        return std::nullopt;
    }
};

/// The ship of a slot in a snapshot part.
std::optional<rtype::game::EntityState> shipOf(const rtype::game::SnapshotPart &part, std::uint8_t slot)
{
    for (const rtype::game::EntityState &entity : part.entities) {
        if (entity.kind == rtype::game::EntityKind::PlayerShip && entity.owner == slot) {
            return entity;
        }
    }
    return std::nullopt;
}

/// A transport with no socket: the test hands datagrams in from any address, and reads those sent.
struct FakeTransport final : GameTransport {
    struct State {
        ReceiveHandler handler;
        std::vector<std::pair<Address, Bytes>> sent;
        bool closed = false;
    };
    /// Shared with the test: the host owns the transport.
    std::shared_ptr<State> state = std::make_shared<State>();
    Address local = Address::loopbackIpv4(4999);

    void start(ReceiveHandler handler) override { state->handler = std::move(handler); }
    void send(const Address &to, std::span<const std::byte> datagram) override
    {
        state->sent.emplace_back(to, copy(datagram));
    }
    void close() override { state->closed = true; }
    [[nodiscard]] const Address &localAddress() const override { return local; }
};

/// The relay of a master, in the test: the clients send to its public socket, it forwards their datagrams in
/// relay frames to the instance on the loopback, and sends the frames of the instance on to their clients.
struct FakeRelay {
    UdpSocket loopback;
    UdpSocket open;
    /// Where the instance receives its frames: the port of READY.
    std::optional<Address> instance;
    /// The clients the instance sent to, frame after frame.
    std::vector<Address> sentTo;

    explicit FakeRelay(EventLoop &loop)
        : loopback(loop, Address::loopbackIpv4(0)), open(loop, Address::loopbackIpv4(0))
    {
        open.startReceiving([this](const Address &from, std::span<const std::byte> datagram) {
            if (instance) {
                loopback.send(*instance, encodeRelayFrame(from, datagram));
            }
        });
        loopback.startReceiving([this](const Address &from, std::span<const std::byte> received) {
            const auto frame = decodeRelayFrame(received);
            if (from != instance || !frame) {
                return;
            }
            sentTo.push_back(frame->client);
            open.send(frame->client, frame->datagram);
        });
    }

    /// The port of the relay, for CONFIGURE.
    [[nodiscard]] std::uint16_t port() const { return loopback.localAddress().port(); }
    /// The one public port of the master, the clients send to.
    [[nodiscard]] std::uint16_t publicPort() const { return open.localAddress().port(); }
};

/// An instance run by runInstance() on a thread of its own, as r-type_server --instance would.
struct RunningInstance {
    std::istringstream input;
    std::future<int> exit;

    RunningInstance(InstanceOptions options, const std::string &standardInput) : input(standardInput)
    {
        exit = std::async(std::launch::async, [this, options] { return runInstance(options, input); });
    }

    /// Its exit code, once it has exited while the loop of the test runs.
    std::optional<int> waitForExit(EventLoop &loop)
    {
        if (!waitUntil(loop, [&] { return exit.wait_for(0s) == std::future_status::ready; })) {
            return std::nullopt;
        }
        return exit.get();
    }
};

/// A whole instance with its fake master. The master leaves at the end, so that an instance never outlives
/// its test, even one that fails.
struct InstanceFixture {
    EventLoop loop;
    FakeMaster master{loop};
    std::optional<RunningInstance> instance;
    std::uint16_t udpPort = 0;

    void start(std::uint32_t id = 7)
    {
        instance.emplace(InstanceOptions{.id = id, .udpPort = 0, .masterPort = master.port()},
                         formatSecret(testSecret()) + "\n");
    }

    /// Up to READY: the instance presented itself, was configured, and opened its port.
    void configure(ConfigureMessage game = {"Test game", 4})
    {
        REQUIRE(waitUntil(loop, [&] { return master.has(instance_message::InstanceHello); }));
        master.send(instance_message::Configure, game);
        REQUIRE(waitUntil(loop, [&] { return master.has(instance_message::Ready); }));
        udpPort = decodeReady(*master.take(instance_message::Ready))->udpPort;
        REQUIRE(udpPort != 0);
    }

    ~InstanceFixture()
    {
        if (instance && instance->exit.valid()) {
            master.connection.reset();
            master.listener.close();
            (void)instance->waitForExit(loop);
        }
    }
};

} // namespace

// --- The messages between the master and its instances ---

TEST_CASE("The messages of the instances survive a round trip", "[instance]")
{
    const InstanceHelloMessage hello{1, 7, testSecret()};
    CHECK(decodeInstanceHello(encode(hello)) == hello);
    CHECK(encode(hello).size() == 38);
    CHECK(decodeReady(encode(ReadyMessage{4251})) == ReadyMessage{4251});
    CHECK(decodePlayerConnected(encode(PlayerConnectedMessage{42})) == PlayerConnectedMessage{42});
    const PlayerDisconnectedMessage disconnected{42, PlayerDisconnectedReason::TimedOut};
    CHECK(decodePlayerDisconnected(encode(disconnected)) == disconnected);
    const GameOverReport report{{{1, 1500}, {2, 0}, {3, 0xFFFFFFFF}, {4, 7}}};
    CHECK(decodeGameOverReport(encode(report)) == report);
    CHECK(decodeGameOverReport(encode(GameOverReport{})) == GameOverReport{});
    const GameStatusMessage status{
        123456,
        3,
        {{1, 1500, true, 42}, {2, 0, false, std::nullopt}, {3, 0xFFFFFFFF, true, MaxStatusLatency}}};
    CHECK(decodeGameStatus(encode(status)) == status);
    CHECK(encode(status).size() == 7 + (3 * 11));
    CHECK(decodeGameStatus(encode(GameStatusMessage{})) == GameStatusMessage{});
    const ConfigureMessage configure{"Bydo hunters", 3};
    CHECK(decodeConfigure(encode(configure)) == configure);
    const ConfigureMessage relayed{"Bydo hunters", 3, 4242};
    CHECK(decodeConfigure(encode(relayed)) == relayed);
    const AddPlayerMessage added = player(42, 5, "Alice");
    CHECK(decodeAddPlayer(encode(added)) == added);
    CHECK(encode(added).size() == 72 + 1 + 5 + 8);
    AddPlayerMessage signedIn = added;
    signedIn.accountId = 0x0102030405060708;
    CHECK(decodeAddPlayer(encode(signedIn)) == signedIn);
    for (const RemovePlayerReason reason :
         {RemovePlayerReason::Left, RemovePlayerReason::LobbyClosed, RemovePlayerReason::Kicked}) {
        const RemovePlayerMessage removed{42, reason};
        CHECK(decodeRemovePlayer(encode(removed)) == removed);
    }
    for (const ShutdownReason reason :
         {ShutdownReason::Empty, ShutdownReason::MasterStopping, ShutdownReason::EndedByAdmin}) {
        CHECK(decodeShutdown(encode(ShutdownMessage{reason})) == ShutdownMessage{reason});
    }
}

TEST_CASE("The messages of the instances are written field by field", "[instance]")
{
    const auto bytes = [](std::initializer_list<int> values) {
        Bytes result;
        for (const int value : values) {
            result.push_back(static_cast<std::byte>(value));
        }
        return result;
    };
    CHECK(encode(ReadyMessage{4251}) == bytes({0x10, 0x9B}));
    CHECK(encode(PlayerDisconnectedMessage{42, PlayerDisconnectedReason::TimedOut}) ==
          bytes({0, 0, 0, 0x2A, 1}));
    CHECK(encode(ConfigureMessage{"Duo", 2}) == bytes({3, 'D', 'u', 'o', 2, 0, 0}));
    CHECK(encode(ConfigureMessage{"Duo", 2, 4242}) == bytes({3, 'D', 'u', 'o', 2, 0x10, 0x92}));
    CHECK(encode(GameOverReport{{{1, 1500}}}) == bytes({1, 0, 0, 0, 1, 0, 0, 0x05, 0xDC}));
    CHECK(encode(ShutdownMessage{ShutdownReason::Empty}) == bytes({0}));
    CHECK(encode(ShutdownMessage{ShutdownReason::EndedByAdmin}) == bytes({2}));
    CHECK(encode(RemovePlayerMessage{42, RemovePlayerReason::Kicked}) == bytes({0, 0, 0, 0x2A, 2}));
    CHECK(encode(GameStatusMessage{600, 2, {{42, 1500, true, 35}, {43, 0, false, std::nullopt}}}) ==
          bytes({0, 0, 0x02, 0x58, 0, 2, 2,                            // tick, wave, players
                 0, 0, 0,    0x2A, 0, 0, 0x05, 0xDC, 1, 0,    35,      // 42, 1500, alive, 35 ms
                 0, 0, 0,    0x2B, 0, 0, 0,    0,    0, 0xFF, 0xFF})); // 43, 0, out, not measured
}

TEST_CASE("The messages of the instances refuse what the protocol forbids", "[instance]")
{
    const auto bytes = [](std::initializer_list<int> values) {
        Bytes result;
        for (const int value : values) {
            result.push_back(static_cast<std::byte>(value));
        }
        return result;
    };
    CHECK_THROWS_AS(encode(InstanceHelloMessage{1, 0, {}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(ConfigureMessage{"", 2}), std::invalid_argument);
    CHECK_THROWS_AS(encode(ConfigureMessage{std::string(33, 'a'), 2}), std::invalid_argument);
    CHECK_THROWS_AS(encode(ConfigureMessage{"Game", 0}), std::invalid_argument);
    CHECK_THROWS_AS(encode(ConfigureMessage{"Game", 5}), std::invalid_argument);
    CHECK_THROWS_AS(encode(player(1, 0, "Bob")), std::invalid_argument);
    CHECK_THROWS_AS(encode(player(1, 2, "")), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameOverReport{std::vector<PlayerScore>(5)}), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameStatusMessage{1, 1, std::vector<PlayerStatus>(5)}), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameStatusMessage{1, 1, {{1, 0, true, 0xFFFF}}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameStatusMessage{1, 1, {{1, 0, true, 1}, {1, 5, true, 2}}}),
                    std::invalid_argument);

    Bytes hello = encode(InstanceHelloMessage{1, 7, {}});
    hello[2] = hello[3] = hello[4] = hello[5] = std::byte{0};
    CHECK_FALSE(decodeInstanceHello(hello));                                                      // id 0
    CHECK_FALSE(decodePlayerDisconnected(bytes({0, 0, 0, 1, 2})));                                // reason 2
    CHECK_FALSE(decodeRemovePlayer(bytes({0, 0, 0, 1, 3})));                                      // reason 3
    CHECK_FALSE(decodeShutdown(bytes({3})));                                                      // reason 3
    CHECK_FALSE(decodeConfigure(bytes({1, 'A', 0, 0, 0})));                                       // no player
    CHECK_FALSE(decodeConfigure(bytes({1, 'A', 5, 0, 0})));                                       // 5 players
    CHECK_FALSE(decodeConfigure(bytes({0, 2, 0, 0})));                                            // no name
    CHECK_FALSE(decodeGameOverReport(bytes({5})));                                                // 5 scores
    CHECK_FALSE(decodeGameOverReport(bytes({1, 0, 0, 0, 1})));                                    // cut
    CHECK_FALSE(decodeGameStatus(bytes({0, 0, 0, 1, 0, 1, 5})));                                  // 5 players
    CHECK_FALSE(decodeGameStatus(bytes({0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 2, 0, 0}))); // alive 2
    CHECK_FALSE(decodeGameStatus(bytes({0, 0, 0, 1, 0, 1, 2, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0,     // player 1
                                        0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}))); // player 1 again
    Bytes added = encode(player(1, 2, "Bob"));
    added[4] = added[5] = added[6] = added[7] = std::byte{0};
    CHECK_FALSE(decodeAddPlayer(added)); // session 0

    // Cut, or one byte more
    const std::vector<std::pair<Bytes, std::function<bool(std::span<const std::byte>)>>> messages = {
        {encode(InstanceHelloMessage{1, 7, {}}), [](auto p) { return decodeInstanceHello(p).has_value(); }},
        {encode(ReadyMessage{1}), [](auto p) { return decodeReady(p).has_value(); }},
        {encode(PlayerConnectedMessage{1}), [](auto p) { return decodePlayerConnected(p).has_value(); }},
        {encode(PlayerDisconnectedMessage{1, {}}),
         [](auto p) { return decodePlayerDisconnected(p).has_value(); }},
        {encode(GameOverReport{{{1, 2}}}), [](auto p) { return decodeGameOverReport(p).has_value(); }},
        {encode(GameStatusMessage{1, 2, {{1, 2, true, 3}, {4, 5, false, std::nullopt}}}),
         [](auto p) { return decodeGameStatus(p).has_value(); }},
        {encode(ConfigureMessage{"G", 1, 4242}), [](auto p) { return decodeConfigure(p).has_value(); }},
        {encode(player(1, 2, "Bob")), [](auto p) { return decodeAddPlayer(p).has_value(); }},
        {encode(RemovePlayerMessage{1, {}}), [](auto p) { return decodeRemovePlayer(p).has_value(); }},
        {encode(ShutdownMessage{}), [](auto p) { return decodeShutdown(p).has_value(); }},
    };
    for (const auto &[payload, decodes] : messages) {
        CHECK(decodes(payload));
        for (std::size_t size = 0; size < payload.size(); ++size) {
            CHECK_FALSE(decodes(std::span(payload).first(size)));
        }
        Bytes longer = payload;
        longer.push_back(std::byte{0});
        CHECK_FALSE(decodes(longer));
    }
}

TEST_CASE("Random bytes never crash the decoders of the instances", "[instance]")
{
    std::mt19937 random(13);
    std::uniform_int_distribution<int> size(0, 120);
    std::uniform_int_distribution<int> byte(0, 255);
    for (int i = 0; i < 20000; ++i) {
        Bytes payload(static_cast<std::size_t>(size(random)));
        for (std::byte &value : payload) {
            value = static_cast<std::byte>(byte(random));
        }
        (void)decodeInstanceHello(payload);
        (void)decodeReady(payload);
        (void)decodePlayerConnected(payload);
        (void)decodePlayerDisconnected(payload);
        (void)decodeGameOverReport(payload);
        (void)decodeGameStatus(payload);
        (void)decodeConfigure(payload);
        (void)decodeAddPlayer(payload);
        (void)decodeRemovePlayer(payload);
        (void)decodeShutdown(payload);
    }
    SUCCEED();
}

// --- The simulation thread ---

TEST_CASE("The game thread steps 60 times per second and applies each input at its tick", "[instance]")
{
    std::mutex mutex;
    std::vector<TickResult> results;
    GameThread game({}, {}, [&](TickResult result) {
        const std::scoped_lock lock(mutex);
        results.push_back(std::move(result));
    });
    const rtype::game::PlayerInput left{rtype::game::PlayerInput::Left};
    const rtype::game::PlayerInput right{rtype::game::PlayerInput::Right};
    game.addPlayer(1);
    // Right for the ticks 6, 5, 4, then Left for 3, 2, 1: each one at its tick, whatever the order they come
    // in
    game.receiveInputs(1, 6, {right, right, right});
    game.receiveInputs(1, 3, {left, left, left});
    const auto start = Clock::now();
    game.start();
    std::this_thread::sleep_for(500ms);
    game.stop();
    const auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();

    const std::scoped_lock lock(mutex);
    REQUIRE(results.size() >= 6);
    // About 60 ticks per second: generous bounds for a loaded machine
    CHECK(static_cast<double>(results.size()) <= elapsed * 60.0 + 2.0);
    CHECK(static_cast<double>(results.size()) >= elapsed * 60.0 * 0.5);
    CHECK(results.back().snapshot.tick == results.size());
    CHECK(game.tick() == results.size());

    const auto velocityAt = [&](std::size_t tick) {
        const rtype::game::Snapshot &snapshot = results[tick - 1].snapshot;
        const auto ship = std::ranges::find(snapshot.entities, rtype::game::EntityKind::PlayerShip,
                                            &rtype::game::EntityState::kind);
        REQUIRE(ship != snapshot.entities.end());
        return ship->velocityX;
    };
    CHECK(velocityAt(1) < 0.0f);
    CHECK(velocityAt(3) < 0.0f);
    CHECK(velocityAt(4) > 0.0f);
    CHECK(velocityAt(6) > 0.0f);
    // The tick of the last input applied, for the snapshots of its client
    CHECK(results[2].lastInputs[1] == 3);
    CHECK(results[5].lastInputs[1] == 6);
    // How early they arrived, the smallest of the two, once: the game was at tick 0
    CHECK(results[0].inputLeads[1] == 3);
    CHECK_FALSE(results[1].inputLeads[1]);

    // Nothing after stop()
    const std::size_t count = results.size();
    std::this_thread::sleep_for(50ms);
    CHECK(results.size() == count);
}

// --- The link to the master ---

TEST_CASE("The link presents the instance, then gives the configuration and the players", "[instance]")
{
    EventLoop loop;
    FakeMaster master(loop);
    std::optional<ConfigureMessage> configured;
    std::vector<AddPlayerMessage> added;
    std::optional<std::string> failure;
    MasterLink link(loop, master.port(), {.id = 7, .secret = testSecret()},
                    {.onConfigure = [&](const ConfigureMessage &game) { configured = game; },
                     .onAddPlayer = [&](const AddPlayerMessage &p) { added.push_back(p); },
                     .onRemovePlayer = {},
                     .onShutdown = {},
                     .onFailed = [&](const std::string &reason) { failure = reason; }});

    REQUIRE(waitUntil(loop, [&] { return master.has(instance_message::InstanceHello); }));
    CHECK(decodeInstanceHello(*master.take(instance_message::InstanceHello)) ==
          InstanceHelloMessage{InstanceProtocolVersion, 7, testSecret()});

    master.send(instance_message::Configure, ConfigureMessage{"Game", 2});
    master.send(instance_message::AddPlayer, player(42, 5, "Alice"));
    REQUIRE(waitUntil(loop, [&] { return !added.empty(); }));
    CHECK(configured == ConfigureMessage{"Game", 2});
    CHECK(added[0] == player(42, 5, "Alice"));

    link.sendReady(4251);
    link.sendPlayerDisconnected(42, PlayerDisconnectedReason::TimedOut);
    REQUIRE(waitUntil(loop, [&] { return master.has(instance_message::PlayerDisconnected); }));
    CHECK(decodeReady(*master.take(instance_message::Ready)) == ReadyMessage{4251});
    CHECK(decodePlayerDisconnected(*master.take(instance_message::PlayerDisconnected)) ==
          PlayerDisconnectedMessage{42, PlayerDisconnectedReason::TimedOut});
    CHECK_FALSE(failure);
}

TEST_CASE("The link fails on what the protocol does not expect", "[instance]")
{
    EventLoop loop;
    FakeMaster master(loop);
    std::optional<std::string> failure;
    const auto link = [&](std::chrono::milliseconds timeout = MasterLink::ConfigureTimeout) {
        return std::make_unique<MasterLink>(
            loop, master.port(), InstanceHelloMessage{.id = 7, .secret = {}},
            MasterLink::Handlers{.onConfigure = {},
                                 .onAddPlayer = {},
                                 .onRemovePlayer = {},
                                 .onShutdown = {},
                                 .onFailed = [&](const std::string &reason) { failure = reason; }},
            timeout);
    };
    const auto connect = [&](std::chrono::milliseconds timeout = MasterLink::ConfigureTimeout) {
        failure.reset();
        master.connection.reset();
        master.received.clear();
        auto created = link(timeout);
        REQUIRE(waitUntil(loop, [&] { return master.has(instance_message::InstanceHello); }));
        return created;
    };

    SECTION("A player before CONFIGURE")
    {
        const auto created = connect();
        master.send(instance_message::AddPlayer, player(1, 2, "Bob"));
        REQUIRE(waitUntil(loop, [&] { return failure.has_value(); }));
        CHECK(failure->find("before CONFIGURE") != std::string::npos);
        // The link closed the connection
        CHECK(waitUntil(loop, [&] { return master.closed; }));
    }
    SECTION("CONFIGURE twice")
    {
        const auto created = connect();
        master.send(instance_message::Configure, ConfigureMessage{"Game", 2});
        master.send(instance_message::Configure, ConfigureMessage{"Game", 2});
        REQUIRE(waitUntil(loop, [&] { return failure.has_value(); }));
    }
    SECTION("A malformed message")
    {
        const auto created = connect();
        master.send(instance_message::Configure, ConfigureMessage{"Game", 2});
        master.connection->send(instance_message::AddPlayer, Bytes(3));
        REQUIRE(waitUntil(loop, [&] { return failure.has_value(); }));
        CHECK(failure->find("malformed") != std::string::npos);
    }
    SECTION("No CONFIGURE in time")
    {
        const auto created = connect(200ms);
        REQUIRE(waitUntil(loop, [&] { return failure.has_value(); }));
        CHECK(failure->find("in time") != std::string::npos);
    }
    SECTION("The master closes the connection")
    {
        const auto created = connect();
        master.connection.reset();
        REQUIRE(waitUntil(loop, [&] { return failure.has_value(); }));
    }
    SECTION("No master")
    {
        const std::uint16_t port = master.port();
        master.listener.close();
        loop.poll();
        MasterLink orphan(loop, port, {.id = 7, .secret = {}},
                          {.onConfigure = {},
                           .onAddPlayer = {},
                           .onRemovePlayer = {},
                           .onShutdown = {},
                           .onFailed = [&](const std::string &reason) { failure = reason; }});
        REQUIRE(waitUntil(loop, [&] { return failure.has_value(); }));
        CHECK(failure->find("cannot connect") != std::string::npos);
    }
}

// --- Whole instances ---

TEST_CASE("A game host sends and receives through its transport, with the address of each client",
          "[instance]")
{
    EventLoop loop;
    GameThread game({}, {}, [](TickResult) {});
    auto transport = std::make_unique<FakeTransport>();
    const std::shared_ptr<FakeTransport::State> wire = transport->state;
    std::optional<std::uint32_t> connected;
    GameHost host(loop, std::move(transport), 4, game,
                  {.onConnected = [&](std::uint32_t playerId) { connected = playerId; },
                   .onDisconnected = {},
                   .onGameOver = {}});
    REQUIRE(wire->handler);
    CHECK(host.localAddress() == Address::loopbackIpv4(4999));

    // A client far away, whose datagrams the test carries both ways
    const AddPlayerMessage alice = player(42, 5, "Alice");
    REQUIRE(host.addPlayer(alice));
    const Address aliceAddress = *Address::parse("203.0.113.7", 50000);
    std::optional<rtype::game::AcceptedMessage> accepted;
    ClientSession client(alice.sessionId, DatagramKey(alice.clientKey), DatagramKey(alice.serverKey),
                         [&](std::span<const std::byte> datagram) { wire->handler(aliceAddress, datagram); });
    client.start([&](std::span<const std::byte> payload) { accepted = rtype::game::decodeAccepted(payload); },
                 [](std::uint8_t, std::span<const std::byte>) {}, [](SessionEnd) {}, Clock::now());
    const auto carry = [&] {
        client.update(Clock::now());
        for (const auto &[to, datagram] : std::exchange(wire->sent, {})) {
            // Everything the host sends goes to the address the client sent from
            CHECK(to == aliceAddress);
            client.receive(datagram, Clock::now());
        }
    };

    REQUIRE(waitUntil(loop, [&] { return accepted.has_value(); }, carry));
    CHECK(accepted->slot == 0);
    CHECK(connected == 42u);

    // Closing the game closes the transport
    host.closeAll(DisconnectReason::Shutdown);
    CHECK(wire->closed);
}

TEST_CASE("A relayed transport takes the frames of the relay alone, and answers in frames to it",
          "[instance]")
{
    EventLoop loop;
    UdpSocket relay(loop, Address::loopbackIpv4(0));
    std::vector<Bytes> frames;
    relay.startReceiving(
        [&](const Address &, std::span<const std::byte> frame) { frames.push_back(copy(frame)); });
    RelayedTransport transport(loop, relay.localAddress().port());
    CHECK(transport.localAddress().ip() == "127.0.0.1");
    CHECK(transport.localAddress().port() != 0);
    CHECK(transport.relay() == relay.localAddress());

    std::vector<std::pair<Address, Bytes>> received;
    transport.start([&](const Address &from, std::span<const std::byte> datagram) {
        received.emplace_back(from, copy(datagram));
    });

    const Address alice = *Address::parse("203.0.113.7", 50000);
    const Address bob = *Address::parse("2001:db8::9", 61000);
    const Bytes hello = {std::byte{1}, std::byte{2}, std::byte{3}};
    // A frame from another port of the machine, then one that does not decode: both dropped
    UdpSocket intruder(loop, Address::loopbackIpv4(0));
    intruder.send(transport.localAddress(), encodeRelayFrame(bob, hello));
    relay.send(transport.localAddress(), Bytes(5, std::byte{4}));
    relay.send(transport.localAddress(), encodeRelayFrame(alice, hello));
    REQUIRE(waitUntil(loop, [&] { return !received.empty(); }));
    for (int i = 0; i < 50; ++i) {
        loop.poll();
        std::this_thread::sleep_for(1ms);
    }
    REQUIRE(received.size() == 1);
    CHECK(received[0].first == alice);
    CHECK(received[0].second == hello);

    // What the game sends goes to the relay, with the address of its client
    transport.send(bob, hello);
    transport.send(bob, {});                // nothing to carry: dropped
    transport.send(bob.withPort(0), hello); // nowhere to go: dropped
    transport.send(alice, hello);
    REQUIRE(waitUntil(loop, [&] { return frames.size() >= 2; }));
    for (int i = 0; i < 50; ++i) {
        loop.poll();
        std::this_thread::sleep_for(1ms);
    }
    REQUIRE(frames.size() == 2);
    const auto first = decodeRelayFrame(frames[0]);
    const auto second = decodeRelayFrame(frames[1]);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->client == bob);
    CHECK(second->client == alice);
    CHECK(copy(first->datagram) == hello);
    transport.close();
}

TEST_CASE("A game over is sent to the clients, reported with the scores, and nothing follows", "[instance]")
{
    EventLoop loop;
    GameThread game({}, {}, [](TickResult) {});
    std::optional<GameOverReport> report;
    GameHost host(loop, std::make_unique<DirectTransport>(loop, Address::loopbackIpv4(0)), 4, game,
                  {.onConnected = {}, .onDisconnected = {}, .onGameOver = [&](const GameOverReport &scores) {
                       report = scores;
                   }});
    const AddPlayerMessage alice = player(42, 5, "Alice");
    REQUIRE(host.addPlayer(alice));
    TestClient client(loop, host.localAddress().port(), alice);
    REQUIRE(waitUntil(loop, [&] { return client.accepted.has_value(); }, [&] { client.update(); }));

    TickResult last;
    last.snapshot.tick = 7;
    last.snapshot.players = {{0, 0, 1234}};
    last.snapshot.gameOver = rtype::game::GameOutcome::Defeat;
    host.publish(last);

    const auto received = [&](std::uint8_t type) {
        return std::ranges::count(client.messages, type, &std::pair<std::uint8_t, Bytes>::first);
    };
    REQUIRE(waitUntil(
        loop, [&] { return received(rtype::game::message_type::GameOver) == 1; }, [&] { client.update(); }));
    CHECK(host.over());
    REQUIRE(report);
    CHECK(report->scores == std::vector<PlayerScore>{{42, 1234}});
    // The last snapshot went with it, whatever its tick: the final scores
    CHECK(received(rtype::game::message_type::Snapshot) == 1);
    // The client acknowledges it: the instance can end
    CHECK(waitUntil(loop, [&] { return host.acknowledged(); }, [&] { client.update(); }));

    // Nothing is published after the game over
    last.snapshot.tick = 8;
    last.snapshot.gameOver.reset();
    host.publish(last);
    (void)waitUntil(loop, [] { return false; }, [&] { client.update(); }, 100ms);
    CHECK(received(rtype::game::message_type::Snapshot) == 1);
    CHECK(received(rtype::game::message_type::GameOver) == 1);
}

TEST_CASE("The status of a game comes from its last tick and the sessions of its players", "[instance]")
{
    EventLoop loop;
    GameThread game({}, {}, [](TickResult) {});
    GameHost host(loop, std::make_unique<DirectTransport>(loop, Address::loopbackIpv4(0)), 4, game, {});
    const AddPlayerMessage alice = player(42, 5, "Alice");
    const AddPlayerMessage bob = player(43, 6, "Bob");
    REQUIRE(host.addPlayer(alice));
    REQUIRE(host.addPlayer(bob));
    CHECK(host.status() == GameStatusMessage{});

    // Only Alice connects: Bob, expected, is not in the game yet
    TestClient client(loop, host.localAddress().port(), alice);
    REQUIRE(waitUntil(loop, [&] { return client.accepted.has_value(); }, [&] { client.update(); }));

    TickResult result;
    result.snapshot.tick = 300;
    result.snapshot.players = {{0, 2, 700}, {1, 3, 0}};
    result.snapshot.waveStarted = 2;
    host.publish(result);
    GameStatusMessage status = host.status();
    CHECK(status.tick == 300);
    CHECK(status.wave == 2);
    REQUIRE(status.players.size() == 1);
    CHECK(status.players[0].playerId == 42);
    CHECK(status.players[0].score == 700);
    CHECK(status.players[0].alive);

    // The wave stays until the next one starts; a player without lives is out
    result.snapshot.tick = 301;
    result.snapshot.players = {{0, 0, 900}};
    result.snapshot.waveStarted.reset();
    host.publish(result);
    status = host.status();
    CHECK(status.tick == 301);
    CHECK(status.wave == 2);
    CHECK(status.players[0].score == 900);
    CHECK_FALSE(status.players[0].alive);

    // Its round-trip time, once the session measured it from the acknowledgements
    CHECK(waitUntil(
        loop, [&] { return host.status().players[0].latency.has_value(); }, [&] { client.update(); }));
    CHECK(*host.status().players[0].latency < 1000);
}

TEST_CASE("An instance runs a game for the players the master gives it", "[instance]")
{
    InstanceFixture fixture;
    fixture.start();
    fixture.configure();
    EventLoop &loop = fixture.loop;
    FakeMaster &master = fixture.master;

    const AddPlayerMessage alice = player(42, 5, "Alice");
    master.send(instance_message::AddPlayer, alice);
    loop.poll();
    std::this_thread::sleep_for(20ms);
    TestClient aliceClient(loop, fixture.udpPort, alice);
    const auto updateClients = [&] { aliceClient.update(); };

    // Connected: ACCEPTED with its slot, and its own PLAYER_JOINED
    REQUIRE(waitUntil(loop, [&] { return aliceClient.accepted.has_value(); }, updateClients));
    CHECK(aliceClient.accepted->slot == 0);
    CHECK(aliceClient.accepted->tickRate == 60);
    REQUIRE(waitUntil(loop, [&] { return master.has(instance_message::PlayerConnected); }, updateClients));
    CHECK(decodePlayerConnected(*master.take(instance_message::PlayerConnected)) ==
          PlayerConnectedMessage{42});
    REQUIRE(waitUntil(loop, [&] { return !aliceClient.joined().empty(); }, updateClients));
    CHECK(aliceClient.joined()[0] == rtype::game::PlayerJoinedMessage{0, 42, "Alice"});

    // Snapshots with its ship, which moves with its input
    REQUIRE(waitUntil(
        loop, [&] { return aliceClient.lastSnapshot() && shipOf(*aliceClient.lastSnapshot(), 0); },
        updateClients));
    const float start = shipOf(*aliceClient.lastSnapshot(), 0)->x;
    CHECK(aliceClient.lastSnapshot()->players.size() == 1);
    // Like a real client: one INPUT per tick, stamped a few ticks ahead of the latest snapshot. Faster, the
    // game would cut it, as any client sending more than 200 datagrams per second
    std::uint32_t stamped = 0;
    auto nextInput = Clock::now();
    REQUIRE(waitUntil(
        loop,
        [&] {
            const auto snapshot = aliceClient.lastSnapshot();
            return snapshot && stamped != 0 && snapshot->lastInput >= stamped - 30 &&
                   shipOf(*snapshot, 0)->x > start + 50.0f;
        },
        [&] {
            aliceClient.update();
            if (Clock::now() >= nextInput) {
                nextInput += std::chrono::nanoseconds(std::chrono::seconds(1)) / 60;
                stamped = std::max(stamped + 1, aliceClient.lastSnapshot()->tick + 5);
                aliceClient.sendInput(stamped, rtype::game::PlayerInput{rtype::game::PlayerInput::Right});
            }
        }));

    // A second player, signed in to an account: both know each other, and the account of Bob
    AddPlayerMessage bob = player(43, 6, "Bob");
    bob.accountId = 77;
    master.send(instance_message::AddPlayer, bob);
    loop.poll();
    std::this_thread::sleep_for(20ms);
    TestClient bobClient(loop, fixture.udpPort, bob);
    const auto updateBoth = [&] {
        aliceClient.update();
        bobClient.update();
    };
    REQUIRE(waitUntil(loop, [&] { return bobClient.joined().size() == 2; }, updateBoth));
    CHECK(bobClient.accepted->slot == 1);
    REQUIRE(waitUntil(loop, [&] { return aliceClient.joined().size() == 2; }, updateBoth));
    CHECK(aliceClient.joined()[1] == rtype::game::PlayerJoinedMessage{1, 43, "Bob", 77});

    // The master removes Bob: Bob is disconnected, Alice is told, the master is not told back
    master.send(instance_message::RemovePlayer, RemovePlayerMessage{43, RemovePlayerReason::LobbyClosed});
    REQUIRE(waitUntil(
        loop, [&] { return bobClient.end.has_value() && !aliceClient.left().empty(); }, updateBoth));
    CHECK(bobClient.end->cause == SessionEnd::Cause::Disconnected);
    CHECK(bobClient.end->reason == DisconnectReason::Quit);
    CHECK(aliceClient.left()[0] == rtype::game::PlayerLeftMessage{1, rtype::game::LeaveReason::LobbyClosed});

    // Bob comes back, in a new session, and an administrator removes Bob: the game protocol tells Alice that
    // Bob quit
    const AddPlayerMessage bobBack = player(43, 7, "Bob");
    master.send(instance_message::AddPlayer, bobBack);
    loop.poll();
    std::this_thread::sleep_for(20ms);
    TestClient bobAgain(loop, fixture.udpPort, bobBack);
    const auto updateAgain = [&] {
        aliceClient.update();
        bobAgain.update();
    };
    REQUIRE(waitUntil(loop, [&] { return bobAgain.accepted.has_value(); }, updateAgain));
    master.send(instance_message::RemovePlayer, RemovePlayerMessage{43, RemovePlayerReason::Kicked});
    REQUIRE(waitUntil(
        loop, [&] { return bobAgain.end.has_value() && aliceClient.left().size() == 2; }, updateAgain));
    CHECK(bobAgain.end->reason == DisconnectReason::Quit);
    CHECK(aliceClient.left()[1] == rtype::game::PlayerLeftMessage{1, rtype::game::LeaveReason::Quit});

    // Alice quits: the master is told
    aliceClient.session.close(DisconnectReason::Quit);
    REQUIRE(waitUntil(loop, [&] { return master.has(instance_message::PlayerDisconnected); }));
    CHECK(decodePlayerDisconnected(*master.take(instance_message::PlayerDisconnected)) ==
          PlayerDisconnectedMessage{42, PlayerDisconnectedReason::Quit});
    CHECK_FALSE(master.has(instance_message::PlayerDisconnected));

    // SHUTDOWN: the instance exits with 0
    master.send(instance_message::Shutdown, ShutdownMessage{ShutdownReason::Empty});
    CHECK(fixture.instance->waitForExit(loop) == Success);
}

TEST_CASE("An instance behind a relay runs a game for the clients behind it", "[instance]")
{
    InstanceFixture fixture;
    EventLoop &loop = fixture.loop;
    FakeMaster &master = fixture.master;
    FakeRelay relay(loop);
    fixture.start();
    fixture.configure({"Relayed", 4, relay.port()});
    // READY gives the port on the loopback, where the relay sends its frames
    relay.instance = Address::loopbackIpv4(fixture.udpPort);

    const AddPlayerMessage alice = player(42, 5, "Alice");
    const AddPlayerMessage bob = player(43, 6, "Bob");
    const AddPlayerMessage carol = player(44, 7, "Carol");
    master.send(instance_message::AddPlayer, alice);
    master.send(instance_message::AddPlayer, bob);
    master.send(instance_message::AddPlayer, carol);
    loop.poll();
    std::this_thread::sleep_for(20ms);

    // Carol talks to the loopback port of the instance from elsewhere than the relay, in valid frames: not
    // heard
    UdpSocket intruder(loop, Address::loopbackIpv4(0));
    bool carolAccepted = false;
    ClientSession carolSession(carol.sessionId, DatagramKey(carol.clientKey), DatagramKey(carol.serverKey),
                               [&](std::span<const std::byte> datagram) {
                                   intruder.send(*relay.instance,
                                                 encodeRelayFrame(intruder.localAddress(), datagram));
                               });
    intruder.startReceiving([&](const Address &, std::span<const std::byte> datagram) {
        carolSession.receive(datagram, Clock::now());
    });
    carolSession.start([&](std::span<const std::byte>) { carolAccepted = true; },
                       [](std::uint8_t, std::span<const std::byte>) {}, [](SessionEnd) {}, Clock::now());

    // Alice and Bob, two clients on the one public port, as with a master
    TestClient aliceClient(loop, relay.publicPort(), alice);
    TestClient bobClient(loop, relay.publicPort(), bob);
    const auto updateAll = [&] {
        aliceClient.update();
        bobClient.update();
        carolSession.update(Clock::now());
    };
    REQUIRE(waitUntil(loop, [&] { return aliceClient.accepted && bobClient.accepted; }, updateAll));
    CHECK(aliceClient.accepted->slot != bobClient.accepted->slot);
    REQUIRE(waitUntil(
        loop, [&] { return aliceClient.joined().size() == 2 && bobClient.joined().size() == 2; }, updateAll));
    REQUIRE(waitUntil(
        loop, [&] { return aliceClient.lastSnapshot().has_value() && bobClient.lastSnapshot().has_value(); },
        updateAll));

    // The instance answered each client at its own address, the one the relay saw
    const Address aliceAddress = aliceClient.socket.localAddress();
    const Address bobAddress = bobClient.socket.localAddress();
    CHECK(std::ranges::find(relay.sentTo, aliceAddress) != relay.sentTo.end());
    CHECK(std::ranges::find(relay.sentTo, bobAddress) != relay.sentTo.end());
    CHECK(std::ranges::all_of(relay.sentTo,
                              [&](const Address &to) { return to == aliceAddress || to == bobAddress; }));
    CHECK_FALSE(carolAccepted);
    while (const auto connected = master.take(instance_message::PlayerConnected)) {
        CHECK(decodePlayerConnected(*connected)->playerId != 44);
    }

    // Alice quits through the relay: the master is told
    aliceClient.session.close(DisconnectReason::Quit);
    REQUIRE(waitUntil(loop, [&] { return master.has(instance_message::PlayerDisconnected); }, updateAll));
    CHECK(decodePlayerDisconnected(*master.take(instance_message::PlayerDisconnected)) ==
          PlayerDisconnectedMessage{42, PlayerDisconnectedReason::Quit});

    // SHUTDOWN: Bob receives DISCONNECT through the relay, and the instance exits with 0
    master.send(instance_message::Shutdown, ShutdownMessage{ShutdownReason::Empty});
    REQUIRE(waitUntil(loop, [&] { return bobClient.end.has_value(); }, updateAll));
    CHECK(bobClient.end->reason == DisconnectReason::Shutdown);
    CHECK(fixture.instance->waitForExit(loop) == Success);
    CHECK_FALSE(carolAccepted);
}

TEST_CASE("An instance tells its master every second where its game stands", "[instance]")
{
    InstanceFixture fixture;
    fixture.start();
    fixture.configure();
    EventLoop &loop = fixture.loop;
    FakeMaster &master = fixture.master;
    const AddPlayerMessage alice = player(42, 5, "Alice");
    const AddPlayerMessage bob = player(43, 6, "Bob");
    master.send(instance_message::AddPlayer, alice);
    master.send(instance_message::AddPlayer, bob);
    loop.poll();
    std::this_thread::sleep_for(20ms);
    TestClient aliceClient(loop, fixture.udpPort, alice);
    TestClient bobClient(loop, fixture.udpPort, bob);
    const auto updateBoth = [&] {
        aliceClient.update();
        bobClient.update();
    };
    REQUIRE(waitUntil(loop, [&] { return aliceClient.accepted && bobClient.accepted; }, updateBoth));

    // The statuses that follow list both players, alive, with their scores and their latencies
    std::optional<GameStatusMessage> status;
    std::uint32_t previousTick = 0;
    int received = 0;
    const auto start = Clock::now();
    REQUIRE(waitUntil(
        loop,
        [&] {
            while (const std::optional<Bytes> payload = master.take(instance_message::GameStatus)) {
                status = decodeGameStatus(*payload);
                REQUIRE(status);
                CHECK(status->tick >= previousTick);
                previousTick = status->tick;
                ++received;
            }
            return status && status->players.size() == 2 && status->players[0].latency &&
                   status->players[1].latency && received >= 3;
        },
        updateBoth));
    // One a second: three in about two seconds at least, never a flood
    CHECK(Clock::now() - start >= 1500ms);
    CHECK(status->tick > 60);
    std::vector<std::uint32_t> ids;
    for (const PlayerStatus &entry : status->players) {
        ids.push_back(entry.playerId);
        CHECK(entry.alive);
        CHECK(entry.score == 0);
    }
    std::ranges::sort(ids);
    CHECK(ids == std::vector<std::uint32_t>{42, 43});
}

TEST_CASE("An instance whose master goes away disconnects its players and fails", "[instance]")
{
    InstanceFixture fixture;
    fixture.start();
    fixture.configure();
    const AddPlayerMessage alice = player(42, 5, "Alice");
    fixture.master.send(instance_message::AddPlayer, alice);
    fixture.loop.poll();
    std::this_thread::sleep_for(20ms);
    TestClient client(fixture.loop, fixture.udpPort, alice);
    REQUIRE(waitUntil(fixture.loop, [&] { return client.accepted.has_value(); }, [&] { client.update(); }));

    fixture.master.connection.reset();

    REQUIRE(waitUntil(fixture.loop, [&] { return client.end.has_value(); }, [&] { client.update(); }));
    CHECK(client.end->reason == DisconnectReason::Shutdown);
    CHECK(fixture.instance->waitForExit(fixture.loop) == Failure);
}

TEST_CASE("An instance refuses more players than its game takes", "[instance]")
{
    InstanceFixture fixture;
    fixture.start();
    fixture.configure({"Solo", 1});

    fixture.master.send(instance_message::AddPlayer, player(1, 1, "Alice"));
    fixture.master.send(instance_message::AddPlayer, player(2, 2, "Bob"));

    CHECK(fixture.instance->waitForExit(fixture.loop) == Failure);
    CHECK(waitUntil(fixture.loop, [&] { return fixture.master.closed; }));
}

TEST_CASE("An instance whose UDP port is taken fails", "[instance]")
{
    EventLoop other;
    const UdpSocket taken(other, Address::anyIpv4(0));
    InstanceFixture fixture;
    fixture.instance.emplace(
        InstanceOptions{.id = 7, .udpPort = taken.localAddress().port(), .masterPort = fixture.master.port()},
        formatSecret(testSecret()) + "\n");
    REQUIRE(waitUntil(fixture.loop, [&] { return fixture.master.has(instance_message::InstanceHello); }));

    fixture.master.send(instance_message::Configure, ConfigureMessage{"Game", 2});

    CHECK(fixture.instance->waitForExit(fixture.loop) == Failure);
    CHECK_FALSE(fixture.master.has(instance_message::Ready));
}

TEST_CASE("A standalone instance takes clients with the public keys, until stopped", "[instance]")
{
    if (!standaloneAvailable()) {
        SKIP("The standalone mode only exists in debug builds");
    }
    EventLoop loop;
    // A free port, given back to the system just before
    std::uint16_t port = 0;
    {
        const UdpSocket probe(loop, Address::anyIpv4(0));
        port = probe.localAddress().port();
    }
    loop.poll();
    RunningInstance instance(InstanceOptions{.id = 1, .udpPort = port, .masterPort = 0, .standalone = true},
                             "");
    std::this_thread::sleep_for(100ms);

    TestClient client(loop, port, rtype::game::standalone::sessionId(2),
                      rtype::game::standalone::clientKey(2), rtype::game::standalone::serverKey(2));
    REQUIRE(waitUntil(loop, [&] { return client.accepted.has_value(); }, [&] { client.update(); }));
    REQUIRE(waitUntil(loop, [&] { return !client.joined().empty(); }, [&] { client.update(); }));
    CHECK(client.joined()[0].name == "Player 3");

    // Ctrl+C: its clients are told, and it exits with 0
    std::raise(SIGINT);
    REQUIRE(waitUntil(loop, [&] { return client.end.has_value(); }, [&] { client.update(); }));
    CHECK(client.end->reason == DisconnectReason::Shutdown);
    CHECK(instance.waitForExit(loop) == Success);
}

// --- The voice ---

TEST_CASE("The rate of the voice lets a burst of 200 ms pass, then 60 frames per second", "[instance][voice]")
{
    VoiceRate rate;
    const auto start = VoiceRate::Clock::now();
    int allowed = 0;
    for (int i = 0; i < 40; ++i) {
        allowed += rate.allow(start) ? 1 : 0;
    }
    CHECK(allowed == 10);

    // A second later, the bucket is full again, no more
    allowed = 0;
    for (int i = 0; i < 40; ++i) {
        allowed += rate.allow(start + 1s) ? 1 : 0;
    }
    CHECK(allowed == 10);
}

TEST_CASE("The rate of the voice never drops a voice of 50 frames per second, and cuts one of 100",
          "[instance][voice]")
{
    VoiceRate voice;
    VoiceRate flood;
    const auto start = VoiceRate::Clock::now();
    int voiceAllowed = 0;
    int floodAllowed = 0;
    for (int frame = 0; frame < 500; ++frame) {
        voiceAllowed += voice.allow(start + frame * 20ms) ? 1 : 0;
    }
    for (int frame = 0; frame < 1000; ++frame) {
        floodAllowed += flood.allow(start + frame * 10ms) ? 1 : 0;
    }
    CHECK(voiceAllowed == 500);
    // 10 seconds at 60 per second, and the burst of the start
    CHECK(floodAllowed >= 600);
    CHECK(floodAllowed <= 611);
}

namespace
{

/// A game host on the loopback with four clients connected, slots 0 to 3.
struct VoiceGame {
    EventLoop loop;
    GameThread game{{}, {}, [](TickResult) {}};
    GameHost host{loop, std::make_unique<DirectTransport>(loop, Address::loopbackIpv4(0)), 4, game, {}};
    std::vector<std::unique_ptr<TestClient>> clients;

    explicit VoiceGame(std::size_t count = 4)
    {
        for (std::uint32_t i = 0; i < count; ++i) {
            const AddPlayerMessage added = player(40 + i, 10 + i, "Player" + std::to_string(i));
            REQUIRE(host.addPlayer(added));
            clients.push_back(std::make_unique<TestClient>(loop, host.localAddress().port(), added));
            // One at a time, so that each takes the next slot
            REQUIRE(waitUntil(loop, [&] { return clients.back()->accepted.has_value(); }, [&] { update(); }));
            REQUIRE(clients.back()->accepted->slot == i);
        }
    }

    void update()
    {
        for (const auto &client : clients) {
            client->update();
        }
    }

    /// Every client knows these backends.
    void setBackends(const std::array<rtype::voice::Backend, 4> &backends)
    {
        for (std::size_t i = 0; i < clients.size(); ++i) {
            clients[i]->sendVoiceState(backends[i]);
        }
        rtype::game::VoiceRosterMessage expected;
        std::copy_n(backends.begin(), clients.size(), expected.backends.begin());
        REQUIRE(waitUntil(
            loop,
            [&] {
                return std::ranges::all_of(
                    clients, [&](const auto &client) { return client->lastRoster() == expected; });
            },
            [&] { update(); }));
    }

    /// Lets the datagrams under way arrive.
    void settle()
    {
        (void)waitUntil(loop, [] { return false; }, [&] { update(); }, 150ms);
    }
};

std::vector<std::uint8_t> sorted(std::vector<std::uint8_t> slots)
{
    std::ranges::sort(slots);
    return slots;
}

} // namespace

TEST_CASE("A client knows what carries the voice of each player: at its connection, at every change",
          "[instance][voice]")
{
    VoiceGame voice(2);
    // Nobody said anything: no voice for anyone
    CHECK(voice.clients[1]->lastRoster() == rtype::game::VoiceRosterMessage{});

    voice.setBackends({rtype::voice::Backend::Game, rtype::voice::Backend::Discord});

    // A newcomer is told at once what carries the voice of the others
    const AddPlayerMessage carol = player(50, 20, "Carol");
    REQUIRE(voice.host.addPlayer(carol));
    TestClient newcomer(voice.loop, voice.host.localAddress().port(), carol);
    const rtype::game::VoiceRosterMessage expected{
        {rtype::voice::Backend::Game, rtype::voice::Backend::Discord, rtype::voice::Backend::None,
         rtype::voice::Backend::None}};
    REQUIRE(waitUntil(
        voice.loop, [&] { return newcomer.lastRoster() == expected; },
        [&] {
            voice.update();
            newcomer.update();
        }));

    // The player on Discord leaves: the others are told its slot has no voice
    voice.host.removePlayer(41, RemovePlayerReason::Kicked);
    const rtype::game::VoiceRosterMessage afterLeave{
        {rtype::voice::Backend::Game, rtype::voice::Backend::None, rtype::voice::Backend::None,
         rtype::voice::Backend::None}};
    CHECK(waitUntil(
        voice.loop, [&] { return newcomer.lastRoster() == afterLeave; },
        [&] {
            voice.update();
            newcomer.update();
        }));
}

TEST_CASE("A client knows what each player tells about its voice: at its connection, at every change",
          "[instance][voice]")
{
    using rtype::voice::Status;
    VoiceGame voice(2);
    CHECK(waitUntil(
        voice.loop, [&] { return voice.clients[1]->lastStatuses() == rtype::game::VoiceStatusesMessage{}; },
        [&] { voice.update(); }));

    voice.clients[0]->sendVoiceStatus({.muted = true});
    voice.clients[1]->sendVoiceStatus({.noMicrophone = true});
    const rtype::game::VoiceStatusesMessage expected{{Status{.muted = true}, Status{.noMicrophone = true}}};
    REQUIRE(waitUntil(
        voice.loop,
        [&] {
            return voice.clients[0]->lastStatuses() == expected &&
                   voice.clients[1]->lastStatuses() == expected;
        },
        [&] { voice.update(); }));

    // A newcomer is told at once
    const AddPlayerMessage carol = player(50, 20, "Carol");
    REQUIRE(voice.host.addPlayer(carol));
    TestClient newcomer(voice.loop, voice.host.localAddress().port(), carol);
    REQUIRE(waitUntil(
        voice.loop, [&] { return newcomer.lastStatuses() == expected; },
        [&] {
            voice.update();
            newcomer.update();
        }));

    // The muted player leaves: its slot is told empty
    voice.host.removePlayer(40, RemovePlayerReason::Kicked);
    const rtype::game::VoiceStatusesMessage afterLeave{{Status{}, Status{.noMicrophone = true}}};
    CHECK(waitUntil(
        voice.loop, [&] { return newcomer.lastStatuses() == afterLeave; },
        [&] {
            voice.update();
            newcomer.update();
        }));
}

TEST_CASE("In a game of two players on Discord and two on the game, the game carries each voice once",
          "[instance][voice]")
{
    using rtype::voice::Backend;
    VoiceGame voice;
    // Slots 0 and 1 on the game, 2 and 3 on Discord
    voice.setBackends({Backend::Game, Backend::Game, Backend::Discord, Backend::Discord});
    for (std::uint8_t slot = 0; slot < 4; ++slot) {
        voice.clients[slot]->sendVoice(slot);
    }
    REQUIRE(waitUntil(
        voice.loop,
        [&] {
            return voice.clients[0]->voicesFrom().size() == 3 && voice.clients[2]->voicesFrom().size() == 2;
        },
        [&] { voice.update(); }));
    voice.settle();

    // The players on the game hear the three others through it, never themselves
    CHECK(sorted(voice.clients[0]->voicesFrom()) == std::vector<std::uint8_t>{1, 2, 3});
    CHECK(sorted(voice.clients[1]->voicesFrom()) == std::vector<std::uint8_t>{0, 2, 3});
    // The players on Discord hear the players on the game through it, each other through Discord only
    CHECK(sorted(voice.clients[2]->voicesFrom()) == std::vector<std::uint8_t>{0, 1});
    CHECK(sorted(voice.clients[3]->voicesFrom()) == std::vector<std::uint8_t>{0, 1});
}

TEST_CASE("A player without voice is neither heard nor sent the voice of the others", "[instance][voice]")
{
    using rtype::voice::Backend;
    VoiceGame voice(3);
    voice.setBackends({Backend::Game, Backend::Game, Backend::None});
    // Slot 2 talks without having said it has a voice: dropped
    voice.clients[2]->sendVoice(1);
    voice.clients[0]->sendVoice(1);
    REQUIRE(waitUntil(
        voice.loop, [&] { return voice.clients[1]->voicesFrom().size() == 1; }, [&] { voice.update(); }));
    voice.settle();

    CHECK(voice.clients[1]->voicesFrom() == std::vector<std::uint8_t>{0});
    CHECK(voice.clients[0]->voicesFrom().empty());
    CHECK(voice.clients[2]->voicesFrom().empty());
}

TEST_CASE("The game forwards no more voice than its rate, and drops a malformed one", "[instance][voice]")
{
    using rtype::voice::Backend;
    VoiceGame voice(2);
    voice.setBackends({Backend::Game, Backend::Game});
    // A frame of 161 bytes, as no client of ours writes it
    std::vector<std::byte> tooBig{std::byte{0}, std::byte{1}, std::byte{0}, std::byte{161}};
    tooBig.resize(tooBig.size() + 161, std::byte{0xF8});
    voice.clients[0]->session.send(rtype::game::message_type::Voice, tooBig, Delivery::Unreliable);
    // 40 frames at once: the burst of the rate passes, not the rest
    for (std::uint16_t sequence = 0; sequence < 40; ++sequence) {
        voice.clients[0]->sendVoice(sequence);
    }
    REQUIRE(waitUntil(
        voice.loop, [&] { return voice.clients[1]->voicesFrom().size() >= 10; }, [&] { voice.update(); }));
    voice.settle();
    CHECK(voice.clients[1]->voicesFrom().size() == 10);
}
