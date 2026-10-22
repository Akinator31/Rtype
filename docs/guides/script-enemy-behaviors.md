# How to script the behavior of an enemy

This guide writes the behavior of a kind of enemy in Lua: when it moves, where it goes, when it fires and at
whom. It is code in the file of the enemy, run by the game at every tick, without writing C++ or recompiling.
It is for anyone who designs the content of the game.

How the behaviors are run inside the game is in [Game](../architecture/game.md#enemy-behaviors); the choice of
Lua and of its binding is in [ADR 0019](../decisions/0019-luabridge3-scripting-library.md).

## Prerequisites

- An enemy in `config/enemies/`: see [Configure enemies and weapons](configure-enemies-and-weapons.md), which
  also covers its ship, its weapon and the waves that bring it.
- Basic Lua: local variables, functions, `if`, `for`, and tables (`memory.shots = 0`).

## How it works

An enemy without a behavior flies straight to the left and fires whenever its weapon is ready. Give its file
an `update` field, a function, and the game calls it instead:

```lua
return {
    id = "bydo_scout",
    ship = "bydo_scout",
    health = 30,
    weapon = "blaster",

    update = function(enemy, game)
        enemy:setVelocity(-enemy.speed, 0)
    end,
}
```

- **At every tick**, 60 times a second, **for each enemy** of that kind alive: `enemy` is the enemy of this
  call, `game` is the game. Two scouts on the screen are two calls per tick, each with its own `enemy`.
- **It reads** the enemy (position, velocity, age, health, weapon), the players and the time, and **gives
  orders**: a velocity, a point to fly to, a shot. Without an order, the enemy keeps its velocity and does not
  fire: a function that does nothing leaves it where it is.
- **Each enemy remembers** its own values in `enemy.memory`, a table kept from one tick to the next. That is
  how one enemy can be coming in while another one of the same kind is already firing.
- The **wave** still decides which enemies appear, where and when (`config/waves/`); the behavior decides what
  each one does once it is there.

## Steps

The example builds the behavior of `config/enemies/bydo_gunner.lua`: it comes in, stops, waits a second, fires
three orbs at the nearest player, then leaves.

1. **Fly in and stop.** `moveTo()` flies the enemy to a point at its top speed, and stops it there;
   `enemy.arrived` says when it is on it. Keeping its own height, it only moves to the left:

    ```lua
    update = function(enemy, game)
        enemy:moveTo(1500, enemy.y)
    end,
    ```

2. **Wait, with its memory.** Note when it arrived, once, in `enemy.memory`; `game.time` is the time of the
   game, in seconds:

    ```lua
    update = function(enemy, game)
        local memory = enemy.memory
        enemy:moveTo(1500, enemy.y)
        if not enemy.arrived then
            return
        end
        memory.stoppedAt = memory.stoppedAt or game.time
        if game.time - memory.stoppedAt < 1 then
            return
        end
        -- one second after it stopped: step 3
    end,
    ```

3. **Aim and fire.** `game.players` lists the players who have a ship; a function of the file finds the
   nearest one. `fireAt(x, y)` fires towards a point; a shot only goes if the weapon is ready, so count it only
   then:

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
    ```

    and, at the end of `update`:

    ```lua
        local target = findNearestPlayer(enemy, game.players)
        if target and enemy.weapon.ready then
            enemy:fireAt(target.x, target.y)
            memory.shots = (memory.shots or 0) + 1
        end
    ```

4. **Leave.** Once the three orbs are fired, it flies away to the left, out of the game area, where it is
   removed. Put this right after `local memory = enemy.memory`, before `moveTo()`:

    ```lua
        if (memory.shots or 0) >= 3 then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end
    ```

    The whole file, with a way out after 6 seconds when no player is there to be shot at, is
    `config/enemies/bydo_gunner.lua`.

5. **Try it.** Copy the configuration next to the programs, `cmake --build build --target r-type_config` (any
   build of the client or the server does it too), then start the client and play **Play**, then **Solo**: the
   waves with gunners are those of difficulty 2, 4 and 5. A server reads the files when it starts: restart it.
   To check a behavior without playing, write a test like
   `The gunner of the repository stops, fires three orbs at the nearest player, then leaves` in
   `tests/game_config.cpp`, which steps a game and looks at the enemy.

## Reference

Positions are in game units: the game area is 1920 × 1080, `x` to the right, `y` **down**; the centre of a ship
is its position. Velocities are in units per second, times in seconds.

### What a behavior reads

Everything is read-only: writing `enemy.x = 0` is an error, which stops the behavior.

| Value | What it is |
|---|---|
| `enemy.x`, `enemy.y` | Its position |
| `enemy.velocityX`, `enemy.velocityY` | Its velocity: its direction |
| `enemy.speed` | The top speed of its file (`speed`), the speed of `moveTo()` |
| `enemy.age` | Seconds since it appeared, 0 at its first call |
| `enemy.health` | Hit points left |
| `enemy.arrived` | It is on the point of its last `moveTo()` |
| `enemy.weapon` | Its weapon, see below |
| `enemy.memory` | A table of its own, empty when it appears, kept from one tick to the next: write in it what it must remember |
| `game.tick` | The number of the tick, from 1 |
| `game.time` | Seconds since the game started |
| `game.players` | The players who have a ship, by slot (a list: `game.players[1]`, `ipairs(game.players)`); empty while every ship is down |
| `player.slot` | Its slot, 0 to 3 |
| `player.x`, `player.y`, `player.velocityX`, `player.velocityY` | The position and the velocity of its ship |
| `player.weapon` | Its weapon, see below |
| `weapon.id` | The id of the weapon, `"plasma_gun"` |
| `weapon.ready` | A shot ordered at this tick goes |
| `weapon.ammo` | Shots before it reloads; `math.huge` for a weapon that never runs dry |
| `weapon.reloading` | It is reloading its magazine |

### What a behavior orders

Orders are methods, called with a colon: `enemy:fire()`.

| Order | What it does |
|---|---|
| `enemy:setVelocity(x, y)` | Its velocity from now on, until another order; it forgets the point of `moveTo()` |
| `enemy:moveTo(x, y)` | Flies to the point at its top speed, in a straight line, and stops on it; kept until another order |
| `enemy:fire()` | Fires its weapon straight ahead, to the left, if it is ready |
| `enemy:fireAt(x, y)` | Fires towards a point, if it is ready: the shot flies along the line from the nose of the ship to the point, and on past it |
| `game:randomNumber(min, max)` | A random number from `min` to `max` |
| `game:randomInteger(first, last)` | A random whole number from `first` to `last` included |

## Recipes

Each one is the `update` field of an enemy file, with what it needs above the `return` of the file.

**A wave up and down while it flies left**, 120 units high, a whole wave every 2 seconds. The vertical velocity
is the derivative of the height `Amplitude * sin(age * pulsation)`:

```lua
local Amplitude = 120
local Period = 2

update = function(enemy, game)
    local pulsation = 2 * math.pi / Period
    enemy:setVelocity(-enemy.speed, Amplitude * pulsation * math.cos(enemy.age * pulsation))
end,
```

**Bursts**: three shots as fast as the weapon allows, then two seconds of rest, only once it is on the
screen. An ordered shot goes even out of the game area, where the players cannot see it coming:

```lua
local Burst = 3
local Pause = 2

local function isOnScreen(enemy)
    return enemy.x >= 0 and enemy.x <= 1920 and enemy.y >= 0 and enemy.y <= 1080
end

update = function(enemy, game)
    local memory = enemy.memory
    memory.shots = memory.shots or 0
    memory.restUntil = memory.restUntil or 0
    enemy:setVelocity(-enemy.speed, 0)
    if not isOnScreen(enemy) or game.time < memory.restUntil or not enemy.weapon.ready then
        return
    end
    enemy:fire()
    memory.shots = memory.shots + 1
    if memory.shots % Burst == 0 then
        memory.restUntil = game.time + Pause
    end
end,
```

**Follow the height of the nearest player** while it comes in slowly (`findNearestPlayer` is that of the
steps). Lua has no `clamp`: `math.max` and `math.min` keep the vertical speed within the top speed:

```lua
update = function(enemy, game)
    local target = findNearestPlayer(enemy, game.players)
    local climb = 0
    if target then
        climb = math.max(-enemy.speed, math.min(enemy.speed, (target.y - enemy.y) * 2))
    end
    enemy:setVelocity(-enemy.speed / 2, climb)
end,
```

**A random place for each enemy**, drawn once and kept in its memory: every enemy of the wave stops somewhere
else, and the same game draws the same places:

```lua
update = function(enemy, game)
    local memory = enemy.memory
    memory.stopX = memory.stopX or game:randomNumber(1100, 1700)
    memory.stopY = memory.stopY or game:randomInteger(2, 8) * 108
    enemy:moveTo(memory.stopX, memory.stopY)
end,
```

**Phases**, for a behavior with several steps: a function per phase, the name of the current one in the
memory. This one comes in, fires at the first player for three seconds or until it is badly hit, then flees up
and to the left:

```lua
local phases = {}

function phases.enter(enemy, game, memory)
    enemy:moveTo(1400, enemy.y)
    if enemy.arrived then
        memory.phase, memory.since = "attack", game.time
    end
end

function phases.attack(enemy, game, memory)
    local target = game.players[1]
    if target and enemy.weapon.ready then
        enemy:fireAt(target.x, target.y)
    end
    if game.time - memory.since > 3 or enemy.health < 20 then
        memory.phase = "flee"
    end
end

function phases.flee(enemy, game, memory)
    enemy:setVelocity(-enemy.speed, -enemy.speed)
end

update = function(enemy, game)
    local memory = enemy.memory
    memory.phase = memory.phase or "enter"
    phases[memory.phase](enemy, game, memory)
end,
```

## Things to know

- **`enemy.memory`, not a local of the file.** A variable of the file (`local count = 0` above the `return`) is
  shared by every enemy of the kind, in the same game: use it for constants and functions, and the memory for
  what belongs to one enemy.
- **A shot is ordered at every tick you want one.** An order given while the weapon is not ready is lost, not
  kept for later; the weapon still waits its fire interval (`fireInterval`) and reloads its magazine. Check
  `enemy.weapon.ready` before counting a shot.
- **An aimed shot goes where the player was.** `fireAt()` aims at a point when it fires; the shot does not
  follow the player. To aim ahead of a moving ship, add its velocity: `target.x + target.velocityX * 0.5`.
- **An enemy must leave or be destroyed.** A wave ends once all its enemies are gone: an enemy that stays on
  the screen forever holds the game on that wave. Give every behavior a way out, as the gunner after 6 seconds;
  an enemy is removed once it is out of the game area by more than its size and a margin.
- **Same game, same moves.** The random numbers come from `game:randomNumber()` and `game:randomInteger()`,
  drawn from the seed of the game; there is no `math.random`, no clock and no file. The server and the client
  offline then play the same game.
- **The file runs again for the game.** When the configuration is loaded, the file runs once to be checked;
  each game runs it again, in its own Lua state, at the first enemy of the kind: what it computes above the
  `return` must not depend on anything else.
- **Short calls.** A call may execute 100 000 Lua instructions, far more than a behavior needs: a loop that
  does not end is stopped.

## When it goes wrong

- A file whose `update` is not a function is an error of the configuration, as any wrong field: the server does
  not start, the client shows it in its console (`enemies/bydo_gunner.lua: update: expected a function`).
- An error during the game (calling a field that does not exist, writing a read-only value, `error(...)`, a loop
  that runs too long) is logged with the file and the line, in the logs of the server or of the client:

    ```
    ERROR game     The behavior of the enemy 'bydo_gunner' is stopped for the rest of the game: enemies/bydo_gunner.lua:41: attempt to index a nil value (local 'target')
    ```

    The behavior is stopped for the rest of the game: its enemies fly left and fire as if they had none, and
    the game goes on. Fix the file and start a new game.
- To see what a behavior does, `print(...)` writes on the standard output of the program that runs the game:
  `if enemy.age == 0 then print("a gunner appears at", enemy.x, enemy.y) end`.

## Result

The enemies of the kind follow their behavior in every game, on the server and in the client offline: the
gunners come in, stop, fire three orbs at the nearest player, and fly away, each one at its own pace.
