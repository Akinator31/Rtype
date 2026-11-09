// Tests of the messages of the game protocol (libs/game, Messages.hpp and RemoteWorld.hpp): the bytes of the
// specification, round trips, every malformed case, and a whole game sent through them
// (docs/protocol/game.md, sections 8 and 9).

#include "rtype/game/Game.hpp"
#include "rtype/network/DatagramBody.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

using namespace rtype::game;
using rtype::network::MaxReliablePayloadSize;
using rtype::network::MaxUnreliablePayloadSize;

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

std::vector<std::byte> withExtraByte(std::vector<std::byte> payload)
{
    payload.push_back(std::byte{0});
    return payload;
}

/// The snapshot of the example of the specification (section 10).
SnapshotPart examplePart()
{
    SnapshotPart part;
    part.tick = 1000;
    part.lastInput = 998;
    part.inputLead = 2;
    part.weapon = {4, 7, 0};
    part.players = {{.slot = 0,
                     .lives = 3,
                     .score = 1500,
                     .kills = 12,
                     .wavesCleared = 2,
                     .streak = 2,
                     .bestStreak = 2,
                     .xp = 350}};
    part.entities = {
        {7, EntityKind::PlayerShip, 0, 200.0f, 540.5f, 0.0f, -300.0f, 3, EntityState::Invulnerable}};
    return part;
}

/// Its payload, as the specification writes it.
const std::vector<std::byte> ExampleSnapshot = bytes({
    0x00, 0x00, 0x03, 0xE8,             // tick 1000
    0x00, 0x00, 0x03, 0xE6,             // last input 998
    0x02,                               // input lead 2
    0x00, 0x04, 0x00, 0x07, 0x00, 0x00, // weapon: cooldown 4, ammo 7, not reloading
    0x00, 0x01,                         // part 0 of 1
    0x01, 0x00, 0x03, 0x00, 0x00, 0x05, // 1 player: slot 0, 3 lives,
    0xDC,                               //   score 1500
    0x00, 0x00, 0x00, 0x0C,             //   12 Bydos shot down
    0x00, 0x02, 0x00, 0x02, 0x00, 0x02, //   2 waves cleared, streak 2, best streak 2
    0x00, 0x00, 0x01, 0x5E,             //   350 experience
    0x01,                               // 1 entity
    0x00, 0x00, 0x00, 0x07,             //   id 7
    0x01, 0x00,                         //   PLAYER_SHIP, slot 0
    0x06, 0x40, 0x10, 0xE4,             //   x 200, y 540.5
    0x00, 0x00, 0xF6, 0xA0,             //   velocity 0, -300
    0x03, 0x01,                         //   health 3, invulnerable
});

EntityState entity(std::uint32_t id, float x = 100.0f, float y = 100.0f)
{
    return {id, EntityKind::BydoFlyer, NoOwner, x, y, -120.0f, 0.0f, 2, 0};
}

/// Every prefix of a valid payload, and the payload with one more byte, are refused.
template <typename Decode>
void checkSizeIsExact(const std::vector<std::byte> &payload, Decode decode)
{
    for (std::size_t size = 0; size < payload.size(); ++size) {
        INFO("size " << size);
        CHECK_FALSE(decode(std::span(payload).first(size)));
    }
    CHECK_FALSE(decode(withExtraByte(payload)));
    CHECK(decode(payload));
}

} // namespace

// --- The bytes of the specification ---

TEST_CASE("A snapshot part is the one of the example of the specification", "[game][messages]")
{
    CHECK(encode(examplePart()) == ExampleSnapshot);
    CHECK(decodeSnapshotPart(ExampleSnapshot) == examplePart());
}

TEST_CASE("An input is the one of the example of the specification", "[game][messages]")
{
    const InputMessage input{1000,
                             {PlayerInput{PlayerInput::Up | PlayerInput::Fire},
                              PlayerInput{PlayerInput::Up | PlayerInput::Fire},
                              PlayerInput{PlayerInput::Up}}};
    const std::vector<std::byte> payload = bytes({0x00, 0x00, 0x03, 0xE8, 0x03, 0x11, 0x11, 0x01});

    CHECK(encode(input) == payload);
    CHECK(decodeInput(payload) == input);
}

TEST_CASE("The small messages are written field by field", "[game][messages]")
{
    // The voice carried, then not (version 4); its other bits reserved
    CHECK(decodeAccepted(bytes({0x02, 0x3C, 0x00, 0x00, 0x03, 0xE8, 0xFE}))->voice == false);
    CHECK(decodeAccepted(bytes({0x02, 0x3C, 0x00, 0x00, 0x03, 0xE8, 0x01}))->voice);
    CHECK_FALSE(decodeAccepted(bytes({0x02, 0x3C, 0x00, 0x00, 0x03, 0xE8})));
    CHECK(encode(AcceptedMessage{2, 60, 1000}) == bytes({0x02, 0x3C, 0x00, 0x00, 0x03, 0xE8, 0x01}));
    CHECK(encode(AcceptedMessage{2, 60, 1000, false}) == bytes({0x02, 0x3C, 0x00, 0x00, 0x03, 0xE8, 0x00}));
    // A guest, account 0; then the account 7 (version 3)
    CHECK(encode(PlayerJoinedMessage{1, 42, "Pavel"}) ==
          bytes({0x01, 0x00, 0x00, 0x00, 0x2A, 0x05, 'P', 'a', 'v', 'e', 'l', 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00}));
    CHECK(encode(PlayerJoinedMessage{1, 42, "Pavel", 7}) ==
          bytes({0x01, 0x00, 0x00, 0x00, 0x2A, 0x05, 'P', 'a', 'v', 'e', 'l', 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x07}));
    CHECK(encode(PlayerLeftMessage{3, LeaveReason::TimedOut}) == bytes({0x03, 0x01}));
    CHECK(encode(EntitiesDestroyedMessage{1000, {{7, DestroyCause::Killed}, {9, DestroyCause::Removed}}}) ==
          bytes({0x00, 0x00, 0x03, 0xE8, 0x02, 0x00, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x09, 0x00}));
    CHECK(encode(WaveStartedMessage{3, ""}) == bytes({0x00, 0x03, 0x00}));
    CHECK(encode(WaveStartedMessage{3, "a.mp3"}) == bytes({0x00, 0x03, 0x05, 'a', '.', 'm', 'p', '3'}));
    CHECK(encode(GameOverMessage{GameOutcome::Victory}) == bytes({0x01}));
}

// --- The voice ---

TEST_CASE("The messages of the voice are written field by field", "[game][messages][voice]")
{
    CHECK(encode(VoiceMessage{0x1234, true, bytes({0xAA, 0xBB, 0xCC})}) ==
          bytes({0x12, 0x34, 0x01, 0x03, 0xAA, 0xBB, 0xCC}));
    CHECK(encode(VoiceFrameMessage{2, 7, false, bytes({0xF8})}) ==
          bytes({0x02, 0x00, 0x07, 0x00, 0x01, 0xF8}));
    CHECK(encode(VoiceStateMessage{rtype::voice::Backend::Discord}) == bytes({0x02}));
    CHECK(encode(VoiceRosterMessage{{rtype::voice::Backend::Game, rtype::voice::Backend::None,
                                     rtype::voice::Backend::Discord, rtype::voice::Backend::Game}}) ==
          bytes({0x01, 0x00, 0x02, 0x01}));
}

TEST_CASE("The messages of the voice survive a round trip, with frames of 1 to 160 bytes",
          "[game][messages][voice]")
{
    for (const std::size_t size : {std::size_t{1}, std::size_t{60}, rtype::voice::MaxFrameBytes}) {
        std::vector<std::byte> frame(size);
        for (std::size_t i = 0; i < size; ++i) {
            frame[i] = static_cast<std::byte>(i * 13);
        }
        const VoiceMessage voice{65535, size == 1, frame};
        CHECK(decodeVoice(encode(voice)) == voice);
        const VoiceFrameMessage forwarded{3, 0, size != 1, frame};
        CHECK(decodeVoiceFrame(encode(forwarded)) == forwarded);
    }
    for (const auto backend :
         {rtype::voice::Backend::None, rtype::voice::Backend::Game, rtype::voice::Backend::Discord}) {
        CHECK(decodeVoiceState(encode(VoiceStateMessage{backend})) == VoiceStateMessage{backend});
    }
    const VoiceRosterMessage roster{{rtype::voice::Backend::Discord, rtype::voice::Backend::Game,
                                     rtype::voice::Backend::None, rtype::voice::Backend::Discord}};
    CHECK(decodeVoiceRoster(encode(roster)) == roster);
}

TEST_CASE("What each player tells about its voice is one byte of flags", "[game][messages][voice]")
{
    using rtype::voice::Status;
    CHECK(encode(VoiceStatusMessage{}) == bytes({0x00}));
    CHECK(encode(VoiceStatusMessage{{.muted = true}}) == bytes({0x01}));
    CHECK(encode(VoiceStatusMessage{{.deafened = true}}) == bytes({0x02}));
    CHECK(encode(VoiceStatusMessage{{.noMicrophone = true}}) == bytes({0x04}));
    CHECK(encode(VoiceStatusesMessage{{Status{.muted = true, .deafened = true}, Status{}, Status{},
                                       Status{.noMicrophone = true}}}) == bytes({0x03, 0x00, 0x00, 0x04}));

    const VoiceStatusMessage all{{.muted = true, .deafened = true, .noMicrophone = true}};
    CHECK(decodeVoiceStatus(encode(all)) == all);
    // The reserved bits are dropped
    CHECK(decodeVoiceStatus(bytes({0xF9})) == VoiceStatusMessage{{.muted = true}});
    CHECK_FALSE(decodeVoiceStatus(bytes({})));
    CHECK_FALSE(decodeVoiceStatus(bytes({0x01, 0x00})));
    const VoiceStatusesMessage statuses{
        {Status{}, Status{.muted = true}, Status{.deafened = true}, Status{}}};
    CHECK(decodeVoiceStatuses(encode(statuses)) == statuses);
    CHECK_FALSE(decodeVoiceStatuses(bytes({0x00, 0x00, 0x00})));
    CHECK_FALSE(decodeVoiceStatuses(bytes({0x00, 0x00, 0x00, 0x00, 0x00})));

    // Muted by an administrator (version 4): bit 3, the game's to say in VOICE_STATUSES only
    CHECK(encode(VoiceStatusesMessage{{Status{.mutedByAdmin = true}, Status{}, Status{}, Status{}}}) ==
          bytes({0x08, 0x00, 0x00, 0x00}));
    const VoiceStatusesMessage muted{
        {Status{.muted = true, .mutedByAdmin = true}, Status{}, Status{}, Status{}}};
    CHECK(decodeVoiceStatuses(encode(muted)) == muted);
    CHECK(decodeVoiceStatuses(bytes({0xF9, 0x00, 0x00, 0x00}))->statuses[0] ==
          Status{.muted = true, .mutedByAdmin = true});
    // A player cannot tell it: neither written nor read in VOICE_STATUS
    CHECK(encode(VoiceStatusMessage{{.mutedByAdmin = true}}) == bytes({0x00}));
    CHECK(decodeVoiceStatus(bytes({0x08})) == VoiceStatusMessage{});
}

TEST_CASE("PLAYER_JOINED carries the account of the player", "[game][messages]")
{
    const PlayerJoinedMessage signedIn{2, 9, "Nova", 0xFFFFFFFFFFFFFFFF};
    CHECK(decodePlayerJoined(encode(signedIn)) == signedIn);
    // Version 2, without the account: too short
    CHECK_FALSE(decodePlayerJoined(bytes({0x01, 0x00, 0x00, 0x00, 0x2A, 0x05, 'P', 'a', 'v', 'e', 'l'})));
}

TEST_CASE("The voice refuses an empty frame, one over 160 bytes, a slot above 3, or bytes left over",
          "[game][messages][voice]")
{
    CHECK_FALSE(decodeVoice(bytes({0, 1, 0, 0})));                  // no frame
    CHECK_FALSE(decodeVoice(bytes({0, 1, 0, 2, 0xAA})));            // fewer bytes than said
    CHECK_FALSE(decodeVoiceFrame(bytes({0x04, 0, 1, 0, 1, 0xF8}))); // slot 4
    std::vector<std::byte> tooBig = bytes({0, 1, 0, 161});
    tooBig.resize(tooBig.size() + 161, std::byte{0xF8});
    CHECK_FALSE(decodeVoice(tooBig));

    checkSizeIsExact(encode(VoiceMessage{1, false, bytes({1, 2})}), decodeVoice);
    checkSizeIsExact(encode(VoiceFrameMessage{1, 1, false, bytes({1, 2})}), decodeVoiceFrame);
    checkSizeIsExact(encode(VoiceStateMessage{}), decodeVoiceState);
    checkSizeIsExact(encode(VoiceRosterMessage{}), decodeVoiceRoster);

    CHECK_THROWS_AS(encode(VoiceMessage{1, false, {}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(VoiceMessage{1, false, std::vector<std::byte>(161)}), std::invalid_argument);
    CHECK_THROWS_AS(encode(VoiceFrameMessage{4, 1, false, bytes({1})}), std::invalid_argument);
}

TEST_CASE("An unknown backend is read as no voice, and the reserved bits of the voice are dropped",
          "[game][messages][voice]")
{
    CHECK(decodeVoiceState(bytes({0x07}))->backend == rtype::voice::Backend::None);
    CHECK(decodeVoiceRoster(bytes({0x01, 0xFF, 0x02, 0x03}))->backends ==
          std::array{rtype::voice::Backend::Game, rtype::voice::Backend::None, rtype::voice::Backend::Discord,
                     rtype::voice::Backend::None});
    const auto voice = decodeVoice(bytes({0, 1, 0xFE, 1, 0xF8}));
    REQUIRE(voice);
    CHECK_FALSE(voice->last);
    CHECK(decodeVoice(bytes({0, 1, 0xFF, 1, 0xF8}))->last);
}

// --- Positions and velocities ---

TEST_CASE("Positions and velocities are rounded to eighths, and kept within an i16", "[game][messages]")
{
    const auto written = [](float x) {
        SnapshotPart part;
        part.entities = {entity(1, x)};
        return decodeSnapshotPart(encode(part))->entities[0].x;
    };

    CHECK(written(200.0f) == 200.0f);
    CHECK(written(-0.125f) == -0.125f);
    // 0.06 is 0.48 eighths, 0.07 is 0.56
    CHECK(written(0.06f) == 0.0f);
    CHECK(written(0.07f) == 0.125f);
    CHECK(written(4095.875f) == 4095.875f);
    CHECK(written(5000.0f) == 4095.875f);
    CHECK(written(-5000.0f) == -4096.0f);
    CHECK(written(std::numeric_limits<float>::infinity()) == 4095.875f);
    CHECK(written(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
}

// --- Round trips ---

TEST_CASE("Every message survives a round trip", "[game][messages]")
{
    const AcceptedMessage accepted{3, 60, 0xFFFFFFFF};
    CHECK(decodeAccepted(encode(accepted)) == accepted);

    const PlayerJoinedMessage joined{0, 0xDEADBEEF, "Élodie 🚀"};
    CHECK(decodePlayerJoined(encode(joined)) == joined);

    for (const LeaveReason reason : {LeaveReason::Quit, LeaveReason::TimedOut, LeaveReason::LobbyClosed}) {
        CHECK(decodePlayerLeft(encode(PlayerLeftMessage{2, reason})) == PlayerLeftMessage{2, reason});
    }

    InputMessage input{31, {}};
    for (std::uint8_t i = 0; i < MaxInputsPerMessage; ++i) {
        input.inputs.push_back(PlayerInput{static_cast<std::uint8_t>(i & 0x1F)});
    }
    CHECK(decodeInput(encode(input)) == input);

    SnapshotPart part;
    part.tick = 77;
    part.lastInput = 75;
    part.inputLead = -127;
    part.weapon = {0xFFFF, 0, 1};
    part.part = 15;
    part.parts = 16;
    part.players = {{3, 0, 0xFFFFFFFF}, {0, 255, 0}};
    part.entities = {
        {0xFFFFFFFF, EntityKind::BydoShot, NoOwner, -4096.0f, 4095.875f, 12.5f, -0.125f, 0, 0x07}};
    CHECK(decodeSnapshotPart(encode(part)) == part);

    const EntitiesDestroyedMessage destroyed{5, {{1, DestroyCause::Removed}, {2, DestroyCause::Killed}}};
    CHECK(decodeEntitiesDestroyed(encode(destroyed)) == destroyed);
    CHECK(decodeWaveStarted(encode(WaveStartedMessage{65535, ""})) == WaveStartedMessage{65535, ""});
    CHECK(decodeWaveStarted(encode(WaveStartedMessage{2, "sounds/boss.mp3"})) ==
          WaveStartedMessage{2, "sounds/boss.mp3"});
    CHECK(decodeGameOver(encode(GameOverMessage{GameOutcome::Defeat})) ==
          GameOverMessage{GameOutcome::Defeat});
}

TEST_CASE("Unknown reasons, causes and outcomes are read as the specification says", "[game][messages]")
{
    CHECK(decodePlayerLeft(bytes({0x01, 0x07}))->reason == LeaveReason::Quit);
    CHECK(decodeEntitiesDestroyed(bytes({0, 0, 0, 1, 1, 0, 0, 0, 9, 0x42})) ==
          EntitiesDestroyedMessage{1, {{9, DestroyCause::Removed}}});
    CHECK(decodeGameOver(bytes({0x09}))->outcome == GameOutcome::Defeat);
}

TEST_CASE("The reserved bits of inputs are dropped; every bit of the flags is defined", "[game][messages]")
{
    CHECK(decodeInput(bytes({0, 0, 0, 0, 1, 0xFF}))->inputs[0].buttons == 0x7F);
    CHECK(encode(InputMessage{0, {PlayerInput{0xFF}}}).back() == std::byte{0x7F});

    // The stances of the Bydos take the last three bits
    std::vector<std::byte> payload = ExampleSnapshot;
    payload.back() = std::byte{0xFF};
    CHECK(decodeSnapshotPart(payload)->entities[0].flags == 0xFF);

    SnapshotPart part = examplePart();
    part.entities[0].flags = EntityState::Guarding | EntityState::Slashing | EntityState::Shooting;
    CHECK(encode(part).back() == std::byte{0xE0});
}

TEST_CASE("A kind this version does not know is decoded, and recognized as unknown", "[game][messages]")
{
    std::vector<std::byte> payload = ExampleSnapshot;
    payload[43] = std::byte{0x60}; // the kind of the entity: reserved for later kinds
    const auto part = decodeSnapshotPart(payload);

    REQUIRE(part);
    CHECK(part->entities[0].kind == EntityKind{0x60});
    CHECK_FALSE(isKnown(part->entities[0].kind));
    CHECK(isKnown(EntityKind::BydoBoss));
}

// --- Malformed messages ---

TEST_CASE("A message is refused if it is cut or longer than its fields", "[game][messages]")
{
    checkSizeIsExact(encode(AcceptedMessage{1, 60, 5}), decodeAccepted);
    checkSizeIsExact(encode(PlayerJoinedMessage{1, 5, "Bob"}), decodePlayerJoined);
    checkSizeIsExact(encode(PlayerLeftMessage{1, LeaveReason::Quit}), decodePlayerLeft);
    checkSizeIsExact(encode(InputMessage{10, {PlayerInput{}, PlayerInput{}}}), decodeInput);
    checkSizeIsExact(ExampleSnapshot, decodeSnapshotPart);
    checkSizeIsExact(
        encode(EntitiesDestroyedMessage{3, {{1, DestroyCause::Killed}, {2, DestroyCause::Killed}}}),
        decodeEntitiesDestroyed);
    checkSizeIsExact(encode(WaveStartedMessage{2, "a.mp3"}), decodeWaveStarted);
    checkSizeIsExact(encode(GameOverMessage{}), decodeGameOver);
}

TEST_CASE("The small messages refuse the values the specification forbids", "[game][messages]")
{
    CHECK_FALSE(decodeAccepted(bytes({0x04, 60, 0, 0, 0, 0})));                  // slot 4
    CHECK_FALSE(decodeAccepted(bytes({0x00, 0, 0, 0, 0, 0})));                   // tick rate 0
    CHECK_FALSE(decodePlayerJoined(bytes({0x04, 0, 0, 0, 1, 1, 'A'})));          // slot 4
    CHECK_FALSE(decodePlayerJoined(bytes({0x00, 0, 0, 0, 1, 0})));               // empty name
    CHECK_FALSE(decodePlayerJoined(bytes({0x00, 0, 0, 0, 1, 1, 0x07})));         // control character
    CHECK_FALSE(decodePlayerLeft(bytes({0x04, 0})));                             // slot 4
    CHECK_FALSE(decodeWaveStarted(bytes({0, 0, 0})));                            // wave 0
    CHECK_FALSE(decodeEntitiesDestroyed(bytes({0, 0, 0, 1, 0})));                // no entity
    CHECK_FALSE(decodeEntitiesDestroyed(bytes({0, 0, 0, 1, 1, 0, 0, 0, 0, 1}))); // id 0
    CHECK_FALSE(decodeEntitiesDestroyed(bytes({0, 0, 0, 1, 2, 0, 0, 0, 9, 1}))); // fewer entities than said

    std::vector<std::byte> longName = bytes({0x00, 0, 0, 0, 1, 17});
    longName.resize(longName.size() + 17, std::byte{'a'});
    CHECK_FALSE(decodePlayerJoined(longName));
}

TEST_CASE("An input refuses no input, more than 32, or inputs before tick 0", "[game][messages]")
{
    CHECK_FALSE(decodeInput(bytes({0, 0, 0, 9, 0})));
    std::vector<std::byte> tooMany = bytes({0, 0, 1, 0, 33});
    tooMany.resize(tooMany.size() + 33);
    CHECK_FALSE(decodeInput(tooMany));
    // Tick 1 has inputs for ticks 1 and 0, not -1
    CHECK(decodeInput(bytes({0, 0, 0, 1, 2, 0, 0})));
    CHECK_FALSE(decodeInput(bytes({0, 0, 0, 1, 3, 0, 0, 0})));
}

TEST_CASE("A snapshot part refuses the values the specification forbids", "[game][messages]")
{
    const auto changed = [](std::size_t offset, int value) {
        std::vector<std::byte> payload = ExampleSnapshot;
        payload[offset] = static_cast<std::byte>(value);
        return decodeSnapshotPart(payload);
    };

    CHECK(changed(8, 0x80));      // input lead: any value
    CHECK(changed(9, 0xFF));      // weapon: any value
    CHECK_FALSE(changed(15, 1));  // part 1 of 1
    CHECK_FALSE(changed(16, 0));  // 0 parts
    CHECK_FALSE(changed(16, 17)); // 17 parts
    CHECK(changed(16, 16));       // part 0 of 16
    CHECK_FALSE(changed(18, 4));  // slot 4
    CHECK_FALSE(changed(42, 0));  // entity id 0
    CHECK_FALSE(changed(43, 0));  // kind 0
    CHECK_FALSE(changed(38, 2));  // 2 entities announced, 1 given
    CHECK_FALSE(changed(38, 0));  // 0 entities announced, 1 given
    CHECK_FALSE(changed(17, 5));  // 5 players

    // The same slot twice
    SnapshotPart part = examplePart();
    part.players = {{1, 3, 0}, {2, 3, 0}};
    std::vector<std::byte> payload = encode(part);
    payload[38] = std::byte{1}; // the slot of the second player
    CHECK_FALSE(decodeSnapshotPart(payload));
}

TEST_CASE("The encoders refuse the values the protocol forbids", "[game][messages]")
{
    CHECK_THROWS_AS(encode(AcceptedMessage{4, 60, 0}), std::invalid_argument);
    CHECK_THROWS_AS(encode(AcceptedMessage{0, 0, 0}), std::invalid_argument);
    CHECK_THROWS_AS(encode(PlayerJoinedMessage{0, 1, ""}), std::invalid_argument);
    CHECK_THROWS_AS(encode(PlayerJoinedMessage{0, 1, "seventeen bytes!!"}), std::invalid_argument);
    CHECK_THROWS_AS(encode(PlayerLeftMessage{4, LeaveReason::Quit}), std::invalid_argument);
    CHECK_THROWS_AS(encode(InputMessage{100, {}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(InputMessage{0, {PlayerInput{}, PlayerInput{}}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(InputMessage{100, std::vector<PlayerInput>(33)}), std::invalid_argument);
    CHECK_THROWS_AS(encode(EntitiesDestroyedMessage{1, {}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(EntitiesDestroyedMessage{1, {{0, DestroyCause::Killed}}}), std::invalid_argument);
    CHECK_THROWS_AS(encode(WaveStartedMessage{0, ""}), std::invalid_argument);

    const auto invalid = [](auto change) {
        SnapshotPart part = examplePart();
        change(part);
        return part;
    };
    CHECK_THROWS_AS(encode(invalid([](SnapshotPart &p) { p.parts = 0; })), std::invalid_argument);
    CHECK_THROWS_AS(encode(invalid([](SnapshotPart &p) { p.part = 1; })), std::invalid_argument);
    CHECK_THROWS_AS(encode(invalid([](SnapshotPart &p) { p.players.push_back({0, 1, 0}); })),
                    std::invalid_argument);
    CHECK_THROWS_AS(encode(invalid([](SnapshotPart &p) { p.players.assign(5, {}); })), std::invalid_argument);
    CHECK_THROWS_AS(encode(invalid([](SnapshotPart &p) { p.entities[0].id = 0; })), std::invalid_argument);
    CHECK_THROWS_AS(encode(invalid([](SnapshotPart &p) { p.entities[0].kind = EntityKind{0}; })),
                    std::invalid_argument);
    CHECK_THROWS_AS(encode(invalid([](SnapshotPart &p) { p.entities[0].owner = 4; })), std::invalid_argument);
    CHECK_THROWS_AS(
        encode(invalid([](SnapshotPart &p) { p.entities.assign(maxEntitiesPerPart(1) + 1, entity(1)); })),
        std::length_error);
    CHECK_THROWS_AS(encode(EntitiesDestroyedMessage{1, std::vector<DestroyedEntity>(229, {1})}),
                    std::length_error);
}

TEST_CASE("Random bytes never crash a decoder", "[game][messages]")
{
    std::mt19937 random(2026);
    std::uniform_int_distribution<int> size(0, 300);
    std::uniform_int_distribution<int> byte(0, 255);
    for (int i = 0; i < 20000; ++i) {
        std::vector<std::byte> payload(static_cast<std::size_t>(size(random)));
        for (std::byte &value : payload) {
            value = static_cast<std::byte>(byte(random));
        }
        (void)decodeAccepted(payload);
        (void)decodePlayerJoined(payload);
        (void)decodePlayerLeft(payload);
        (void)decodeInput(payload);
        (void)decodeSnapshotPart(payload);
        (void)decodeEntitiesDestroyed(payload);
        (void)decodeWaveStarted(payload);
        (void)decodeGameOver(payload);
        (void)decodeVoice(payload);
        (void)decodeVoiceFrame(payload);
        (void)decodeVoiceState(payload);
        (void)decodeVoiceRoster(payload);
    }
    SUCCEED();
}

// --- Splitting ---

TEST_CASE("A part of a snapshot holds 69 entities with four players, and always fits in a datagram",
          "[game][messages]")
{
    CHECK(maxEntitiesPerPart(4) == 65);
    CHECK(maxEntitiesPerPart(0) == 70);
    CHECK_THROWS_AS(maxEntitiesPerPart(5), std::invalid_argument);

    for (std::size_t players = 0; players <= MaxPlayers; ++players) {
        SnapshotPart part;
        part.part = 15;
        part.parts = 16;
        for (std::uint8_t slot = 0; slot < players; ++slot) {
            part.players.push_back({slot, 3, 0});
        }
        part.entities.assign(maxEntitiesPerPart(players), entity(1));
        CHECK(encode(part).size() <= MaxUnreliablePayloadSize);
    }
}

TEST_CASE("A snapshot is split in parts that give back all its entities", "[game][messages]")
{
    Snapshot snapshot;
    snapshot.tick = 500;
    snapshot.players = {{0, 3, 10}, {1, 2, 20}, {2, 1, 30}, {3, 0, 40}};
    for (std::uint32_t id = 1; id <= 190; ++id) {
        // Positions in eighths of a unit: exact once written
        snapshot.entities.push_back(entity(id, static_cast<float>(id) * 0.125f, static_cast<float>(id)));
    }

    const std::vector<SnapshotPart> parts = splitSnapshot(snapshot, 498, 3);

    REQUIRE(parts.size() == 3);
    std::vector<EntityState> entities;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const std::vector<std::byte> payload = encode(parts[i]);
        CHECK(payload.size() <= MaxUnreliablePayloadSize);
        const auto part = decodeSnapshotPart(payload);
        REQUIRE(part);
        CHECK(part->tick == 500);
        CHECK(part->lastInput == 498);
        CHECK(part->inputLead == 3);
        CHECK(part->part == i);
        CHECK(part->parts == 3);
        CHECK(part->players == snapshot.players);
        entities.insert(entities.end(), part->entities.begin(), part->entities.end());
    }
    CHECK(parts[0].entities.size() == 65);
    CHECK(parts[2].entities.size() == 60);
    CHECK(entities == snapshot.entities);
}

TEST_CASE("A snapshot without entities is one part, one with too many is cut at 16 parts", "[game][messages]")
{
    Snapshot empty;
    empty.tick = 3;
    const std::vector<SnapshotPart> one = splitSnapshot(empty, 0);
    REQUIRE(one.size() == 1);
    CHECK(one[0].parts == 1);
    CHECK(one[0].entities.empty());

    Snapshot crowded;
    crowded.players = {{0, 3, 0}, {1, 3, 0}, {2, 3, 0}, {3, 3, 0}};
    for (std::uint32_t id = 1; id <= 2000; ++id) {
        crowded.entities.push_back(entity(id));
    }
    const std::vector<SnapshotPart> parts = splitSnapshot(crowded, 0);
    REQUIRE(parts.size() == MaxSnapshotParts);
    CHECK(parts.back().entities.back().id == 16 * 65);
}

TEST_CASE("The destroyed entities of a tick go in as many messages as needed", "[game][messages]")
{
    Snapshot snapshot;
    snapshot.tick = 42;
    CHECK(destroyedMessages(snapshot).empty());

    for (std::uint32_t id = 1; id <= 500; ++id) {
        snapshot.destroyed.push_back({id, DestroyCause::Killed});
    }
    const std::vector<EntitiesDestroyedMessage> messages = destroyedMessages(snapshot);

    REQUIRE(messages.size() == 3);
    std::vector<DestroyedEntity> destroyed;
    for (const EntitiesDestroyedMessage &message : messages) {
        CHECK(message.tick == 42);
        CHECK(encode(message).size() <= MaxReliablePayloadSize);
        destroyed.insert(destroyed.end(), message.entities.begin(), message.entities.end());
    }
    CHECK(messages[0].entities.size() == MaxDestroyedPerMessage);
    CHECK(destroyed == snapshot.destroyed);
}

// --- The world of a client ---

TEST_CASE("The world of a client follows the parts, and an entity missing from one stays", "[game][world]")
{
    RemoteWorld world;
    CHECK_FALSE(world.hasSnapshot());

    SnapshotPart first{10, 8, 0, 1, {{0, 3, 100}}, {entity(1, 50.0f), entity(2)}, NoInputLead, {}};
    CHECK(world.apply(first));
    CHECK(world.hasSnapshot());
    CHECK(world.tick() == 10);
    CHECK(world.lastInput() == 8);
    CHECK(world.players() == first.players);
    CHECK(world.entities().size() == 2);

    // Entity 2 is in a part that was lost
    SnapshotPart second{12, 10, 0, 1, {{0, 2, 150}}, {entity(1, 60.0f)}, NoInputLead, {}};
    CHECK(world.apply(second));
    REQUIRE(world.find(1));
    CHECK(world.find(1)->x == 60.0f);
    CHECK(world.find(2));
    CHECK(world.players() == second.players);
}

TEST_CASE("The world of a client ignores late parts, but applies every part of the latest tick",
          "[game][world]")
{
    RemoteWorld world;
    CHECK(world.apply({20, 0, 0, 2, {}, {entity(1, 10.0f)}, NoInputLead, {}}));
    CHECK(world.apply({20, 0, 1, 2, {}, {entity(2)}, NoInputLead, {}}));
    CHECK(world.entities().size() == 2);

    // Tick 18 arrives after tick 20
    CHECK_FALSE(world.apply({18, 0, 0, 1, {}, {entity(1, 99.0f), entity(3)}, NoInputLead, {}}));
    CHECK(world.find(1)->x == 10.0f);
    CHECK_FALSE(world.find(3));
    CHECK(world.tick() == 20);
}

TEST_CASE("The world of a client removes the destroyed entities, and gives their last state", "[game][world]")
{
    RemoteWorld world;
    world.apply({10, 0, 0, 1, {}, {entity(1, 30.0f), entity(2)}, NoInputLead, {}});

    const std::vector<RemovedEntity> removed =
        world.apply(EntitiesDestroyedMessage{10, {{1, DestroyCause::Killed}, {99, DestroyCause::Removed}}});

    REQUIRE(removed.size() == 1);
    CHECK(removed[0] == RemovedEntity{entity(1, 30.0f), DestroyCause::Killed});
    CHECK_FALSE(world.find(1));
    CHECK(world.find(2));
}

TEST_CASE("A destruction that arrives before older parts is not undone by them", "[game][world]")
{
    RemoteWorld world;
    world.apply({10, 0, 0, 1, {}, {entity(1)}, NoInputLead, {}});

    // The reliable message of tick 14 arrives before the snapshot of tick 12
    world.apply(EntitiesDestroyedMessage{14, {{1, DestroyCause::Killed}, {5, DestroyCause::Killed}}});
    CHECK(world.apply({12, 0, 0, 1, {}, {entity(1), entity(5)}, NoInputLead, {}}));
    CHECK_FALSE(world.find(1));
    // Even an entity it had never seen: its first snapshot was older than its destruction
    CHECK_FALSE(world.find(5));

    // Once the snapshots reach tick 14, the ids are forgotten: they are never given again
    CHECK(world.apply({14, 0, 0, 1, {}, {entity(2)}, NoInputLead, {}}));
    CHECK(world.find(2));
}

TEST_CASE("The world of a client skips the kinds it does not know", "[game][world]")
{
    RemoteWorld world;
    EntityState future = entity(4);
    future.kind = EntityKind{0x60};

    world.apply({1, 0, 0, 1, {}, {future, entity(5)}, NoInputLead, {}});

    CHECK_FALSE(world.find(4));
    CHECK(world.find(5));
}

TEST_CASE("A whole game sent through the messages gives the client the world of the game", "[game][world]")
{
    Simulation simulation;
    RemoteWorld world;
    std::mt19937 random(11);
    std::uniform_int_distribution<int> buttons(0, 0x1F);
    for (std::uint8_t slot = 0; slot < MaxPlayers; ++slot) {
        simulation.addPlayer(slot);
    }

    for (int tick = 1; tick <= 1200; ++tick) {
        if (tick % 20 == 0) {
            for (std::uint8_t slot = 0; slot < MaxPlayers; ++slot) {
                simulation.setInput(slot, PlayerInput{static_cast<std::uint8_t>(buttons(random))});
            }
        }
        simulation.step();
        const Snapshot &snapshot = simulation.snapshot();
        // Reliable, every tick; snapshots every 2 ticks
        for (const EntitiesDestroyedMessage &message : destroyedMessages(snapshot)) {
            world.apply(*decodeEntitiesDestroyed(encode(message)));
        }
        if (tick % 2 != 0) {
            continue;
        }
        for (const SnapshotPart &part : splitSnapshot(snapshot, simulation.tick())) {
            REQUIRE(world.apply(*decodeSnapshotPart(encode(part))));
        }

        REQUIRE(world.entities().size() == snapshot.entities.size());
        for (const EntityState &expected : snapshot.entities) {
            const EntityState *received = world.find(expected.id);
            REQUIRE(received);
            CHECK(received->kind == expected.kind);
            CHECK(received->owner == expected.owner);
            // Within half an eighth of a unit
            REQUIRE(std::abs(received->x - expected.x) <= 0.0625f);
            REQUIRE(std::abs(received->y - expected.y) <= 0.0625f);
        }
        CHECK(world.players() == snapshot.players);
    }
}

TEST_CASE("A shot gives its age in the byte of the health, the other kinds their health", "[game][messages]")
{
    CHECK(isShot(EntityKind::PlayerShot));
    CHECK(isShot(EntityKind::PlayerChargedShot));
    CHECK(isShot(EntityKind::BydoShot));
    CHECK(isShot(EntityKind::BydoPlasmaShot));
    CHECK(isShot(EntityKind{0x5F}));
    CHECK_FALSE(isShot(EntityKind::PlayerShip));
    CHECK_FALSE(isShot(EntityKind::BydoBoss));
    CHECK_FALSE(isShot(EntityKind{0x60}));

    SnapshotPart part = examplePart();
    part.entities.push_back({8, EntityKind::PlayerShot, 0, 300.0f, 540.0f, 1500.0f, 0.0f, 0, 0, 12});
    const auto decoded = decodeSnapshotPart(encode(part));
    REQUIRE(decoded);
    CHECK(decoded == part);
    CHECK(decoded->entities[0].health == 3);
    CHECK(decoded->entities[0].age == 0);
    CHECK(decoded->entities[1].age == 12);
    CHECK(decoded->entities[1].health == 0);
}

TEST_CASE("The world of a client ignores a destruction received again", "[game][world]")
{
    RemoteWorld world;
    REQUIRE(world.apply({10, 0, 0, 1, {}, {entity(1), entity(2)}, NoInputLead, {}}));
    const EntitiesDestroyedMessage destroyed{11, {{1, DestroyCause::Killed}}};

    CHECK(world.apply(destroyed).size() == 1);
    // Its copies, with the next snapshots: nothing more removed, no second explosion
    CHECK(world.apply(destroyed).empty());
    CHECK(world.apply(destroyed).empty());
    CHECK_FALSE(world.find(1));
    CHECK(world.find(2));
}

TEST_CASE("The world of a client removes an entity no snapshot had for StaleTicks", "[game][world]")
{
    RemoteWorld world;
    REQUIRE(world.apply({10, 0, 0, 1, {}, {entity(1), entity(2)}, NoInputLead, {}}));
    // Entity 2 is no longer in the snapshots, and every copy of its destruction was lost
    for (std::uint32_t tick = 12; tick <= 10 + RemoteWorld::StaleTicks; tick += 2) {
        REQUIRE(world.apply({tick, 0, 0, 1, {}, {entity(1)}, NoInputLead, {}}));
        CHECK(world.forgetStale().empty());
    }
    CHECK(world.find(2));

    REQUIRE(world.apply({12 + RemoteWorld::StaleTicks, 0, 0, 1, {}, {entity(1)}, NoInputLead, {}}));
    const std::vector<RemovedEntity> removed = world.forgetStale();
    REQUIRE(removed.size() == 1);
    CHECK(removed[0].last.id == 2);
    CHECK(removed[0].cause == DestroyCause::Removed);
    CHECK_FALSE(world.find(2));
    CHECK(world.find(1));
}
