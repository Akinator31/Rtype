# How to configure enemies and weapons

This guide adds a new enemy to the game, with its ship, its weapon and the projectiles it fires, then puts it
in a wave, without writing C++ or recompiling: each one is a Lua file of the `config/` folder. It is for anyone
who designs the content of the game.

The format and its rules are decided in [ADR 0018](../decisions/0018-lua-game-config.md) and
[ADR 0019](../decisions/0019-luabridge3-scripting-library.md); how the loader works is in
[Game](../architecture/game.md#configuration).

## Prerequisites

- The project builds: see [Build](../getting-started/build.md).
- Basic Lua: a file returns a table, `return { name = value, ... }`. Strings are in quotes, numbers are bare,
  `--` starts a comment.

## Where the files are

```
config/
  projectiles/   what a weapon fires, grouped by bullet/, missile/, laser/ and plasma/
  weapons/       how it fires, grouped by bullet/, missile/, laser/ and plasma/
  ships/         what a ship looks and sounds like, whoever flies it
  enemies/       a kind of Bydo: its ship, its health, its speed, its weapon
  waves/         a group of enemies that appear one after the other, and the items they bring
  items/         what the ships of the players pick up: its model, its hitbox, its effects
  player.lua     what every player starts with: its ship, its weapon, its health
  progression.lua  the experience the players earn online, optional
  hangar.lua     the level of the account each ship and each weapon of the hangar needs, and the
                 characteristics of the ships, optional
```

- **One file per definition.** The file returns the table of one projectile, one weapon... Its name is free
  (`bydo_scout.lua` for the enemy `bydo_scout` is the habit); the **id** inside is what the other files use.
- Sub-folders are read too (`enemies/bosses/dobkeratops.lua`), and every file not ending in `.lua` is ignored.
- The server and the client read the **same files**. The build copies `config/` next to them
  (`build/bin/config/`), and the release archives ship it there; the server takes another folder with
  `--config <dir>`.
- The files already in `config/` define the game and additional projectile variants, including weapons,
  ships, enemies, waves and the player's starting equipment.

## Steps

1. **A projectile.** Create `config/projectiles/bullet/spark.lua`:

    ```lua
    return {
        id = "spark",
        category = "bullet",
        collider = { shape = "box", halfWidth = 10, halfHeight = 3 },
    }
    ```

2. **A weapon** that fires it, `config/weapons/bullet/spark_gun.lua`:

    ```lua
    return {
        id = "spark_gun",
        projectile = "spark",     -- the id of the projectile
        projectileSpeed = 1200,
        fireInterval = 15,        -- ticks: 4 shots per second
        magazineSize = 6,
        reloadTicks = 120,
        damage = 15,              -- hit points each spark takes from a player ship
    }
    ```

3. **A ship**, `config/ships/bydo_hunter.lua`. Its model is a path under the assets of the client
   (`apps/client/assets/`):

    ```lua
    return {
        id = "bydo_hunter",
        model = "models/spaceship_ezno.glb",
    }
    ```

4. **The enemy**, `config/enemies/bydo_hunter.lua`:

    ```lua
    return {
        id = "bydo_hunter",
        ship = "bydo_hunter",     -- the id of the ship
        health = 50,
        speed = 320,
        weapon = "spark_gun",     -- the id of the weapon
        score = 200,              -- points for the player who destroys it
    }
    ```

5. **A wave** to make it appear, `config/waves/wave3/hunters.lua`. A game plays five waves, one of each difficulty,
   drawn among the waves of that difficulty: this one may be the third wave of a game.

    ```lua
    return {
        name = "hunters",
        label = "Hunters",
        difficulty = 3,
        placement = "random",
        enemy = "bydo_hunter",
        quantity = 4,
        spawnInterval = 1.5,
    }
    ```

6. **Check it.** Rebuild, which copies `config/` next to the programs, then let the server read it:

    ```sh
    cmake --build build --target r-type_server
    ./build/bin/r-type_server --key keys/server.key --check
    ```

    ```
    INFO  master   Game configuration /home/me/Rtype/build/bin/config: 4 projectiles, 4 weapons, 3 ships, 3 enemies, 7 waves
    ```

    Without the keys of the server, the tests check the configuration of the repository too:
    `./build/bin/rtype_tests "The configuration of the repository loads, and a Simulation registers it"`.

## Fields

A field marked *required* must be there; the others take the default shown. A field that is not in these
tables is an error, so that a misspelt name never goes unnoticed. Durations are in ticks (60 per second),
distances in game units (the game area is 1920 × 1080), speeds in game units per second. Whole numbers may be
written `5` or `5.0`, not `5.5`.

### Projectiles (`config/projectiles/`)

| Field | Type | Default | What it is |
|---|---|---|---|
| `id` | string | *required* | Unique among the projectiles |
| `model` | string | `""` | Path of the 3D model, under the assets of the client. The client draws the shots of the players and of the Bydos with it, to the width of the collider: give the collider the proportions of the core of the model. Without one, a shot is drawn with the model of its kind (shot of a player, plasma orb or other shot of a Bydo) |
| `category` | `"bullet"`, `"missile"`, `"laser"` or `"plasma"` | `"bullet"` | How it moves: straight, speeding up, straight, zigzagging ([behaviors](../architecture/game.md#projectiles)) |
| `collider` | table | *required* | The shape that hits, see below |

The `collider` is a box or a sphere, centred on the projectile:

| Field | Type | Default | What it is |
|---|---|---|---|
| `shape` | `"box"` or `"sphere"` | *required* | |
| `halfWidth`, `halfHeight` | number above 0 | *required* for a box | Half its size |
| `radius` | number above 0 | *required* for a sphere | |
| `offsetX`, `offsetY` | number | `0` | Shift from the centre of the projectile |
| `trigger` | `true` or `false` | `false` | Detects without pushing |

### Weapons (`config/weapons/`)

| Field | Type | Default | What it is |
|---|---|---|---|
| `id` | string | *required* | Unique among the weapons |
| `projectile` | string | *required* | The id of the projectile it fires |
| `mechanic` | `"bullet"`, `"missile"`, `"laser"` or `"plasma"` | `"bullet"` | How its projectiles move, whatever their own category |
| `projectileSpeed` | number above 0 | `1500` | Speed the projectiles leave with, forward: to the right for a player, to the left for a Bydo |
| `fireInterval` | whole number from 1 | `10` | Ticks between two shots while firing |
| `range` | number, 0 or more | `0` | Distance a projectile flies before it vanishes, 0 for no limit |
| `magazineSize` | whole number, 0 or more | `0` | Shots before reloading, 0 for a weapon that never runs out |
| `reloadTicks` | whole number, 0 or more | `0` | Ticks to reload an empty magazine |
| `damage` | whole number from 1 to 255 | `10` | Hit points each projectile takes from what it hits |

### Ships (`config/ships/`)

| Field | Type | Default | What it is |
|---|---|---|---|
| `id` | string | *required* | Unique among the ships |
| `model` | string | *required* | Path of the 3D model, under the assets of the client |
| `sounds` | table | no sound | `move`, `destruction`, `collision`: paths of sounds, each optional (`""` for none) |
| `collider` | table, like that of a projectile | a box, `halfWidth = 48, halfHeight = 24` | The shape that hits and is hit. The client draws the model of the Bydos to the width of this box: give it the proportions of the hull of the model ([the client in a game](../architecture/client-game.md)). Ignored for the ship of the players, whose size is that of the rules of the game |

### Enemies (`config/enemies/`)

| Field | Type | Default | What it is |
|---|---|---|---|
| `id` | string | *required* | Unique among the enemies |
| `ship` | string | *required* | The id of its ship |
| `health` | whole number from 1 to 255 | *required* | Hit points it starts with |
| `speed` | number above 0 | `300` | Top speed |
| `acceleration` | number, 0 or more | `600` | Speed gained per second until the top speed, 0 for its top speed at once |
| `weapon` | string | *required* | The id of its weapon |
| `score` | whole number, 0 or more | `100` | Points for the player who destroys it |
| `xp` | whole number, 0 or more | a tenth of `score` | Experience for the player who destroys it, online only, times its streak ([The experience](#the-experience-configprogressionlua)) |
| `collisionDamage` | whole number from 0 to 255 | `20` | Hit points a player ship loses when it runs into it; the enemy is destroyed, and the player scores it, unless it is a boss, which keeps going |
| `update` | function | none | Its behavior, called at every tick: see [Script the behavior of an enemy](script-enemy-behaviors.md). Without one, it flies left and fires as soon as it is in the game area |
| `phases` | whole number from 1 to 255 | `1` | How many times its `health` is filled: once it runs out, it gets all of it back, until its last phase. A boss lasts `health × phases` hit points |
| `boss` | boolean | `false` | The clients draw it as a boss (the shogun model) instead of a Bydo ship |

### Waves (`config/waves/`)

| Field | Type | Default | What it is |
|---|---|---|---|
| `name` | string | *required* | Unique among the waves |
| `label` | string | `""` | Short text shown to the players |
| `description` | string | `""` | Longer text |
| `difficulty` | whole number from 1 to 6 | *required* | How hard the wave is, 1 the easiest, 6 the final boss |
| `placement` | `"manual"`, `"random"` or `"music"` | *required* | Each enemy listed with its position, one kind of enemy placed at random, or enemies that come on the beats of a music |
| `spawnInterval` | number, 0 or more | `1` | Seconds between two enemies, 0 for all at once |
| `music` | string, at most 128 bytes | `""` | Music the players hear when the wave starts, a path in the assets of the client (`"sounds/boss_theme.mp3"`); none: the music playing goes on. *Required* for a music wave, which follows it |
| `spawns` | list of `{ enemy = "...", x = ..., y = ... }` | | Manual only, *required*: each enemy and where it appears, in order |
| `enemy` | string | | Random only, *required*: the id of the kind of every enemy |
| `area` | `{ minX, maxX, minY, maxY }` | a band just past the right edge, over the whole height | Random only: where they appear; each bound optional |
| `quantity` | whole number from 1 to 65535 | the number of `spawns` | How many enemies; *required* for a random wave; not allowed for a music wave (it is the number of beats found) |
| `beats` | `{ bass = {...}, mid = {...}, high = {...} }` | | Music only, *required*: what comes at the beats of each band, each band optional (see below) |
| `lead` | number, 0 or more | `1.5` | Music only: seconds an enemy needs to cross the screen. It appears this long before its beat, so that it is in sight on it |

| `items` | list of `{ item = "...", quantity = ..., maxQuantity = ... }` | none | The items that appear during the wave, whatever its placement (see [Items](#items-configitems)) |

A manual wave sets no `enemy` and a random wave no `spawns`: a wave does not mix the modes.

#### Music waves

A `"music"` wave brings the Bydos in on the rhythm of an mp3 file. At load, the server and the client analyse
the file (`libs/beat`): it is cut in three bands of frequencies, and each attack found is a beat of the band it
is the clearest in, `bass` (the kick drum), `mid` (the snare, the voices) or `high` (the cymbals). Each band you
list in `beats` has:

| Field | Type | Default | What it is |
|---|---|---|---|
| `enemies` | list of strings | *required to be used* | The ids of the kinds of enemies; one of them, drawn, comes at each beat of the band. A band with none is ignored |
| `minGap` | number, 0 or more | `0.25` | Seconds: no two beats of the band closer than this. Raise it if the wave is too busy |

The enemies appear in five lanes of the `area` (the same default as a random wave), never in the lane of
one of the two beats before, so that they come from everywhere. The positions are fixed by the name of the wave:
every game of it is the same. The file is looked for in `assets/` next to `config/` (or `apps/client/assets/` for
the configuration of the repository), so a server needs the music beside its configuration, which its package
installs. Only the mp3 and wav formats are read.

```lua
return {
    name = "drums_style", difficulty = 3, placement = "music",
    music = "sounds/drums_style.mp3",
    beats = {
        bass = { enemies = { "bydo_heavy" }, minGap = 4.5 },
        mid = { enemies = { "bydo_scout", "bydo_gunner" }, minGap = 3.5 },
        high = { enemies = { "bydo_scout" }, minGap = 3.5 },
    },
}
```

The wave lasts as long as the music and ends once its last enemy is gone. `config/waves/wave3/drums_style.lua` and
`wave4/the_sports_music.lua` are the examples; `wave1/space_voyager.lua`, `wave2/christmas_rock.lua`,
`wave5/punk.lua`, `wave5/epic_anthem.lua`, `wave1/cyberpunk_short.lua` and `wave4/cyberpunk_main.lua` follow the other tracks.

A game plays the waves by difficulty, from 1 to 6: for each one, a wave drawn among those of that difficulty.
The repository keeps its examples in `config/waves/wave1/` through `wave5/`, with four choices
per level, and the final boss in `wave6/`: the shogun (`config/enemies/bydo_shogun.lua`), alone, in four
phases. The loader also accepts Lua files elsewhere under `config/waves/`.
A difficulty without a wave is skipped, and the game is won once the last wave is over.

### Items (`config/items/`)

An item appears at a random place of the screen and drifts to the left; the ship of a player that touches it
picks it up and gets its **effects**, in order. The behaviors are written in C++
(`libs/game/src/Item/ItemEffects.cpp`): a file only chooses them and sets their numbers.

| Field | Type | Default | What it is |
|---|---|---|---|
| `id` | string | *required* | Unique among the items; what the waves name |
| `model` | string | *required* | The 3D model the client draws, a path in its assets (`"models/items/pu_immunity.glb"`), facing the camera, fitted to the hitbox. Its animation `Idle`, if any, loops while the item lies in the game; its animation `Collect`, if any, plays once where it is picked up (a golden flash otherwise) |
| `collider` | table | *required* | Its hitbox, like that of a projectile: `{ shape = "sphere", radius = 28 }` or `{ shape = "box", halfWidth = 22, halfHeight = 32 }` |
| `effects` | list of tables | *required*, at least one | What it does, below |
| `lifetime` | number above 0 | `10` | Seconds before it vanishes, if nobody picks it up |
| `speed` | number, 0 or more | `60` | Units per second it drifts to the left at |

Each effect is a table with a `type` and only the fields of its type:

| `type` | Fields | What it does |
|---|---|---|
| `"heal"` | `amount`, 1 to 255, *required* | Gives back that many hit points, up to the `health` of the player |
| `"extra_life"` | `amount`, 1 to 255, default `1` | Gives that many lives more |
| `"damage"` | `amount`, 1 to 255, *required* | Takes that many hit points: a trap. A ship left with none is lost, as when it is shot; a ship that cannot be hit takes nothing |
| `"rapid_fire"` | `factor` above 1, `duration` in seconds, both *required* | The weapon fires `factor` times faster (its `fireInterval` divided, at least 1 tick) for `duration` seconds |
| `"shield"` | `duration` in seconds, *required* | The ship cannot be hit for `duration` seconds; it blinks |

```lua
-- config/items/shield.lua
return {
    id = "shield",
    model = "models/items/pu_immunity.glb",
    collider = { shape = "sphere", radius = 30 },
    effects = { { type = "shield", duration = 6 } },
}
```

A wave makes items appear with its `items` field: `quantity` of each, or, with a `maxQuantity` above it, a
number drawn at random from `quantity` to `maxQuantity` each time the wave is played. Each one appears at a
random tick while the enemies of the wave come, at a random place of the screen (right of x = 640, 120 units
from the edges), popping up in half a second, during which it cannot be picked up. Those still to come when the
wave ends appear then.

```lua
items = {
    { item = "repair_kit", quantity = 1, maxQuantity = 3 },
    { item = "bydo_mine", quantity = 2 },
},
```

The items of the repository fly the orbs of `apps/client/assets/models/items/` (`pu_heal.glb`,
`pu_extra_life.glb`, `pu_damage.glb`, `pu_fire_rate.glb`, `pu_immunity.glb`), each with its `Idle` and
`Collect` animations.

Online, the client predicts the shots of its ship from the `fireInterval` of its weapon: during a rapid fire,
its predicted shots are corrected by the game, which fires them faster.

### The players (`config/player.lua`)

One file, at the root of `config/`, **required**: without it, the configuration is not valid.

| Field | Type | Default | What it is |
|---|---|---|---|
| `ship` | string | *required* | The id of the ship of every player |
| `weapon` | string | *required* | The id of the weapon every player fires while Fire is held, unless it chose two in the hangar (`config/hangar.lua`) |
| `health` | whole number from 1 to 255 | `100` | Hit points of each ship: the damage of the Bydos is taken from them, and a ship with none left costs a life |

### The experience (`config/progression.lua`)

One file, at the root of `config/`, optional: without it, the defaults below. Online, the waves follow each
other until every player is out of lives, and the players signed in earn experience for their account
([ADR 0032](../decisions/0032-endless-waves-and-progression.md)). A player **clears** a wave when its ship is
alive as the wave ends; its **streak** counts the waves it cleared in a row.

| Field | Type | Default | What it is |
|---|---|---|---|
| `xpPerWave` | whole number, 0 or more | `100` | Experience for a wave cleared, times the multiplier of the streak |
| `streakBonus` | whole number from 0 to 65535 | `25` | Percent added to the multiplier for each wave of the streak after the first: `25` gives x1, x1.25, x1.5... It counts for the Bydos shot down too |
| `streakCap` | whole number from 100 to 65535 | `300` | Highest multiplier, in percent |

### The ships and the weapons of the hangar (`config/hangar.lua`)

One file, at the root of `config/`, optional: without it, every ship of the hangar is open, and the players
only have the weapon of `config/player.lua`. A ship needs a
**level** of the account before the player can equip it; a locked one can still be looked at, and its button
says the level it needs. The level is that of the account signed in, or, offline and as a guest, of the last
account signed in on this machine (`account.level` in the [settings](../architecture/settings.md)). A level
up that opens a ship shows a toast. The ships of the hangar are those of the client, named by their code.

Each ship has three **characteristics**, from 0 to 1, shown as gauges in the hangar and applied in the game:
`speed` multiplies the speed of the ship, `power` the damage of its shots, `armor` its health (that of
`config/player.lua`). A characteristic multiplies by 0.75 at 0, 1 at 0.5 and 1.25 at 1: a ship left out of
the list, or a characteristic left out, is the ship of the rules. The client tells the server the ship it
flies, and the server applies the characteristics of its own file: the server and the clients need the same
file.

The **weapons** are those the players choose two of in the hangar, under the ship: a primary weapon, in hand
when the ship appears, and a secondary one, put away. In a game, Left Shift (X on a gamepad, rebindable in the
Controls page as *Switch weapon*) takes the other one in hand; each one keeps its cooldown, its ammunition and
its reload while it is put away. A weapon needs a level too, and only an open one can be chosen. The game
checks the weapons a player sends against this list, and refuses those that are not in it (or that of
`config/player.lua`): the server and the clients need the same file. The name a weapon has in the hangar is
the translation `weapon.<id>` of the client (`apps/client/assets/locales/`).

```lua
return {
    ships = {
        { ship = "VX-01", level = 1, speed = 0.9, power = 0.3, armor = 0.25 },
        { ship = "VX-02", level = 3 },
    },
    weapons = {
        { weapon = "player_cannon", level = 1 },
        { weapon = "rapid_rifle", level = 1 },
        { weapon = "plasma_gun", level = 4 },
    },
}
```

| Field | Type | Default | What it is |
|---|---|---|---|
| `ships` | list of tables | empty | The ships that need a level; a ship of the hangar left out is open from level 1 |
| `ships[i].ship` | string | *required* | The code of a ship of the hangar, such as `VX-03`, listed once; an unknown code is logged by the client |
| `ships[i].level` | whole number, 1 or more | *required* | The level of the account from which the ship can be equipped |
| `ships[i].speed` | number from 0 to 1 | `0.5` | Its speed: `Rules::shipSpeed` times 0.75 at 0, 1.25 at 1 |
| `ships[i].power` | number from 0 to 1 | `0.5` | The damage of its shots, rounded, times 0.75 at 0, 1.25 at 1 |
| `ships[i].armor` | number from 0 to 1 | `0.5` | Its health, rounded, times 0.75 at 0, 1.25 at 1, at most 255 |
| `weapons` | list of tables | empty | The weapons the players can choose, in the order of the hangar; with fewer than two open, the players only have the weapon of `config/player.lua` |
| `weapons[i].weapon` | string | *required* | The id of a weapon of `config/weapons/`, listed once |
| `weapons[i].level` | whole number, 1 or more | *required* | The level of the account from which the weapon can be chosen |

## Computing values

A file is Lua code, so it can compute what it returns:

```lua
local size = 6
return {
    id = "orb_" .. size,
    category = "plasma",
    collider = { shape = "sphere", radius = size * 1.5 },
}
```

The files run in a **sandbox**: only the base functions, `table`, `string` and `math`. There is no `io`, `os`,
`require`, `load` nor `dofile`, and no `math.random`: a file cannot read the disk, the clock or a random number,
so the server and every client get the same values. A file that loops for too long is stopped with an error.
Each file has its own globals: a file cannot read what another one sets.

## Scripting a behavior

An enemy without `update` flies straight to the left and fires whenever its weapon is ready. To decide when
it moves, where it goes and when it fires, give it an `update(enemy, game)` function, called at every tick for
each enemy of that kind: see [Script the behavior of an enemy](script-enemy-behaviors.md).

## When a file is wrong

Every error names the file and the field, one error per file (the first one found in it), for every file
that is wrong:

```
enemies/bydo_hunter.lua: health: expected an integer from 1 to 255
enemies/bydo_scout.lua: spped: unknown field
waves/hunters.lua: quantity: missing
ships/bydo_hunter.lua:3: '}' expected (to close '{' at line 1) near 'model'
```

A configuration is taken whole or not at all:

- **The server does not start.** It logs each error, then
  `The game configuration in ... is not valid: fix it, or give another one with --config`, and exits with
  code 1 ([Run a server](run-a-server.md)).
- **The client starts without the configuration**: the game has no waves, and the ships cannot fire. Open the
  console to see the errors, one line each, under
  `The game configuration has errors: the game runs without it`; they are in the logs of the client too.

The ids a file names (a projectile, a ship, a weapon, an enemy) are only checked once every file is read
without error, as are the ids defined twice: fix the errors of a first run, and the next one may show those,
for instance `weapons/spark_gun.lua: projectile: unknown projectile 'sprak'`.

## Result

The new projectile, weapon, ship, enemy and wave are in every game, on the server and in the client offline:
when the wave is drawn for difficulty 3, four hunters come from the right, fly left and fire their sparks at
the players. The ship model and the sounds are not drawn nor played yet: the clients see every Bydo as the
same box until the protocol carries the kind of each one.
