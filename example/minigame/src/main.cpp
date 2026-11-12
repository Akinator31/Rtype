// Orbit Breaker: a brick breaker made with the engine of R-Type alone, to show that it runs another game.

#include "Breaker.hpp"
#include "Scenes.hpp"
#include "Systems.hpp"
#include "rtype/audio/AudioDevice.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Rendering/SceneRenderer.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Window.hpp"

#include <exception>
#include <string>

namespace
{

namespace engine = rtype::engine;
namespace render = rtype::render;

constexpr render::Color background{10, 12, 24, 255};

/// Stops the assets worker before the resources it may still fill are destroyed.
struct StopWorkers {
    engine::ThreadManager &threads;

    ~StopWorkers() { threads.stopAll(); }
};

/// The best score of the previous games, from the settings of the player.
int readBestScore(engine::Settings &settings)
{
    settings.setPath(render::userDataDirectory() / "orbit_breaker.txt");
    try {
        settings.readFile();
    } catch (const std::exception &error) {
        rtype::log::Logger("orbit").warning("Cannot read the settings: {}", error.what());
    }
    return settings.getInt(orbit::BestScoreKey).value_or(0);
}

orbit::Picture loadPicture(render::ResourceManager &resources, const render::Renderer &renderer,
                           const std::string &path)
{
    const render::TextureId texture = resources.loadTexture(path, render::TextureFiltering::Smooth);
    const render::Vector2 size = renderer.textureSize(texture);
    return {texture, {0.0f, 0.0f, size.x, size.y}};
}

orbit::Looks loadLooks(render::ResourceManager &resources, const render::Renderer &renderer,
                       render::FontId font)
{
    const auto sky = [&resources](const char *path) {
        return resources.loadTexture(path, render::TextureFiltering::Smooth);
    };
    return {.font = font,
            .glow = loadPicture(resources, renderer, "images/glow.png"),
            .spark = loadPicture(resources, renderer, "images/spark.png"),
            .shockwave = loadPicture(resources, renderer, "images/shockwave.png"),
            .sky = {sky("images/sky_deep.png"), sky("images/sky_nebula.png"), sky("images/sky_stars.png"),
                    sky("images/sky_near.png")},
            .planet = loadPicture(resources, renderer, "images/planet.png")};
}

engine::InterfaceStyle makeStyle(render::ResourceManager &resources, rtype::audio::AudioPlayer &audio)
{
    const render::FontId display = resources.loadFont("fonts/Audiowide-Regular.ttf", 96);
    return {
        .font = display,
        .button = {.font = resources.loadFont("fonts/Exo2-SemiBold.ttf", 96),
                   .hoverSound = engine::SoundEffect{audio.loadSound("sounds/hover.wav"), {.volume = 0.4f}},
                   .clickSound = engine::SoundEffect{audio.loadSound("sounds/click.wav"), {}}},
        .flare = {.font = resources.loadFont("fonts/Orbitron-Medium.ttf", 96)},
        .title = {.font = display},
    };
}

/// Runs the frames until the window closes: the engine, the sounds, then the world and the interface drawn.
void play(render::Window &window, engine::Engine &engine, rtype::audio::AudioPlayer &audio,
          render::Renderer &renderer, engine::SceneRenderer &sceneRenderer)
{
    while (!window.shouldClose()) {
        window.update();
        engine.update(window.frameTime());
        audio.finishPendingLoads();
        audio.update();
        renderer.beginFrame(background);
        sceneRenderer.draw(renderer, engine::Layer::World);
        sceneRenderer.draw(renderer, engine::Layer::Interface);
        renderer.endFrame();
    }
}

void run()
{
    render::Window window({.width = 1600, .height = 900, .title = "Orbit Breaker"});
    engine::Engine engine;
    orbit::Match match{.best = readBestScore(engine.getSettings())};
    const auto assets = render::applicationDirectory() / "orbit_breaker_assets";
    const rtype::audio::AudioDevice audioDevice;
    rtype::audio::AudioPlayer audio(audioDevice, assets, engine.getThreads().getAssetsWorker());
    render::ResourceManager resources(window, assets, engine.getThreads().getAssetsWorker());
    StopWorkers stopWorkers{engine.getThreads()};
    render::Renderer renderer(window, resources);
    rtype::input::Devices devices(window);
    const engine::InterfaceStyle style = makeStyle(resources, audio);
    const orbit::Looks looks = loadLooks(resources, renderer, style.font);
    const engine::RendererMetrics metrics(renderer);

    engine.setInputSource(devices);
    const orbit::BallSounds sounds{audio.loadSound("sounds/collision.wav"),
                                   audio.loadSound("sounds/brick.wav"), audio.loadSound("sounds/lost.wav")};
    orbit::registerSystems(engine, devices, audio, metrics, style, match, sounds, looks,
                           [&engine] { engine.pushScene(orbit::GameOverScene); });
    orbit::createScenes(engine, style, looks, match, [&window] { window.requestClose(); });
    engine.switchScene(orbit::MenuScene);
    engine::SceneRenderer sceneRenderer(engine.getEcs(), engine.getScenes(), style);
    play(window, engine, audio, renderer, sceneRenderer);
}

} // namespace

int main()
{
    const rtype::log::Logger logger("orbit");
    try {
        run();
    } catch (const std::exception &error) {
        logger.error("{}", error.what());
        return 84;
    }
    return 0;
}
