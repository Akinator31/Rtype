// Tests of a game in the client: the LocalSession, the input of the local player, and the GameSessionSystem
// drawing the world of a session in a scene, without a window (ADR 0015, docs/architecture/client-game.md).

#include "FakeInput.hpp"
#include "Game/DamageLooks.hpp"
#include "Game/DamageNumberSystem.hpp"
#include "Game/DeathScreen.hpp"
#include "Game/DeathScreenSystem.hpp"
#include "Game/EntityLook.hpp"
#include "Game/GameRecorder.hpp"
#include "Game/GameSession.hpp"
#include "Game/GameSessionSystem.hpp"
#include "Game/GameView.hpp"
#include "Game/HealthBarLooks.hpp"
#include "Game/HealthBarSystem.hpp"
#include "Game/Hud.hpp"
#include "Game/HudSystem.hpp"
#include "Game/ImpactLooks.hpp"
#include "Game/ImpactSystem.hpp"
#include "Game/LocalSession.hpp"
#include "Game/Radar.hpp"
#include "Game/RescueCameraSystem.hpp"
#include "Game/ResultsScreen.hpp"
#include "Game/ResultsSystem.hpp"
#include "Game/ShotLooks.hpp"
#include "Quests/StatQuest.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Sound.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/engine/System/ModelAnimationSystem.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/Constants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
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
    [[nodiscard]] std::optional<std::chrono::milliseconds> ping() const override { return std::nullopt; }
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

TEST_CASE("A local session is the story mode: allies come to the rescue against the final boss", "[client]")
{
    const LocalSession session;
    CHECK(session.simulation().rules().rescue);
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

TEST_CASE("Between two steps, a local session draws its ship on the way to the next tick, at every frame",
          "[client]")
{
    LocalSession session;
    CHECK_FALSE(session.between());
    rtype::game::PlayerInput right;
    right.set(rtype::game::PlayerInput::Right, true);
    session.setInput(right);
    session.update(Tick);
    (void)session.takeSnapshots();

    // 144 frames per second: without between(), two frames in three would not move it
    const float speed = session.simulation().rules().shipSpeed;
    const float perFrame = speed / 144.0f;
    float previous = shipOf(*session.between())->x;
    for (int i = 0; i < 144; ++i) {
        session.update(1.0f / 144.0f);
        (void)session.takeSnapshots();
        const auto drawn = session.between();
        REQUIRE(drawn);
        const float x = shipOf(*drawn)->x;
        REQUIRE(x - previous == Approx(perFrame).margin(perFrame * 0.1f));
        previous = x;
    }
    // A key let go shows at the next frame
    session.setInput({});
    CHECK(shipOf(*session.between())->x <= previous + 0.001f);
}

TEST_CASE("What a local session draws between two steps carries no event, and nothing while paused",
          "[client]")
{
    LocalSession session;
    session.update(Tick);
    const rtype::game::Snapshot stepped = session.takeSnapshots().back();
    const auto drawn = session.between();
    REQUIRE(drawn);
    CHECK(drawn->tick == stepped.tick);
    CHECK(drawn->entities.size() == stepped.entities.size());
    CHECK(drawn->destroyed.empty());
    CHECK_FALSE(drawn->waveStarted);
    CHECK_FALSE(drawn->gameOver);

    session.setPaused(true);
    CHECK_FALSE(session.between());
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
    // The allies of the rescue against the final boss, of the size of a ship, in a colour of their own
    const EntityLook ally = lookOf({6, rtype::game::EntityKind::AllyShip, rtype::game::NoOwner});
    CHECK(ally.box.width == Approx(shipLook.box.width));
    CHECK(ally.box.color.r != rtype::render::colors::Gray.r);
    CHECK(ally.order == shipLook.order);
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
    client.ecs.registerSystem<rtype::engine::ModelAnimationSystem>();
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
    // A little flame, halfway between Idle and Move, whatever it does, and no trails
    const auto animationOf = [&]() -> const rtype::engine::ModelAnimation & {
        return client.ecs.getComponent<rtype::engine::ModelAnimation>(*ship);
    };
    CHECK(animationOf().animation == look.model.animations.idle);
    CHECK(animationOf().blendedAnimation == look.model.animations.move);
    CHECK(animationOf().blendAmount == 0.5f);
    CHECK(model.hiddenMeshes == look.trailMeshes);
    client.input.keys = {Key::Right};
    client.run(17);
    CHECK(animationOf().animation == look.model.animations.idle);
    CHECK(animationOf().blendAmount == 0.5f);
    client.input.keys = {Key::Left};
    client.run(17);
    CHECK(animationOf().blendAmount == 0.5f);

    // Climbing: the ship rolls its belly towards the camera little by little, its hull staying on it
    client.input.keys = {Key::Up};
    client.run(1);
    const auto rollOf = [&] { return client.ecs.getComponent<ShipAttitude>(*ship).roll; };
    CHECK(rollOf() < 0.0f);
    CHECK(rollOf() > -12.0f);
    client.run(12);
    CHECK(rollOf() < -28.0f);
    CHECK(rollOf() >= -40.0f);
    // Its flame keeps flickering while it climbs
    const auto timeOf = [&] { return client.ecs.getComponent<rtype::engine::ModelAnimation>(*ship).time; };
    const float before = timeOf();
    client.run(3);
    CHECK(timeOf() > before);
    CHECK(animationOf().blendAmount == 0.5f);
    CHECK(model.hiddenMeshes == look.trailMeshes);
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
    CHECK(animationOf().blendAmount == 0.5f);
}

TEST_CASE("A ship given a look recoils each time it fires, over its flight", "[client]")
{
    Client client({}, armedConfig());
    client.ecs.registerSystem<rtype::engine::ModelAnimationSystem>();
    const ShipLook look{{{}, {0, 1, 2, 3, 6, 0.3f}}, {-1.0f, -2.0f, 4.0f, 2.0f}, false, {}};
    client.sessions.setShipLook(rtype::game::EntityKind::PlayerShip, look);

    client.run(1);
    const std::optional<rtype::ecs::entity_t> ship =
        client.sessions.entityOf(client.session->simulation().snapshot().entities[0].id);
    REQUIRE(ship);
    const auto animationOf = [&]() -> const rtype::engine::ModelAnimation & {
        return client.ecs.getComponent<rtype::engine::ModelAnimation>(*ship);
    };
    CHECK(animationOf().addedAnimation == rtype::engine::NoAnimation);

    // Its first shot appears at its nose: Shoot is added from its start, the flame stays
    const auto countShots = [&] { return client.sessions.entityCount() - 1; };
    client.input.keys = {Key::Space};
    for (int frame = 0; frame < 30 && countShots() == 0; ++frame) {
        client.run(1);
    }
    REQUIRE(countShots() == 1);
    CHECK(animationOf().addedAnimation == 6);
    CHECK(animationOf().addedDuration == 0.3f);
    CHECK(animationOf().addedTime < 0.3f);
    CHECK(animationOf().animation == look.model.animations.idle);
    CHECK(animationOf().blendedAnimation == look.model.animations.move);
    CHECK(animationOf().blendAmount == 0.5f);

    // Released: the recoil ends once played
    client.input.keys.clear();
    client.run(30);
    CHECK(animationOf().addedAnimation == rtype::engine::NoAnimation);
    CHECK(animationOf().blendAmount == 0.5f);
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

TEST_CASE("A shot fired askew turns its model the way it flies, its core still on the shot", "[client]")
{
    rtype::ecs::Ecs ecs;
    ShotLooks looks;
    // A model that faces left, its core 4 units long, centred 1 unit right of its origin when straight
    looks.setLook(rtype::game::EntityKind::BydoShot, {{}, {-1.0f, -0.5f, 4.0f, 1.0f}, true, {}});
    rtype::game::EntityState state{7, rtype::game::EntityKind::BydoShot, rtype::game::NoOwner};
    state.x = 500.0f;
    state.y = 300.0f;
    // Where the nose of the model points after its turns: a half turn around its vertical axis to face left,
    // then a half turn around the axis of the model
    const auto noseOf = [](const rtype::engine::Model &model) {
        const rtype::render::Vector3 &axis = model.rotationAxis;
        return rtype::render::Vector2{1.0f - (2.0f * axis.x * axis.x), -2.0f * axis.x * axis.y};
    };

    state.velocityX = -600.0f;
    const rtype::ecs::entity_t straight = ecs.createEntity();
    looks.addModel(ecs, straight, state, lookOf(state));
    const rtype::engine::Model &ahead = ecs.getComponent<rtype::engine::Model>(straight);
    CHECK(noseOf(ahead).x == Approx(-1.0f));
    CHECK(noseOf(ahead).y == Approx(0.0f).margin(1e-6));
    CHECK(ahead.position.x == Approx(500.0f - (1.0f * ahead.scale)));

    // Down and to the left, at 45 degrees
    state.velocityY = 600.0f;
    const rtype::ecs::entity_t askew = ecs.createEntity();
    looks.addModel(ecs, askew, state, lookOf(state));
    const rtype::engine::Model &turned = ecs.getComponent<rtype::engine::Model>(askew);
    CHECK(noseOf(turned).x == Approx(-std::numbers::sqrt2_v<float> / 2.0f));
    CHECK(noseOf(turned).y == Approx(std::numbers::sqrt2_v<float> / 2.0f));
    CHECK(turned.position.x == Approx(500.0f - (std::numbers::sqrt2_v<float> / 2.0f * turned.scale)));
    CHECK(turned.position.y == Approx(300.0f + (std::numbers::sqrt2_v<float> / 2.0f * turned.scale)));
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

TEST_CASE("A shot of a kind given a sound appears with it, for the sound system of the engine", "[client]")
{
    Client client({}, armedConfig());
    const rtype::audio::SoundId fire{7};
    client.sessions.setShotSound(rtype::game::EntityKind::PlayerShot, {fire, 0.5f});

    client.input.keys = {Key::Space};
    client.run(30);
    std::size_t shots = 0;
    auto &replicated = client.ecs.cache<Replicated>();
    for (rtype::ecs::entity_t entity = 0; entity < replicated.size(); ++entity) {
        if (!replicated.has(entity) || !client.ecs.isAlive(entity)) {
            continue;
        }
        if (replicated.get(entity).kind != rtype::game::EntityKind::PlayerShot) {
            CHECK_FALSE(client.ecs.hasComponent<rtype::engine::SoundEffect>(entity));
            continue;
        }
        ++shots;
        REQUIRE(client.ecs.hasComponent<rtype::engine::SoundEffect>(entity));
        const auto &effect = client.ecs.getComponent<rtype::engine::SoundEffect>(entity);
        CHECK(effect.sound == fire);
        CHECK(effect.settings.volume == 0.5f);
        // Fired from the left of the game area: heard on the left
        CHECK(effect.settings.pan < 0.0f);
    }
    CHECK(shots >= 2);
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

// --- Impacts ---

namespace
{

/// A session that gives the snapshots written by the test.
class ScriptedSession final : public GameSession
{
public:
    std::vector<rtype::game::Snapshot> snapshots;
    std::optional<std::chrono::milliseconds> rtt;
    std::map<std::uint8_t, std::string> names;
    std::optional<VoiceView> voice;

    [[nodiscard]] std::uint8_t localSlot() const override { return 0; }
    void setInput(rtype::game::PlayerInput /*input*/) override {}
    void update(float /*elapsed*/) override {}
    [[nodiscard]] std::vector<rtype::game::Snapshot> takeSnapshots() override
    {
        return std::exchange(snapshots, {});
    }
    void setPaused(bool /*paused*/) override {}
    [[nodiscard]] bool isOver() const override { return false; }
    [[nodiscard]] std::optional<std::chrono::milliseconds> ping() const override { return rtt; }
    [[nodiscard]] std::map<std::uint8_t, std::string> playerNames() const override { return names; }
    [[nodiscard]] std::optional<VoiceView> voiceView() const override { return voice; }
};

/// The engine of the client playing a scripted session, its impacts played.
struct ImpactClient {
    rtype::engine::Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    GameSessionSystem &sessions;
    rtype::engine::SceneId scene;
    ScriptedSession *session = nullptr;

    ImpactClient()
        : sessions(engine.getEcs().registerSystem<GameSessionSystem>(engine.getEcs(), engine.getScenes(),
                                                                     engine.getControls()))
    {
        ecs.registerSystem<ImpactSystem>(ecs);
        ecs.registerSystem<DamageNumberSystem>(ecs);
        ecs.registerSystem<HealthBarSystem>();
        scene = engine.getScenes().open({});
        auto created = std::make_unique<ScriptedSession>();
        session = created.get();
        sessions.start(std::move(created), scene);
    }

    /// One frame that receives a snapshot.
    void receive(rtype::game::Snapshot snapshot)
    {
        session->snapshots.push_back(std::move(snapshot));
        engine.update(Tick);
    }

    [[nodiscard]] std::vector<rtype::ecs::entity_t> damageNumbers()
    {
        std::vector<rtype::ecs::entity_t> found;
        auto &numbers = ecs.cache<DamageNumber>();
        for (rtype::ecs::entity_t entity = 0; entity < numbers.size(); ++entity) {
            if (numbers.has(entity) && ecs.isAlive(entity)) {
                found.push_back(entity);
            }
        }
        return found;
    }

    /// The health bar of an entity of the game, if it has one.
    [[nodiscard]] std::optional<rtype::ecs::entity_t> healthBarOf(std::uint32_t networkId)
    {
        const std::optional<rtype::ecs::entity_t> entity = sessions.entityOf(networkId);
        return entity ? ecs.getComponent<Replicated>(*entity).healthBar : std::nullopt;
    }

    [[nodiscard]] std::vector<rtype::ecs::entity_t> impacts()
    {
        std::vector<rtype::ecs::entity_t> found;
        auto &impacts = ecs.cache<Impact>();
        for (rtype::ecs::entity_t entity = 0; entity < impacts.size(); ++entity) {
            if (impacts.has(entity) && ecs.isAlive(entity)) {
                found.push_back(entity);
            }
        }
        return found;
    }
};

rtype::game::EntityState shotAt(std::uint32_t id, float x, float y)
{
    rtype::game::EntityState shot{id, rtype::game::EntityKind::PlayerShot, 0};
    shot.x = x;
    shot.y = y;
    return shot;
}

rtype::game::EntityState bydoAt(std::uint32_t id, float x, float y, std::uint8_t health)
{
    rtype::game::EntityState bydo{id, rtype::game::EntityKind::BydoFlyer, rtype::game::NoOwner};
    bydo.x = x;
    bydo.y = y;
    bydo.health = health;
    return bydo;
}

const DamageLook YellowDamage{{}, 40.0f, {255, 220, 90, 255}};

const HealthBarLook BarLook{100.0f, 42.0f, 0.3f};

const ImpactLook SparkLook{{}, 2, 0.4f, 3.0f, {10, 20, 30, 200}, {}};

rtype::game::Snapshot snapshotOf(std::uint32_t tick, std::vector<rtype::game::EntityState> entities,
                                 std::vector<rtype::game::DestroyedEntity> destroyed = {})
{
    rtype::game::Snapshot snapshot;
    snapshot.tick = tick;
    snapshot.entities = std::move(entities);
    snapshot.destroyed = std::move(destroyed);
    return snapshot;
}

/// The half size of the box collider of the first entity of a kind drawn in the scene.
std::optional<rtype::physics::Vec3> findHalfSize(rtype::ecs::Ecs &ecs, rtype::game::EntityKind kind)
{
    auto &replicated = ecs.cache<Replicated>();
    auto &colliders = ecs.cache<rtype::physics::Collider>();
    for (rtype::ecs::entity_t entity = 0; entity < replicated.size(); ++entity) {
        if (replicated.has(entity) && ecs.isAlive(entity) && replicated.get(entity).kind == kind &&
            colliders.has(entity)) {
            return std::get<rtype::physics::Aabb>(colliders.get(entity).shape).halfSize;
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("Offline, an entity drawn has the collider of the game, for the hitbox overlay", "[client]")
{
    Client client({}, armedConfig());

    client.input.keys = {Key::Space};
    client.run(15);

    const std::optional<rtype::physics::Vec3> ship =
        findHalfSize(client.ecs, rtype::game::EntityKind::PlayerShip);
    REQUIRE(ship.has_value());
    CHECK(ship->x == Approx(rtype::game::Rules{}.shipHalfWidth));
    // The collider of the projectile of the config, not the box the shot is drawn as
    const std::optional<rtype::physics::Vec3> shot =
        findHalfSize(client.ecs, rtype::game::EntityKind::PlayerShot);
    REQUIRE(shot.has_value());
    CHECK(shot->x == Approx(12.0f));
    CHECK(shot->y == Approx(4.0f));
}

TEST_CASE("Online, an entity drawn has a collider of the box it is drawn as", "[client]")
{
    ImpactClient client;
    const rtype::game::EntityState bydo{3, rtype::game::EntityKind::BydoFlyer, rtype::game::NoOwner};

    client.receive(snapshotOf(1, {bydo}));

    const std::optional<rtype::physics::Vec3> halfSize =
        findHalfSize(client.ecs, rtype::game::EntityKind::BydoFlyer);
    REQUIRE(halfSize.has_value());
    CHECK(halfSize->x == Approx(lookOf(bydo).box.width / 2.0f));
    CHECK(halfSize->y == Approx(lookOf(bydo).box.height / 2.0f));
}

TEST_CASE("A shot killed in the game leaves its impact where it was last drawn, played once", "[client]")
{
    ImpactClient client;
    client.sessions.setImpactLook(rtype::game::EntityKind::PlayerShot, SparkLook);
    client.receive(snapshotOf(1, {shotAt(5, 300.0f, 200.0f)}));

    // Online, the destruction comes first, while the shot is still drawn
    client.receive(snapshotOf(2, {shotAt(5, 310.0f, 200.0f)}, {{5, rtype::game::DestroyCause::Killed}}));
    CHECK(client.impacts().empty());
    client.receive(snapshotOf(3, {}));

    const std::vector<rtype::ecs::entity_t> impacts = client.impacts();
    REQUIRE(impacts.size() == 1);
    const rtype::ecs::entity_t impact = impacts[0];
    CHECK(client.ecs.getComponent<rtype::engine::SceneMember>(impact).scene == client.scene);
    const rtype::engine::Model &model = client.ecs.getComponent<rtype::engine::Model>(impact);
    CHECK(model.position.x == Approx(310.0f));
    CHECK(model.position.y == Approx(200.0f));
    // In front of the game, towards the camera
    CHECK(model.position.z < 0.0f);
    CHECK(model.scale == Approx(3.0f));
    CHECK(model.tint.r == 10);
    CHECK(model.energy);
    const auto &animation = client.ecs.getComponent<rtype::engine::ModelAnimation>(impact);
    CHECK(animation.animation == 2);
    CHECK(animation.speed == 0.0f);
    CHECK(animation.time == Approx(Tick));

    // Opaque, then fading out in the last quarter of its animation, then gone before it loops
    client.receive(snapshotOf(4, {}));
    CHECK(client.ecs.getComponent<rtype::engine::Model>(impact).tint.a == 200);
    for (int frame = 0; frame < 17; ++frame) {
        client.receive(snapshotOf(static_cast<std::uint32_t>(5 + frame), {}));
    }
    REQUIRE(client.ecs.isAlive(impact));
    const std::uint8_t fading = client.ecs.getComponent<rtype::engine::Model>(impact).tint.a;
    CHECK(fading < 200);
    CHECK(fading > 0);
    for (int frame = 0; frame < 6; ++frame) {
        client.receive(snapshotOf(static_cast<std::uint32_t>(22 + frame), {}));
    }
    CHECK(client.impacts().empty());
}

TEST_CASE("An entity removed without effect, or of a kind without an impact, leaves none", "[client]")
{
    ImpactClient client;
    client.sessions.setImpactLook(rtype::game::EntityKind::PlayerShot, SparkLook);
    rtype::game::EntityState bydoShot{7, rtype::game::EntityKind::BydoShot, rtype::game::NoOwner};
    client.receive(snapshotOf(1, {shotAt(5, 300.0f, 200.0f), bydoShot}));

    client.receive(
        snapshotOf(2, {}, {{5, rtype::game::DestroyCause::Removed}, {7, rtype::game::DestroyCause::Killed}}));

    CHECK(client.sessions.entityCount() == 0);
    CHECK(client.impacts().empty());
}

TEST_CASE("A ship killed in the game sounds with its impact, on its side of the screen", "[client]")
{
    ImpactClient client;
    const rtype::audio::SoundId destruction{4};
    client.sessions.setImpactLook(rtype::game::EntityKind::BydoFlyer, SparkLook);
    client.sessions.setDestructionSound(rtype::game::EntityKind::BydoFlyer, {destruction, 0.8f});
    rtype::game::EntityState bydo{5, rtype::game::EntityKind::BydoFlyer, rtype::game::NoOwner};
    bydo.x = 1700.0f;
    rtype::game::EntityState fled{6, rtype::game::EntityKind::BydoFlyer, rtype::game::NoOwner};
    client.receive(snapshotOf(1, {bydo, fled}));
    CHECK_FALSE(client.ecs.hasComponent<rtype::engine::SoundEffect>(*client.sessions.entityOf(5)));

    client.receive(
        snapshotOf(2, {}, {{5, rtype::game::DestroyCause::Killed}, {6, rtype::game::DestroyCause::Removed}}));

    const std::vector<rtype::ecs::entity_t> impacts = client.impacts();
    REQUIRE(impacts.size() == 1);
    REQUIRE(client.ecs.hasComponent<rtype::engine::SoundEffect>(impacts[0]));
    const auto &effect = client.ecs.getComponent<rtype::engine::SoundEffect>(impacts[0]);
    CHECK(effect.sound == destruction);
    CHECK(effect.settings.volume == 0.8f);
    // On the right of the game area: heard on the right
    CHECK(effect.settings.pan > 0.0f);
}

TEST_CASE("A ship struck by the blade of the shogun shows the cut where it is drawn, sounds, and is told",
          "[client]")
{
    ImpactClient client;
    const rtype::audio::SoundId cut{9};
    client.sessions.setStrikeLook(rtype::game::EntityKind::AllyShip, SparkLook);
    client.sessions.setStrikeSound(rtype::game::EntityKind::AllyShip, {cut, 1.0f});
    int strikes = 0;
    client.sessions.setStrikeHandler([&strikes] { ++strikes; });
    rtype::game::EntityState ally{5, rtype::game::EntityKind::AllyShip, 0};
    ally.x = 470.0f;
    ally.y = 360.0f;
    rtype::game::EntityState ship{6, rtype::game::EntityKind::PlayerShip, 0};
    client.receive(snapshotOf(1, {ally, ship}));
    CHECK(client.impacts().empty());

    ally.x = 450.0f;
    rtype::game::Snapshot struck = snapshotOf(2, {ally, ship});
    // The ship of the player has no strike look: told, but without a cut
    struck.struck = {5, 6};
    client.receive(struck);

    const std::vector<rtype::ecs::entity_t> impacts = client.impacts();
    REQUIRE(impacts.size() == 1);
    const rtype::engine::Model &model = client.ecs.getComponent<rtype::engine::Model>(impacts[0]);
    CHECK(model.position.x == Approx(450.0f));
    CHECK(model.position.y == Approx(360.0f));
    REQUIRE(client.ecs.hasComponent<rtype::engine::SoundEffect>(impacts[0]));
    CHECK(client.ecs.getComponent<rtype::engine::SoundEffect>(impacts[0]).sound == cut);
    CHECK(strikes == 2);
    // Both ships are unhurt
    CHECK(client.sessions.entityCount() == 2);
}

TEST_CASE("The camera closes up on the shogun while it dashes, shakes at a strike, then sees the whole game",
          "[client]")
{
    ImpactClient client;
    client.ecs.registerSystem<RescueCameraSystem>(client.sessions);
    const rtype::render::Rectangle area{0.0f, 0.0f, 1920.0f, 1080.0f};
    const rtype::ecs::entity_t camera = client.ecs.createEntity();
    client.ecs.addComponent<rtype::engine::Camera>(camera, computeGameView(area));
    const rtype::render::Vector2 centre{960.0f, 540.0f};
    client.ecs.addComponent<RescueCamera>(camera, area, 1.0f, 0.0f, centre, centre);
    const auto view = [&client, camera] {
        return client.ecs.getComponent<rtype::engine::Camera>(camera).view;
    };
    rtype::game::EntityState boss{7, rtype::game::EntityKind::BydoBoss, rtype::game::NoOwner};
    boss.x = 1600.0f;
    boss.y = 540.0f;
    rtype::game::Snapshot snapshot = snapshotOf(1, {boss});

    // At rest, it sees the whole game
    client.receive(snapshot);
    CHECK(view().fieldOfView == Approx(1080.0f));
    CHECK(view().target.x == Approx(960.0f));
    CHECK(view().up.y == Approx(-1.0f));

    // While the shogun dashes, closer, towards it, still inside the game
    snapshot.rescue = rtype::game::RescueStage::Dash;
    for (int frame = 0; frame < 240; ++frame) {
        client.receive(snapshot);
    }
    const rtype::render::Camera closeUp = view();
    CHECK(closeUp.fieldOfView < 0.7f * 1080.0f);
    CHECK(closeUp.target.x > 960.0f);
    const float halfWidth = closeUp.fieldOfView * (1920.0f / 1080.0f) / 2.0f;
    CHECK(closeUp.target.x + halfWidth <= Approx(1920.0f));
    CHECK(closeUp.up.y == Approx(-1.0f));

    // A strike shakes it, then it calms down
    shakeCamera(client.ecs.getComponent<RescueCamera>(camera), 0.75f);
    client.receive(snapshot);
    const rtype::render::Camera shaken = view();
    CHECK((shaken.target.x != Approx(closeUp.target.x) || shaken.target.y != Approx(closeUp.target.y)));
    for (int frame = 0; frame < 60; ++frame) {
        client.receive(snapshot);
    }
    CHECK(view().target.x == Approx(closeUp.target.x).margin(1.0f));
    CHECK(view().target.y == Approx(closeUp.target.y).margin(1.0f));

    // Once the dashes are over, back to the whole game
    snapshot.rescue = rtype::game::RescueStage::Prompt;
    for (int frame = 0; frame < 300; ++frame) {
        client.receive(snapshot);
    }
    CHECK(view().fieldOfView == Approx(1080.0f).epsilon(0.001));
    CHECK(view().target.x == Approx(960.0f).epsilon(0.001));
}

TEST_CASE("The camera pushes in on the ship of the player during the voice of the rescue, where it was lost",
          "[client]")
{
    ImpactClient client;
    client.ecs.registerSystem<RescueCameraSystem>(client.sessions);
    const rtype::render::Rectangle area{0.0f, 0.0f, 1920.0f, 1080.0f};
    const rtype::ecs::entity_t camera = client.ecs.createEntity();
    client.ecs.addComponent<rtype::engine::Camera>(camera, computeGameView(area));
    const rtype::render::Vector2 centre{960.0f, 540.0f};
    client.ecs.addComponent<RescueCamera>(camera, area, 1.0f, 0.0f, centre, centre);
    const auto view = [&client, camera] {
        return client.ecs.getComponent<rtype::engine::Camera>(camera).view;
    };
    rtype::game::EntityState boss{7, rtype::game::EntityKind::BydoBoss, rtype::game::NoOwner};
    boss.x = 1600.0f;
    boss.y = 540.0f;
    // The ship of the local player (slot 0), and one of another player, elsewhere
    rtype::game::EntityState ship{5, rtype::game::EntityKind::PlayerShip, 0};
    ship.x = 700.0f;
    ship.y = 300.0f;
    rtype::game::EntityState other{6, rtype::game::EntityKind::PlayerShip, 1};
    other.x = 300.0f;
    other.y = 800.0f;
    client.receive(snapshotOf(1, {boss, ship, other}));

    // Lost: the voice plays, the camera closes up where it was last drawn
    rtype::game::Snapshot voice = snapshotOf(2, {boss, other});
    voice.rescue = rtype::game::RescueStage::Voice;
    for (int frame = 0; frame < 360; ++frame) {
        client.receive(voice);
    }
    CHECK(view().fieldOfView < 0.55f * 1080.0f);
    CHECK(view().target.x == Approx(700.0f).margin(1.0f));
    CHECK(view().target.y == Approx(300.0f).margin(1.0f));
    CHECK(view().up.x == Approx(0.0f).margin(0.001));

    // The allies arrive: back to the whole game
    voice.rescue = rtype::game::RescueStage::Arrival;
    for (int frame = 0; frame < 300; ++frame) {
        client.receive(voice);
    }
    CHECK(view().fieldOfView == Approx(1080.0f).epsilon(0.001));
    CHECK(view().target.x == Approx(960.0f).epsilon(0.001));
}

TEST_CASE(
    "The camera pushes in on the shogun during the combined attack, tilted and rumbling, then pulls back",
    "[client]")
{
    ImpactClient client;
    client.ecs.registerSystem<RescueCameraSystem>(client.sessions);
    const rtype::render::Rectangle area{0.0f, 0.0f, 1920.0f, 1080.0f};
    const rtype::ecs::entity_t camera = client.ecs.createEntity();
    client.ecs.addComponent<rtype::engine::Camera>(camera, computeGameView(area));
    const rtype::render::Vector2 centre{960.0f, 540.0f};
    client.ecs.addComponent<RescueCamera>(camera, area, 1.0f, 0.0f, centre, centre);
    const auto view = [&client, camera] {
        return client.ecs.getComponent<rtype::engine::Camera>(camera).view;
    };
    rtype::game::EntityState boss{7, rtype::game::EntityKind::BydoBoss, rtype::game::NoOwner};
    boss.x = 1600.0f;
    boss.y = 540.0f;
    rtype::game::Snapshot snapshot = snapshotOf(1, {boss});
    snapshot.rescue = rtype::game::RescueStage::Combined;

    // Slowly: still far after a fifth of a second, closer than for the dashes by the end
    for (int frame = 0; frame < 12; ++frame) {
        client.receive(snapshot);
    }
    CHECK(view().fieldOfView > 0.85f * 1080.0f);
    for (int frame = 0; frame < 150; ++frame) {
        client.receive(snapshot);
    }
    CHECK(view().fieldOfView < 0.62f * 1080.0f);
    // Tilted, and it does not stop rumbling
    CHECK(std::abs(view().up.x) > 0.02f);
    const RescueCamera &rescue = client.ecs.getComponent<RescueCamera>(camera);
    CHECK(rescue.shake > 0.0f);
    const rtype::render::Vector3 before = view().target;
    client.receive(snapshot);
    CHECK((view().target.x != Approx(before.x) || view().target.y != Approx(before.y)));

    // The shogun fallen, it pulls back to the whole game, level
    snapshot = snapshotOf(2, {});
    snapshot.rescue = rtype::game::RescueStage::Finished;
    for (int frame = 0; frame < 30; ++frame) {
        client.receive(snapshot);
    }
    CHECK(view().fieldOfView < 0.8f * 1080.0f);
    for (int frame = 0; frame < 900; ++frame) {
        client.receive(snapshot);
    }
    CHECK(view().fieldOfView == Approx(1080.0f).epsilon(0.001));
    CHECK(view().up.x == Approx(0.0f).margin(0.001));
    CHECK(client.ecs.getComponent<RescueCamera>(camera).shake == 0.0f);
}

TEST_CASE("A hit writes the hit points lost above the entity, which rises, fades out and goes", "[client]")
{
    ImpactClient client;
    client.sessions.setDamageLook(rtype::game::EntityKind::BydoFlyer, YellowDamage);
    client.receive(snapshotOf(1, {bydoAt(5, 1200.0f, 400.0f, 30)}));
    client.receive(snapshotOf(2, {bydoAt(5, 1195.0f, 400.0f, 30)}));
    CHECK(client.damageNumbers().empty());

    client.receive(snapshotOf(3, {bydoAt(5, 1190.0f, 400.0f, 20)}));

    const std::vector<rtype::ecs::entity_t> numbers = client.damageNumbers();
    REQUIRE(numbers.size() == 1);
    const rtype::ecs::entity_t number = numbers[0];
    CHECK(client.ecs.getComponent<rtype::engine::SceneMember>(number).scene == client.scene);
    const rtype::engine::Text &text = client.ecs.getComponent<rtype::engine::Text>(number);
    CHECK(text.text == "10");
    CHECK(text.size == Approx(40.0f));
    CHECK(text.color.g == 220);
    const rtype::ecs::Position start = client.ecs.getComponent<rtype::ecs::Position>(number);
    CHECK(start.x == Approx(1190.0f));
    CHECK(start.y < 400.0f);

    // Rising, opaque for the first half of its time, then fading out, then gone
    client.receive(snapshotOf(4, {bydoAt(5, 1185.0f, 400.0f, 20)}));
    CHECK(client.ecs.getComponent<rtype::ecs::Position>(number).y < start.y);
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(number).value == Approx(1.0f));
    for (int frame = 0; frame < 30; ++frame) {
        client.receive(snapshotOf(static_cast<std::uint32_t>(5 + frame), {bydoAt(5, 1180.0f, 400.0f, 20)}));
    }
    REQUIRE(client.ecs.isAlive(number));
    const float fading = client.ecs.getComponent<rtype::engine::Opacity>(number).value;
    CHECK(fading < 1.0f);
    CHECK(fading > 0.0f);
    for (int frame = 0; frame < 20; ++frame) {
        client.receive(snapshotOf(static_cast<std::uint32_t>(35 + frame), {bydoAt(5, 1180.0f, 400.0f, 20)}));
    }
    CHECK(client.damageNumbers().empty());
}

TEST_CASE("A killed entity writes the hit points it had left, one removed or without a look writes none",
          "[client]")
{
    ImpactClient client;
    client.sessions.setDamageLook(rtype::game::EntityKind::BydoFlyer, YellowDamage);
    rtype::game::EntityState ship{7, rtype::game::EntityKind::PlayerShip, 0};
    ship.health = 100;
    client.receive(snapshotOf(1, {bydoAt(5, 1200.0f, 400.0f, 10), bydoAt(6, 1500.0f, 300.0f, 30), ship}));
    ship.health = 75;
    client.receive(snapshotOf(
        2, {ship}, {{5, rtype::game::DestroyCause::Killed}, {6, rtype::game::DestroyCause::Removed}}));

    const std::vector<rtype::ecs::entity_t> numbers = client.damageNumbers();
    REQUIRE(numbers.size() == 1);
    CHECK(client.ecs.getComponent<rtype::engine::Text>(numbers[0]).text == "10");
    CHECK(client.ecs.getComponent<rtype::ecs::Position>(numbers[0]).x == Approx(1200.0f));
}

TEST_CASE("A hit shows the health bar above the entity, its hit points left from the left, then it fades out",
          "[client]")
{
    ImpactClient client;
    client.sessions.setHealthBarLook(rtype::game::EntityKind::BydoFlyer, BarLook);
    client.receive(snapshotOf(1, {bydoAt(5, 1200.0f, 400.0f, 40)}));
    client.receive(snapshotOf(2, {bydoAt(5, 1195.0f, 400.0f, 40)}));
    CHECK_FALSE(client.healthBarOf(5));

    client.receive(snapshotOf(3, {bydoAt(5, 1190.0f, 400.0f, 30)}));

    REQUIRE(client.healthBarOf(5));
    const rtype::ecs::entity_t bar = *client.healthBarOf(5);
    CHECK(client.ecs.getComponent<rtype::engine::SceneMember>(bar).scene == client.scene);
    CHECK(client.ecs.getComponent<rtype::ecs::Parent>(bar).entity.id == *client.sessions.entityOf(5));
    CHECK(client.ecs.getComponent<rtype::ecs::LocalPosition>(bar).y == Approx(-42.0f));
    CHECK(client.ecs.getComponent<HealthBar>(bar).health == Approx(0.75f));
    // The gauge of the interface, without a caption, red when low
    const rtype::engine::Gauge &gauge = client.ecs.getComponent<rtype::engine::Gauge>(bar);
    CHECK(gauge.caption.empty());
    CHECK(gauge.width == Approx(100.0f));
    CHECK(gauge.low == Approx(0.3f));
    // It slides down to the hit points left
    const float sliding = gauge.get();
    CHECK(sliding < 1.0f);
    CHECK(sliding > 0.75f);
    for (int frame = 0; frame < 60; ++frame) {
        client.receive(snapshotOf(static_cast<std::uint32_t>(4 + frame), {bydoAt(5, 1190.0f, 400.0f, 30)}));
    }
    CHECK(client.ecs.getComponent<rtype::engine::Gauge>(bar).get() == Approx(0.75f));
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(bar).value == Approx(1.0f));

    // Faded out a while after the hit, shown again by the next one, the same bar
    for (int frame = 0; frame < 90; ++frame) {
        client.receive(snapshotOf(static_cast<std::uint32_t>(64 + frame), {bydoAt(5, 1190.0f, 400.0f, 30)}));
    }
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(bar).value == Approx(0.0f));
    client.receive(snapshotOf(154, {bydoAt(5, 1190.0f, 400.0f, 10)}));
    CHECK(client.healthBarOf(5) == bar);
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(bar).value == Approx(1.0f));
    CHECK(client.ecs.getComponent<HealthBar>(bar).health == Approx(0.25f));

    // It goes with its entity
    client.receive(snapshotOf(155, {}, {{5, rtype::game::DestroyCause::Killed}}));
    CHECK_FALSE(client.ecs.isAlive(bar));
}

TEST_CASE("The ships of the other players show their health bar, not the one of the local player", "[client]")
{
    ImpactClient client;
    client.sessions.setHealthBarLook(rtype::game::EntityKind::PlayerShip, BarLook);
    rtype::game::EntityState local{7, rtype::game::EntityKind::PlayerShip, 0};
    local.health = 100;
    rtype::game::EntityState ally{8, rtype::game::EntityKind::PlayerShip, 1};
    ally.health = 100;
    client.receive(snapshotOf(1, {local, ally, bydoAt(5, 1200.0f, 400.0f, 40)}));

    local.health = 60;
    ally.health = 60;
    client.receive(snapshotOf(2, {local, ally, bydoAt(5, 1200.0f, 400.0f, 20)}));

    CHECK_FALSE(client.healthBarOf(7));
    // A kind without a look shows none
    CHECK_FALSE(client.healthBarOf(5));
    REQUIRE(client.healthBarOf(8));
    CHECK(client.ecs.getComponent<HealthBar>(*client.healthBarOf(8)).health == Approx(0.6f));

    // Back to all its hit points, respawned: the bar fills up again
    ally.health = 100;
    client.receive(snapshotOf(3, {local, ally}));
    const HealthBar &health = client.ecs.getComponent<HealthBar>(*client.healthBarOf(8));
    CHECK(health.health == Approx(1.0f));
    CHECK(health.shown == Approx(1.0f));
}

TEST_CASE("The ship of the local player is drawn with the local look, the others with the look of their kind",
          "[client]")
{
    ImpactClient client;
    const ShipLook kindLook{{{1}, {0, 1, 2, 3}}, {-1.0f, -2.0f, 4.0f, 2.0f}, false, {4}};
    // The ship equipped in the hangar: another model, twice as long
    const ShipLook localLook{{{2}, {0, 1, 2, 3}}, {-2.0f, -2.0f, 8.0f, 2.0f}, false, {7}};
    client.sessions.setShipLook(rtype::game::EntityKind::PlayerShip, kindLook);
    client.sessions.setLocalShipLook(localLook);
    const rtype::game::EntityState local{7, rtype::game::EntityKind::PlayerShip, 0};
    const rtype::game::EntityState ally{8, rtype::game::EntityKind::PlayerShip, 1};
    client.receive(snapshotOf(1, {local, ally}));

    const std::optional<rtype::ecs::entity_t> localShip = client.sessions.entityOf(7);
    const std::optional<rtype::ecs::entity_t> allyShip = client.sessions.entityOf(8);
    REQUIRE(localShip);
    REQUIRE(allyShip);
    const rtype::engine::Model &localModel = client.ecs.getComponent<rtype::engine::Model>(*localShip);
    const rtype::engine::Model &allyModel = client.ecs.getComponent<rtype::engine::Model>(*allyShip);
    CHECK(localModel.model == localLook.model.model);
    CHECK(localModel.hiddenMeshes == localLook.trailMeshes);
    CHECK(allyModel.model == kindLook.model.model);
    CHECK(allyModel.hiddenMeshes == kindLook.trailMeshes);
    // Each one fitted to the box of the ship by its own hull
    CHECK(localModel.scale == Approx(allyModel.scale / 2.0f));
}

// --- Kills ---

TEST_CASE("A Bydo killed is told with the owner of the shot of a player killed nearest to it", "[client]")
{
    ImpactClient client;
    std::vector<Kill> kills;
    client.sessions.setKillHandler([&kills](const Kill &kill) { kills.push_back(kill); });
    rtype::game::EntityState far = shotAt(11, 600.0f, 400.0f);
    rtype::game::EntityState near = shotAt(12, 1190.0f, 400.0f);
    near.owner = 2;
    client.receive(snapshotOf(1, {bydoAt(5, 1200.0f, 400.0f, 10), bydoAt(6, 1500.0f, 300.0f, 30),
                                  bydoAt(7, 900.0f, 200.0f, 10), far, near}));

    client.receive(snapshotOf(2, {bydoAt(7, 900.0f, 200.0f, 10)},
                              {{5, rtype::game::DestroyCause::Killed},
                               {6, rtype::game::DestroyCause::Removed},
                               {11, rtype::game::DestroyCause::Killed},
                               {12, rtype::game::DestroyCause::Killed}}));
    client.receive(snapshotOf(3, {}, {{7, rtype::game::DestroyCause::Killed}}));

    REQUIRE(kills.size() == 2);
    CHECK(kills[0].kind == rtype::game::EntityKind::BydoFlyer);
    CHECK(kills[0].owner == rtype::game::NoOwner);
    CHECK(kills[0].killer == std::optional<std::uint8_t>{2});
    // Killed by no shot: by nobody
    CHECK_FALSE(kills[1].killer);
}

TEST_CASE("The ship of a player killed is told with its owner, and no killer", "[client]")
{
    ImpactClient client;
    std::vector<Kill> kills;
    client.sessions.setKillHandler([&kills](const Kill &kill) { kills.push_back(kill); });
    rtype::game::EntityState ship{7, rtype::game::EntityKind::PlayerShip, 3};
    client.receive(snapshotOf(1, {ship, shotAt(11, 300.0f, 200.0f)}));

    client.receive(
        snapshotOf(2, {}, {{7, rtype::game::DestroyCause::Killed}, {11, rtype::game::DestroyCause::Killed}}));

    REQUIRE(kills.size() == 1);
    CHECK(kills[0].kind == rtype::game::EntityKind::PlayerShip);
    CHECK(kills[0].owner == 3);
    CHECK_FALSE(kills[0].killer);
}

// --- Waves ---

TEST_CASE("Each wave started in the game is told with its number, once", "[client]")
{
    ImpactClient client;
    std::vector<std::uint16_t> waves;
    client.sessions.setWaveHandler(
        [&waves](std::uint16_t wave, const std::string &) { waves.push_back(wave); });
    rtype::game::Snapshot first = snapshotOf(1, {});
    first.waveStarted = 1;

    client.receive(first);
    client.receive(snapshotOf(2, {}));
    rtype::game::Snapshot second = snapshotOf(3, {});
    second.waveStarted = 2;
    client.receive(second);

    CHECK(waves == std::vector<std::uint16_t>{1, 2});
}

// --- Death screen ---

namespace
{

// Every character is half as wide as the text is high, on a screen of the shape of the game; with `apart`,
// the surfaces of a VR headset are drawn apart, each of the size of its screen in the cockpit
class GameMetrics final : public rtype::engine::Metrics
{
public:
    bool apart = false;

    [[nodiscard]] rtype::render::Vector2 measureText(rtype::render::FontId /*font*/, const std::string &text,
                                                     float size, float spacing) const override
    {
        const auto count = static_cast<float>(text.size());
        return {(0.5f * size * count) + (spacing * std::max(count - 1.0f, 0.0f)), size};
    }
    [[nodiscard]] rtype::render::Vector2 gameSize() const override { return {1920.0f, 1080.0f}; }
    [[nodiscard]] rtype::render::Rectangle visibleArea() const override
    {
        return {0.0f, 0.0f, 1920.0f, 1080.0f};
    }
    [[nodiscard]] rtype::render::Vector2 windowToGame(rtype::render::Vector2 position) const override
    {
        return position;
    }
    [[nodiscard]] bool surfacesApart() const override { return apart; }
};

const DeathScreenLook DeathLook{{1}, {2}, {}, {}, "LIVES LEFT", "RESPAWN"};

/// The engine of the client playing a scripted session, with its death screens.
struct DeathClient : ImpactClient {
    GameMetrics metrics;

    DeathClient() { ecs.registerSystem<DeathScreenSystem>(ecs, sessions); }

    /// Opens the death screen of a ship lost at the tick of the latest snapshot.
    rtype::ecs::entity_t open(std::uint8_t lives, std::uint32_t respawnDelay)
    {
        return openDeathScreen(ecs, scene, metrics, rtype::engine::ToastStyle{}, DeathLook,
                               {lives, 3, sessions.tick() + respawnDelay, respawnDelay});
    }

    /// Frames of one tick each, without snapshot: the game is paused, or the network late.
    void wait(float seconds)
    {
        for (float time = 0.0f; time < seconds; time += Tick) {
            engine.update(Tick);
        }
    }

    [[nodiscard]] float opacityOf(rtype::ecs::entity_t entity)
    {
        return ecs.getComponent<rtype::engine::Opacity>(entity).value;
    }

    /// The pictures of the lives drawn with a texture.
    [[nodiscard]] std::size_t countLives(rtype::render::TextureId texture)
    {
        std::size_t count = 0;
        auto &sprites = ecs.cache<rtype::engine::Sprite>();
        for (rtype::ecs::entity_t entity = 0; entity < sprites.size(); ++entity) {
            if (sprites.has(entity) && ecs.isAlive(entity) && sprites.get(entity).texture == texture) {
                ++count;
            }
        }
        return count;
    }
};

} // namespace

TEST_CASE("The death screen counts down the seconds to the next ship in its ring, then goes once it is there",
          "[client]")
{
    DeathClient client;
    client.receive(snapshotOf(1, {}));
    const rtype::ecs::entity_t screen = client.open(2, 180);
    const DeathScreen parts = client.ecs.getComponent<DeathScreen>(screen);

    client.receive(snapshotOf(2, {}));
    CHECK(client.ecs.getComponent<rtype::engine::Text>(parts.countdown).text == "3");
    CHECK(client.ecs.getComponent<rtype::engine::Ring>(parts.ring).value == Approx(179.0f / 180.0f));
    // A new second pops
    CHECK(client.ecs.hasComponent<rtype::engine::Tween>(parts.countdown));

    // The countdown follows the game: it waits for the snapshots
    client.wait(2.0f);
    CHECK(client.ecs.getComponent<rtype::engine::Text>(parts.countdown).text == "3");

    client.receive(snapshotOf(121, {}));
    CHECK(client.ecs.getComponent<rtype::engine::Text>(parts.countdown).text == "1");
    CHECK(client.ecs.getComponent<rtype::engine::Ring>(parts.ring).value == Approx(1.0f / 3.0f));

    client.receive(snapshotOf(181, {}));
    CHECK(client.ecs.isAlive(screen));
    client.wait(DeathScreenSystem::FadeOut + Tick);
    CHECK_FALSE(client.ecs.isAlive(screen));
    CHECK_FALSE(client.ecs.isAlive(parts.veil));
    CHECK_FALSE(client.ecs.isAlive(parts.countdown));
    CHECK_FALSE(client.ecs.isAlive(parts.lostLife));
}

TEST_CASE("The death screen shows the lives left lit, and puts out the one just lost after its entrance",
          "[client]")
{
    DeathClient client;
    client.receive(snapshotOf(1, {}));
    const rtype::ecs::entity_t screen = client.open(1, 600);
    const DeathScreen parts = client.ecs.getComponent<DeathScreen>(screen);
    client.receive(snapshotOf(2, {}));

    // The life left, and the life just lost over its picture of a life lost; then the other life lost
    CHECK(client.countLives(DeathLook.life) == 2);
    CHECK(client.countLives(DeathLook.lostLife) == 2);
    // The veil darkens the game at once, the rest waits for the band of the banner
    CHECK(client.opacityOf(parts.veil) > 0.0f);
    CHECK(client.opacityOf(parts.countdown) == 0.0f);

    client.wait(DeathScreenSystem::ContentDelay + DeathScreenSystem::ContentIn);
    CHECK(client.opacityOf(parts.veil) == Approx(1.0f));
    CHECK(client.opacityOf(parts.countdown) == Approx(1.0f));
    CHECK(client.opacityOf(parts.lostLife) == Approx(1.0f));

    client.wait(DeathScreenSystem::LifeOutDelay + DeathScreenSystem::LifeOut);
    CHECK(client.opacityOf(parts.lostLife) == Approx(0.0f));
    CHECK(client.opacityOf(parts.countdown) == Approx(1.0f));
}

// --- Results ---

namespace
{

const ResultsLook ResultsCaptions{
    .captionFont = {},
    .valueFont = {},
    .sounds = {.gameOver = {1}, .stat = {2}, .tick = {3}, .counted = {4}, .levelUp = {5}},
    .title = "THE R-TYPE SECTION HAS FALLEN",
    .waves = "waves cleared",
    .kills = "Bydos shot down",
    .streak = "best streak",
    .score = "score",
    .xp = "experience earned",
    .level = "LEVEL",
    .levelUp = "level up",
    .guest = "SIGN IN TO KEEP THE EXPERIENCE YOU EARN"};

/// The engine of the client with its results screens.
struct ResultsClient {
    rtype::engine::Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    rtype::engine::SceneId scene = engine.getScenes().open({});
    GameMetrics metrics;

    ResultsClient() { ecs.registerSystem<ResultsSystem>(ecs); }

    rtype::ecs::entity_t open(std::optional<std::uint64_t> accountXp)
    {
        return openResultsScreen(ecs, scene, metrics, ResultsCaptions,
                                 {.wavesCleared = 7, .kills = 42, .bestStreak = 5, .score = 12300, .xp = 820},
                                 accountXp);
    }

    /// Frames of one tick each, up to `seconds` after the screen opened.
    void waitUntil(rtype::ecs::entity_t screen, float seconds)
    {
        while (ecs.getComponent<ResultsScreen>(screen).time < seconds) {
            engine.update(Tick);
        }
    }

    [[nodiscard]] const std::string &textOf(rtype::ecs::entity_t entity)
    {
        return ecs.getComponent<rtype::engine::Text>(entity).text;
    }

    [[nodiscard]] float opacityOf(rtype::ecs::entity_t entity)
    {
        return ecs.getComponent<rtype::engine::Opacity>(entity).value;
    }

    /// The sound given to an entity to play, if any.
    [[nodiscard]] std::optional<rtype::audio::SoundId> soundOf(rtype::ecs::entity_t entity)
    {
        if (!ecs.hasComponent<rtype::engine::SoundEffect>(entity)) {
            return std::nullopt;
        }
        return ecs.getComponent<rtype::engine::SoundEffect>(entity).sound;
    }
};

} // namespace

TEST_CASE("The results land the figures of the game one after the other, each with its sound", "[client]")
{
    ResultsClient client;
    const rtype::ecs::entity_t screen = client.open(400);
    const ResultsScreen parts = client.ecs.getComponent<ResultsScreen>(screen);
    REQUIRE(parts.figures.size() == 4);
    CHECK(client.textOf(parts.figures[0]) == "7");
    CHECK(client.textOf(parts.figures[1]) == "42");
    CHECK(client.textOf(parts.figures[2]) == "x5");
    CHECK(client.textOf(parts.figures[3]) == "0012300");
    CHECK(client.textOf(parts.captions[0]) == "WAVES CLEARED");

    client.engine.update(Tick);
    // The game over as it opens, nothing landed yet
    CHECK(client.soundOf(parts.title) == ResultsCaptions.sounds.gameOver);
    CHECK(client.opacityOf(parts.figures[0]) == 0.0f);

    client.waitUntil(screen, ResultsSystem::FiguresDelay);
    CHECK(client.soundOf(parts.figures[0]) == ResultsCaptions.sounds.stat);
    CHECK(client.ecs.hasComponent<rtype::engine::Tween>(parts.figures[0]));
    CHECK_FALSE(client.soundOf(parts.figures[1]));

    client.waitUntil(screen, ResultsSystem::FiguresDelay + (3 * ResultsSystem::FiguresGap));
    CHECK(client.soundOf(parts.figures[3]) == ResultsCaptions.sounds.stat);
    CHECK(client.opacityOf(parts.figures[0]) == Approx(1.0f));
    // The experience waits for the figures
    CHECK(client.opacityOf(parts.xp) == 0.0f);
}

TEST_CASE("The results count the experience up while the bar of the level fills, LEVEL UP as it goes up",
          "[client]")
{
    ResultsClient client;
    // Level 1, 400 of its 500
    const rtype::ecs::entity_t screen = client.open(400);
    const ResultsScreen parts = client.ecs.getComponent<ResultsScreen>(screen);

    // Shown as it fades in, before it counts
    client.waitUntil(screen, ResultsSystem::CountDelay - Tick);
    CHECK(client.textOf(parts.level) == "LEVEL 1");
    CHECK(client.textOf(parts.levelXp) == "400 / 500 XP");
    CHECK(client.ecs.getComponent<rtype::engine::Box>(parts.barFill).width ==
          Approx(ResultsBarWidth * 400.0f / 500.0f));
    CHECK(client.opacityOf(parts.levelUp) == 0.0f);

    client.waitUntil(screen, ResultsSystem::CountDelay + (ResultsSystem::CountDuration / 2.0f));
    CHECK(client.soundOf(parts.xp) == ResultsCaptions.sounds.tick);
    // 1220 in all: level 2 from 500, 720 of its 1414
    client.waitUntil(screen, ResultsSystem::CountDelay + ResultsSystem::CountDuration);
    client.engine.update(Tick);
    CHECK(client.textOf(parts.xp) == "+820 XP");
    CHECK(client.textOf(parts.level) == "LEVEL 2");
    CHECK(client.textOf(parts.levelXp) == "720 / 1414 XP");
    CHECK(client.opacityOf(parts.levelUp) == 1.0f);
    CHECK(client.soundOf(parts.levelUp) == ResultsCaptions.sounds.levelUp);
    CHECK(client.soundOf(parts.xpCaption) == ResultsCaptions.sounds.counted);
}

TEST_CASE("The results fill the bar up to the experience the master tells", "[client]")
{
    ResultsClient client;
    const rtype::ecs::entity_t screen = client.open(400);
    // The account earned more than the game showed: 2000 in all, level 3 from 1914
    setAccountXp(client.ecs.getComponent<ResultsScreen>(screen), 2000);

    client.waitUntil(screen, ResultsSystem::CountDelay + ResultsSystem::CountDuration);
    client.engine.update(Tick);
    const ResultsScreen parts = client.ecs.getComponent<ResultsScreen>(screen);
    CHECK(client.textOf(parts.xp) == "+820 XP");
    CHECK(client.textOf(parts.level) == "LEVEL 3");
    CHECK(client.textOf(parts.levelXp) == "86 / 2598 XP");
    CHECK(parts.levelShown == 3);
}

TEST_CASE("The results of a guest show what it earned and that it keeps nothing", "[client]")
{
    ResultsClient client;
    const rtype::ecs::entity_t screen = client.open(std::nullopt);
    const ResultsScreen parts = client.ecs.getComponent<ResultsScreen>(screen);
    REQUIRE(parts.guest);
    CHECK(parts.experience.size() == 2);

    client.waitUntil(screen, ResultsSystem::CountDelay + ResultsSystem::CountDuration - Tick);
    CHECK(client.opacityOf(*parts.guest) == 0.0f);
    client.waitUntil(screen,
                     ResultsSystem::CountDelay + ResultsSystem::CountDuration + ResultsSystem::TitleIn);
    client.engine.update(Tick);
    CHECK(client.textOf(parts.xp) == "+820 XP");
    CHECK(client.opacityOf(*parts.guest) == Approx(1.0f));
}

// --- HUD ---

namespace
{

const HudLook HudPictures{{1}, {2}, {}, {{{10}, {11}, {12}, {13}, {14}}}};

/// The engine of the client playing a scripted session, with its HUD.
struct HudClient : ImpactClient {
    GameMetrics metrics;
    rtype::ecs::entity_t entity;

    HudClient()
    {
        ecs.registerSystem<HudSystem>(sessions, metrics);
        entity = openHud(ecs, scene, HudPictures, 3);
    }

    [[nodiscard]] const Hud &hud() { return ecs.getComponent<Hud>(entity); }

    [[nodiscard]] const std::string &textOf(rtype::ecs::entity_t part)
    {
        return ecs.getComponent<rtype::engine::Text>(part).text;
    }

    /// The texture of an icon of the voice, if it shows.
    [[nodiscard]] std::optional<rtype::render::TextureId> iconOf(rtype::ecs::entity_t part)
    {
        if (ecs.getComponent<rtype::engine::Opacity>(part).value == 0.0f) {
            return std::nullopt;
        }
        return ecs.getComponent<rtype::engine::Sprite>(part).texture;
    }

    /// The pictures of the lives of the HUD drawn with a texture.
    [[nodiscard]] std::size_t countLives(rtype::render::TextureId texture)
    {
        return static_cast<std::size_t>(std::ranges::count_if(hud().lives, [&](rtype::ecs::entity_t life) {
            return ecs.getComponent<rtype::engine::Sprite>(life).texture == texture;
        }));
    }

    /// The signal bars shown in a colour.
    [[nodiscard]] std::size_t countBars(rtype::render::Color color)
    {
        return static_cast<std::size_t>(std::ranges::count_if(hud().signal, [&](rtype::ecs::entity_t bar) {
            const rtype::render::Color shown = ecs.getComponent<rtype::engine::Box>(bar).color;
            return ecs.getComponent<rtype::engine::Opacity>(bar).value > 0.0f && shown.r == color.r &&
                   shown.g == color.g && shown.b == color.b;
        }));
    }
};

rtype::game::Snapshot snapshotWith(std::uint32_t tick, std::vector<rtype::game::PlayerState> players)
{
    rtype::game::Snapshot snapshot = snapshotOf(tick, {});
    snapshot.players = std::move(players);
    return snapshot;
}

} // namespace

TEST_CASE("The HUD shows the lives left of the local player lit, its slot and its score", "[client]")
{
    HudClient client;
    client.engine.update(Tick);
    // Before the first snapshot, the player has all its lives
    CHECK(client.countLives(HudPictures.life) == 3);
    CHECK(client.textOf(client.hud().score) == "0000000");

    client.receive(snapshotWith(1, {{0, 2, 24500}, {1, 3, 98100}}));
    CHECK(client.countLives(HudPictures.life) == 2);
    CHECK(client.countLives(HudPictures.lostLife) == 1);
    CHECK(client.textOf(client.hud().pilot) == "1P");
    CHECK(client.textOf(client.hud().score) == "0024500");
    // The shadows go across the screen
    CHECK(client.ecs.getComponent<rtype::engine::Box>(client.hud().topShadow).width == Approx(1920.0f));
}

TEST_CASE("The HUD lists the score of each other player, the first line on top", "[client]")
{
    HudClient client;
    client.receive(snapshotWith(1, {{0, 3, 100}, {1, 3, 98100}, {3, 1, 71300}}));
    const Hud &hud = client.hud();
    CHECK(client.textOf(hud.allies[0].slot) == "P2");
    CHECK(client.textOf(hud.allies[0].score) == "0098100");
    CHECK(client.textOf(hud.allies[1].slot) == "P4");
    CHECK(client.textOf(hud.allies[1].score) == "0071300");
    CHECK(client.textOf(hud.allies[2].slot).empty());
    // Offline, the players have no name
    CHECK(client.textOf(hud.allies[0].name).empty());
    const auto &anchors = client.ecs.cache<rtype::engine::Anchor>();
    CHECK(anchors.get(hud.allies[0].score).y < anchors.get(hud.allies[1].score).y);
    // The slot is left of the score
    CHECK(anchors.get(hud.allies[0].slot).x < anchors.get(hud.allies[0].score).x);

    // A player who leaves the game leaves the HUD
    client.receive(snapshotWith(2, {{0, 3, 100}, {3, 1, 71300}}));
    CHECK(client.textOf(hud.allies[0].slot) == "P4");
    CHECK(client.textOf(hud.allies[1].slot).empty());
}

TEST_CASE("The HUD shows the ping with its signal bars online, and nothing offline", "[client]")
{
    HudClient client;
    client.receive(snapshotWith(1, {{0, 3, 0}}));
    CHECK(client.textOf(client.hud().ping).empty());
    CHECK(client.countBars(rtype::render::colors::White) == 0);

    client.session->rtt = std::chrono::milliseconds(28);
    client.engine.update(Tick);
    CHECK(client.textOf(client.hud().ping) == "28 MS");
    CHECK(client.countBars(rtype::render::colors::White) == 3);
    CHECK(client.countBars({58, 62, 67, 255}) == 1);
}

TEST_CASE("The signal bars light up as the ping gets shorter", "[client]")
{
    using std::chrono::milliseconds;
    CHECK(countSignalBars(milliseconds(10)) == 4);
    CHECK(countSignalBars(HudSystem::FourBars) == 3);
    CHECK(countSignalBars(milliseconds(90)) == 2);
    CHECK(countSignalBars(milliseconds(500)) == 1);
    CHECK(formatScore(42) == "0000042");
}

TEST_CASE("The parts of the HUD are on the screens of the cockpit that show them", "[client][vr]")
{
    using rtype::engine::OnSurface;
    using rtype::engine::Surface;
    HudClient client;
    const Hud &hud = client.hud();
    const auto surfaceOf = [&](rtype::ecs::entity_t part) {
        return client.ecs.getComponent<OnSurface>(part).surface;
    };
    CHECK(surfaceOf(hud.score) == Surface::HudShip);
    CHECK(surfaceOf(hud.lives[0]) == Surface::HudShip);
    CHECK(surfaceOf(hud.pilotVoice) == Surface::HudShip);
    CHECK(surfaceOf(hud.ping) == Surface::HudNav);
    CHECK(surfaceOf(hud.signal[0]) == Surface::HudNav);
    CHECK(surfaceOf(hud.wave) == Surface::HudNav);
    CHECK(surfaceOf(hud.allies[0].name) == Surface::HudAux);
    CHECK(surfaceOf(hud.pilots) == Surface::HudAux);
    CHECK(surfaceOf(hud.radarFrame) == Surface::Holo);
    CHECK(surfaceOf(hud.radarBlips[0]) == Surface::Holo);
}

TEST_CASE("On the screen, the HUD keeps its look and shows nothing of the cockpit", "[client][vr]")
{
    HudClient client;
    client.receive(snapshotWith(1, {{0, 3, 24500}}));
    const Hud &hud = client.hud();
    CHECK(client.ecs.getComponent<rtype::engine::Text>(hud.score).size == Approx(ScreenHud.score.size));
    CHECK(client.ecs.getComponent<rtype::engine::Anchor>(hud.score).point ==
          rtype::engine::AnchorPoint::BottomLeft);
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(hud.topShadow).value == Approx(1.0f));
    for (const rtype::ecs::entity_t part :
         {hud.waveLabel, hud.wave, hud.boss, hud.mode, hud.pilots, hud.radarTitle}) {
        CHECK(client.textOf(part).empty());
    }
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(hud.radarFrame).value == Approx(0.0f));
}

TEST_CASE("In the cockpit, the HUD is larger, placed on its screens, with the wave and the kind of game",
          "[client][vr]")
{
    HudClient client;
    client.metrics.apart = true;
    client.receive(snapshotWith(1, {{0, 3, 24500}}));
    const Hud &hud = client.hud();
    const auto &anchors = client.ecs.cache<rtype::engine::Anchor>();
    CHECK(client.ecs.getComponent<rtype::engine::Text>(hud.score).size == Approx(CockpitHud.score.size));
    CHECK(anchors.get(hud.score).point == rtype::engine::AnchorPoint::TopLeft);
    // From the top of the ship screen: the pilot, then the score, then the hit points
    CHECK(anchors.get(hud.pilot).y < anchors.get(hud.score).y);
    CHECK(anchors.get(hud.score).y < anchors.get(hud.health).y);
    CHECK(client.ecs.getComponent<rtype::engine::Sprite>(hud.lives[0]).width == Approx(CockpitHud.life.x));
    // The shadows belong to the screen
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(hud.topShadow).value == Approx(0.0f));
    // Offline, before the first wave, alone
    CHECK(client.textOf(hud.waveLabel) == "WAVE");
    CHECK(client.textOf(hud.wave) == "00");
    CHECK(client.textOf(hud.mode) == "SOLO");
    CHECK(client.textOf(hud.boss).empty());
    CHECK(client.textOf(hud.pilots) == "FLYING SOLO");

    // Back on the screen, the HUD looks as before
    client.metrics.apart = false;
    client.engine.update(Tick);
    CHECK(client.ecs.getComponent<rtype::engine::Text>(hud.score).size == Approx(ScreenHud.score.size));
    CHECK(anchors.get(hud.score).point == rtype::engine::AnchorPoint::BottomLeft);
    CHECK(client.textOf(hud.wave).empty());
}

TEST_CASE("In the cockpit, the screens of the sides list the other pilots under their title", "[client][vr]")
{
    HudClient client;
    client.metrics.apart = true;
    client.receive(snapshotWith(1, {{0, 3, 100}, {1, 3, 98100}, {3, 1, 71300}}));
    const Hud &hud = client.hud();
    const auto &anchors = client.ecs.cache<rtype::engine::Anchor>();
    CHECK(client.textOf(hud.pilots) == "PILOTS");
    CHECK(anchors.get(hud.pilots).y < anchors.get(hud.allies[0].slot).y);
    CHECK(anchors.get(hud.allies[0].slot).y < anchors.get(hud.allies[1].slot).y);
    CHECK(anchors.get(hud.allies[0].score).point == rtype::engine::AnchorPoint::TopRight);
}

TEST_CASE("The radar of the hologram shows the ships and the Bydos, not the shots, and warns of the boss",
          "[client][vr]")
{
    HudClient client;
    client.metrics.apart = true;
    rtype::game::Snapshot snapshot = snapshotWith(1, {{0, 3, 0}});
    snapshot.entities = {{1, rtype::game::EntityKind::PlayerShip, 0, 100.0f, 540.0f},
                         {2, rtype::game::EntityKind::BydoFlyer, rtype::game::NoOwner, 1500.0f, 300.0f},
                         {3, rtype::game::EntityKind::PlayerShot, 0, 400.0f, 540.0f},
                         {4, rtype::game::EntityKind::BydoBoss, rtype::game::NoOwner, 1700.0f, 540.0f}};
    client.receive(std::move(snapshot));
    const Hud &hud = client.hud();
    const auto shown = std::ranges::count_if(hud.radarBlips, [&](rtype::ecs::entity_t blip) {
        return client.ecs.getComponent<rtype::engine::Opacity>(blip).value > 0.0f;
    });
    CHECK(shown == 3);
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(hud.radarFrame).value == Approx(1.0f));
    CHECK(client.textOf(hud.boss) == "BOSS APPROACHING");

    // On the screen, no radar
    client.metrics.apart = false;
    client.engine.update(Tick);
    CHECK(std::ranges::none_of(hud.radarBlips, [&](rtype::ecs::entity_t blip) {
        return client.ecs.getComponent<rtype::engine::Opacity>(blip).value > 0.0f;
    }));
}

TEST_CASE("A blip of the radar is where its entity is in the frame of the board, by its kind", "[client][vr]")
{
    using rtype::game::EntityKind;
    const rtype::render::Vector2 game{1920.0f, 1080.0f};
    const auto pilot = radarBlip(EntityKind::PlayerShip, 0, 0, {0.0f, 0.0f}, game);
    REQUIRE(pilot.has_value());
    CHECK(pilot->centre.x == Approx(RadarFrame.x));
    CHECK(pilot->centre.y == Approx(RadarFrame.y));
    const auto other = radarBlip(EntityKind::PlayerShip, 1, 0, {1920.0f, 1080.0f}, game);
    REQUIRE(other.has_value());
    CHECK(other->centre.x == Approx(RadarFrame.x + RadarFrame.width));
    CHECK(other->centre.y == Approx(RadarFrame.y + RadarFrame.height));
    // The pilot stands out from the others
    CHECK(pilot->size > other->size);
    CHECK((pilot->color.r != other->color.r || pilot->color.g != other->color.g));
    // An entity coming in stays on the edge of the frame
    const auto coming = radarBlip(EntityKind::BydoFlyer, rtype::game::NoOwner, 0, {2100.0f, 540.0f}, game);
    REQUIRE(coming.has_value());
    CHECK(coming->centre.x == Approx(RadarFrame.x + RadarFrame.width));
    const auto boss = radarBlip(EntityKind::BydoBoss, rtype::game::NoOwner, 0, {960.0f, 540.0f}, game);
    REQUIRE(boss.has_value());
    CHECK(boss->outlined);
    CHECK_FALSE(radarBlip(EntityKind::PlayerShot, 0, 0, {960.0f, 540.0f}, game).has_value());
    CHECK_FALSE(
        radarBlip(EntityKind::BydoPlasmaShot, rtype::game::NoOwner, 0, {960.0f, 540.0f}, game).has_value());
    // The frame of the board is inside the hologram, of the shape of the game
    CHECK(RadarFrame.x + RadarFrame.width <= RadarSurface.x);
    CHECK(RadarFrame.y + RadarFrame.height <= RadarSurface.y);
    CHECK(RadarFrame.width / RadarFrame.height == Approx(game.x / game.y).epsilon(0.01));
}

TEST_CASE("The navigation screen writes the wave with the number of waves, or alone", "[client][vr]")
{
    CHECK(formatWave(3, 8) == "03 / 08");
    CHECK(formatWave(12, std::nullopt) == "12");
}

TEST_CASE("The recorder counts what happens in the games in the stats", "[client][quest]")
{
    rtype::engine::Engine engine;
    rtype::client::GameRecorder recorder(engine);
    const rtype::engine::Settings &settings = engine.getSettings();
    using rtype::game::EntityKind;
    constexpr std::uint8_t local = 1;

    recorder.recordStart();
    recorder.recordKill({EntityKind::BydoFlyer, rtype::game::NoOwner, local}, local);
    recorder.recordKill({EntityKind::BydoTank, rtype::game::NoOwner, 2}, local);
    recorder.recordKill({EntityKind::BydoBoss, rtype::game::NoOwner, std::nullopt}, local);
    recorder.recordKill({EntityKind::PlayerShip, local, std::nullopt}, local);
    recorder.recordKill({EntityKind::PlayerShip, 2, std::nullopt}, local);
    recorder.recordWave(4);
    recorder.recordWave(2);
    recorder.recordOutcome(rtype::game::GameOutcome::Victory);
    recorder.recordEnd(1200);
    recorder.recordEnd(800);

    CHECK(rtype::client::getStat(settings, rtype::client::GamesPlayedStat) == 1);
    CHECK(rtype::client::getStat(settings, rtype::client::BydosKilledStat) == 1);
    CHECK(rtype::client::getStat(settings, rtype::client::FlyersKilledStat) == 1);
    CHECK(rtype::client::getStat(settings, rtype::client::TanksKilledStat) == 0);
    CHECK(rtype::client::getStat(settings, rtype::client::BossesDefeatedStat) == 1);
    CHECK(rtype::client::getStat(settings, rtype::client::DeathsStat) == 1);
    CHECK(rtype::client::getStat(settings, rtype::client::BestWaveStat) == 4);
    CHECK(rtype::client::getStat(settings, rtype::client::VictoriesStat) == 1);
    CHECK(rtype::client::getStat(settings, rtype::client::DefeatsStat) == 0);
    CHECK(rtype::client::getStat(settings, rtype::client::BestScoreStat) == 1200);
    CHECK(rtype::client::getStat(settings, rtype::client::TotalScoreStat) == 2000);
}

TEST_CASE("A stat quest counts every kill, even several in a frame", "[client][quest]")
{
    rtype::engine::Engine engine;
    rtype::client::GameRecorder recorder(engine);
    auto &quest = engine.getEcs().registerSystem<rtype::client::StatQuest>(
        "triple", rtype::client::BydoKilledEvent, rtype::client::BydosKilledStat, 3, engine.getSettings());
    int completions = 0;
    quest.setCompletionHandler([&completions](const rtype::engine::QuestSystem &) { ++completions; });
    engine.addQuest(quest);
    const rtype::client::Kill kill{rtype::game::EntityKind::BydoFlyer, rtype::game::NoOwner, 0};

    recorder.recordKill(kill, 0);
    recorder.recordKill(kill, 0);
    engine.update(0.016f);
    CHECK_FALSE(quest.isCompleted());

    recorder.recordKill(kill, 0);
    engine.update(0.016f);
    CHECK(quest.isCompleted());
    CHECK(completions == 1);
}

// --- The voice in the HUD ---

TEST_CASE("Offline, the HUD shows no voice and no name", "[client][voice]")
{
    HudClient client;
    client.receive(snapshotWith(1, {{0, 3, 100}, {1, 3, 98100}}));
    CHECK(client.textOf(client.hud().pilot) == "1P");
    CHECK_FALSE(client.iconOf(client.hud().pilotVoice));
    CHECK_FALSE(client.iconOf(client.hud().allies[0].voice));
}

TEST_CASE("Online, the HUD names the pilots and shows the icon of their voice", "[client][voice]")
{
    HudClient client;
    client.session->names = {{0, "KAZE"}, {1, "VORTEX"}, {2, "NOVA"}, {3, "ORION"}};
    rtype::voice::Participant vortex{.slot = 1, .speaking = true};
    rtype::voice::Participant nova{.slot = 2, .mutedByMe = true};
    rtype::voice::Participant orion{.slot = 3, .status = {.noMicrophone = true}};
    client.session->voice = VoiceView{.local = {.muted = true}, .others = {vortex, nova, orion}};
    client.receive(snapshotWith(1, {{0, 3, 100}, {1, 3, 98100}, {2, 3, 71300}, {3, 3, 42700}}));

    const Hud &hud = client.hud();
    CHECK(client.textOf(hud.pilot) == "1P · KAZE");
    CHECK(client.iconOf(hud.pilotVoice) == HudPictures.voice[1]);
    CHECK(client.textOf(hud.allies[0].name) == "VORTEX");
    CHECK(client.iconOf(hud.allies[0].voice) == HudPictures.voice[0]);
    CHECK(client.iconOf(hud.allies[1].voice) == HudPictures.voice[2]);
    CHECK(client.iconOf(hud.allies[2].voice) == HudPictures.voice[3]);
    // The name of the pilot who talks lights up
    const auto &texts = client.ecs.cache<rtype::engine::Text>();
    CHECK(texts.get(hud.allies[0].name).color.r == 255);
    CHECK(texts.get(hud.allies[1].name).color.r < 255);
    // From the right: the score, the name, the slot, the icon
    const auto &anchors = client.ecs.cache<rtype::engine::Anchor>();
    CHECK(anchors.get(hud.allies[0].name).x < anchors.get(hud.allies[0].score).x);
    CHECK(anchors.get(hud.allies[0].slot).x < anchors.get(hud.allies[0].name).x);
    CHECK(anchors.get(hud.allies[0].voice).x < anchors.get(hud.allies[0].slot).x);

    // VORTEX stops talking
    client.session->voice->others[0].speaking = false;
    client.engine.update(Tick);
    CHECK_FALSE(client.iconOf(hud.allies[0].voice));
    CHECK(texts.get(hud.allies[0].name).color.r < 255);
}

TEST_CASE("The ship of another player shows a badge while that player talks", "[client][voice]")
{
    HudClient client;
    const VoiceBadgeLook look{
        .texture = {20}, .source = {0.0f, 0.0f, 64.0f, 64.0f}, .size = 30.0f, .x = 80.0f, .y = -60.0f};
    client.sessions.setVoiceBadgeLook(look);
    client.session->voice = VoiceView{.local = {}, .others = {rtype::voice::Participant{.slot = 1}}};
    const rtype::game::EntityState mine{1, rtype::game::EntityKind::PlayerShip, 0};
    const rtype::game::EntityState theirs{2, rtype::game::EntityKind::PlayerShip, 1};
    client.receive(snapshotOf(1, {mine, theirs}));
    const auto badgeOf = [&](std::uint32_t id) {
        const rtype::ecs::entity_t ship = *client.sessions.entityOf(id);
        return client.ecs.getComponent<Replicated>(ship).voiceBadge;
    };
    // Silent: no badge yet
    CHECK_FALSE(badgeOf(2));

    client.session->voice->others[0].speaking = true;
    for (int frame = 0; frame < 30; ++frame) {
        client.engine.update(Tick);
    }
    REQUIRE(badgeOf(2));
    CHECK_FALSE(badgeOf(1));
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(*badgeOf(2)).value == Approx(1.0f));
    CHECK(client.ecs.getComponent<rtype::engine::Sprite>(*badgeOf(2)).texture == look.texture);

    // Muted by me: it is not heard, the badge fades out
    client.session->voice->others[0].mutedByMe = true;
    for (int frame = 0; frame < 30; ++frame) {
        client.engine.update(Tick);
    }
    CHECK(client.ecs.getComponent<rtype::engine::Opacity>(*badgeOf(2)).value == Approx(0.0f));
}

TEST_CASE("The icon of a pilot says what its voice does", "[client][voice]")
{
    using rtype::voice::Participant;
    CHECK(voiceIconOf(Participant{}) == VoiceIcon::None);
    CHECK(voiceIconOf(Participant{.speaking = true}) == VoiceIcon::Speaking);
    CHECK(voiceIconOf(Participant{.status = {.muted = true}}) == VoiceIcon::Muted);
    // Deafened is muted too: the others see it muted
    CHECK(voiceIconOf(Participant{.status = {.deafened = true}}) == VoiceIcon::Muted);
    CHECK(voiceIconOf(Participant{.status = {.noMicrophone = true}}) == VoiceIcon::NoMicrophone);
    // My choice first
    CHECK(voiceIconOf(Participant{.speaking = true, .mutedByMe = true}) == VoiceIcon::MutedByMe);
    // Before it, an administrator, as nothing else lifts it
    CHECK(voiceIconOf(Participant{.status = {.muted = true, .mutedByAdmin = true}, .mutedByMe = true}) ==
          VoiceIcon::MutedByAdmin);

    CHECK(voiceIconOf(LocalVoice{}) == VoiceIcon::None);
    CHECK(voiceIconOf(LocalVoice{.transmitting = true}) == VoiceIcon::Speaking);
    CHECK(voiceIconOf(LocalVoice{.muted = true}) == VoiceIcon::Muted);
    CHECK(voiceIconOf(LocalVoice{.muted = true, .deafened = true}) == VoiceIcon::Deafened);
    CHECK(voiceIconOf(LocalVoice{.muted = true, .deafened = true, .mutedByAdmin = true}) ==
          VoiceIcon::MutedByAdmin);

    CHECK(voiceIconColor(VoiceIcon::Speaking).r == 255);
    CHECK(voiceIconColor(VoiceIcon::Muted).r < 255);
    // Red, as in the design
    CHECK(voiceIconColor(VoiceIcon::MutedByAdmin).r == 232);
    CHECK(voiceIconColor(VoiceIcon::MutedByAdmin).g == 114);
}

TEST_CASE("The session tells the full health of the ship of the player, where the ships are, and no boss",
          "[client]")
{
    Client client({}, armedConfig());

    client.run(3);

    REQUIRE(client.sessions.localHealth() > 0);
    CHECK(client.sessions.localMaxHealth() == client.sessions.localHealth());
    CHECK_FALSE(client.sessions.bossIsThere());
    const auto &ships = client.sessions.ships();
    REQUIRE(ships.size() == 1);
    CHECK(ships.front().first == 0);
    CHECK(ships.front().second < rtype::game::GameWidth / 2.0f);

    client.sessions.stop();
    CHECK(client.sessions.localMaxHealth() == 0);
    CHECK(client.sessions.ships().empty());
}
