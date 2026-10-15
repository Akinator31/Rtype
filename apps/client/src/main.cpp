#include "Controls.hpp"
#include "Menus.hpp"
#include "rtype/audio/AudioDevice.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Engine.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/physics/Physics.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/render/Window.hpp"

#include <cmath>
#include <exception>
#include <string>

namespace
{

struct PlayerControlled {};

class PlayerInputSystem : public rtype::ecs::System
{
    rtype::ecs::ComponentCache<PlayerControlled> &_players;
    rtype::ecs::ComponentCache<rtype::physics::Velocity> &_velocities;
    rtype::engine::Scenes &_scenes;
    rtype::input::ActionMap<rtype::client::Action> &_controls;
    float _speed;

public:
    PlayerInputSystem(rtype::ecs::ComponentCaches &caches, rtype::engine::Scenes &scenes,
                      rtype::input::ActionMap<rtype::client::Action> &controls, float speed)
        : _players(use<PlayerControlled>(caches)), _velocities(use<rtype::physics::Velocity>(caches)),
          _scenes(scenes), _controls(controls), _speed(speed)
    {
    }

    void update(float) override
    {
        for (rtype::ecs::entity_t entity = 0; entity < _players.size(); ++entity) {
            if (!_players.has(entity) || !_velocities.has(entity)) {
                continue;
            }

            rtype::physics::Velocity &velocity = _velocities.get(entity);
            if (!_scenes.receivesInput(entity)) {
                velocity = {};
                continue;
            }

            float dx = _controls.strength(rtype::client::Action::MoveRight) -
                       _controls.strength(rtype::client::Action::MoveLeft);
            float dy = _controls.strength(rtype::client::Action::MoveDown) -
                       _controls.strength(rtype::client::Action::MoveUp);
            const float length = std::hypot(dx, dy);
            if (length > 1.0f) {
                dx /= length;
                dy /= length;
            }
            velocity = {dx * _speed, dy * _speed};
        }
    }
};

void run()
{
    using namespace rtype::render;
    using namespace rtype::engine;
    using rtype::client::Action;
    using rtype::ecs::entity_t;
    using rtype::ecs::Position;

    Window window;
    ResourceManager resources(window, applicationDirectory() / "assets");
    Renderer renderer(window, resources);
    // After the window, so that raylib's messages about the audio output are logged like the others
    rtype::audio::AudioDevice audioDevice;
    rtype::audio::AudioPlayer audio(audioDevice, applicationDirectory() / "assets");
    const rtype::audio::SoundId shotSound = audio.loadSound("sounds/train.mp3");
    rtype::input::Devices devices(window);
    rtype::input::ActionMap<Action> controls = rtype::client::defaultControls();

    const FontId font = resources.loadFont("fonts/FiraSans-Black.ttf", 160);
    const TextureId texture = resources.loadTexture("images/big_mama.png");
    const InterfaceStyle style{.font = font};
    const RendererMetrics metrics(renderer);

    Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    Scenes &scenes = engine.getScenes();
    constexpr float speed = 600.0f;
    ecs.registerSystem<InterfaceSystem>(ecs, scenes, devices, metrics, style);
    ecs.registerSystem<WidgetTextSystem>();
    ecs.registerSystem<TweenSystem>();
    ecs.registerSystem<HitboxSystem>(metrics, style);
    ecs.registerSystem<ColumnLayoutSystem>(ecs);
    ecs.registerSystem<PlayerInputSystem>(scenes, controls, speed);
    auto &physics = ecs.registerSystem<rtype::physics::PhysicsStep>();
    physics.registerSystem<rtype::physics::MovementSystem>([&scenes](entity_t entity) {
        return scenes.updates(entity);
    });
    physics.registerSystem<rtype::physics::BoundsSystem>(ecs, [&scenes](entity_t entity) {
        return scenes.updates(entity);
    });
    physics.registerSystem<rtype::ecs::TransformSystem>(ecs);
    SceneRenderer sceneRenderer(ecs, scenes, style);

    std::string playerName = "Player";
    const rtype::client::MenuTargets menus{ecs,      scenes, metrics,   style,     window,
                                           renderer, audio,  shotSound, playerName};

    // Demo of the game scene: the image moves with the movement actions, Shoot counts the shots, Menu opens
    // the pause menu. The game keeps updating under the pause menu: it is shared with the other players.
    const SceneId game = scenes.open({.updatesWhenCovered = true});
    const Vector2 gameSize = renderer.gameSize();
    const Vector2 textureSize = renderer.textureSize(texture);
    const float imageSize = 300.0f;
    const float halfImageSize = imageSize / 2.0f;
    const entity_t ship = ecs.createEntity();
    ecs.addComponent<SceneMember>(ship, game);
    ecs.addComponent<Position>(ship, 350.0f, 350.0f);
    ecs.addComponent<PlayerControlled>(ship);
    ecs.addComponent<rtype::physics::Velocity>(ship);
    ecs.addComponent<rtype::physics::MaxSpeed>(ship, speed);
    ecs.addComponent<rtype::physics::ConfinedToBounds>(
        ship, Position{halfImageSize, halfImageSize},
        Position{gameSize.x - halfImageSize, gameSize.y - halfImageSize});
    ecs.addComponent<Drawable>(ship, Layer::World, 1);
    ecs.addComponent<Sprite>(ship, texture, Rectangle{0.0f, 0.0f, textureSize.x, textureSize.y}, imageSize,
                             imageSize);

    const entity_t title = ecs.createEntity();
    ecs.addComponent<SceneMember>(title, game);
    ecs.addComponent<Position>(title, gameSize.x / 2.0f, gameSize.y / 2.0f);
    ecs.addComponent<Drawable>(title, Layer::World, 0);
    ecs.addComponent<Text>(title, "R-Type", font, 160.0f, colors::White);

    // A HUD element: in the interface layer of the game scene
    const entity_t hud = ecs.createEntity();
    ecs.addComponent<SceneMember>(hud, game);
    ecs.addComponent<Position>(hud, 260.0f, 60.0f);
    ecs.addComponent<Drawable>(hud, Layer::Interface, 0);
    ecs.addComponent<Text>(hud, "", font, 40.0f, colors::Red);
    int shots = 0;

    while (!window.shouldClose()) {
        const float elapsed = window.frameTime();
        // Keeps the music stream fed, once per frame
        audio.update();
        controls.update(devices, elapsed);
        if (controls.wasPressed(Action::ToggleFullscreen)) {
            window.setFullscreen(!window.isFullscreen());
        }

        engine.update(elapsed);

        // The game reads its actions only while its scene receives the input: not while a menu is open, nor
        // during the update where the menu was closed by the same key
        const bool playing = scenes.receivesInput(game);
        // The pointer is only useful in menus: it would hide part of the game
        window.setCursorVisible(!playing);
        if (playing) {
            if (controls.wasPressed(Action::Menu)) {
                rtype::client::openPauseMenu(menus);
            }
            if (controls.wasPressed(Action::Shoot)) {
                ++shots;
                audio.play(shotSound);
            }
        }
        ecs.getComponent<Text>(hud).text = playerName + " - shots: " + std::to_string(shots);

        renderer.beginFrame(colors::White);
        sceneRenderer.draw(renderer, Layer::World);
        sceneRenderer.draw(renderer, Layer::Interface);
        renderer.drawFps({10.0f, 10.0f});
        renderer.endFrame();
    }
}

} // namespace

int main()
{
    const rtype::log::Logger logger("client");

    if (const auto problem = rtype::log::configureFromEnvironment()) {
        logger.warning("{}", *problem);
    }
    try {
        run();
    } catch (const std::exception &error) {
        logger.error("{}", error.what());
        return 84;
    }
    return 0;
}
