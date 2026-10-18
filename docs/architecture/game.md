# Game

The game library (`libs/game`, namespace `rtype::game`) holds the **rules of the R-Type**: the players, their
ships and shots, and later the Bydos, the waves and the collisions. It is used by the server instances, which
run the games online, and by the client offline, in its `LocalSession`
([ADR 0015](../decisions/0015-one-game-scene-local-or-remote-session.md)): the rules are written once, and a
Bydo behaves the same offline and online.

It depends on the ECS, the physics and the log only: no window, no sound, no network.

## A game in three calls

A `Simulation` is one game. It takes the inputs of the players, steps the world **one tick at a time**, and
gives a **snapshot** of it:

```cpp
#include "rtype/game/Game.hpp"

rtype::game::Simulation simulation;
simulation.addPlayer(0); // slots 0 to 3

// Every tick, 60 times per second
simulation.setInput(0, input);                               // the controls of player 0
simulation.step();                                           // exactly 1/60 s of game
const rtype::game::Snapshot &world = simulation.snapshot();  // what to draw
```

```mermaid
flowchart LR
    Inputs["PlayerInput<br/>per slot"] --> Sim
    subgraph Sim["Simulation"]
        direction TB
        Ecs["its own ECS<br/>ships, shots..."]
        Systems["the systems of the rules"]
    end
    Sim --> Snapshot["Snapshot<br/>players, entities, events"]
```

*The simulation turns the inputs of the players into a snapshot of the world, one tick at a time.*

### Who calls step()

The simulation never reads a clock: whoever uses it decides when a tick passes. That is what makes a game the
same on every machine, and the tests exact.

| Where | Who calls `step()` |
|---|---|
| Client offline | `LocalSession::update(elapsed)` adds up the frame times and steps once per 1/60 s, like `PhysicsStep` |
| Server instance | The simulation thread, with a `Timer` at 60 Hz |
| Tests | A loop: 600 steps are 10 seconds of game, computed in a few milliseconds |

**The same inputs, in the same order, give the same snapshots**, bit for bit: a test checks it over 20
seconds of a game with three players. Keep it that way: no clock, no `std::rand()`, no iteration over an
`std::unordered_map` in the rules; a random generator, when the Bydos need one, is seeded by the game.

## Inputs

`PlayerInput` is the byte of the protocol ([game protocol](../protocol/game.md#92-inputs)): one bit per control.
It is **data, not a device**: `libs/game` does not use `libs/input`, which reads the keyboard and the gamepads
through raylib and only exists in the client. The client makes the `PlayerInput` from the virtual gamepad of the
engine (`rtype::engine::Controls`, fed by `libs/input` with the bindings of the player), once per tick; the server
receives it in `INPUT` messages.

```
Keyboard, gamepads ─► libs/input ─► engine::Controls ─► GameSessionSystem ─► game::PlayerInput ─► Simulation
   (raylib)          (bindings)    (virtual gamepad)       (client)           (one byte)        (client or server)
```

```cpp
rtype::game::PlayerInput input;
input.set(rtype::game::PlayerInput::Up, controls.isDown(rtype::engine::Control::Up));
input.set(rtype::game::PlayerInput::Fire, controls.isDown(rtype::engine::Control::FaceDown));

input.direction();                   // {x, y}, each -1, 0 or 1: up and down together cancel
rtype::game::PlayerInput::fromByte(b); // from the network: the reserved bits are dropped
```

An input stays until the next `setInput()`: a player who holds Right keeps moving.

## Snapshots

A `Snapshot` is the world at the end of a tick, with the fields of the `SNAPSHOT` message
([game protocol](../protocol/game.md#snapshot-0x80)), plus the events of the tick:

| Field | Content |
|---|---|
| `tick` | The tick it describes, 1 after the first step |
| `players` | Slot, lives and score of each player, by slot |
| `entities` | Each entity: id, kind, owner, position (its centre), velocity, health, flags, by id |
| `destroyed` | The entities destroyed since the previous snapshot, with the cause (removed, or killed: an explosion) |
| `waveStarted`, `gameOver` | A wave started, the game ended |

- **Ids are never reused** within a game (`NetworkId`, unlike the ids of the ECS): a client keeps them to
  recognize its entities from one snapshot to the next.
- **Positions are floats** in game units; the network encodes them in eighths of a unit. Every entity stays in
  the area the protocol can encode (`PositionLimit`): what leaves the game area is removed.
- An entity in `destroyed` is no longer in `entities`.

## What the rules do now

| Rule | Where | Numbers (`Rules`) |
|---|---|---|
| A ship appears on the left, one height per slot | `spawnPlayerShip()` | — |
| It moves with the input, as fast in diagonal | `PlayerControlSystem` | `shipSpeed` 600 units/s |
| It stays entirely in the 1920 × 1080 area | `physics::BoundsSystem`, `ConfinedToBounds` | `shipHalfWidth`, `shipHalfHeight` |
| Fire held shoots from its nose | `PlayerControlSystem`, `spawnPlayerShot()` | `fireInterval` 10 ticks (6 per second), `shotSpeed` 1500 units/s |
| What leaves the area is removed | `AreaSystem`, `LeavesArea` | `areaMargin` 128 units |
| A player who leaves takes its ship away | `Simulation::removePlayer()` | — |

The Bydos, the collisions, the damage, the lives and the score come with `server: gameplay`.

A tick runs the systems in this order: `PlayerControlSystem` (inputs), `physics::MovementSystem` (movement),
`physics::BoundsSystem` (the walls), `AreaSystem` (what left), then the snapshot is taken.

## Adding gameplay

The rules are components and systems on the ECS, like the physics, so that anyone can add some:

1. **A component** in `Component/Components.hpp`: plain data, `struct Charge { std::uint32_t ticks = 0; };`.
2. **A kind of entity**, if the clients must draw it: a value in `EntityKind` (`Snapshot.hpp`) and in the table
   of the [protocol](../protocol/game.md#91-the-world), in the range of its family; then a `spawn...()`
   function in `Entities.hpp`, which gives it a `NetworkId`, its `Kind`, its `Owner` and its `Position`.
3. **A system** in `System/`: it receives the caches, the `Ecs` and the `GameContext` (rules, tick, players and
   their inputs, events), and is registered in the constructor of `Simulation`, at its place in the tick.
4. **The numbers** in `Rules`, so that they can be tuned in one place, and the tests use their own.
5. **A test** in `tests/game_simulation.cpp`: inputs in, snapshot checked.

Inside a system, create and destroy entities through `ecs.defer()`: changing the world while a system goes
through the caches is not safe. `destroyEntity(ecs, context, entity, cause)` destroys an entity and records it
for the snapshot.

## Tests

The tests in `tests/game_simulation.cpp` (tag `[game]`) step simulations and check their snapshots: the
inputs, the players added and removed, the movement and its speed in diagonal, the walls, the shots and their
interval, shots removed once out of the area with ids never given again, the same game twice with the same
inputs, and an hour of random inputs with four players keeping every entity in the range of the protocol.

```sh
./build/bin/rtype_tests "[game]"
```
