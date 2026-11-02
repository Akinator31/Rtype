#include "Game/GameScenes.hpp"

#include "Assets.hpp"
#include "Game/GameView.hpp"
#include "Game/Hud.hpp"
#include "Game/LocalSession.hpp"
#include "Game/RescueCameraSystem.hpp"
#include "MainMenu/MainMenu.hpp"
#include "ShipModel.hpp"
#include "ShipSelector/ShipCatalog.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Interface/MenuBuilder.hpp"
#include "rtype/game/Constants.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

using engine::AssetType;

/// The models, textures and sounds of the ships, the shots and the impacts, loaded before the game opens.
const std::vector<engine::SceneAsset> GameAssets{
    {AssetType::SmoothTexture, RTYPE_IMAGE_ENERGY_NOISE},
    {AssetType::SmoothTexture, RTYPE_IMAGE_GLOW_CORE},
    {AssetType::SmoothTexture, RTYPE_IMAGE_LIFE},
    {AssetType::SmoothTexture, RTYPE_IMAGE_LIFE_LOST},
    {AssetType::Model, RTYPE_MODEL_SHIP_EZNO},
    {AssetType::Model, RTYPE_MODEL_SHIP_DROP_SHIP},
    {AssetType::Model, RTYPE_MODEL_BOSS_SHOGUN},
    // The ships of the hangar, which the local player may fly
    {AssetType::Model, RTYPE_MODEL_SHIP_CAVED},
    {AssetType::Model, RTYPE_MODEL_SHIP_BATTLESHIP},
    {AssetType::Model, RTYPE_MODEL_SHIP_SPACESHIP},
    {AssetType::Model, RTYPE_MODEL_SHIP_BATTLESTAR},
    {AssetType::Model, RTYPE_MODEL_SHOT_BASIC},
    {AssetType::Model, RTYPE_MODEL_SHOT_SPIRAL},
    {AssetType::Model, RTYPE_MODEL_ORB_PLASMA},
    {AssetType::Model, RTYPE_MODEL_IMPACT_SPARK},
    {AssetType::Model, RTYPE_MODEL_IMPACT_BURST},
    {AssetType::Model, RTYPE_MODEL_IMPACT_SPLASH},
    {AssetType::Model, RTYPE_MODEL_IMPACT_EXPLOSION},
    {AssetType::Sound, RTYPE_SOUND_BASICAL_BLASTER},
    {AssetType::Sound, RTYPE_SOUND_LOW_BLASTER},
    {AssetType::Sound, RTYPE_SOUND_BIG_BLASTER},
    {AssetType::Sound, RTYPE_SOUND_DESTRUCTION},
    {AssetType::Sound, RTYPE_SOUND_COLLISION},
    {AssetType::Sound, RTYPE_SOUND_SABER},
    {AssetType::Sound, RTYPE_SOUND_DASH_WIND},
};

/// The hulls of the ship models seen through the camera of the game, flames excluded, in model units:
/// measured on the models in their Move pose, the ship of the players facing right, the drop ship of the
/// Bydos facing left. The colliders of the ships have the same proportions (Rules::shipHalfWidth,
/// config/ships/bydo_scout.lua).
constexpr render::Rectangle EznoHull{-8.66f, -3.31f, 17.31f, 10.01f};
constexpr render::Rectangle DropShipHull{-11.95f, -5.74f, 23.9f, 9.11f};

/// The body of the shogun, the final boss, in its Idle pose, blade and gun excluded, in model units. The model
/// faces +z (its toes, its visor): turned by -70° around its vertical axis, it faces left, towards the players,
/// and a little towards the camera. Its collider has the same proportions (config/ships/bydo_shogun.lua).
constexpr render::Rectangle ShogunHull{-5.0f, -33.0f, 17.0f, 33.5f};
constexpr float ShogunSpin = -70.0f;

/// The model of the allies of the rescue against the final boss, one of the ships of the hangar.
constexpr std::string_view AllyModel = RTYPE_MODEL_SHIP_SPACESHIP;

/// The prompt of the combined attack, during the rescue: in the titles, under the middle of the screen,
/// blinking.
constexpr float PromptSize = 44.0f;
constexpr float PromptSpacing = 6.0f;
constexpr float PromptOffset = 260.0f;
constexpr float PromptBlink = 0.45f;
/// Over the HUD and the death screen, under the toasts.
constexpr int PromptOrder = 20;
/// How loud the voice of the rescue is.
constexpr float RescueVoiceVolume = 1.0f;
/// Time the banner of the combined attack stays fully shown, in seconds.
constexpr float CombinedBannerDuration = 1.2f;

/// The meshes of the trails behind the flames of the ship models, by index in their files.
const std::vector<int> EznoTrails{6};
const std::vector<int> DropShipTrails{10, 12};

/// The cores of the shot models seen through the camera of the game, trails and rings excluded, in model
/// units, measured on the models: the shot of the players facing right, the shots of the Bydos facing left.
/// The colliders of the shots have the same proportions (config/projectiles/).
constexpr render::Rectangle ShotBasicCore{-1.6f, -0.55f, 4.0f, 1.1f};
constexpr render::Rectangle ShotSpiralCore{-2.4f, -0.73f, 4.48f, 1.46f};
constexpr render::Rectangle OrbPlasmaCore{-0.9f, -0.9f, 1.8f, 1.8f};

/// The ships are lit from the top left, in front of them, in the world of the game where y points down. A
/// cold rim light from behind, at the top right where the Bydos come from, outlines their hulls against
/// space.
constexpr render::Light GameLight{.direction = {0.5f, 0.8f, 0.6f},
                                  .color = {255, 248, 240, 255},
                                  .ambient = {48, 50, 62, 255},
                                  .rimDirection = {-0.6f, 0.35f, -1.0f},
                                  .rim = {95, 135, 210, 255}};

/// The colours of the light around the shots, those of their energy; the alpha is how bright it is.
constexpr render::Color PlayerShotLight{80, 200, 255, 130};
constexpr render::Color BydoShotLight{185, 90, 255, 115};
constexpr render::Color PlasmaOrbLight{255, 120, 35, 135};
/// How far the light of a shot spreads around its core, in heights of the core.
constexpr float ShotLightSpread = 1.5f;

/// The damage taken by the Bydos, written where they are hit: in the font of the titles, loaded at this size
/// for the whole interface, in a warm yellow that stands out from the blue shots and the purple Bydos.
constexpr int DamageFontSize = 96;
constexpr float DamageTextSize = 40.0f;
constexpr render::Color DamageColor{255, 220, 90, 255};

/// The health bars shown above the Bydos hit and above the ships of the other players, over their hull, under
/// the damage numbers: red under a third of their hit points.
constexpr HealthBarLook BydoHealthBar{100.0f, 42.0f, 0.3f};
constexpr HealthBarLook BossHealthBar{240.0f, 180.0f, 0.3f};
constexpr HealthBarLook AllyHealthBar{110.0f, 62.0f, 0.3f};

/// The animation of the impact models, played once.
constexpr const char *ImpactAnimation = "Impact";
/// The colours of the impacts of the shots, those of their energy: the impact models are white.
constexpr render::Color PlayerShotImpact{80, 200, 255, 255};
constexpr render::Color BydoShotImpact{185, 90, 255, 255};
constexpr render::Color PlasmaOrbImpact{255, 120, 35, 255};
/// The explosion model is pale orange, and its layers add up: deepened and dimmed, it stays orange instead of
/// turning white, and its glow does not cover the screen.
constexpr render::Color ExplosionTint{255, 150, 80, 190};
constexpr float ExplosionBloom = 0.4f;
/// The sizes of the impacts, in game units per model unit: about the size of what was destroyed.
constexpr float SparkScale = 20.0f;
constexpr float BurstScale = 15.0f;
constexpr float SplashScale = 24.0f;
constexpr float BydoExplosionScale = 25.0f;
constexpr float BossExplosionScale = 70.0f;
constexpr float ShipExplosionScale = 28.0f;
/// The cut of the blade of the shogun on a ship it goes through, during the rescue: a burst larger than the
/// ship, in the cold white of the blade.
constexpr float StrikeScale = 42.0f;
constexpr render::Color StrikeTint{170, 225, 255, 255};

/// How loud the shots are: the players fire all the time, the Bydos often and many at once, their plasma orbs
/// rarely.
constexpr float PlayerShotVolume = 0.5f;
constexpr float BydoShotVolume = 0.35f;
constexpr float PlasmaOrbVolume = 0.7f;
/// How loud the ships are when destroyed: louder than the shots, the ship of a player more than a Bydo.
constexpr float BydoDestructionVolume = 0.8f;
constexpr float ShipDestructionVolume = 1.0f;
/// How loud a shot is when it hits something: its file is much quieter than the others.
constexpr float CollisionVolume = 1.0f;
/// How loud the blade of the boss is when it strikes, and the wind of its dash.
constexpr float SaberVolume = 0.9f;
constexpr float DashWindVolume = 0.7f;
/// How loud the blade of the boss is when it goes through a ship.
constexpr float StrikeVolume = 1.0f;

/// The energy of a shot with a light around its core.
/// @param core The core of the shot model, as in ShotLook.
render::Energy shine(render::Energy energy, render::TextureId light, render::Rectangle core,
                     render::Color color)
{
    const float margin = core.height * ShotLightSpread;
    // The cores are measured through the camera of the game, where y points down: the models have y up
    energy.halo = render::Energy::Halo{
        light,
        color,
        {core.x + (core.width / 2.0f), -(core.y + (core.height / 2.0f)), 0.0f},
        {core.width + (2.0f * margin), core.height + (2.0f * margin)},
    };
    return energy;
}

/// The impact played by the model at a path, its animation played once.
ImpactLook loadImpactLook(render::ResourceManager &resources, const char *path, float scale,
                          render::Color tint, const render::Energy &energy)
{
    const render::ModelId model = resources.loadModel(path);
    const int animation = resources.getModelAnimation(model, ImpactAnimation);
    return {model, animation, resources.getModelAnimationDuration(model, animation), scale, tint, energy};
}

/// Gives each kind of shot the impact it leaves where it hits, and each kind of ship its explosion.
void setImpactLooks(GameSessionSystem &sessions, render::ResourceManager &resources,
                    const render::Energy &energy)
{
    sessions.setImpactLook(
        game::EntityKind::PlayerShot,
        loadImpactLook(resources, RTYPE_MODEL_IMPACT_SPARK, SparkScale, PlayerShotImpact, energy));
    sessions.setImpactLook(game::EntityKind::BydoShot, loadImpactLook(resources, RTYPE_MODEL_IMPACT_BURST,
                                                                      BurstScale, BydoShotImpact, energy));
    sessions.setImpactLook(
        game::EntityKind::BydoPlasmaShot,
        loadImpactLook(resources, RTYPE_MODEL_IMPACT_SPLASH, SplashScale, PlasmaOrbImpact, energy));
    render::Energy explosion = energy;
    explosion.bloom = ExplosionBloom;
    sessions.setImpactLook(game::EntityKind::BydoFlyer,
                           loadImpactLook(resources, RTYPE_MODEL_IMPACT_EXPLOSION, BydoExplosionScale,
                                          ExplosionTint, explosion));
    sessions.setImpactLook(game::EntityKind::BydoBoss,
                           loadImpactLook(resources, RTYPE_MODEL_IMPACT_EXPLOSION, BossExplosionScale,
                                          ExplosionTint, explosion));
    sessions.setImpactLook(game::EntityKind::PlayerShip,
                           loadImpactLook(resources, RTYPE_MODEL_IMPACT_EXPLOSION, ShipExplosionScale,
                                          ExplosionTint, explosion));
}

/// Gives the ships of the formation of the rescue the cut the blade of the shogun leaves on them, and its
/// sound.
void setStrikeLooks(GameSessionSystem &sessions, render::ResourceManager &resources, audio::AudioPlayer &audio,
                    const render::Energy &energy)
{
    const ImpactLook strike =
        loadImpactLook(resources, RTYPE_MODEL_IMPACT_BURST, StrikeScale, StrikeTint, energy);
    const audio::SoundId sound = audio.loadSound(RTYPE_SOUND_COLLISION);
    for (const game::EntityKind kind : {game::EntityKind::PlayerShip, game::EntityKind::AllyShip}) {
        sessions.setStrikeLook(kind, strike);
        sessions.setStrikeSound(kind, {sound, StrikeVolume});
    }
}

/// Gives each kind of shot the sound of the weapon that fires it: the cannon of the players, the blaster of
/// the Bydo scouts, the plasma gun of the gunners.
void setShotSounds(GameSessionSystem &sessions, audio::AudioPlayer &audio)
{
    sessions.setShotSound(game::EntityKind::PlayerShot,
                          {audio.loadSound(RTYPE_SOUND_BASICAL_BLASTER), PlayerShotVolume});
    sessions.setShotSound(game::EntityKind::BydoShot,
                          {audio.loadSound(RTYPE_SOUND_LOW_BLASTER), BydoShotVolume});
    sessions.setShotSound(game::EntityKind::BydoPlasmaShot,
                          {audio.loadSound(RTYPE_SOUND_BIG_BLASTER), PlasmaOrbVolume});
}

/// Gives each kind of ship the sound of its explosion.
void setDestructionSounds(GameSessionSystem &sessions, audio::AudioPlayer &audio)
{
    const audio::SoundId destruction = audio.loadSound(RTYPE_SOUND_DESTRUCTION);
    sessions.setDestructionSound(game::EntityKind::BydoFlyer, {destruction, BydoDestructionVolume});
    sessions.setDestructionSound(game::EntityKind::BydoBoss, {destruction, ShipDestructionVolume});
    sessions.setDestructionSound(game::EntityKind::PlayerShip, {destruction, ShipDestructionVolume});
}

/// Gives each kind of shot the sound of its collision: the game kills a shot that hits something.
void setCollisionSounds(GameSessionSystem &sessions, audio::AudioPlayer &audio)
{
    const audio::SoundId collision = audio.loadSound(RTYPE_SOUND_COLLISION);
    for (const game::EntityKind kind :
         {game::EntityKind::PlayerShot, game::EntityKind::BydoShot, game::EntityKind::BydoPlasmaShot}) {
        sessions.setDestructionSound(kind, {collision, CollisionVolume});
    }
}

/// Gives each kind of entity of the game its look and its sounds, from the GameAssets, loaded by then, and
/// the ship of the local player the look of the ship equipped.
void configureGameLooks(GameSessionSystem &sessions, render::ResourceManager &resources,
                        audio::AudioPlayer &audio, const ShipEntry &equipped)
{
    sessions.setShipLook(game::EntityKind::PlayerShip,
                         {loadShipModel(resources, RTYPE_MODEL_SHIP_EZNO), EznoHull, false, EznoTrails});
    sessions.setLocalShipLook(
        {loadShipModel(resources, equipped.modelPath), equipped.hull, false, equipped.trailMeshes});
    // The allies of the rescue against the final boss fly the ship of the hangar with the model AllyModel
    const auto &catalog = getShipCatalog();
    const auto ally = std::ranges::find_if(
        catalog, [](const ShipEntry &entry) { return std::string_view(entry.modelPath) == AllyModel; });
    if (ally != catalog.end()) {
        sessions.setShipLook(game::EntityKind::AllyShip,
                             {loadShipModel(resources, ally->modelPath), ally->hull, false,
                              ally->trailMeshes});
    }
    sessions.setShipLook(game::EntityKind::BydoFlyer, {loadShipModel(resources, RTYPE_MODEL_SHIP_DROP_SHIP),
                                                       DropShipHull, true, DropShipTrails});
    BossLook shogun = loadBossLook(resources, RTYPE_MODEL_BOSS_SHOGUN, ShogunHull, ShogunSpin);
    shogun.slashSounds = {{audio.loadSound(RTYPE_SOUND_SABER), SaberVolume},
                          {audio.loadSound(RTYPE_SOUND_DASH_WIND), DashWindVolume}};
    sessions.setBossLook(game::EntityKind::BydoBoss, shogun);
    const render::Energy energy{
        resources.loadTexture(RTYPE_IMAGE_ENERGY_NOISE, render::TextureFiltering::Smooth)};
    const render::TextureId light =
        resources.loadTexture(RTYPE_IMAGE_GLOW_CORE, render::TextureFiltering::Smooth);
    sessions.setShotLook(game::EntityKind::PlayerShot,
                         {resources.loadModel(RTYPE_MODEL_SHOT_BASIC), ShotBasicCore, false,
                          shine(energy, light, ShotBasicCore, PlayerShotLight)});
    sessions.setShotLook(game::EntityKind::BydoShot,
                         {resources.loadModel(RTYPE_MODEL_SHOT_SPIRAL), ShotSpiralCore, true,
                          shine(energy, light, ShotSpiralCore, BydoShotLight)});
    sessions.setShotLook(game::EntityKind::BydoPlasmaShot,
                         {resources.loadModel(RTYPE_MODEL_ORB_PLASMA), OrbPlasmaCore, true,
                          shine(energy, light, OrbPlasmaCore, PlasmaOrbLight)});
    setImpactLooks(sessions, resources, energy);
    setStrikeLooks(sessions, resources, audio, energy);
    const render::FontId damageFont = resources.loadFont(RTYPE_FONT_DISPLAY, DamageFontSize);
    for (const game::EntityKind kind : {game::EntityKind::BydoFlyer, game::EntityKind::BydoBoss}) {
        sessions.setDamageLook(kind, {damageFont, DamageTextSize, DamageColor});
    }
    for (const game::EntityKind kind :
         {game::EntityKind::BydoFlyer, game::EntityKind::BydoShooter, game::EntityKind::BydoTank}) {
        sessions.setHealthBarLook(kind, BydoHealthBar);
    }
    sessions.setHealthBarLook(game::EntityKind::BydoBoss, BossHealthBar);
    sessions.setHealthBarLook(game::EntityKind::PlayerShip, AllyHealthBar);
    setShotSounds(sessions, audio);
    setDestructionSounds(sessions, audio);
    setCollisionSounds(sessions, audio);
}

/// Time the banner of a wave stays fully shown, in seconds: shorter than the other banners, as it comes back
/// with each wave.
constexpr float WaveBannerDuration = 1.5f;

/// The name of a player in the events, as in the HUD: P1 to P4.
std::string playerName(std::uint8_t slot)
{
    return std::format("P{}", slot + 1);
}

/// The lives left of the player in a slot, 0 when it is not in the game.
std::uint8_t livesOf(const std::vector<game::PlayerState> &players, std::uint8_t slot)
{
    const auto player = std::ranges::find(players, slot, &game::PlayerState::slot);
    return player == players.end() ? 0 : player->lives;
}

/// The translation key of the name of each kind of Bydo in the events, the flyer being the default.
constexpr std::array<std::pair<game::EntityKind, const char *>, 3> BydoNameKeys{{
    {game::EntityKind::BydoShooter, "event.shooter"},
    {game::EntityKind::BydoTank, "event.tank"},
    {game::EntityKind::BydoBoss, "event.boss"},
}};

const char *bydoNameKey(game::EntityKind kind)
{
    const auto name =
        std::ranges::find(BydoNameKeys, kind, &std::pair<game::EntityKind, const char *>::first);
    return name == BydoNameKeys.end() ? "event.flyer" : name->second;
}

/// Tells whether a path sent by the game stays in the assets: relative, without ".." (the game may be a
/// server).
bool isAssetPath(const std::string &path)
{
    const std::filesystem::path file = std::filesystem::path(path).lexically_normal();
    return file.is_relative() && !file.has_root_name() && !file.has_root_directory() &&
           std::ranges::none_of(file, [](const std::filesystem::path &part) { return part == ".."; });
}

} // namespace

GameScenes::GameScenes(engine::Engine &engine, render::ResourceManager &resources, audio::AudioPlayer &audio,
                       const engine::Metrics &metrics, const engine::InterfaceStyle &style,
                       GameSessionSystem &sessions, localization::Localization &translations,
                       const ShipSelection &ships)
    : _engine(engine), _audio(audio), _metrics(metrics), _style(style), _sessions(sessions), _translations(translations),
      _ships(ships)
{
    sessions.setKillHandler([this](const Kill &kill) { showKill(kill); });
    sessions.setWaveHandler([this](std::uint16_t wave, const std::string &music) {
        showWave(wave);
        playWaveMusic(music);
    });
    sessions.setRescueHandler([this](game::RescueStage stage) { showRescue(stage); });
    sessions.setStrikeHandler([this] { showStrike(); });
    engine.createScene(
        GameScene,
        {.updatesWhenCovered = true,
         .onBack = [&engine] { engine.pushScene(PauseScene); },
         .assets = GameAssets},
        [this, &resources, &audio, &metrics, &style](engine::Engine & /*target*/, engine::SceneId scene) {
            build(scene, resources, audio, metrics, style);
        });
    engine.createScene(PauseScene, {.overlay = true},
                       [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
                           engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                                    _translations.text("game.pause"), true);
                           menu.button(_translations.text("game.resume"), [&target] { target.popScene(); });
                           menu.button(_translations.text("game.leave"), [this] { end(); });
                       });
}

void GameScenes::play(std::unique_ptr<GameSession> session, std::function<void()> onEnd)
{
    _next = std::move(session);
    _nextEnd = std::move(onEnd);
    _engine.switchScene(GameScene);
}

void GameScenes::end()
{
    _audio.stopMusic();
    _sessions.stop();
    const std::function<void()> onEnd = std::exchange(_onEnd, nullptr);
    if (onEnd) {
        onEnd();
    } else {
        _engine.switchScene(MainMenuScene);
    }
}

void GameScenes::showKill(const Kill &kill)
{
    const GameSession *session = _sessions.session();
    if (session == nullptr) {
        return;
    }
    const std::uint8_t local = session->localSlot();
    if (kill.kind == game::EntityKind::PlayerShip) {
        _engine.showToast(playerName(kill.owner), _translations.text("event.player_down"), "",
                          kill.owner == local ? engine::ToastType::Error : engine::ToastType::Warning);
        if (kill.owner == local) {
            showDeath();
        }
    } else if (kill.killer == local) {
        _engine.showToast(_translations.text(bydoNameKey(kill.kind)), _translations.text("event.bydo_down"),
                          playerName(local), engine::ToastType::Success);
    }
}

void GameScenes::showWave(std::uint16_t wave)
{
    if (wave == game::MaxWaveDifficulty) {
        _engine.showBanner(_translations.text("event.boss_wave"),
                           _translations.text("event.boss_wave_detail"), engine::ToastType::Error);
        return;
    }
    _engine.showBanner(std::format("{} {:02}", _translations.text("event.wave"), wave),
                       _translations.text("event.wave_detail"), engine::ToastType::Info, WaveBannerDuration);
}

void GameScenes::playWaveMusic(const std::string &music)
{
    if (music.empty()) {
        return;
    }
    const log::Logger logger("game");
    if (!isAssetPath(music)) {
        logger.warning("Music \"{}\" of the wave is not a path of the assets, ignored", music);
        return;
    }
    try {
        _audio.playMusic(_audio.loadMusic(music));
    } catch (const std::runtime_error &error) {
        logger.warning("Music of the wave not played: {}", error.what());
    }
}

void GameScenes::showDeath()
{
    const std::uint8_t lives = livesOf(_sessions.players(), _sessions.session()->localSlot());
    if (lives == 0 || !_scene) {
        return;
    }
    // The banner leaves as the next ship comes
    const game::Rules rules;
    const float respawn = static_cast<float>(rules.respawnDelay) / static_cast<float>(game::TickRate);
    const float entrances = _style.toast.getFadeIn(engine::ToastKind::Banner) +
                            _style.toast.getFadeOut(engine::ToastKind::Banner);
    _engine.showBanner(_translations.text("death.title"), "", engine::ToastType::Error,
                       std::max(respawn - entrances, 0.0f));
    openDeathScreen(_engine.getEcs(), *_scene, _metrics, _style.toast, _deathLook,
                    {lives, rules.lives, _sessions.tick() + rules.respawnDelay, rules.respawnDelay});
}

void GameScenes::showRescue(game::RescueStage stage)
{
    if (stage != game::RescueStage::Prompt) {
        closePrompt();
    }
    switch (stage) {
    case game::RescueStage::Voice:
        playRescueVoice();
        break;
    case game::RescueStage::Arrival:
        _engine.showBanner(_translations.text("rescue.title"), _translations.text("rescue.detail"),
                           engine::ToastType::Success);
        break;
    case game::RescueStage::Prompt:
        openPrompt();
        break;
    case game::RescueStage::Combined:
        _engine.showBanner(_translations.text("rescue.combined"), "", engine::ToastType::Info,
                           CombinedBannerDuration);
        break;
    default:
        break;
    }
}

void GameScenes::showStrike()
{
    ecs::Ecs &ecs = _engine.getEcs();
    if (_camera && ecs.isAlive(*_camera)) {
        shakeCamera(ecs.getComponent<RescueCamera>(_camera->id));
    }
}

void GameScenes::playRescueVoice()
{
    try {
        _audio.play(_audio.loadSound(RTYPE_SOUND_RESCUE_VOICE), {.volume = RescueVoiceVolume});
    } catch (const std::runtime_error &error) {
        log::Logger("game").warning("Voice of the rescue not played: {}", error.what());
    }
}

void GameScenes::openPrompt()
{
    if (!_scene || _prompt) {
        return;
    }
    ecs::Ecs &ecs = _engine.getEcs();
    const ecs::entity_t prompt = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(prompt, *_scene);
    ecs.addComponent<engine::Anchor>(prompt, engine::AnchorPoint::Center, 0.0f, PromptOffset);
    ecs.addComponent<ecs::Position>(prompt);
    ecs.addComponent<engine::Drawable>(prompt, engine::Layer::Interface, PromptOrder);
    ecs.addComponent<engine::Text>(prompt, _translations.text("rescue.prompt"), _style.toast.nameFont,
                                   PromptSize, render::colors::White, PromptSpacing);
    ecs.addComponent<engine::Title>(prompt);
    ecs.addComponent<engine::Opacity>(prompt, 1.0f);
    ecs.addComponent<engine::Tween>(prompt, engine::Tween{.target = engine::TweenTarget::Opacity,
                                                          .from = 1.0f,
                                                          .to = 0.25f,
                                                          .duration = PromptBlink,
                                                          .easing = engine::Easing::Linear,
                                                          .pingPong = true});
    _prompt = ecs.ref(prompt);
}

void GameScenes::closePrompt()
{
    const std::optional<ecs::EntityRef> prompt = std::exchange(_prompt, std::nullopt);
    ecs::Ecs &ecs = _engine.getEcs();
    if (prompt && ecs.isAlive(*prompt)) {
        ecs.destroyEntity(prompt->id);
    }
}

void GameScenes::build(engine::SceneId scene, render::ResourceManager &resources, audio::AudioPlayer &audio,
                       const engine::Metrics &metrics, const engine::InterfaceStyle &style)
{
    configureGameLooks(_sessions, resources, audio, getShipCatalog()[_ships.equipped]);
    _scene = scene;
    _prompt.reset();
    _deathLook = {resources.loadTexture(RTYPE_IMAGE_LIFE, render::TextureFiltering::Smooth),
                  resources.loadTexture(RTYPE_IMAGE_LIFE_LOST, render::TextureFiltering::Smooth),
                  style.toast.messageFont,
                  style.toast.nameFont,
                  _translations.text("death.lives"),
                  _translations.text("death.respawn")};

    ecs::Ecs &ecs = _engine.getEcs();

    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Backdrop>(
        sky, resources.loadTexture(RTYPE_IMAGE_STARFIELD, render::TextureFiltering::Smooth));
    ecs.addComponent<engine::Camera>(sky, computeGameView(metrics.visibleArea()));
    const render::Rectangle area = metrics.visibleArea();
    ecs.addComponent<RescueCamera>(
        sky, area, 0.0f, render::Vector2{area.x + (area.width / 2.0f), area.y + (area.height / 2.0f)});
    _camera = ecs.ref(sky);
    ecs.addComponent<engine::Light>(sky, GameLight);

    openHud(ecs, scene, {_deathLook.life, _deathLook.lostLife, style.toast.nameFont}, game::Rules{}.lives);

    _onEnd = std::exchange(_nextEnd, nullptr);
    _sessions.start(_next ? std::move(_next) : std::make_unique<LocalSession>(), scene, [this] { end(); });
}

} // namespace rtype::client
