// Tests of a game in the client: the LocalSession, the input of the local player, and the GameSessionSystem
// drawing the world of a session in a scene, without a window (ADR 0015, docs/architecture/client-game.md).

#include "FakeInput.hpp"
#include "Game/EntityLook.hpp"
#include "Game/GameSession.hpp"
#include "Game/GameSessionSystem.hpp"
#include "Game/GameView.hpp"
#include "Game/LocalSession.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/Constants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
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
/// The players with a cannon firing every 10 ticks at 1500 units per second.
rtype::game::GameConfig armedConfig()
{
    rtype::game::GameConfig config;
    config.projectiles.push_back({"shot",
                                  "",
                                  rtype::game::ProjectileCategory::Bullet,
                                  {rtype::physics::Aabb{{}, {12.0f, 4.0f, 0.0f}}, {}, false}});
    config.weapons.push_back({"cannon", "shot"});
    config.ships.push_back({"fighter", "models/fighter.glb", {}});
    config.player = {"fighter", "cannon", 100};
    return config;
}

struct Client {
    FakeInput input;
    rtype::engine::Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    rtype::engine::Scenes &scenes = engine.getScenes();
    GameSessionSystem &sessions;
    rtype::engine::SceneId scene;
    LocalSession *session = nullptr;

    explicit Client(rtype::game::Rules rules = {}, const rtype::game::GameConfig &config = {})
        : sessions(engine.getEcs().registerSystem<GameSessionSystem>(engine.getEcs(), engine.getScenes(),
                                                                     engine.getControls()))
    {
        engine.setInputSource(input);
        scene = scenes.open({.updatesWhenCovered = true});
        auto created = std::make_unique<LocalSession>(rules, config);
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

    /// The boxes narrower than a ship.
    [[nodiscard]] std::size_t countShots()
    {
        const float shipWidth = rtype::game::Rules{}.shipHalfWidth * 2.0f;
        std::size_t shots = 0;
        auto &replicated = ecs.cache<Replicated>();
        auto &boxes = ecs.cache<rtype::engine::Box>();
        for (rtype::ecs::entity_t entity = 0; entity < replicated.size(); ++entity) {
            if (replicated.has(entity) && ecs.isAlive(entity) && boxes.get(entity).width < shipWidth) {
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

TEST_CASE("Each entity of the game has its size, the ships of the players the colour of their player",
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
    CHECK(lookOf(shot).box.width < shipLook.box.width);
    CHECK(lookOf(shot).box.color.r == playerColor(2).r);
    CHECK(lookOf(shot).order < shipLook.order);
    // The Bydos have a look of their own, not the grey box of the unknown kinds
    const EntityLook bydo = lookOf({3, rtype::game::EntityKind::BydoFlyer, rtype::game::NoOwner});
    const EntityLook bydoShot = lookOf({4, rtype::game::EntityKind::BydoShot, rtype::game::NoOwner});
    CHECK(bydo.box.color.r != rtype::render::colors::Gray.r);
    CHECK(bydo.order == shipLook.order);
    CHECK(bydoShot.order < bydo.order);
    // Its plasma orbs too, round
    const EntityLook orb = lookOf({5, rtype::game::EntityKind::BydoPlasmaShot, rtype::game::NoOwner});
    CHECK(orb.box.color.r != rtype::render::colors::Gray.r);
    CHECK(orb.box.width == Approx(orb.box.height));
    // The four players have four colours
    for (std::uint8_t first = 0; first < 4; ++first) {
        for (std::uint8_t second = first + 1; second < 4; ++second) {
            CHECK((playerColor(first).r != playerColor(second).r ||
                   playerColor(first).g != playerColor(second).g));
        }
    }
}

TEST_CASE("The camera of the game sees the game plane as the screen sees the game", "[client]")
{
    // A 16:10 screen shows more of the game above and under it
    const rtype::render::Camera view = computeGameView({0.0f, -60.0f, 1920.0f, 1200.0f});

    CHECK(view.projection == rtype::render::Projection::Orthographic);
    CHECK(view.fieldOfView == Approx(1200.0f));
    CHECK(view.target.x == Approx(960.0f));
    CHECK(view.target.y == Approx(540.0f));
    CHECK(view.target.z == Approx(0.0f));
    CHECK(view.position.x == Approx(960.0f));
    CHECK(view.position.y == Approx(540.0f));
    // In front of the plane, y pointing down on the screen, as in the game
    CHECK(view.position.z < 0.0f);
    CHECK(view.up.y == Approx(-1.0f));
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

TEST_CASE("A kind of ship given a look is a 3D model, its hull on the box of the ship", "[client]")
{
    Client client;
    // A hull 4 units long, centred 1 unit right of the origin of the model and 1 unit above it
    const ShipLook look{{{}, {0, 1, 2, 3}}, {-1.0f, -2.0f, 4.0f, 2.0f}, false, {4, 5}};
    client.sessions.setShipLook(rtype::game::EntityKind::PlayerShip, look);
    const rtype::game::Rules rules;

    client.run(1);
    const rtype::game::EntityState &state = client.session->simulation().snapshot().entities[0];
    const std::optional<rtype::ecs::entity_t> ship = client.sessions.entityOf(state.id);
    REQUIRE(ship);
    REQUIRE(client.ecs.hasComponent<rtype::engine::Model>(*ship));
    CHECK_FALSE(client.ecs.hasComponent<rtype::engine::Box>(*ship));
    const rtype::engine::Model &model = client.ecs.getComponent<rtype::engine::Model>(*ship);
    // As long as the box of the ship, the centre of its hull on the ship
    CHECK(model.scale == Approx(rules.shipHalfWidth * 2.0f / 4.0f));
    CHECK(model.position.x == Approx(state.x - (1.0f * model.scale)));
    CHECK(model.position.y == Approx(state.y + (1.0f * model.scale)));
    // No flames while it does not fly forward, its flames animated while it does
    const auto animationOf = [&] {
        return client.ecs.getComponent<rtype::engine::ModelAnimation>(*ship).animation;
    };
    CHECK(animationOf() == look.model.animations.idle);
    CHECK(model.hiddenMeshes == look.flameMeshes);
    client.input.keys = {Key::Right};
    client.run(2);
    CHECK(animationOf() == look.model.animations.move);
    CHECK(model.hiddenMeshes.empty());
    client.input.keys = {Key::Left};
    client.run(2);
    CHECK(animationOf() == look.model.animations.idle);
    CHECK(model.hiddenMeshes == look.flameMeshes);

    // Climbing: the ship rolls its belly towards the camera little by little, its hull staying on it
    client.input.keys = {Key::Up};
    client.run(1);
    CHECK(animationOf() == look.model.animations.idle);
    CHECK(model.hiddenMeshes == look.flameMeshes);
    const auto rollOf = [&] { return client.ecs.getComponent<ShipAttitude>(*ship).roll; };
    CHECK(rollOf() < 0.0f);
    CHECK(rollOf() > -12.0f);
    client.run(12);
    CHECK(rollOf() < -28.0f);
    CHECK(rollOf() >= -40.0f);
    // Its flame holds still while it climbs
    const auto timeOf = [&] { return client.ecs.getComponent<rtype::engine::ModelAnimation>(*ship).time; };
    const float held = timeOf();
    client.run(3);
    CHECK(timeOf() == held);
    // Around its length, the axis of its tilt
    CHECK(model.rotationAxis.x == Approx(1.0f));
    CHECK(model.rotationAngle == Approx(200.0f + rollOf()));
    const rtype::ecs::Position &position = client.ecs.getComponent<rtype::ecs::Position>(*ship);
    const float roll = rollOf() * std::numbers::pi_v<float> / 180.0f;
    CHECK(model.position.x == Approx(position.x - (1.0f * model.scale)));
    CHECK(model.position.y == Approx(position.y + (1.0f * model.scale * std::cos(roll))));

    // Diving: its top towards the camera, less far; then flying straight again
    client.input.keys = {Key::Down};
    client.run(30);
    CHECK(rollOf() > 18.0f);
    CHECK(rollOf() <= 25.0f);
    client.input.keys.clear();
    client.run(60);
    CHECK(rollOf() == Approx(0.0f).margin(0.1f));
    CHECK(animationOf() == look.model.animations.idle);
}

TEST_CASE("A kind of shot given a look is a 3D model of energy, its core on the box of the shot", "[client]")
{
    Client client({}, armedConfig());
    // A core 4 units long, centred 1 unit right of the origin of the model
    const ShotLook look{{}, {-1.0f, -0.5f, 4.0f, 1.0f}, false, {{}, 2.0f}};
    client.sessions.setShotLook(rtype::game::EntityKind::PlayerShot, look);

    client.input.keys = {Key::Space};
    client.run(1);
    std::optional<rtype::game::EntityState> fired;
    for (const rtype::game::EntityState &state : client.session->simulation().snapshot().entities) {
        if (state.kind == rtype::game::EntityKind::PlayerShot) {
            fired = state;
        }
    }
    REQUIRE(fired);
    const std::optional<rtype::ecs::entity_t> shot = client.sessions.entityOf(fired->id);
    REQUIRE(shot);
    REQUIRE(client.ecs.hasComponent<rtype::engine::Model>(*shot));
    CHECK_FALSE(client.ecs.hasComponent<rtype::engine::Box>(*shot));
    const rtype::engine::Model &model = client.ecs.getComponent<rtype::engine::Model>(*shot);
    REQUIRE(model.energy);
    CHECK(model.energy->boost == Approx(2.0f));
    // As long as the box of the shot, the centre of its core on the shot
    CHECK(model.scale == Approx(lookOf(*fired).box.width / 4.0f));
    CHECK(model.position.x == Approx(fired->x - (1.0f * model.scale)));
    CHECK(model.position.y == Approx(fired->y));
}

TEST_CASE("Shots appear when fired, and their entities go when they leave the game", "[client]")
{
    Client client({}, armedConfig());

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
