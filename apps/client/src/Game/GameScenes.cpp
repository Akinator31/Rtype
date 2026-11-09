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
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Interface/MenuBuilder.hpp"
#include "rtype/game/Constants.hpp"
#include "rtype/game/Hitbox.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Ship/ShipRegistry.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
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
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_DEEP},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_NEBULA},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_STARS},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_NEAR},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_GALAXY},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_MOON},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_PLANET},
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

/// The sky of the game, behind the results, and their sounds.
const std::vector<engine::SceneAsset> ResultsAssets{
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_DEEP},   {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_NEBULA},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_STARS},  {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_NEAR},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_GALAXY}, {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_MOON},
    {AssetType::SmoothTexture, RTYPE_IMAGE_SKY_PLANET}, {AssetType::Sound, RTYPE_SOUND_RESULTS_GAME_OVER},
    {AssetType::Sound, RTYPE_SOUND_RESULTS_STAT},       {AssetType::Sound, RTYPE_SOUND_RESULTS_TICK},
    {AssetType::Sound, RTYPE_SOUND_RESULTS_COUNTED},    {AssetType::Sound, RTYPE_SOUND_RESULTS_LEVEL_UP},
};

/// The Continue of the results, at the bottom of the screen.
constexpr float ContinueAboveBottom = 140.0f;

/// The hulls of the ship models seen through the camera of the game, flames excluded, in model units:
/// measured on the models in their Move pose, the ship of the players facing right, the drop ship of the
/// Bydos facing left. The colliders of the ships have the same proportions (Rules::shipHalfWidth,
/// config/ships/bydo_scout.lua).
constexpr render::Rectangle EznoHull{-8.66f, -3.31f, 17.31f, 10.01f};
constexpr render::Rectangle DropShipHull{-11.95f, -5.74f, 23.9f, 9.11f};

/// The body of the shogun, the final boss, in its Idle pose, blade and gun excluded, in model units. The
/// model faces +z (its toes, its visor): turned by -70° around its vertical axis, it faces left, towards the
/// players, and a little towards the camera. Its collider has the same proportions
/// (config/ships/bydo_shogun.lua).
constexpr render::Rectangle ShogunHull{-5.0f, -33.0f, 17.0f, 33.5f};
constexpr float ShogunSpin = -70.0f;

/// The model of the Bydo drop ship, also in the hangar: never flown by the allies of the rescue.
constexpr std::string_view BydoModel = RTYPE_MODEL_SHIP_DROP_SHIP;

/// The prompt of the combined attack, during the rescue: in the titles, under the middle of the screen,
/// blinking.
constexpr float PromptSize = 44.0f;
constexpr float PromptSpacing = 6.0f;
constexpr float PromptOffset = 260.0f;
constexpr float PromptBlink = 0.45f;
/// Over the HUD and the death screen, under the toasts.
constexpr int PromptOrder = 20;
/// How hard the camera shakes when the blade of the shogun goes through a ship, and when the shogun falls.
constexpr float StrikeShake = 0.75f;
constexpr float FallShake = 1.0f;
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
void setStrikeLooks(GameSessionSystem &sessions, render::ResourceManager &resources,
                    audio::AudioPlayer &audio, const render::Energy &energy)
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

/// The ship of the hangar with this model, if any.
const ShipEntry *findCatalogEntry(const std::string &model)
{
    const auto &catalog = getShipCatalog();
    const auto found = std::ranges::find(catalog, std::string_view(model), [](const ShipEntry &entry) {
        return std::string_view(entry.modelPath);
    });
    return found != catalog.end() ? &*found : nullptr;
}

/// Gives each ship of the configuration that flies a model of the hangar its look, facing left, at the size
/// of its collider: each Bydo is drawn with the model of its ship (game::EntityState::ship), whose index is
/// the one of the ShipRegistry of the game.
void setConfiguredShipLooks(GameSessionSystem &sessions, render::ResourceManager &resources,
                            const std::vector<game::ShipDefinition> &definitions)
{
    game::ShipRegistry ships;
    for (const game::ShipDefinition &definition : definitions) {
        ships.addDefinition(definition);
    }
    const std::size_t count = std::min<std::size_t>(ships.getDefinitionCount(), game::NoShip);
    for (std::size_t index = 0; index < count; ++index) {
        const game::ShipDefinition &ship = ships.getDefinitionAt(index);
        const ShipEntry *entry = findCatalogEntry(ship.model);
        if (entry == nullptr) {
            continue;
        }
        const physics::Vec3 half = game::halfSizeOf(ship.collider);
        sessions.setShipLook(
            static_cast<std::uint8_t>(index),
            {loadShipModel(resources, entry->modelPath), entry->hull, true, entry->trailMeshes},
            {half.x * 2.0f, half.y * 2.0f});
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
    // The allies of the rescue against the final boss fly all the ships of the hangar but the Bydo one, mixed
    std::vector<ShipLook> allyLooks;
    for (const ShipEntry &entry : getShipCatalog()) {
        if (std::string_view(entry.modelPath) != BydoModel) {
            allyLooks.push_back(
                {loadShipModel(resources, entry.modelPath), entry.hull, false, entry.trailMeshes});
        }
    }
    sessions.setShipLooks(game::EntityKind::AllyShip, std::move(allyLooks));
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

/// A layer of the sky of the game, covering the screen, scrolling at its speed in game units per second.
struct SkyLayer {
    const char *path;
    float speed;
};

/// A picture passing in the sky of the game, between the layers, as in engine::BackdropSprite.
struct SkyObject {
    const char *path;
    float speed;
    float height;
    float y;
    float period;
    float start;
};

/// The layers from the back to the front: the nearer, the faster, so that the ships seem to fly to the right.
/// Even the deep sky moves visibly: nothing of the sky stays still on the screen.
const std::array<SkyLayer, 4> SkyLayers{{
    {RTYPE_IMAGE_SKY_DEEP, 40.0f},
    {RTYPE_IMAGE_SKY_NEBULA, 80.0f},
    {RTYPE_IMAGE_SKY_STARS, 140.0f},
    {RTYPE_IMAGE_SKY_NEAR, 300.0f},
}};

/// The objects of the sky, each drawn just in front of a layer: the galaxy in front of the deep sky, the
/// moon and the ringed planet in front of the stars, which they hide, at about their speed. They come back
/// every minute or so, the galaxy seen from the start.
const std::array<std::pair<std::size_t, SkyObject>, 3> SkyObjects{{
    {0, {RTYPE_IMAGE_SKY_GALAXY, 50.0f, 360.0f, 280.0f, 4200.0f, 1300.0f}},
    {2, {RTYPE_IMAGE_SKY_MOON, 160.0f, 200.0f, 760.0f, 9000.0f, 3000.0f}},
    {2, {RTYPE_IMAGE_SKY_PLANET, 170.0f, 640.0f, 380.0f, 13000.0f, 8000.0f}},
}};

/// The order of the Backdrop of a layer: room is left in between for the objects in front of it.
int skyOrder(std::size_t layer)
{
    return static_cast<int>(layer) * 2;
}

/// Creates the scrolling sky of the game: its layers and the objects passing between them.
void createGameSky(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    for (std::size_t layer = 0; layer < SkyLayers.size(); ++layer) {
        const ecs::entity_t entity = ecs.createEntity();
        ecs.addComponent<engine::SceneMember>(entity, scene);
        ecs.addComponent<engine::Backdrop>(
            entity, resources.loadTexture(SkyLayers[layer].path, render::TextureFiltering::Smooth),
            render::colors::White, skyOrder(layer));
        ecs.addComponent<engine::Scroll>(entity, SkyLayers[layer].speed);
    }
    for (const auto &[layer, object] : SkyObjects) {
        const ecs::entity_t entity = ecs.createEntity();
        ecs.addComponent<engine::SceneMember>(entity, scene);
        ecs.addComponent<engine::BackdropSprite>(
            entity, engine::BackdropSprite{
                        .texture = resources.loadTexture(object.path, render::TextureFiltering::Smooth),
                        .height = object.height,
                        .y = object.y,
                        .period = object.period,
                        .start = object.start,
                        .order = skyOrder(layer) + 1});
        ecs.addComponent<engine::Scroll>(entity, object.speed);
    }
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
                       const ShipSelection &ships, const std::vector<game::ShipDefinition> &shipDefinitions)
    : _engine(engine), _audio(audio), _metrics(metrics), _style(style), _sessions(sessions),
      _translations(translations), _ships(ships), _shipDefinitions(shipDefinitions), _recorder(engine)
{
    sessions.setKillHandler([this](const Kill &kill) {
        recordKill(kill);
        showKill(kill);
    });
    sessions.setWaveHandler([this](std::uint16_t wave, const std::string &music) {
        _recorder.recordWave(wave);
        showWave(wave);
        playWaveMusic(music);
    });
    sessions.setRescueHandler([this](game::RescueStage stage) { showRescue(stage); });
    sessions.setOutcomeHandler([this](game::GameOutcome outcome) {
        _recorder.recordOutcome(outcome);
        recordResults();
    });
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
    engine.createScene(ResultsScene, {.onBack = [this] { leaveResults(); }, .assets = ResultsAssets},
                       [this, &resources](engine::Engine & /*target*/, engine::SceneId scene) {
                           buildResults(scene, resources);
                       });
}

void GameScenes::play(std::unique_ptr<GameSession> session, std::function<void()> onEnd)
{
    _next = std::move(session);
    _nextEnd = std::move(onEnd);
    _results.reset();
    _xpAfter.reset();
    _engine.switchScene(GameScene);
}

void GameScenes::showResults(std::optional<std::uint64_t> accountXp, bool canSignIn,
                             std::function<void()> onContinue)
{
    _xpBefore = accountXp;
    _canSignIn = canSignIn;
    _onContinue = std::move(onContinue);
    _engine.switchScene(ResultsScene);
}

void GameScenes::setAccountXp(std::uint64_t xp)
{
    _xpAfter = xp;
    ecs::Ecs &ecs = _engine.getEcs();
    if (_resultsScreen && ecs.isAlive(*_resultsScreen)) {
        client::setAccountXp(ecs.getComponent<ResultsScreen>(_resultsScreen->id), xp);
    }
}

void GameScenes::leaveResults()
{
    _resultsScreen.reset();
    const std::function<void()> onContinue = std::exchange(_onContinue, nullptr);
    if (onContinue) {
        onContinue();
    } else {
        _engine.switchScene(MainMenuScene);
    }
}

void GameScenes::end()
{
    _audio.stopMusic();
    recordEnd();
    _sessions.stop();
    const std::function<void()> onEnd = std::exchange(_onEnd, nullptr);
    if (onEnd) {
        onEnd();
    } else {
        _engine.switchScene(MainMenuScene);
    }
}

void GameScenes::recordKill(const Kill &kill)
{
    if (const GameSession *session = _sessions.session()) {
        _recorder.recordKill(kill, session->localSlot());
    }
}

void GameScenes::recordEnd()
{
    const GameSession *session = _sessions.session();
    if (session == nullptr) {
        return;
    }
    const std::vector<game::PlayerState> &players = _sessions.players();
    const auto local = std::ranges::find(players, session->localSlot(), &game::PlayerState::slot);
    _recorder.recordEnd(local == players.end() ? 0 : local->score);
}

void GameScenes::showKill(const Kill &kill)
{
    const GameSession *session = _sessions.session();
    if (session == nullptr) {
        return;
    }
    const std::uint8_t local = session->localSlot();
    if (kill.kind == game::EntityKind::BydoBoss && _sessions.rescue() != game::RescueStage::None) {
        shakeGameCamera(FallShake);
    }
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

void GameScenes::recordResults()
{
    const GameSession *session = _sessions.session();
    if (session == nullptr || !session->isEndless()) {
        return;
    }
    const std::vector<game::PlayerState> &players = _sessions.players();
    const auto local = std::ranges::find(players, session->localSlot(), &game::PlayerState::slot);
    if (local == players.end()) {
        return;
    }
    _results = GameResults{.wavesCleared = local->wavesCleared,
                           .kills = local->kills,
                           .bestStreak = local->bestStreak,
                           .score = local->score,
                           .xp = local->xp};
}

void GameScenes::buildResults(engine::SceneId scene, render::ResourceManager &resources)
{
    ecs::Ecs &ecs = _engine.getEcs();
    createGameSky(ecs, scene, resources);
    const ResultsLook look{.captionFont = _style.toast.messageFont,
                           .valueFont = _style.toast.nameFont,
                           .sounds = {.gameOver = _audio.loadSound(RTYPE_SOUND_RESULTS_GAME_OVER),
                                      .stat = _audio.loadSound(RTYPE_SOUND_RESULTS_STAT),
                                      .tick = _audio.loadSound(RTYPE_SOUND_RESULTS_TICK),
                                      .counted = _audio.loadSound(RTYPE_SOUND_RESULTS_COUNTED),
                                      .levelUp = _audio.loadSound(RTYPE_SOUND_RESULTS_LEVEL_UP)},
                           .title = _translations.text("summary.title"),
                           .waves = _translations.text("summary.waves"),
                           .kills = _translations.text("summary.kills"),
                           .streak = _translations.text("summary.streak"),
                           .score = _translations.text("summary.score"),
                           .xp = _translations.text("progress.gained"),
                           .level = _translations.text("progress.level"),
                           .levelUp = _translations.text("progress.level_up"),
                           .guest = _canSignIn ? _translations.text("progress.guest") : std::string()};
    const ecs::entity_t screen =
        openResultsScreen(ecs, scene, _metrics, look, _results.value_or(GameResults{}), _xpBefore);
    if (_xpAfter) {
        client::setAccountXp(ecs.getComponent<ResultsScreen>(screen), *_xpAfter);
    }
    _resultsScreen = ecs.ref(screen);
    engine::InterfaceBuilder(ecs, scene, _style)
        .button(_translations.text("summary.continue"), [this] { leaveResults(); },
                {engine::AnchorPoint::Bottom, 0.0f, -ContinueAboveBottom});
}

void GameScenes::showWave(std::uint16_t wave)
{
    const GameSession *session = _sessions.session();
    if (wave == game::MaxWaveDifficulty && (session == nullptr || !session->isEndless())) {
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
    shakeGameCamera(StrikeShake);
}

void GameScenes::shakeGameCamera(float strength)
{
    ecs::Ecs &ecs = _engine.getEcs();
    if (_camera && ecs.isAlive(*_camera)) {
        shakeCamera(ecs.getComponent<RescueCamera>(_camera->id), strength);
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
    setConfiguredShipLooks(_sessions, resources, _shipDefinitions);
    _scene = scene;
    _prompt.reset();
    _deathLook = {resources.loadTexture(RTYPE_IMAGE_LIFE, render::TextureFiltering::Smooth),
                  resources.loadTexture(RTYPE_IMAGE_LIFE_LOST, render::TextureFiltering::Smooth),
                  style.toast.messageFont,
                  style.toast.nameFont,
                  _translations.text("death.lives"),
                  _translations.text("death.respawn")};

    ecs::Ecs &ecs = _engine.getEcs();

    createGameSky(ecs, scene, resources);
    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Camera>(sky, computeGameView(metrics.visibleArea()));
    const render::Rectangle area = metrics.visibleArea();
    const render::Vector2 centre{area.x + (area.width / 2.0f), area.y + (area.height / 2.0f)};
    ecs.addComponent<RescueCamera>(sky, area, 1.0f, 0.0f, centre, centre);
    _camera = ecs.ref(sky);
    ecs.addComponent<engine::Light>(sky, GameLight);

    openHud(ecs, scene, {_deathLook.life, _deathLook.lostLife, style.toast.nameFont}, game::Rules{}.lives);

    _onEnd = std::exchange(_nextEnd, nullptr);
    _sessions.start(_next ? std::move(_next) : std::make_unique<LocalSession>(), scene, [this] { end(); });
    _recorder.recordStart();
}

} // namespace rtype::client
