// Tests of the messages of the lobby protocol (libs/lobby): the bytes of the specification, round trips, and
// every malformed case (docs/protocol/lobby.md, section 5).

#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Frame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace rtype::lobby;
using Bytes = std::vector<std::byte>;

namespace
{

Bytes bytes(std::initializer_list<int> values)
{
    Bytes result;
    for (const int value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

std::array<std::byte, 32> key(int value)
{
    std::array<std::byte, 32> result{};
    result.fill(static_cast<std::byte>(value));
    return result;
}

GameListMessage twoGames()
{
    return {{{3, "Bydo hunters", 2, 4}, {9, "Solo", 0, 1}}};
}

} // namespace

TEST_CASE("HELLO is the frame of the example of the specification", "[lobby]")
{
    const Bytes payload = encode(HelloMessage{2, "Pavel"});

    CHECK(payload == bytes({0x00, 0x02, 0x05, 'P', 'a', 'v', 'e', 'l'}));
    CHECK(rtype::network::makeFrame(message_type::Hello, payload) ==
          bytes({0x00, 0x09, 0x01, 0x00, 0x02, 0x05, 'P', 'a', 'v', 'e', 'l'}));
    CHECK(decodeHello(payload) == HelloMessage{2, "Pavel"});
}

TEST_CASE("The lobby messages are written field by field", "[lobby]")
{
    CHECK(encode(CreateGameMessage{"Duo", 2}) == bytes({3, 'D', 'u', 'o', 2}));
    CHECK(encode(JoinGameMessage{0x01020304}) == bytes({1, 2, 3, 4}));
    CHECK(encode(WelcomeMessage{7}) == bytes({0, 0, 0, 7}));
    CHECK(encode(GameListMessage{{{3, "Duo", 1, 2}}}) == bytes({0, 1, 0, 0, 0, 3, 3, 'D', 'u', 'o', 1, 2}));
    CHECK(encode(GameListMessage{}) == bytes({0, 0}));
    CHECK(encode(LeftGameMessage{5, LeftGameReason::GameCrashed}) == bytes({0, 0, 0, 5, 2}));
    CHECK(encode(ErrorMessage{message_type::JoinGame, ErrorCode::GameFull}) == bytes({0x04, 0x00, 0x08}));
    CHECK(encode(ErrorMessage{0, ErrorCode::Kicked}) == bytes({0x00, 0x00, 0x0D}));
    CHECK(encode(LeftGameMessage{5, LeftGameReason::EndedByAdmin}) == bytes({0, 0, 0, 5, 5}));
    CHECK(encode(ServerMessageMessage{"Hi"}) == bytes({2, 'H', 'i'}));

    const Bytes joined = encode(GameJoinedMessage{7, 4251, 9, key(1), key(2)});
    REQUIRE(joined.size() == 74);
    CHECK(Bytes(joined.begin(), joined.begin() + 10) == bytes({0, 0, 0, 7, 0x10, 0x9B, 0, 0, 0, 9}));
    CHECK(joined[10] == std::byte{1});
    CHECK(joined[42] == std::byte{2});
}

TEST_CASE("Every lobby message survives a round trip", "[lobby]")
{
    CHECK(decodeHello(encode(HelloMessage{1, "Élodie 🚀"})) == HelloMessage{1, "Élodie 🚀"});
    CHECK(decodeCreateGame(encode(CreateGameMessage{"Bydo hunters", 4})) ==
          CreateGameMessage{"Bydo hunters", 4});
    CHECK(decodeJoinGame(encode(JoinGameMessage{0xFFFFFFFF})) == JoinGameMessage{0xFFFFFFFF});
    CHECK(decodeWelcome(encode(WelcomeMessage{0xFFFFFFFF})) == WelcomeMessage{0xFFFFFFFF});
    CHECK(decodeGameList(encode(twoGames())) == twoGames());
    const GameJoinedMessage joined{7, 4251, 9, key(1), key(2)};
    CHECK(decodeGameJoined(encode(joined)) == joined);
    for (const LeftGameReason reason :
         {LeftGameReason::Requested, LeftGameReason::GameOver, LeftGameReason::GameCrashed,
          LeftGameReason::TimedOut, LeftGameReason::Kicked, LeftGameReason::EndedByAdmin}) {
        CHECK(decodeLeftGame(encode(LeftGameMessage{5, reason})) == LeftGameMessage{5, reason});
    }
    CHECK(decodeError(encode(ErrorMessage{0, ErrorCode::Malformed})) ==
          ErrorMessage{0, ErrorCode::Malformed});
    for (const ErrorCode code : {ErrorCode::Kicked, ErrorCode::Banned}) {
        CHECK(decodeError(encode(ErrorMessage{0, code})) == ErrorMessage{0, code});
    }
    const ServerMessageMessage message{"Server restarts in 5 minutes: finish your wave 🚀"};
    CHECK(decodeServerMessage(encode(message)) == message);
    const ServerMessageMessage longest{std::string(MaxServerMessageLength, 'm')};
    CHECK(decodeServerMessage(encode(longest)) == longest);
    CHECK(decodeEmpty({}));
    CHECK_FALSE(decodeEmpty(bytes({0})));

    // 256 games with the longest names still fit in a frame
    GameListMessage full;
    for (std::uint32_t id = 1; id <= MaxGamesInList; ++id) {
        full.games.push_back({id, std::string(MaxGameNameLength, 'g'), 4, 4});
    }
    const Bytes payload = encode(full);
    CHECK(payload.size() <= rtype::network::MaxFramePayloadSize);
    CHECK(decodeGameList(payload) == full);
}

TEST_CASE("Names are checked apart from the decoding, for INVALID_NAME", "[lobby]")
{
    CHECK(isValidPlayerName("Pavel"));
    CHECK(isValidPlayerName(std::string(16, 'p')));
    CHECK_FALSE(isValidPlayerName(""));
    CHECK_FALSE(isValidPlayerName(std::string(17, 'p')));
    CHECK_FALSE(isValidPlayerName("tab\there"));
    CHECK_FALSE(isValidPlayerName("\xFF\xFE"));
    CHECK(isValidGameName(std::string(32, 'g')));
    CHECK_FALSE(isValidGameName(std::string(33, 'g')));

    // Decoded as they come: the master answers INVALID_NAME, not MALFORMED
    CHECK(decodeHello(bytes({0, 1, 0}))->name.empty());
    CHECK(decodeHello(bytes({0, 1, 2, 0xFF, 0xFE}))->name == "\xFF\xFE");
    CHECK(decodeHello(bytes({0, 2, 1, 'A'}))->version == 2);
    CHECK(decodeCreateGame(bytes({0, 2}))->name.empty());

    CHECK_THROWS_AS(encode(HelloMessage{1, ""}), std::invalid_argument);
    CHECK_THROWS_AS(encode(CreateGameMessage{"", 2}), std::invalid_argument);
}

TEST_CASE("The lobby messages refuse what the protocol forbids", "[lobby]")
{
    CHECK_FALSE(decodeCreateGame(bytes({1, 'A', 0})));   // no player
    CHECK_FALSE(decodeCreateGame(bytes({1, 'A', 5})));   // 5 players
    CHECK_FALSE(decodeWelcome(bytes({0, 0, 0, 0})));     // player 0
    CHECK_FALSE(decodeLeftGame(bytes({0, 0, 0, 1, 6}))); // reason 6
    CHECK_FALSE(decodeGameList(bytes({0x01, 0x01})));    // 257 games
    // Not sorted, the same id twice, id 0, an empty name, 0 or 5 players at most
    CHECK_FALSE(decodeGameList(bytes({0, 2, 0, 0, 0, 9, 1, 'B', 0, 1, 0, 0, 0, 3, 1, 'A', 0, 1})));
    CHECK_FALSE(decodeGameList(bytes({0, 2, 0, 0, 0, 3, 1, 'B', 0, 1, 0, 0, 0, 3, 1, 'A', 0, 1})));
    CHECK_FALSE(decodeGameList(bytes({0, 1, 0, 0, 0, 0, 1, 'A', 0, 1})));
    CHECK_FALSE(decodeGameList(bytes({0, 1, 0, 0, 0, 1, 0, 0, 1})));
    CHECK_FALSE(decodeGameList(bytes({0, 1, 0, 0, 0, 1, 1, 'A', 0, 0})));
    CHECK_FALSE(decodeGameList(bytes({0, 1, 0, 0, 0, 1, 1, 'A', 0, 5})));
    Bytes joined = encode(GameJoinedMessage{7, 4251, 9, {}, {}});
    joined[9] = std::byte{0};
    CHECK_FALSE(decodeGameJoined(joined)); // session 0
    // A server message is 1 to 200 bytes of text
    CHECK_FALSE(decodeServerMessage(bytes({0})));
    CHECK_FALSE(decodeServerMessage(bytes({1, '\n'})));
    CHECK_FALSE(decodeServerMessage(bytes({2, 0xFF, 0xFE})));
    Bytes tooLong = encode(ServerMessageMessage{std::string(MaxServerMessageLength, 'm')});
    tooLong[0] = std::byte{MaxServerMessageLength + 1};
    tooLong.push_back(std::byte{'m'});
    CHECK_FALSE(decodeServerMessage(tooLong));
    // An unknown error code is kept: the client shows a generic error
    CHECK(decodeError(bytes({0, 0x01, 0x00}))->code == ErrorCode{256});

    CHECK_THROWS_AS(encode(WelcomeMessage{0}), std::invalid_argument);
    CHECK_THROWS_AS(encode(CreateGameMessage{"Game", 0}), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameJoinedMessage{7, 4251, 0, {}, {}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameListMessage{{{9, "B", 0, 1}, {3, "A", 0, 1}}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameListMessage{{{0, "A", 0, 1}}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameListMessage{{{1, "A", 0, 5}}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(GameListMessage{std::vector<GameInfo>(257)}), std::invalid_argument);
    CHECK_THROWS_AS(encode(ServerMessageMessage{""}), std::invalid_argument);
    CHECK_THROWS_AS(encode(ServerMessageMessage{std::string(MaxServerMessageLength + 1, 'm')}),
                    std::invalid_argument);
    CHECK_THROWS_AS(encode(ServerMessageMessage{"two\nlines"}), std::invalid_argument);

    CHECK(closesConnection(ErrorCode::Malformed));
    CHECK(closesConnection(ErrorCode::UnsupportedVersion));
    CHECK(closesConnection(ErrorCode::Kicked));
    CHECK(closesConnection(ErrorCode::Banned));
    CHECK_FALSE(closesConnection(ErrorCode::HelloRequired));
    CHECK_FALSE(closesConnection(ErrorCode::GameStartFailed));
}

TEST_CASE("A lobby message is refused if it is cut or longer than its fields", "[lobby]")
{
    const std::vector<std::pair<Bytes, std::function<bool(std::span<const std::byte>)>>> messages = {
        {encode(HelloMessage{1, "Bob"}), [](auto p) { return decodeHello(p).has_value(); }},
        {encode(CreateGameMessage{"G", 1}), [](auto p) { return decodeCreateGame(p).has_value(); }},
        {encode(JoinGameMessage{1}), [](auto p) { return decodeJoinGame(p).has_value(); }},
        {encode(WelcomeMessage{1}), [](auto p) { return decodeWelcome(p).has_value(); }},
        {encode(twoGames()), [](auto p) { return decodeGameList(p).has_value(); }},
        {encode(GameJoinedMessage{1, 2, 3, {}, {}}), [](auto p) { return decodeGameJoined(p).has_value(); }},
        {encode(LeftGameMessage{1, {}}), [](auto p) { return decodeLeftGame(p).has_value(); }},
        {encode(ErrorMessage{1, ErrorCode::GameFull}), [](auto p) { return decodeError(p).has_value(); }},
        {encode(ServerMessageMessage{"Hi"}), [](auto p) { return decodeServerMessage(p).has_value(); }},
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

TEST_CASE("Random bytes never crash a lobby decoder", "[lobby]")
{
    std::mt19937 random(42);
    std::uniform_int_distribution<int> size(0, 200);
    std::uniform_int_distribution<int> byte(0, 255);
    for (int i = 0; i < 20000; ++i) {
        Bytes payload(static_cast<std::size_t>(size(random)));
        for (std::byte &value : payload) {
            value = static_cast<std::byte>(byte(random));
        }
        (void)decodeHello(payload);
        (void)decodeCreateGame(payload);
        (void)decodeJoinGame(payload);
        (void)decodeWelcome(payload);
        (void)decodeGameList(payload);
        (void)decodeGameJoined(payload);
        (void)decodeLeftGame(payload);
        (void)decodeError(payload);
        (void)decodeServerMessage(payload);
    }
    SUCCEED();
}
