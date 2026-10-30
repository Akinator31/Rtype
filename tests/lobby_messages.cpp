// Tests of the messages of the lobby protocol (libs/lobby): the bytes of the specification, round trips, and
// every malformed case (docs/protocol/lobby.md, section 5).

#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Frame.hpp"
#include "rtype/network/Writer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace rtype::lobby;
using Bytes = std::vector<std::byte>;
using rtype::network::Writer;

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

/// The HELLO of a guest.
HelloMessage guest(const std::string &name, const std::string &locale = "en")
{
    return {.version = ProtocolVersion, .name = name, .locale = locale, .token = std::nullopt};
}

WelcomeMessage welcome(std::uint32_t playerId, ServerFeatures features = {},
                       std::optional<AccountInfo> account = std::nullopt)
{
    return {.playerId = playerId, .features = features, .account = std::move(account)};
}

HelloMessage resuming()
{
    return {.version = ProtocolVersion, .name = "", .locale = "fr-FR", .token = key(7)};
}

AccountInfo pavel()
{
    return {.id = 0x0102030405060708, .username = "Pavel_42", .role = AccountRole::Admin};
}

/// Every message of the accounts with a payload, with its decoder.
std::vector<std::pair<Bytes, std::function<bool(std::span<const std::byte>)>>> accountMessages()
{
    return {
        {encode(RegisterMessage{"Pavel", "pavel@example.org", "correct horse"}),
         [](auto p) { return decodeRegister(p).has_value(); }},
        {encode(VerifyEmailMessage{"Pavel", "012345"}),
         [](auto p) { return decodeVerifyEmail(p).has_value(); }},
        {encode(ResendCodeMessage{"pavel@example.org"}),
         [](auto p) { return decodeResendCode(p).has_value(); }},
        {encode(SignInMessage{"Pavel", "correct horse"}), [](auto p) { return decodeSignIn(p).has_value(); }},
        {encode(ForgotPasswordMessage{"pavel@example.org"}),
         [](auto p) { return decodeForgotPassword(p).has_value(); }},
        {encode(ResetPasswordMessage{"pavel@example.org", "999999", "battery staple"}),
         [](auto p) { return decodeResetPassword(p).has_value(); }},
        {encode(DiscordConfirmMessage{"K7P2ZX"}), [](auto p) { return decodeDiscordConfirm(p).has_value(); }},
        {encode(ChooseUsernameMessage{"Pavel"}), [](auto p) { return decodeChooseUsername(p).has_value(); }},
        {encode(DeleteAccountMessage{"correct horse"}),
         [](auto p) { return decodeDeleteAccount(p).has_value(); }},
        {encode(DeleteAccountMessage{""}), [](auto p) { return decodeDeleteAccount(p).has_value(); }},
        {encode(SignedInMessage{key(3), pavel()}), [](auto p) { return decodeSignedIn(p).has_value(); }},
        {encode(DiscordUrlMessage{"https://discord.com/oauth2/authorize?state=x"}),
         [](auto p) { return decodeDiscordUrl(p).has_value(); }},
    };
}

} // namespace

TEST_CASE("HELLO is the frame of the example of the specification", "[lobby]")
{
    const HelloMessage hello{.version = 3, .name = "Pavel", .locale = "fr", .token = std::nullopt};
    const Bytes payload = encode(hello);

    CHECK(payload == bytes({0x00, 0x03, 0x05, 'P', 'a', 'v', 'e', 'l', 0x02, 'f', 'r', 0x00}));
    CHECK(rtype::network::makeFrame(message_type::Hello, payload) ==
          bytes({0x00, 0x0D, 0x01, 0x00, 0x03, 0x05, 'P', 'a', 'v', 'e', 'l', 0x02, 'f', 'r', 0x00}));
    CHECK(decodeHello(payload) == hello);
}

TEST_CASE("A HELLO with a session token has no name, and the token after the locale", "[lobby]")
{
    const Bytes payload = encode(resuming());

    REQUIRE(payload.size() == 2 + 1 + 6 + 1 + 32);
    CHECK(Bytes(payload.begin(), payload.begin() + 10) ==
          bytes({0x00, 0x03, 0x00, 0x05, 'f', 'r', '-', 'F', 'R', 0x01}));
    CHECK(payload[10] == std::byte{7});
    CHECK(decodeHello(payload) == resuming());

    // A name beside a token is malformed: the token signs in an account, whose name the master knows
    Bytes named = bytes({0x00, 0x03, 0x01, 'A', 0x02, 'e', 'n', 0x01});
    named.resize(named.size() + 32);
    CHECK_FALSE(decodeHello(named));
    HelloMessage both = resuming();
    both.name = "Pavel";
    CHECK_THROWS_AS(encode(both), std::invalid_argument);
}

TEST_CASE("A HELLO of another version is read as its version alone, for UNSUPPORTED_VERSION", "[lobby]")
{
    // HELLO of version 2: a version and a name, without locale
    const auto older = decodeHello(bytes({0x00, 0x02, 0x05, 'P', 'a', 'v', 'e', 'l'}));
    REQUIRE(older);
    CHECK(older->version == 2);
    CHECK(older->name.empty());
    CHECK_FALSE(older->token);
    CHECK(decodeHello(bytes({0x00, 0x04}))->version == 4);
    CHECK(decodeHello(bytes({0x00, 0x01, 0xFF, 0xFF, 0xFF}))->version == 1);
    CHECK_FALSE(decodeHello(bytes({0x00})));
}

TEST_CASE("The lobby messages are written field by field", "[lobby]")
{
    CHECK(encode(CreateGameMessage{"Duo", 2}) == bytes({3, 'D', 'u', 'o', 2}));
    CHECK(encode(JoinGameMessage{0x01020304}) == bytes({1, 2, 3, 4}));
    CHECK(encode(welcome(7)) == bytes({0, 0, 0, 7, 0x01, 0}));
    CHECK(encode(welcome(7, {.guests = false, .passwordAccounts = true, .discord = true})) ==
          bytes({0, 0, 0, 7, 0x06, 0}));
    CHECK(encode(welcome(7, {}, AccountInfo{9, "Ann", AccountRole::Player})) ==
          bytes({0, 0, 0, 7, 0x01, 1, 0, 0, 0, 0, 0, 0, 0, 9, 3, 'A', 'n', 'n', 0}));
    CHECK(encode(GameListMessage{{{3, "Duo", 1, 2}}}) == bytes({0, 1, 0, 0, 0, 3, 3, 'D', 'u', 'o', 1, 2}));
    CHECK(encode(GameListMessage{}) == bytes({0, 0}));
    CHECK(encode(LeftGameMessage{5, LeftGameReason::GameCrashed}) == bytes({0, 0, 0, 5, 2}));
    CHECK(encode(ErrorMessage{message_type::JoinGame, ErrorCode::GameFull}) == bytes({0x04, 0x00, 0x08}));
    CHECK(encode(ErrorMessage{0, ErrorCode::Kicked}) == bytes({0x00, 0x00, 0x0D}));
    CHECK(encode(LeftGameMessage{5, LeftGameReason::EndedByAdmin}) == bytes({0, 0, 0, 5, 5}));
    CHECK(encode(ServerMessageMessage{"Hi"}) == bytes({2, 'H', 'i'}));
    CHECK(encode(ErrorMessage{message_type::SignIn, ErrorCode::InvalidCredentials}) ==
          bytes({0x0A, 0x00, 0x12}));

    const Bytes joined = encode(GameJoinedMessage{7, 4251, 9, key(1), key(2)});
    REQUIRE(joined.size() == 74);
    CHECK(Bytes(joined.begin(), joined.begin() + 10) == bytes({0, 0, 0, 7, 0x10, 0x9B, 0, 0, 0, 9}));
    CHECK(joined[10] == std::byte{1});
    CHECK(joined[42] == std::byte{2});
}

TEST_CASE("Every lobby message survives a round trip", "[lobby]")
{
    CHECK(decodeHello(encode(guest("Élodie 🚀", "fr_CA"))) == guest("Élodie 🚀", "fr_CA"));
    CHECK(decodeHello(encode(resuming())) == resuming());
    CHECK(decodeCreateGame(encode(CreateGameMessage{"Bydo hunters", 4})) ==
          CreateGameMessage{"Bydo hunters", 4});
    CHECK(decodeJoinGame(encode(JoinGameMessage{0xFFFFFFFF})) == JoinGameMessage{0xFFFFFFFF});
    CHECK(decodeWelcome(encode(welcome(0xFFFFFFFF))) == welcome(0xFFFFFFFF));
    for (const ServerFeatures features :
         {ServerFeatures{false, false, false}, ServerFeatures{true, false, false},
          ServerFeatures{false, true, false}, ServerFeatures{false, false, true},
          ServerFeatures{true, true, true}}) {
        const WelcomeMessage welcome{1, features, pavel()};
        CHECK(decodeWelcome(encode(welcome)) == welcome);
    }
    const WelcomeMessage player{
        2, {}, AccountInfo{1, std::string(MaxUsernameLength, 'u'), AccountRole::Player}};
    CHECK(decodeWelcome(encode(player)) == player);
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
    CHECK(decodeHello(bytes({0, 3, 0, 2, 'e', 'n', 0}))->name.empty());
    CHECK(decodeHello(bytes({0, 3, 2, 0xFF, 0xFE, 2, 'e', 'n', 0}))->name == "\xFF\xFE");
    CHECK(decodeHello(bytes({0,   3,   17,  'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
                             'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 2,   'e', 'n', 0}))
              ->name.size() == 17);
    CHECK(decodeCreateGame(bytes({0, 2}))->name.empty());

    CHECK_THROWS_AS(encode(guest("")), std::invalid_argument);
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

    CHECK_THROWS_AS(encode(welcome(0)), std::invalid_argument);
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
    CHECK(closesConnection(ErrorCode::SessionExpired));
    CHECK_FALSE(closesConnection(ErrorCode::HelloRequired));
    CHECK_FALSE(closesConnection(ErrorCode::AccountsDisabled));
    CHECK_FALSE(closesConnection(ErrorCode::InvalidCredentials));
    CHECK_FALSE(closesConnection(ErrorCode::GameStartFailed));
}

TEST_CASE("A lobby message is refused if it is cut or longer than its fields", "[lobby]")
{
    const std::vector<std::pair<Bytes, std::function<bool(std::span<const std::byte>)>>> messages = {
        {encode(guest("Bob")), [](auto p) { return decodeHello(p).has_value(); }},
        {encode(resuming()), [](auto p) { return decodeHello(p).has_value(); }},
        {encode(CreateGameMessage{"G", 1}), [](auto p) { return decodeCreateGame(p).has_value(); }},
        {encode(JoinGameMessage{1}), [](auto p) { return decodeJoinGame(p).has_value(); }},
        {encode(welcome(1)), [](auto p) { return decodeWelcome(p).has_value(); }},
        {encode(welcome(1, {}, pavel())), [](auto p) { return decodeWelcome(p).has_value(); }},
        {encode(twoGames()), [](auto p) { return decodeGameList(p).has_value(); }},
        {encode(GameJoinedMessage{1, 2, 3, {}, {}}), [](auto p) { return decodeGameJoined(p).has_value(); }},
        {encode(LeftGameMessage{1, {}}), [](auto p) { return decodeLeftGame(p).has_value(); }},
        {encode(ErrorMessage{1, ErrorCode::GameFull}), [](auto p) { return decodeError(p).has_value(); }},
        {encode(ServerMessageMessage{"Hi"}), [](auto p) { return decodeServerMessage(p).has_value(); }},
    };
    auto all = messages;
    for (auto &message : accountMessages()) {
        all.push_back(std::move(message));
    }
    for (const auto &[payload, decodes] : all) {
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
        (void)decodeRegister(payload);
        (void)decodeVerifyEmail(payload);
        (void)decodeResendCode(payload);
        (void)decodeSignIn(payload);
        (void)decodeForgotPassword(payload);
        (void)decodeResetPassword(payload);
        (void)decodeDiscordConfirm(payload);
        (void)decodeChooseUsername(payload);
        (void)decodeDeleteAccount(payload);
        (void)decodeSignedIn(payload);
        (void)decodeDiscordUrl(payload);
    }
    SUCCEED();
}

TEST_CASE("The requests of the accounts are written field by field", "[lobby][accounts]")
{
    CHECK(encode(RegisterMessage{"Ann", "a@b.c", "0123456789"}) ==
          bytes({3,  'A', 'n', 'n', 5,   'a', '@', 'b', '.', 'c', 0,
                 10, '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'}));
    CHECK(encode(VerifyEmailMessage{"Ann", "004217"}) ==
          bytes({3, 'A', 'n', 'n', 6, '0', '0', '4', '2', '1', '7'}));
    CHECK(encode(ResendCodeMessage{"Ann"}) == bytes({3, 'A', 'n', 'n'}));
    CHECK(encode(SignInMessage{"Ann", "pw"}) == bytes({3, 'A', 'n', 'n', 0, 2, 'p', 'w'}));
    CHECK(encode(ForgotPasswordMessage{"a@b.c"}) == bytes({5, 'a', '@', 'b', '.', 'c'}));
    CHECK(encode(ResetPasswordMessage{"a@b.c", "123456", "pw"}) ==
          bytes({5, 'a', '@', 'b', '.', 'c', 6, '1', '2', '3', '4', '5', '6', 0, 2, 'p', 'w'}));
    CHECK(encode(DiscordConfirmMessage{"K7P2ZX"}) == bytes({6, 'K', '7', 'P', '2', 'Z', 'X'}));
    CHECK(encode(ChooseUsernameMessage{"Ann"}) == bytes({3, 'A', 'n', 'n'}));
    CHECK(encode(DeleteAccountMessage{""}) == bytes({0, 0}));
    CHECK(encode(DeleteAccountMessage{"pw"}) == bytes({0, 2, 'p', 'w'}));
}

TEST_CASE("The replies of the accounts are written field by field", "[lobby][accounts]")
{
    const Bytes signedIn = encode(SignedInMessage{key(5), {9, "Ann", AccountRole::Admin}});
    REQUIRE(signedIn.size() == 32 + 8 + 4 + 1);
    CHECK(signedIn[0] == std::byte{5});
    CHECK(signedIn[31] == std::byte{5});
    CHECK(Bytes(signedIn.begin() + 32, signedIn.end()) ==
          bytes({0, 0, 0, 0, 0, 0, 0, 9, 3, 'A', 'n', 'n', 1}));

    CHECK(encode(DiscordUrlMessage{"https://d.c"}) ==
          bytes({0, 11, 'h', 't', 't', 'p', 's', ':', '/', '/', 'd', '.', 'c'}));
}

TEST_CASE("Every message of the accounts survives a round trip", "[lobby][accounts]")
{
    const RegisterMessage registration{"Élodie", "élodie@exemple.fr", "mot de passe très long 🚀"};
    CHECK(decodeRegister(encode(registration)) == registration);
    const VerifyEmailMessage verify{"élodie@exemple.fr", "000000"};
    CHECK(decodeVerifyEmail(encode(verify)) == verify);
    const ResendCodeMessage resend{"Élodie"};
    CHECK(decodeResendCode(encode(resend)) == resend);
    const SignInMessage signIn{"Élodie", "mot de passe très long 🚀"};
    CHECK(decodeSignIn(encode(signIn)) == signIn);
    const ForgotPasswordMessage forgot{"élodie@exemple.fr"};
    CHECK(decodeForgotPassword(encode(forgot)) == forgot);
    const ResetPasswordMessage reset{"élodie@exemple.fr", "999999", "un autre mot de passe"};
    CHECK(decodeResetPassword(encode(reset)) == reset);
    const DiscordConfirmMessage confirm{"23456Z"};
    CHECK(decodeDiscordConfirm(encode(confirm)) == confirm);
    const ChooseUsernameMessage choose{"Elodie-_9"};
    CHECK(decodeChooseUsername(encode(choose)) == choose);
    for (const std::string &password : {std::string(), std::string("pw")}) {
        CHECK(decodeDeleteAccount(encode(DeleteAccountMessage{password})) == DeleteAccountMessage{password});
    }
    for (const AccountRole role : {AccountRole::Player, AccountRole::Admin}) {
        const SignedInMessage signedIn{key(9), {0xFFFFFFFFFFFFFFFF, "Élodie", role}};
        CHECK(decodeSignedIn(encode(signedIn)) == signedIn);
    }
    const DiscordUrlMessage url{"https://discord.com/oauth2/authorize?client_id=1&scope=identify&state=abc"};
    CHECK(decodeDiscordUrl(encode(url)) == url);
    for (const ErrorCode code :
         {ErrorCode::AccountsDisabled, ErrorCode::UsernameTaken, ErrorCode::WeakPassword,
          ErrorCode::InvalidCredentials, ErrorCode::EmailNotVerified, ErrorCode::InvalidCode,
          ErrorCode::CodeExpired, ErrorCode::TooManyAttempts, ErrorCode::SessionExpired, ErrorCode::Busy,
          ErrorCode::GuestsDisabled, ErrorCode::NameReserved, ErrorCode::MailUnavailable,
          ErrorCode::DiscordNotConfirmed, ErrorCode::InvalidEmail, ErrorCode::NotSignedIn,
          ErrorCode::AlreadySignedIn, ErrorCode::DiscordTaken}) {
        CHECK(decodeError(encode(ErrorMessage{message_type::SignIn, code})) ==
              ErrorMessage{message_type::SignIn, code});
    }
}

TEST_CASE("The fields of the accounts hold their longest values, and not one byte more", "[lobby][accounts]")
{
    const std::string username(MaxUsernameLength, 'u');
    const std::string email = std::string(MaxEmailLength - 6, 'e') + "@b.org";
    const std::string password(MaxPasswordLength, 'p');
    // 128 characters of 4 bytes: the longest password the master accepts fits
    std::string emoji;
    for (int i = 0; i < 128; ++i) {
        emoji += "🚀";
    }
    REQUIRE(emoji.size() == MaxPasswordLength);

    const RegisterMessage longest{username, email, password};
    CHECK(decodeRegister(encode(longest)) == longest);
    CHECK(decodeSignIn(encode(SignInMessage{email, emoji})) == SignInMessage{email, emoji});
    CHECK(decodeResetPassword(encode(ResetPasswordMessage{email, "123456", password}))->password == password);
    CHECK(decodeDeleteAccount(encode(DeleteAccountMessage{password}))->password == password);
    const std::string url = "https://" + std::string(MaxDiscordUrlLength - 8, 'u');
    CHECK(decodeDiscordUrl(encode(DiscordUrlMessage{url}))->url == url);
    CHECK(encode(SignedInMessage{key(1), {1, username, AccountRole::Player}}).size() == 32 + 8 + 17 + 1);
    // The longest REGISTER fits in a frame
    CHECK(encode(longest).size() <= rtype::network::MaxFramePayloadSize);

    const std::string tooLongUsername(MaxUsernameLength + 1, 'u');
    const std::string tooLongEmail(MaxEmailLength + 1, 'e');
    const std::string tooLongPassword(MaxPasswordLength + 1, 'p');
    CHECK_THROWS_AS(encode(RegisterMessage{tooLongUsername, email, password}), std::invalid_argument);
    CHECK_THROWS_AS(encode(RegisterMessage{username, tooLongEmail, password}), std::invalid_argument);
    CHECK_THROWS_AS(encode(RegisterMessage{username, email, tooLongPassword}), std::invalid_argument);
    CHECK_THROWS_AS(encode(SignInMessage{tooLongEmail, password}), std::invalid_argument);
    CHECK_THROWS_AS(encode(SignInMessage{email, tooLongPassword}), std::invalid_argument);
    CHECK_THROWS_AS(encode(ChooseUsernameMessage{tooLongUsername}), std::invalid_argument);
    CHECK_THROWS_AS(encode(DeleteAccountMessage{tooLongPassword}), std::invalid_argument);
    CHECK_THROWS_AS(encode(DiscordUrlMessage{url + "u"}), std::invalid_argument);
    CHECK_THROWS_AS(encode(SignedInMessage{key(1), {1, tooLongUsername, AccountRole::Player}}),
                    std::invalid_argument);

    // The same, forged on the wire
    Bytes username17 = bytes({17});
    username17.resize(18, std::byte{'u'});
    CHECK_FALSE(decodeChooseUsername(username17));
    Bytes email255 = bytes({255});
    email255.resize(256, std::byte{'e'});
    CHECK_FALSE(decodeForgotPassword(email255));
    CHECK_FALSE(decodeResendCode(email255));
    Bytes password513 = bytes({0x02, 0x01});
    password513.resize(2 + 513, std::byte{'p'});
    CHECK_FALSE(decodeDeleteAccount(password513));
    Bytes signIn = bytes({1, 'A'});
    signIn.insert(signIn.end(), password513.begin(), password513.end());
    CHECK_FALSE(decodeSignIn(signIn));
    Bytes url2049 = bytes({0x08, 0x01, 'h', 't', 't', 'p', 's', ':', '/', '/'});
    url2049.resize(2 + 2049, std::byte{'u'});
    CHECK_FALSE(decodeDiscordUrl(url2049));
}

TEST_CASE("The messages of the accounts refuse what the protocol forbids", "[lobby][accounts]")
{
    // Empty fields: a username, an email, a login and a password, except the password of DELETE_ACCOUNT
    CHECK_FALSE(decodeRegister(bytes({0, 1, 'e', 0, 1, 'p'})));
    CHECK_FALSE(decodeRegister(bytes({1, 'u', 0, 0, 1, 'p'})));
    CHECK_FALSE(decodeRegister(bytes({1, 'u', 1, 'e', 0, 0})));
    CHECK_FALSE(decodeSignIn(bytes({0, 0, 1, 'p'})));
    CHECK_FALSE(decodeSignIn(bytes({1, 'u', 0, 0})));
    CHECK_FALSE(decodeChooseUsername(bytes({0})));
    CHECK_FALSE(decodeForgotPassword(bytes({0})));
    CHECK(decodeDeleteAccount(bytes({0, 0})));
    // Control characters and invalid UTF-8
    CHECK_FALSE(decodeSignIn(bytes({1, 'u', 0, 2, 'p', '\n'})));
    CHECK_FALSE(decodeSignIn(bytes({1, 'u', 0, 1, 0xFF})));
    CHECK_FALSE(decodeChooseUsername(bytes({1, 0x00})));
    CHECK_THROWS_AS(encode(SignInMessage{"Ann", "tab\there"}), std::invalid_argument);
    CHECK_THROWS_AS(encode(RegisterMessage{"", "a@b.c", "password"}), std::invalid_argument);
    CHECK_THROWS_AS(encode(SignInMessage{"Ann", ""}), std::invalid_argument);

    // A code sent by email is 6 digits
    CHECK_FALSE(decodeVerifyEmail(bytes({1, 'A', 5, '1', '2', '3', '4', '5'})));
    CHECK_FALSE(decodeVerifyEmail(bytes({1, 'A', 7, '1', '2', '3', '4', '5', '6', '7'})));
    CHECK_FALSE(decodeVerifyEmail(bytes({1, 'A', 6, '1', '2', '3', '4', '5', 'A'})));
    CHECK_FALSE(decodeResetPassword(bytes({1, 'e', 6, '1', '2', '3', '4', '5', ' ', 0, 1, 'p'})));
    CHECK_THROWS_AS(encode(VerifyEmailMessage{"Ann", "12345"}), std::invalid_argument);
    CHECK_THROWS_AS(encode(ResetPasswordMessage{"a@b.c", "abcdef", "pw"}), std::invalid_argument);
    // A Discord code is 6 of its alphabet: no 0, O, 1, I, nor small letters
    for (const char *wrong : {"K7P2Z0", "K7P2ZO", "K7P2Z1", "K7P2ZI", "k7p2zx", "K7P2Z", "K7P2ZXY"}) {
        CHECK_FALSE(isValidDiscordCode(wrong));
        CHECK_THROWS_AS(encode(DiscordConfirmMessage{wrong}), std::invalid_argument);
        Bytes payload = bytes({static_cast<int>(std::string_view(wrong).size())});
        for (const char c : std::string_view(wrong)) {
            payload.push_back(static_cast<std::byte>(c));
        }
        CHECK_FALSE(decodeDiscordConfirm(payload));
    }
    CHECK(isValidDiscordCode("23456Z"));
    CHECK(isValidDiscordCode("ABCHJK"));

    // An account has an id, and a role of 0 or 1
    Bytes signedIn = encode(SignedInMessage{key(1), {1, "Ann", AccountRole::Player}});
    Bytes noId = signedIn;
    noId[39] = std::byte{0};
    CHECK_FALSE(decodeSignedIn(noId));
    Bytes role2 = signedIn;
    role2.back() = std::byte{2};
    CHECK_FALSE(decodeSignedIn(role2));
    CHECK_THROWS_AS(encode(SignedInMessage{key(1), {0, "Ann", AccountRole::Player}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(SignedInMessage{key(1), {1, "", AccountRole::Player}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(SignedInMessage{key(1), {1, "Ann", AccountRole{2}}}), std::invalid_argument);

    // WELCOME: no unknown feature, an account flag of 0 or 1
    CHECK_FALSE(decodeWelcome(bytes({0, 0, 0, 1, 0x08, 0})));
    CHECK_FALSE(decodeWelcome(bytes({0, 0, 0, 1, 0x01, 2})));
    CHECK_FALSE(decodeWelcome(bytes({0, 0, 0, 1, 0x01, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 'A', 0})));

    // DISCORD_URL: only https, so that the client never opens another scheme
    for (const char *wrong :
         {"http://discord.com", "javascript:alert(1)", "file:///etc/passwd", "HTTPS://x"}) {
        CHECK_THROWS_AS(encode(DiscordUrlMessage{wrong}), std::invalid_argument);
        Writer writer;
        writer.writeString16(wrong);
        CHECK_FALSE(decodeDiscordUrl(writer.take()));
    }
    CHECK_FALSE(decodeDiscordUrl(bytes({0, 0})));
}

TEST_CASE("A HELLO refuses a locale that is not one, and a token flag that is not a bool", "[lobby]")
{
    CHECK(isValidLocale("en"));
    CHECK(isValidLocale("fr-FR"));
    CHECK(isValidLocale("fr_CA"));
    CHECK(isValidLocale(std::string(MaxLocaleLength, 'a')));
    CHECK_FALSE(isValidLocale("e"));
    CHECK_FALSE(isValidLocale(std::string(MaxLocaleLength + 1, 'a')));
    CHECK_FALSE(isValidLocale("fr FR"));
    CHECK_FALSE(isValidLocale("fr.UTF-8"));
    CHECK_FALSE(isValidLocale("é1"));

    CHECK_FALSE(decodeHello(bytes({0, 3, 1, 'A', 1, 'e', 0})));           // locale of 1 byte
    CHECK_FALSE(decodeHello(bytes({0, 3, 1, 'A', 3, 'e', ' ', 'n', 0}))); // a space
    CHECK_FALSE(decodeHello(bytes({0, 3, 1, 'A', 2, 'e', 'n', 2})));      // token flag 2
    CHECK_FALSE(decodeHello(bytes({0, 3, 1, 'A', 2, 'e', 'n'})));         // no token flag
    CHECK_FALSE(decodeHello(bytes({0, 3, 0, 2, 'e', 'n', 1, 1, 2, 3})));  // a token cut
    CHECK_THROWS_AS(encode(guest("Ann", "e")), std::invalid_argument);
    CHECK_THROWS_AS(encode(guest("Ann", "fr FR")), std::invalid_argument);
}

TEST_CASE("The requests of the accounts are told apart from the others", "[lobby][accounts]")
{
    for (std::uint8_t type = message_type::Register; type <= message_type::DeleteAccount; ++type) {
        CHECK(isAccountRequest(type));
    }
    for (const std::uint8_t type : {message_type::Hello, message_type::ListGames, message_type::CreateGame,
                                    message_type::JoinGame, message_type::LeaveGame, message_type::Ping,
                                    std::uint8_t{0x12}, std::uint8_t{0x7F}, message_type::SignedIn}) {
        CHECK_FALSE(isAccountRequest(type));
    }
}
