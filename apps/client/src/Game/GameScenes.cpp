#include "Game/GameScenes.hpp"

#include "Assets.hpp"
#include "Game/GameSky.hpp"
#include "Game/GameView.hpp"
#include "Game/Hud.hpp"
#include "Game/LocalSession.hpp"
#include "Game/RemoteSession.hpp"
#include "Game/RescueCameraSystem.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/MainMenu.hpp"
#include "MainMenu/SettingsPage.hpp"
#include "Settings/PlayerSettings.hpp"
#include "Settings/VoiceSettings.hpp"
#include "ShipModel.hpp"
#include "ShipSelector/ShipCatalog.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/game/Constants.hpp"
#include "rtype/game/Entities.hpp"
#include "rtype/game/Hitbox.hpp"
#include "rtype/game/Item/ItemRegistry.hpp"
#include "rtype/game/Projectile/ProjectileRegistry.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Ship/ShipRegistry.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
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
    {AssetType::SmoothTexture, RTYPE_IMAGE_VOICE_SPEAKING},
    {AssetType::SmoothTexture, RTYPE_IMAGE_VOICE_MUTED},
    {AssetType::SmoothTexture, RTYPE_IMAGE_VOICE_MUTED_BY_ME},
    {AssetType::SmoothTexture, RTYPE_IMAGE_VOICE_NO_MICROPHONE},
    {AssetType::SmoothTexture, RTYPE_IMAGE_VOICE_DEAFENED},
    {AssetType::SmoothTexture, RTYPE_IMAGE_VOICE_MUTED_BY_ADMIN},
    {AssetType::SmoothTexture, RTYPE_IMAGE_VOICE_BADGE},
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

/// The colours of the light around the shots, those of their energy; the alpha is how bright it is.
constexpr render::Color PlayerShotLight{80, 200, 255, 130};
constexpr render::Color BydoShotLight{185, 90, 255, 115};
constexpr render::Color PlasmaOrbLight{255, 120, 35, 135};
/// How far the light of a shot spreads around its core, in heights of the core.
constexpr float ShotLightSpread = 1.5f;

/// The light around the shots of the players by the mechanic of their weapon, when their projectile has a
/// look of its own: blue bullets, orange missiles, pink lasers, green plasma.
constexpr render::Color PlayerMissileLight{255, 150, 60, 130};
constexpr render::Color PlayerLaserLight{255, 90, 200, 130};
constexpr render::Color PlayerPlasmaLight{110, 255, 140, 130};
/// The light around the shots of the Bydos by the mechanic of their weapon, when their projectile has a look
/// of its own: purple bullets, red missiles, magenta lasers, orange plasma.
constexpr render::Color BydoMissileLight{255, 70, 50, 125};
constexpr render::Color BydoLaserLight{255, 60, 170, 120};
/// The core of a projectile model measured from its bounds: its front, at most this many times as long as it
/// is tall, the rest being its trail.
constexpr float ProjectileCoreRatio = 3.0f;

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

/// The icons of the voice, in the order of VoiceIcon from Speaking, and the badge above the ship of another
/// player who talks: right of its health bar, on its line (Figma, Série 16, 22 px scaled by 4/3).
constexpr std::array<const char *, VoiceIconCount> VoiceIconPaths{
    RTYPE_IMAGE_VOICE_SPEAKING,      RTYPE_IMAGE_VOICE_MUTED,    RTYPE_IMAGE_VOICE_MUTED_BY_ME,
    RTYPE_IMAGE_VOICE_NO_MICROPHONE, RTYPE_IMAGE_VOICE_DEAFENED, RTYPE_IMAGE_VOICE_MUTED_BY_ADMIN};
constexpr render::Rectangle VoiceIconSource{0.0f, 0.0f, 64.0f, 64.0f};
constexpr float VoiceBadgeSize = 29.3f;
/// From the line that tells what carries the voice of the player to the note under it, in the pause menu.
constexpr float VoiceNoteLine = 34.0f;

/// What carries the voice of another pilot, beside its name in the pause menu: nothing without a voice.
std::string backendLabel(const localization::Localization &translations, voice::Backend backend)
{
    switch (backend) {
    case voice::Backend::Game:
        return translations.text("game.voices.backend.game");
    case voice::Backend::Discord:
        return translations.text("game.voices.backend.discord");
    case voice::Backend::None:
        break;
    }
    return {};
}
constexpr float VoiceBadgeGap = 12.0f;

/// The volume of another player in the pause menu changes by this much.
constexpr float PlayerVolumeStep = 0.05f;

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

/// The flash of an item picked up.
constexpr float ItemPickupScale = 24.0f;
constexpr render::Color ItemPickupTint{255, 213, 79, 255};

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
    // An item picked up whose model has no Collect animation: a golden flash where it was
    sessions.setImpactLook(game::EntityKind::Item, loadImpactLook(resources, RTYPE_MODEL_IMPACT_BURST,
                                                                  ItemPickupScale, ItemPickupTint, energy));
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

/// The animations of the models of the items, by name: looped while an item lies in the game, and played once
/// where it is picked up.
constexpr const char *ItemIdleAnimation = "Idle";
constexpr const char *ItemCollectAnimation = "Collect";

/// An animation of the model of an item, if it has one by this name.
std::optional<ItemAnimation> findItemAnimation(render::ResourceManager &resources, render::ModelId model,
                                               const char *name)
{
    try {
        const int animation = resources.getModelAnimation(model, name);
        return ItemAnimation{animation, resources.getModelAnimationDuration(model, animation)};
    } catch (const std::out_of_range &) {
        return std::nullopt;
    }
}

/// The light of the shots of a player with this mechanic.
render::Color playerShotLight(game::ProjectileCategory mechanic)
{
    switch (mechanic) {
    case game::ProjectileCategory::Missile:
        return PlayerMissileLight;
    case game::ProjectileCategory::Laser:
        return PlayerLaserLight;
    case game::ProjectileCategory::Plasma:
        return PlayerPlasmaLight;
    case game::ProjectileCategory::Bullet:
        break;
    }
    return PlayerShotLight;
}

/// The light of the shots of a Bydo with this mechanic.
render::Color bydoShotLight(game::ProjectileCategory mechanic)
{
    switch (mechanic) {
    case game::ProjectileCategory::Missile:
        return BydoMissileLight;
    case game::ProjectileCategory::Laser:
        return BydoLaserLight;
    case game::ProjectileCategory::Plasma:
        return PlasmaOrbLight;
    case game::ProjectileCategory::Bullet:
        break;
    }
    return BydoShotLight;
}

/// The core of a projectile model, as in ShotLook, from its bounds: the models fly along +x, nose first, and
/// their trail behind is left out.
render::Rectangle measureProjectileCore(const render::Box3 &bounds)
{
    const float height = bounds.max.y - bounds.min.y;
    const float length = std::min(bounds.max.x - bounds.min.x, ProjectileCoreRatio * height);
    // Through the camera of the game, y points down: the models have y up
    return {bounds.max.x - length, -bounds.max.y, length, height};
}

/// Gives each projectile of the configuration its model, the asset its file names, at the size of its
/// collider: the shots of the players and of the Bydos are drawn with the model of their projectile
/// (game::EntityState::ship), the one of the ProjectileRegistry of the game, lit in the colour of the mechanic
/// of the weapon that fires it. The models of the shots of the Bydos measured by hand keep their core, facing
/// left. A projectile whose model cannot be loaded is drawn with the look of its kind.
void setConfiguredProjectileLooks(GameSessionSystem &sessions, render::ResourceManager &resources,
                                  const game::GameConfig &config)
{
    game::ProjectileRegistry projectiles;
    for (const game::ProjectileDefinition &definition : config.projectiles) {
        projectiles.addDefinition(definition);
    }
    const render::Energy energy{
        resources.loadTexture(RTYPE_IMAGE_ENERGY_NOISE, render::TextureFiltering::Smooth)};
    const render::TextureId light =
        resources.loadTexture(RTYPE_IMAGE_GLOW_CORE, render::TextureFiltering::Smooth);
    const std::size_t count = std::min<std::size_t>(projectiles.getDefinitionCount(), game::NoShip);
    for (std::size_t index = 0; index < count; ++index) {
        const game::ProjectileDefinition &projectile = projectiles.getDefinitionAt(index);
        if (projectile.model.empty()) {
            continue;
        }
        // The mechanic of the first weapon that fires it, its own otherwise
        game::ProjectileCategory mechanic = projectile.category;
        const auto weapon =
            std::ranges::find(config.weapons, projectile.id, &game::WeaponDefinition::projectile);
        if (weapon != config.weapons.end()) {
            mechanic = weapon->mechanic;
        }
        try {
            const render::ModelId model = resources.loadModel(projectile.model);
            const render::Rectangle core = projectile.model == RTYPE_MODEL_SHOT_BASIC
                                               ? ShotBasicCore
                                               : measureProjectileCore(resources.getModelBounds(model));
            const physics::Vec3 half = game::halfSizeOf(projectile.collider);
            const render::Vector2 size{half.x * 2.0f, half.y * 2.0f};
            const auto slot = static_cast<std::uint8_t>(index);
            sessions.setProjectileLook(
                slot, {model, core, false, shine(energy, light, core, playerShotLight(mechanic))}, size);
            const bool spiral = projectile.model == RTYPE_MODEL_SHOT_SPIRAL;
            const bool orb = projectile.model == RTYPE_MODEL_ORB_PLASMA;
            const render::Rectangle bydoCore = spiral ? ShotSpiralCore : orb ? OrbPlasmaCore : core;
            sessions.setBydoProjectileLook(
                slot,
                {model, bydoCore, spiral || orb, shine(energy, light, bydoCore, bydoShotLight(mechanic))},
                size);
        } catch (const std::runtime_error &error) {
            log::Logger("game").warning("The model of the projectile '{}' cannot be loaded: {}",
                                        projectile.id, error.what());
        }
    }
}

/// Gives each item of the configuration its model, the asset its file names, at the size of its collider:
/// each item is drawn with the model of its index (game::EntityState::ship), the one of the ItemRegistry of
/// the game. An item whose model cannot be loaded is drawn as a box.
void setConfiguredItemLooks(GameSessionSystem &sessions, render::ResourceManager &resources,
                            const std::vector<game::ItemDefinition> &definitions)
{
    game::ItemRegistry items;
    for (const game::ItemDefinition &definition : definitions) {
        items.addDefinition(definition);
    }
    const std::size_t count = std::min<std::size_t>(items.getDefinitionCount(), game::NoShip);
    for (std::size_t index = 0; index < count; ++index) {
        const game::ItemDefinition &item = items.getDefinitionAt(index);
        try {
            const render::ModelId model = resources.loadModel(item.model);
            const physics::Vec3 half = game::halfSizeOf(item.collider);
            sessions.setItemLook(static_cast<std::uint8_t>(index),
                                 {model,
                                  resources.getModelBounds(model),
                                  {half.x * 2.0f, half.y * 2.0f},
                                  findItemAnimation(resources, model, ItemIdleAnimation),
                                  findItemAnimation(resources, model, ItemCollectAnimation)});
        } catch (const std::runtime_error &error) {
            log::Logger("game").warning("The model of the item '{}' cannot be loaded: {}", item.id,
                                        error.what());
        }
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

/// Fades in the HUD of a game opened from the hangar, as the camera pulls back: what shows of it, the parts
/// hidden for now staying hidden.
void fadeInHud(ecs::Ecs &ecs, engine::SceneId scene)
{
    auto &members = ecs.cache<engine::SceneMember>();
    auto &drawables = ecs.cache<engine::Drawable>();
    auto &opacities = ecs.cache<engine::Opacity>();
    for (ecs::entity_t entity = 0; entity < drawables.size(); ++entity) {
        if (!drawables.has(entity) || drawables.get(entity).layer != engine::Layer::Interface ||
            !members.has(entity) || members.get(entity).scene != scene ||
            (opacities.has(entity) && opacities.get(entity).value < 1.0f)) {
            continue;
        }
        opacities.emplace(entity, 0.0f);
        ecs.addComponent<engine::Tween>(entity, engine::Tween{.target = engine::TweenTarget::Opacity,
                                                              .from = 0.0f,
                                                              .to = 1.0f,
                                                              .duration = ArrivalDuration,
                                                              .easing = engine::Easing::EaseOut});
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
                       ShipSelection &ships, const std::vector<game::ShipDefinition> &shipDefinitions,
                       const std::vector<game::ItemDefinition> &itemDefinitions,
                       const game::GameConfig &config)
    : _engine(engine), _audio(audio), _metrics(metrics), _style(style), _sessions(sessions),
      _translations(translations), _ships(ships), _shipDefinitions(shipDefinitions),
      _itemDefinitions(itemDefinitions), _config(config), _recorder(engine)
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
         .assets = GameAssets,
         .keepsAssets = true},
        [this, &resources, &audio, &metrics, &style](engine::Engine & /*target*/, engine::SceneId scene) {
            build(scene, resources, audio, metrics, style);
        });
    engine.createScene(PauseScene, {.overlay = true, .onBack = [this] { resume(); }},
                       [this](engine::Engine & /*target*/, engine::SceneId scene) { buildPause(scene); });
    engine.createScene(ResultsScene,
                       {.onBack = [this] { leaveResults(); }, .assets = ResultsAssets, .keepsAssets = true},
                       [this, &resources](engine::Engine & /*target*/, engine::SceneId scene) {
                           buildResults(scene, resources);
                       });
}

RemoteSession *GameScenes::onlineSession() const
{
    return dynamic_cast<RemoteSession *>(_sessions.session());
}

void GameScenes::keepPlayerVoices()
{
    const RemoteSession *session = onlineSession();
    if (session == nullptr || session->voiceDevices() == nullptr) {
        return;
    }
    engine::Settings &settings = _engine.getSettings();
    keepVoiceSettings(settings, session->voiceDevices()->preferences);
    writeSettings(settings);
}

void GameScenes::resume()
{
    keepPlayerVoices();
    _engine.popScene();
}

void GameScenes::buildPause(engine::SceneId scene)
{
    ecs::Ecs &ecs = _engine.getEcs();
    engine::InterfaceBuilder interface(ecs, scene, _style);
    settings_page::addVeil(interface, ecs);
    const localization::Localization &translations = _translations;
    interface.boundHeader([&translations] { return translations.text("game.pause"); }, [this] { resume(); },
                          {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});

    const float width = _style.setting.width;
    const float left = screenMargin;
    float top = settings_page::addGroup(interface, ecs, _style, translations, "game.group.game", left,
                                        settings_page::top, width);
    const ecs::entity_t resumeRow = interface.linkRow(translations.text("game.resume"), [this] { resume(); },
                                                      {engine::AnchorPoint::TopLeft, left, top});
    settings_page::bindText(ecs, resumeRow, translations, "game.resume");
    top += _style.setting.height;
    const ecs::entity_t leaveRow = interface.linkRow(translations.text("game.leave"),
                                                     [this] {
                                                         keepPlayerVoices();
                                                         end();
                                                     },
                                                     {engine::AnchorPoint::TopLeft, left, top});
    settings_page::bindText(ecs, leaveRow, translations, "game.leave");
    addFriends(interface, left, top + _style.setting.height + settings_page::groupGap);

    addPlayerVoices(interface, left + width + settings_page::columnGap);
}

void GameScenes::addFriends(engine::InterfaceBuilder &interface, float left, float top)
{
    constexpr std::size_t MaxFriendsShown = 5;
    if (_friends == nullptr || !_friends->ready() || onlineSession() == nullptr) {
        return;
    }
    const std::vector<FriendRow> rows =
        friendRows(*_friends, FriendsView::Game, _translations, MaxFriendsShown);
    if (rows.empty()) {
        return;
    }
    ecs::Ecs &ecs = _engine.getEcs();
    top = settings_page::addGroup(interface, ecs, _style, _translations, "game.group.friends", left, top,
                                  _style.setting.width);
    for (const FriendRow &row : rows) {
        const auto act = [this, row] {
            const auto told = [this, name = row.name](bool sent) {
                _engine.showToast(sent ? std::format("{} {}", _translations.text("friends.invite_sent"), name)
                                       : _translations.text("friends.not_sent"),
                                  sent ? engine::ToastType::Success : engine::ToastType::Error);
            };
            if (_friends == nullptr) {
                return;
            }
            if (row.action == FriendRow::Action::AcceptRequest) {
                _friends->accept(
                    row.id, [told](bool accepted, const std::optional<std::string> &) { told(accepted); });
            } else if (row.action == FriendRow::Action::Invite) {
                _friends->invite(row.id, told);
            }
        };
        interface.linkRow(std::format("{} · {} · {}", row.side, row.name, row.detail), act,
                          {engine::AnchorPoint::TopLeft, left, top});
        top += _style.setting.height;
    }
}

void GameScenes::addPlayerVoices(engine::InterfaceBuilder &interface, float left)
{
    const RemoteSession *session = onlineSession();
    const std::optional<VoiceView> view = session != nullptr ? session->voiceView() : std::nullopt;
    if (!view || view->others.empty() || session->voiceDevices() == nullptr) {
        return;
    }
    ecs::Ecs &ecs = _engine.getEcs();
    const localization::Localization &translations = _translations;
    const float width = _style.setting.width;
    float top = settings_page::addGroup(interface, ecs, _style, translations, "game.group.voices", left,
                                        settings_page::top, width);
    VoicePreferences &preferences = session->voiceDevices()->preferences;
    const std::map<std::uint8_t, std::string> names = session->playerNames();
    for (const voice::Participant &pilot : view->others) {
        const std::optional<std::string> key = session->voiceKey(pilot.slot);
        const auto name = names.find(pilot.slot);
        if (!key || name == names.end()) {
            continue;
        }
        // A player not changed yet is heard at full volume: it is kept once changed
        const auto choice = [&preferences, key = *key]() -> PlayerVoice & {
            return preferences.players[key];
        };
        const ecs::entity_t slider = interface.slider(
            name->second, [choice] { return choice().volume; },
            [choice](float volume) { choice().volume = volume; }, PlayerVolumeStep,
            {engine::AnchorPoint::TopLeft, left, top});
        engine::Slider &row = ecs.getComponent<engine::Slider>(slider);
        // Muted by an administrator: what the player chose cannot change it, the row is locked
        const auto byAdmin = [this, slot = pilot.slot] {
            const RemoteSession *online = onlineSession();
            const std::optional<VoiceView> voice = online != nullptr ? online->voiceView() : std::nullopt;
            if (!voice) {
                return false;
            }
            const auto found = std::ranges::find(voice->others, slot, &voice::Participant::slot);
            return found != voice->others.end() && found->status.mutedByAdmin;
        };
        row.locked = byAdmin;
        row.label = [this, slot = pilot.slot, choice, byAdmin, &translations] {
            if (byAdmin()) {
                return translations.text("game.voices.muted_by_admin");
            }
            if (choice().muted) {
                return translations.text("game.voices.muted");
            }
            // What carries its voice, from the roster of the game
            const RemoteSession *online = onlineSession();
            const std::optional<VoiceView> voice = online != nullptr ? online->voiceView() : std::nullopt;
            if (!voice) {
                return std::string();
            }
            const auto found = std::ranges::find(voice->others, slot, &voice::Participant::slot);
            return found != voice->others.end() ? backendLabel(translations, found->backend) : std::string();
        };
        row.onConfirm = [choice] { choice().muted = !choice().muted; };
        row.icon = [this, slot = pilot.slot]() -> std::optional<engine::Slider::Icon> {
            const RemoteSession *online = onlineSession();
            const std::optional<VoiceView> voice = online != nullptr ? online->voiceView() : std::nullopt;
            if (!voice) {
                return std::nullopt;
            }
            const auto found = std::ranges::find(voice->others, slot, &voice::Participant::slot);
            const VoiceIcon icon = found == voice->others.end() ? VoiceIcon::None : voiceIconOf(*found);
            const render::TextureId *texture = voiceIconTexture(_voiceIcons, icon);
            if (texture == nullptr) {
                return std::nullopt;
            }
            return engine::Slider::Icon{*texture, VoiceIconSource, voiceIconColor(icon)};
        };
        top += _style.slider.getHeight();
    }
    // What carries the voice of the player, then where the choices are kept
    const ecs::entity_t through = interface.note(
        {}, {engine::AnchorPoint::TopLeft, left + _style.setting.inset, top + settings_page::groupGap});
    ecs.addComponent<engine::BoundText>(through, [this, &translations] {
        const RemoteSession *online = onlineSession();
        const std::optional<VoiceView> voice = online != nullptr ? online->voiceView() : std::nullopt;
        const voice::Backend backend = voice ? voice->local.backend : voice::Backend::None;
        return translations.text(backend == voice::Backend::Discord ? "game.voices.through.discord"
                                 : backend == voice::Backend::Game  ? "game.voices.through.game"
                                                                    : "game.voices.through.none");
    });
    const ecs::entity_t note = interface.note(translations.text("game.voices.note"),
                                              {engine::AnchorPoint::TopLeft, left + _style.setting.inset,
                                               top + settings_page::groupGap + VoiceNoteLine});
    settings_page::bindText(ecs, note, translations, "game.voices.note");
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
    // Over the game, on the board in a VR headset: it says what to do in the game
    ecs.addComponent<engine::OnSurface>(prompt, engine::Surface::Board);
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
    setConfiguredItemLooks(_sessions, resources, _itemDefinitions);
    setConfiguredProjectileLooks(_sessions, resources, _config);
    _scene = scene;
    _prompt.reset();
    _deathLook = {resources.loadTexture(RTYPE_IMAGE_LIFE, render::TextureFiltering::Smooth),
                  resources.loadTexture(RTYPE_IMAGE_LIFE_LOST, render::TextureFiltering::Smooth),
                  style.toast.messageFont,
                  style.toast.nameFont,
                  _translations.text("death.lives"),
                  _translations.text("death.respawn")};

    ecs::Ecs &ecs = _engine.getEcs();

    // Opened from the take-off of the hangar: on the shot it ends with, its sky going on
    const std::optional<float> arrival = std::exchange(_ships.arrivalSky, std::nullopt);
    createGameSky(ecs, scene, resources, arrival.value_or(0.0f));
    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    const render::Rectangle area = metrics.visibleArea();
    const render::Vector2 centre{area.x + (area.width / 2.0f), area.y + (area.height / 2.0f)};
    RescueCamera &rescue = ecs.addComponent<RescueCamera>(sky, area, 1.0f, 0.0f, centre, centre);
    render::Camera view = computeGameView(area);
    if (arrival) {
        const ecs::Position spawn = game::getShipSpawn(_next ? _next->localSlot() : 0);
        rescue.arrival = 0.0f;
        rescue.ship = {spawn.x, spawn.y};
        view = computeGameView(computeArrivalFrame(area, rescue.ship));
    }
    ecs.addComponent<engine::Camera>(sky, view);
    _camera = ecs.ref(sky);
    ecs.addComponent<engine::Light>(sky, GameLight);

    for (std::size_t index = 0; index < VoiceIconPaths.size(); ++index) {
        _voiceIcons[index] = resources.loadTexture(VoiceIconPaths[index], render::TextureFiltering::Smooth);
    }
    _sessions.setVoiceBadgeLook(
        {.texture = resources.loadTexture(RTYPE_IMAGE_VOICE_BADGE, render::TextureFiltering::Smooth),
         .source = VoiceIconSource,
         .size = VoiceBadgeSize,
         .x = (AllyHealthBar.width / 2.0f) + VoiceBadgeGap + (VoiceBadgeSize / 2.0f),
         .y = -AllyHealthBar.raise});
    std::map<std::string, std::string> weaponNames;
    for (const game::WeaponUnlock &unlock : _config.weaponUnlocks) {
        weaponNames.emplace(unlock.weapon, _translations.text("weapon." + unlock.weapon));
    }
    weaponNames.emplace(_config.player.weapon, _translations.text("weapon." + _config.player.weapon));
    openHud(ecs, scene,
            {.life = _deathLook.life,
             .lostLife = _deathLook.lostLife,
             .font = style.toast.nameFont,
             .voice = _voiceIcons,
             .words = {.wave = _translations.text("hud.wave"),
                       .endless = _translations.text("hud.endless"),
                       .boss = _translations.text("hud.boss"),
                       .online = _translations.text("hud.online"),
                       .solo = _translations.text("hud.solo"),
                       .pilots = _translations.text("hud.pilots"),
                       .alone = _translations.text("hud.alone"),
                       .weapons = std::move(weaponNames)}},
            game::Rules{}.lives);
    if (arrival) {
        fadeInHud(ecs, scene);
    }

    _onEnd = std::exchange(_nextEnd, nullptr);
    _sessions.start(_next ? std::move(_next) : std::make_unique<LocalSession>(), scene, [this] { end(); });
    _recorder.recordStart();
}

} // namespace rtype::client
