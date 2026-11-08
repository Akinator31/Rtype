#pragma once

/// Paths of the assets of the client, relative to the assets directory (apps/client/assets, copied next to
/// the executable). Change a path here to replace an asset everywhere it is used.

// Fonts
#define RTYPE_FONT_TITLE "fonts/FiraSans-Black.ttf"
#define RTYPE_FONT_BUTTON "fonts/Exo2-SemiBold.ttf"
#define RTYPE_FONT_FLARE "fonts/Orbitron-Medium.ttf"
#define RTYPE_FONT_DISPLAY "fonts/Audiowide-Regular.ttf"

// Images
#define RTYPE_IMAGE_STARFIELD "images/starfield.png"
#define RTYPE_IMAGE_DISCORD "images/discord.png"
// The sky of the game, from the back to the front: the layers cover the screen and join on their sides
#define RTYPE_IMAGE_SKY_DEEP "images/backgrounds/space_bg_0_deep.png"
#define RTYPE_IMAGE_SKY_NEBULA "images/backgrounds/space_bg_1_nebula.png"
#define RTYPE_IMAGE_SKY_STARS "images/backgrounds/space_bg_2_stars.png"
#define RTYPE_IMAGE_SKY_NEAR "images/backgrounds/space_bg_3_near.png"
#define RTYPE_IMAGE_SKY_GALAXY "images/backgrounds/space_galaxy.png"
#define RTYPE_IMAGE_SKY_MOON "images/backgrounds/space_moon.png"
#define RTYPE_IMAGE_SKY_PLANET "images/backgrounds/space_planet_ringed.png"
#define RTYPE_IMAGE_LOGO "images/rtpye_logo_without_flare.png"
#define RTYPE_IMAGE_LOGO_FLARE "images/flare.png"
#define RTYPE_IMAGE_ENERGY_NOISE "images/fx/energy_noise.png"
#define RTYPE_IMAGE_GLOW_CORE "images/fx/glow_core.png"
#define RTYPE_IMAGE_LIFE "images/hud/life.png"
#define RTYPE_IMAGE_LIFE_LOST "images/hud/life_lost.png"

// Texts
// The story of the game, in each language, laid out for the crawl of the lore scene (see LoreLineKind)
#define RTYPE_LORE_FRENCH "lore/fr.txt"
#define RTYPE_LORE_ENGLISH "lore/en.txt"

// Models
#define RTYPE_MODEL_PLANET "models/SC_PlanetSystem_export/planet.glb"
#define RTYPE_MODEL_PLANET_DUST "models/SC_PlanetSystem_export/planet_dust.glb"
#define RTYPE_MODEL_RING_ASTEROIDS_INNER "models/SC_PlanetSystem_export/ring_asteroids_inner.glb"
#define RTYPE_MODEL_RING_ASTEROIDS_OUTER "models/SC_PlanetSystem_export/ring_asteroids_outer.glb"
#define RTYPE_MODEL_RING_DUST "models/SC_PlanetSystem_export/ring_dust.glb"
#define RTYPE_MODEL_STATION "models/SC_FabStation_export/station_fab.glb"
#define RTYPE_MODEL_SHIP_EZNO "models/spaceship_ezno.glb"
#define RTYPE_MODEL_SHIP_SPACESHIP "models/spaceship.glb"
#define RTYPE_MODEL_SHIP_BATTLESTAR "models/battlestar_galactica_ship.glb"
#define RTYPE_MODEL_SHIP_DROP_SHIP "models/drop_ship_v2.glb"
#define RTYPE_MODEL_SHIP_BATTLESHIP "models/battleship.glb"
#define RTYPE_MODEL_SHIP_CAVED "models/spaceship_caved.glb"
#define RTYPE_MODEL_BOSS_SHOGUN "models/boss_shogun.glb"
#define RTYPE_MODEL_PEDESTAL "models/scifi_gadget.glb"
#define RTYPE_MODEL_SHOT_BASIC "models/projectiles/player/shot_basic.glb"
#define RTYPE_MODEL_SHOT_SPIRAL "models/projectiles/enemy/shot_spiral.glb"
#define RTYPE_MODEL_ORB_PLASMA "models/projectiles/enemy/orb_plasma.glb"
#define RTYPE_MODEL_IMPACT_SPARK "models/fx/impacts/impact_spark.glb"
#define RTYPE_MODEL_IMPACT_BURST "models/fx/impacts/impact_burst.glb"
#define RTYPE_MODEL_IMPACT_SPLASH "models/fx/impacts/impact_splash.glb"
#define RTYPE_MODEL_IMPACT_EXPLOSION "models/fx/impacts/impact_explosion.glb"

// Sounds
#define RTYPE_SOUND_BASICAL_BLASTER "sounds/fx/blaster/basical_blaster.wav"
#define RTYPE_SOUND_LOW_BLASTER "sounds/fx/blaster/low_blaster.wav"
#define RTYPE_SOUND_BIG_BLASTER "sounds/fx/blaster/big_blaster.wav"
#define RTYPE_SOUND_DESTRUCTION "sounds/fx/destruction.wav"
#define RTYPE_SOUND_COLLISION "sounds/fx/collision.wav"
#define RTYPE_SOUND_SABER "sounds/fx/saber/laser_saber.wav"
#define RTYPE_SOUND_DASH_WIND "sounds/fx/saber/wing_giant_flap.wav"
/// The voice played when the allies are called to the rescue against the final boss, offline: its length is
/// game::Rules::rescueVoiceTicks. Not in the assets yet: the rescue goes on silently without it.
#define RTYPE_SOUND_RESCUE_VOICE "sounds/rescue_voice.wav"
/// The music of the lore scene, played once while the story scrolls: about as long as the crawl.
#define RTYPE_MUSIC_LORE "sounds/lore.mp3"
/// The results of a game online (ResultsScreen): the game over as the screen opens, a figure that lands, the
/// experience counting up, then counted, and a level gained.
#define RTYPE_SOUND_RESULTS_GAME_OVER "sounds/fx/results/game_over.wav"
#define RTYPE_SOUND_RESULTS_STAT "sounds/fx/results/stat.wav"
#define RTYPE_SOUND_RESULTS_TICK "sounds/fx/results/tick.wav"
#define RTYPE_SOUND_RESULTS_COUNTED "sounds/fx/results/counted.wav"
#define RTYPE_SOUND_RESULTS_LEVEL_UP "sounds/fx/results/level_up.wav"
#define RTYPE_SOUND_BUTTON_HOVER "sounds/fx/button/hover.wav"
#define RTYPE_SOUND_BUTTON_CLICK "sounds/fx/button/click.wav"
