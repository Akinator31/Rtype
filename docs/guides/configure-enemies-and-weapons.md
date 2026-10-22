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
  projectiles/   what a weapon fires: its shape, how it moves
  weapons/       how fast and how often it fires, its range, its magazine
  ships/         what a ship looks and sounds like, whoever flies it
  enemies/       a kind of Bydo: its ship, its health, its speed, its weapon
  waves/         a group of enemies that appear one after the other
  player.lua     what every player starts with: its ship, its weapon, its health
```

- **One file per definition.** The file returns the table of one projectile, one weapon... Its name is free
  (`bydo_scout.lua` for the enemy `bydo_scout` is the habit); the **id** inside is what the other files use.
- Sub-folders are read too (`enemies/bosses/dobkeratops.lua`), and every file not ending in `.lua` is ignored.
- The server and the client read the **same files**. The build copies `config/` next to them
  (`build/bin/config/`), and the release archives ship it there; the server takes another folder with
  `--config <dir>`.
- The files already in `config/` are the game as it is played: three projectiles, three weapons, two ships
  (one for the players), two enemies, a wave of each difficulty or more, and the player.

## Steps

1. **A projectile.** Create `config/projectiles/spark.lua`:

    ```lua
    return {
        id = "spark",
        category = "bullet",
        collider = { shape = "box", halfWidth = 10, halfHeight = 3 },
    }
    ```

2. **A weapon** that fires it, `config/weapons/spark_gun.lua`:

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

5. **A wave** to make it appear, `config/waves/hunters.lua`. A game plays five waves, one of each difficulty,
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
| `model` | string | `""` | Path of the 3D model. The client chooses the model by kind of shot (shot of a player, plasma orb or other shot of a Bydo) and draws it to the width of the collider: give the collider the proportions of the core of the model |
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
| `collisionDamage` | whole number from 0 to 255 | `20` | Hit points a player ship loses when it runs into it; the enemy keeps going |
| `update` | function | none | Its behavior, called at every tick: see [Scripting a behavior](#scripting-a-behavior). Without one, it flies left and fires as soon as it is in the game area |

### Waves (`config/waves/`)

| Field | Type | Default | What it is |
|---|---|---|---|
| `name` | string | *required* | Unique among the waves |
| `label` | string | `""` | Short text shown to the players |
| `description` | string | `""` | Longer text |
| `difficulty` | whole number from 1 to 5 | *required* | How hard the wave is, 1 the easiest |
| `placement` | `"manual"` or `"random"` | *required* | Each enemy listed with its position, or one kind of enemy placed at random |
| `spawnInterval` | number, 0 or more | `1` | Seconds between two enemies, 0 for all at once |
| `spawns` | list of `{ enemy = "...", x = ..., y = ... }` | | Manual only, *required*: each enemy and where it appears, in order |
| `enemy` | string | | Random only, *required*: the id of the kind of every enemy |
| `area` | `{ minX, maxX, minY, maxY }` | a band just past the right edge, over the whole height | Random only: where they appear; each bound optional |
| `quantity` | whole number from 1 to 65535 | the number of `spawns` | How many enemies; *required* for a random wave |

A manual wave sets no `enemy` and a random wave no `spawns`: a wave does not mix both modes.

A game plays the waves by difficulty, from 1 to 5: for each one, a wave drawn among those of that difficulty.
A difficulty without a wave is skipped, and the game is won once the last wave is over.

### The players (`config/player.lua`)

One file, at the root of `config/`, **required**: without it, the configuration is not valid.

| Field | Type | Default | What it is |
|---|---|---|---|
| `ship` | string | *required* | The id of the ship of every player |
| `weapon` | string | *required* | The id of the weapon every player fires while Fire is held |
| `health` | whole number from 1 to 255 | `100` | Hit points of each ship: the damage of the Bydos is taken from them, and a ship with none left costs a life |

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
it moves and when it fires, give it an `update(enemy, game)` function: the game calls it at every tick (60 times
a second) for **each** enemy of that kind, with this enemy and the game.

This one comes in, stops at `x = 1500`, waits a second, fires three shots at the nearest player, then leaves:

```lua
local function findNearestPlayer(enemy, players)
    local nearest, nearestDistance = nil, math.huge
    for _, player in ipairs(players) do
        local distance = (player.x - enemy.x) ^ 2 + (player.y - enemy.y) ^ 2
        if distance < nearestDistance then
            nearest, nearestDistance = player, distance
        end
    end
    return nearest
end

return {
    id = "bydo_gunner",
    ship = "bydo_scout",
    health = 80,
    weapon = "plasma_gun",

    update = function(enemy, game)
        local memory = enemy.memory
        memory.shots = memory.shots or 0
        if memory.shots >= 3 then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end
        enemy:moveTo(1500, enemy.y)
        if not enemy.arrived then
            return
        end
        memory.stoppedAt = memory.stoppedAt or game.time
        local target = findNearestPlayer(enemy, game.players)
        if target and enemy.weapon.ready and game.time - memory.stoppedAt >= 1 then
            enemy:fireAt(target.x, target.y)
            memory.shots = memory.shots + 1
        end
    end,
}
```

- **What it sees and orders**: its position, velocity, age, health and weapon, the players and their weapons;
  it orders `setVelocity`, `moveTo`, `fire` and `fireAt`. The full list is in
  [Enemy behaviors](../architecture/game.md#enemy-behaviors).
- **`enemy.memory`** is a table that belongs to this enemy alone and is kept from one tick to the next: count its
  shots there, or the time it stopped. A local variable of the file is shared by every enemy of the kind.
- **Firing** is an order of the tick: give it at every tick you want a shot. The weapon still waits its fire
  interval and reloads; `enemy.weapon.ready` tells you whether a shot ordered now goes.
- **Random numbers** come from `game:randomNumber(min, max)` and `game:randomInteger(first, last)`, drawn from
  the seed of the game, so that the server and the client offline agree.
- **When it goes wrong**: an error in `update`, or a loop that runs too long, is logged with the file and the
  line; the behavior is stopped for the rest of the game and its enemies fly and fire as if they had none. The
  game goes on. A file whose `update` is not a function is an error of the configuration, like any wrong field
  (`enemies/bydo_gunner.lua: update: expected a function`).

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
