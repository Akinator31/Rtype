// Tests of a game in the client: the LocalSession, the input of the local player, and the GameSessionSystem
// drawing the world of a session in a scene, without a window (ADR 0015, docs/architecture/client-game.md).

#include "FakeInput.hpp"
#include "Game/GameSession.hpp"
#include "Game/GameSessionSystem.hpp"
#include "Game/LocalSession.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/game/Constants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

using namespace rtype::client;
using Catch::Approx;
using rtype::input::Key;

namespace
{

constexpr float Tick = 1.0f / rtype::game::TickRate;

std::optional<rtype::game::EntityState> shipOf(const rtype::game::Snapshot &snapshot)
{
    for (const rtype::game::EntityState &entity : snapshot.entities) {
        if (entity.kind == rtype::game::EntityKind::PlayerShip) {
            return entity;
        }
    }
    return std::nullopt;
}

/// A session that records what the system gives it, and never stops: like a game online.
class RecordingSession final : public GameSession
{
public:
    std::vector<rtype::game::PlayerInput> inputs;
    std::vector<bool> pauses;

    [[nodiscard]] std::uint8_t localSlot() const override { return 1; }
    void setInput(rtype::game::PlayerInput input) override { inputs.push_back(input); }
    void update(float /*elapsed*/) override {}
    [[nodiscard]] std::vector<rtype::game::Snapshot> takeSnapshots() override { return {}; }
    void setPaused(bool paused) override { pauses.push_back(paused); }
    [[nodiscard]] bool isOver() const override { return false; }
};

/// The engine of the client without a window: a fake keyboard, and the system that plays the games.
struct Client {
    FakeInput input;
    rtype::engine::Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    rtype::engine::Scenes &scenes = engine.getScenes();
    GameSessionSystem &sessions;
    rtype::engine::SceneId scene;
    LocalSession *session = nullptr;

    explicit Client(rtype::game::Rules rules = {})
        : sessions(engine.getEcs().registerSystem<GameSessionSystem>(engine.getEcs(), engine.getScenes(),
                                                                     engine.getControls()))
    {
        engine.setInputSource(input);
        scene = scenes.open({.updatesWhenCovered = true});
        auto created = std::make_unique<LocalSession>(rules);
        session = created.get();
        sessions.start(std::move(created), scene);
    }

    /// Frames of one tick each.
    void run(int frames)
    {
        for (int i = 0; i < frames; ++i) {
            engine.update(Tick);
        }
    }

    [[nodiscard]] std::size_t countShots()
    {
        std::size_t shots = 0;
        auto &replicated = ecs.cache<Replicated>();
        auto &boxes = ecs.cache<rtype::engine::Box>();
        for (rtype::ecs::entity_t entity = 0; entity < replicated.size(); ++entity) {
            if (replicated.has(entity) && ecs.isAlive(entity) && boxes.get(entity).width < 50.0f) {
                ++shots;
            }
        }
        return shots;
    }
};

} // namespace

// --- The session offline ---

TEST_CASE("A local session steps 60 times per second of game time, whatever the frames", "[client]")
{
    LocalSession session;

    session.update(0.5f);
    const std::vector<rtype::game::Snapshot> half = session.takeSnapshots();
    // At most MaxStepsPerUpdate in one update: half a second at once is a long frame
    CHECK(half.size() == LocalSession::MaxStepsPerUpdate);

    // Frames of 1/120 s: one tick every two frames
    for (int i = 0; i < 120; ++i) {
        session.update(1.0f / 120.0f);
    }
    const std::vector<rtype::game::Snapshot> second = session.takeSnapshots();
    CHECK((second.size() == 60 || second.size() == 61));
    CHECK(second.back().tick == session.simulation().tick());
    CHECK(session.takeSnapshots().empty());
}

TEST_CASE("A local session has its player in slot 0, moved by its input", "[client]")
{
    LocalSession session;
    CHECK(session.localSlot() == 0);
    session.update(Tick);
    const float start = shipOf(session.takeSnapshots().back())->x;

    rtype::game::PlayerInput right;
    right.set(rtype::game::PlayerInput::Right, true);
    session.setInput(right);
    for (int i = 0; i < 30; ++i) {
        session.update(Tick);
    }

    CHECK(shipOf(session.takeSnapshots().back())->x > start + 100.0f);
    CHECK_FALSE(session.isOver());
}

TEST_CASE("A local session paused does not step", "[client]")
{
    LocalSession session;
    session.setPaused(true);
    session.update(1.0f);
    CHECK(session.takeSnapshots().empty());
    CHECK(session.simulation().tick() == 0);

    session.setPaused(false);
    session.update(Tick);
    CHECK(session.takeSnapshots().size() == 1);
}

// --- The input and the look ---

TEST_CASE("The controls of the engine become the input of the player", "[client]")
{
    FakeInput input;
    rtype::engine::Controls controls;

    input.keys = {Key::Right, Key::Up, Key::Space};
    controls.update(input);
    const rtype::game::PlayerInput held = inputFrom(controls);

    CHECK(held.isHeld(rtype::game::PlayerInput::Right));
    CHECK(held.isHeld(rtype::game::PlayerInput::Up));
    CHECK(held.isHeld(rtype::game::PlayerInput::Fire));
    CHECK_FALSE(held.isHeld(rtype::game::PlayerInput::Left));
    CHECK_FALSE(held.isHeld(rtype::game::PlayerInput::Down));

    input.keys.clear();
    controls.update(input);
    CHECK(inputFrom(controls).buttons == 0);
}

TEST_CASE("Each entity of the game has the size of the rules, the ships the colour of their player",
          "[client]")
{
    const rtype::game::Rules rules;
    rtype::game::EntityState ship{1, rtype::game::EntityKind::PlayerShip, 2};
    rtype::game::EntityState shot{2, rtype::game::EntityKind::PlayerShot, 2};

    const EntityLook shipLook = lookOf(ship);
    CHECK(shipLook.box.width == Approx(rules.shipHalfWidth * 2.0f));
    CHECK(shipLook.box.height == Approx(rules.shipHalfHeight * 2.0f));
    CHECK(shipLook.box.color.r == playerColor(2).r);
    CHECK(shipLook.box.color.g == playerColor(2).g);
    CHECK(lookOf(shot).box.width == Approx(rules.shotHalfWidth * 2.0f));
    CHECK(lookOf(shot).order < shipLook.order);
    // The four players have four colours
    for (std::uint8_t first = 0; first < 4; ++first) {
        for (std::uint8_t second = first + 1; second < 4; ++second) {
            CHECK((playerColor(first).r != playerColor(second).r ||
                   playerColor(first).g != playerColor(second).g));
        }
    }
}

// --- The world in the scene ---

TEST_CASE("The world of the session is drawn in the scene, and follows it", "[client]")
{
    Client client;

    client.run(1);
    REQUIRE(client.sessions.entityCount() == 1);
    const rtype::game::Snapshot &snapshot = client.session->simulation().snapshot();
    const std::optional<rtype::ecs::entity_t> ship = client.sessions.entityOf(snapshot.entities[0].id);
    REQUIRE(ship);
    CHECK(client.ecs.getComponent<rtype::ecs::Position>(*ship).x == Approx(snapshot.entities[0].x));
    CHECK(client.ecs.getComponent<rtype::engine::SceneMember>(*ship).scene == client.scene);
    CHECK(client.ecs.getComponent<rtype::engine::Drawable>(*ship).layer == rtype::engine::Layer::World);
    REQUIRE(client.sessions.players().size() == 1);
    CHECK(client.sessions.players()[0].slot == 0);

    // Right held: the ship moves on the screen
    const float start = client.ecs.getComponent<rtype::ecs::Position>(*ship).x;
    client.input.keys = {Key::Right};
    client.run(30);
    CHECK(client.ecs.getComponent<rtype::ecs::Position>(*ship).x > start + 100.0f);
}

TEST_CASE("Shots appear when fired, and their entities go when they leave the game", "[client]")
{
    rtype::game::Rules rules;
    rules.maxEnemies = 0;
    Client client(rules);

    client.input.keys = {Key::Space};
    client.run(30);
    const std::size_t fired = client.countShots();
    CHECK(fired >= 2);
    CHECK(client.sessions.entityCount() == 1 + fired);

    // Released: every shot flies out of the area within two seconds
    client.input.keys.clear();
    client.run(120);
    CHECK(client.countShots() == 0);
    CHECK(client.sessions.entityCount() == 1);
}

TEST_CASE("The game stops while a menu is over its scene", "[client]")
{
    Client client;
    client.run(10);
    const std::uint32_t tick = client.sessions.tick();

    // A pause menu: the game scene no longer receives the input
    client.scenes.open({.overlay = true});
    client.input.keys = {Key::Right};
    client.run(30);
    CHECK(client.sessions.tick() == tick);

    client.scenes.close();
    client.run(1);
    CHECK(client.sessions.tick() == tick + 1);
}

TEST_CASE("Under a menu, the session gets an empty input, even one that goes on", "[client]")
{
    Client client;
    auto created = std::make_unique<RecordingSession>();
    RecordingSession &session = *created;
    client.sessions.start(std::move(created), client.scene);
    client.input.keys = {Key::Right};

    client.run(1);
    REQUIRE_FALSE(session.inputs.empty());
    CHECK(session.inputs.back().isHeld(rtype::game::PlayerInput::Right));
    CHECK_FALSE(session.pauses.back());

    // A menu over the game: online, the ship must not go on moving while the player is in the menu
    client.scenes.open({.overlay = true});
    client.run(1);
    CHECK(session.inputs.back().buttons == 0);
    CHECK(session.pauses.back());
}

TEST_CASE("The session stops when its scene closes", "[client]")
{
    Client client;
    client.run(5);
    REQUIRE(client.sessions.running());

    client.scenes.close(client.scene);
    client.run(1);

    CHECK_FALSE(client.sessions.running());
    CHECK(client.sessions.entityCount() == 0);
    // The engine destroyed the entities of the scene
    auto &replicated = client.ecs.cache<Replicated>();
    for (rtype::ecs::entity_t entity = 0; entity < replicated.size(); ++entity) {
        CHECK_FALSE(replicated.has(entity));
    }
}
