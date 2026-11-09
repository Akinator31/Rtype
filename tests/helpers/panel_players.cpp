// Players for the end-to-end tests of the panel (web/admin/e2e): clients of the lobby of a real master, who
// create a game together and play it, until they are stopped.
//
//   rtype_panel_players <lobby port> <key file of the master> <game name> <name>...
//
// A name written `<login>:<password>` signs in to that account after HELLO, as a guest named `Pilot` until
// then: the player then plays under its username.
//
// The first player creates the game, the others join it; each one then holds its session of the game, over
// UDP, and pings the lobby, which closes a connection silent for 30 seconds. Once every session is accepted,
// it writes "ready <game id>" on its standard output. A player removed, or a game ended, simply stops
// playing.
//
// It never runs more than ten minutes: a test that fails leaves no program behind for long.

#include "LobbyClient.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace
{

namespace lobby = rtype::lobby;
using rtype::network::Address;
using rtype::network::EventLoop;

constexpr int Usage = 2;
constexpr int Failed = 1;
constexpr std::chrono::minutes Lifetime{10};
constexpr std::chrono::seconds PingInterval{5};

/// A client of a game, over UDP, with the keys of its GAME_JOINED.
struct GameClient {
    rtype::network::UdpSocket socket;
    Address game;
    rtype::network::ClientSession session;
    bool accepted = false;
    bool ended = false;

    GameClient(EventLoop &loop, const lobby::GameJoinedMessage &joined)
        : socket(loop, Address::loopbackIpv4(0)), game(Address::loopbackIpv4(joined.udpPort)),
          session(joined.sessionId, rtype::network::DatagramKey(joined.clientKey),
                  rtype::network::DatagramKey(joined.serverKey),
                  [this](std::span<const std::byte> datagram) { socket.send(game, datagram); })
    {
        socket.startReceiving([this](const Address &from, std::span<const std::byte> datagram) {
            if (from == game) {
                session.receive(datagram, std::chrono::steady_clock::now());
            }
        });
        session.start([this](std::span<const std::byte>) { accepted = true; },
                      [](std::uint8_t, std::span<const std::byte>) {},
                      [this](rtype::network::SessionEnd) { ended = true; }, std::chrono::steady_clock::now());
    }

    void update()
    {
        if (!ended) {
            session.update(std::chrono::steady_clock::now());
        }
    }
};

struct Player {
    std::unique_ptr<testing::LobbyClient> lobby;
    std::unique_ptr<GameClient> game;
};

/// The next reply of a player from `index`, which must be of this type, and moves `index` past it. The
/// progression of a signed-in account (Progress) is skipped: the lobby sends it whenever it has read it.
std::optional<testing::LobbyClient::Bytes> expect(EventLoop &loop, testing::LobbyClient &client,
                                                  std::size_t &index, std::uint8_t type)
{
    auto answer = client.reply(loop, index++);
    while (answer && answer->first == lobby::message_type::Progress) {
        answer = client.reply(loop, index++);
    }
    if (!answer || answer->first != type) {
        return std::nullopt;
    }
    return answer->second;
}

int run(std::uint16_t port, const std::string &keyFile, const std::string &gameName,
        const std::vector<std::string> &names)
{
    EventLoop loop;
    const Address master = Address::loopbackIpv4(port);
    const rtype::network::KeyFingerprint pinned = rtype::network::ServerIdentity::load(keyFile).fingerprint();

    std::vector<Player> players;
    std::uint32_t gameId = 0;
    for (const std::string &given : names) {
        const std::size_t colon = given.find(':');
        const std::string name = given.substr(0, colon);
        Player &player = players.emplace_back();
        player.lobby = std::make_unique<testing::LobbyClient>(loop, master, pinned);
        if (!testing::waitUntil(loop, [&] { return player.lobby->connection.has_value(); })) {
            std::cerr << "Cannot connect to the lobby on port " << port << '\n';
            return Failed;
        }
        player.lobby->hello(colon == std::string::npos ? name : "Pilot");
        std::size_t next = 0;
        if (!expect(loop, *player.lobby, next, lobby::message_type::Welcome)) {
            std::cerr << name << " was not welcomed\n";
            return Failed;
        }
        if (colon != std::string::npos) {
            player.lobby->send(lobby::message_type::SignIn,
                               lobby::encode(lobby::SignInMessage{name, given.substr(colon + 1)}));
            if (!expect(loop, *player.lobby, next, lobby::message_type::SignedIn)) {
                std::cerr << name << " could not sign in\n";
                return Failed;
            }
        }
        if (gameId == 0) {
            player.lobby->send(lobby::message_type::CreateGame,
                               lobby::encode(lobby::CreateGameMessage{gameName, lobby::MaxPlayers}));
        } else {
            player.lobby->send(lobby::message_type::JoinGame, lobby::encode(lobby::JoinGameMessage{gameId}));
        }
        const auto payload = expect(loop, *player.lobby, next, lobby::message_type::GameJoined);
        const auto joined = payload ? lobby::decodeGameJoined(*payload) : std::nullopt;
        if (!joined) {
            std::cerr << name << " could not " << (gameId == 0 ? "create" : "join") << " the game\n";
            return Failed;
        }
        gameId = joined->gameId;
        player.game = std::make_unique<GameClient>(loop, *joined);
    }

    const auto play = [&] {
        for (Player &player : players) {
            player.game->update();
        }
    };
    const bool accepted = testing::waitUntil(loop, [&] {
        play();
        for (const Player &player : players) {
            if (!player.game->accepted) {
                return false;
            }
        }
        return true;
    });
    if (!accepted) {
        std::cerr << "The game did not accept every player\n";
        return Failed;
    }
    std::cout << "ready " << gameId << std::endl;

    const auto deadline = std::chrono::steady_clock::now() + Lifetime;
    auto nextPing = std::chrono::steady_clock::now() + PingInterval;
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::chrono::steady_clock::now() >= nextPing) {
            nextPing += PingInterval;
            for (Player &player : players) {
                if (!player.lobby->closed) {
                    player.lobby->send(lobby::message_type::Ping);
                }
            }
        }
        play();
        loop.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 5) {
        std::cerr << "Usage: rtype_panel_players <lobby port> <key file> <game name> <name>...\n";
        return Usage;
    }
    try {
        const auto port = static_cast<std::uint16_t>(std::stoul(argv[1]));
        return run(port, argv[2], argv[3], std::vector<std::string>(argv + 4, argv + argc));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return Failed;
    }
}
