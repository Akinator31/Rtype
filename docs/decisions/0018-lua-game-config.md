# 0018 — Lua for the configuration of enemies and weapons

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-20 |

## Context

Enemies and weapons (and the projectile categories they fire) are described by data: the model of a ship,
its health, speed and acceleration, its weapon; for a weapon, its projectile type, fire rate, range, ammunition
capacity and reload time; for a projectile category, its behavior and its sounds. Creating one must not need a
recompilation: a new enemy or a new weapon is a new file.

Requirements:

- **Data first.** The files describe values. Nothing in them is needed to run the game outside of the
  simulation's rules.
- **Scripting later.** Some behaviors (the movement pattern of an enemy or of a boss) are easier to write as
  code than as data. Adding them must not mean replacing the configuration format.
- **Without the engine library.** The server simulates the game and has no window: the loader depends on
  `libs/game` only, never on raylib ([ADR 0010](0010-physics-engine.md) follows the same rule).
- **Deterministic.** The server and the clients load the same files and must reach the same results, so that
  the prediction and the replication agree ([ADR 0016](0016-replication.md)).
- **Safe.** A configuration file must not be able to touch the disk, the network or the clock.
- **Simple to build.** One more CPM package at most ([ADR 0001](0001-cpm-package-manager.md)), building the
  same way on Linux and Windows, in the CI and on the students' machines.

## Options

| | A. JSON | B. Lua | C. Python |
|---|---|---|---|
| Nature | Data only | Embedded language | Full language |
| Dependency | A header-only parser | Lua (a few hundred KB, plain C) | CPython and pybind11, to install or ship |
| Build and CI | Nothing special | Compiled with the project | The version of Python, its shared library, packaging of the client |
| Determinism | Total: values | Controlled: no clock and no unseeded random in the sandbox | Hard to guarantee |
| Safety | No code runs | Sandboxable: libraries removed from the state | Hard to sandbox |
| Expressiveness | None: no computation, no condition | Functions, computations, patterns | Same, richer |
| Scripting later | Another mechanism to add | Same language, same loader | Same, but heavy |
| Validation | Schema and precise errors | By hand, on the loaded tables | By hand |

JSON is the simplest and covers the data alone, but a behavior script would be a second mechanism added
beside it. Python brings a large dependency to the server and to every client without bringing more than Lua.
Lua is data when the file is a table and code when it needs to be, in a single mechanism.

## Decision

**B: Lua 5.4, embedded in `libs/game`, fetched by CPM.**

### Files

A configuration file is a Lua chunk that returns a table:

```lua
return {
    id = "bydo_scout",
    model = "models/bydo_scout.glb",
    health = 30,
    speed = 4.0,
    acceleration = 12.0,
    weapon = "pulse_cannon",
}
```

The data stay plain tables: no function is needed to describe an enemy or a weapon. A file may later set a
field to a function (a movement pattern) without any change to the others.

### Sandbox

Each file is run in its own state, with only the base functions, `table`, `string` and `math` libraries.
`io`, `os`, `package`, `debug` and `load*`/`dofile`/`require` are not available, and `math.random` is
removed: a file cannot read the disk, the clock or an unseeded random value, so the same files give the same
simulation on the server and on the clients. A script that runs for too long is stopped by an instruction
count hook.

### Validation

Lua does not check the shape of a table. The loader reads the returned table into plain C++ structures
(`EnemyDefinition`, `WeaponDefinition`, `ProjectileCategory`) and checks the types, the required fields and
the ranges. An error names the file and the field (`bydo_scout.lua: health: expected a positive number`),
and a file that fails is rejected as a whole; the server does not start with a partial configuration.

### The binding

The loader reads fields of a few tables: it uses the **plain Lua C API behind a small wrapper** (a state that
owns the `lua_State`, typed getters for a field), not a template binding library such as sol2. The project
favors explicit code over metaprogramming, and reading tables does not need more. If scripted behaviors later
call into the simulation, the binding can be reconsidered in a new record.

### Where it lives

The loader and the definitions are in `libs/game`, so the server and the client share them. Lua is a PRIVATE
dependency of that library. The simulation only sees the C++ definitions, never a Lua state.

## Consequences

- Creating an enemy or a weapon is writing a file, without recompiling.
- Scripting (movement patterns, boss phases) can be added later with the same loader and the same language.
- A new dependency, built from source by CPM in the server too; it is small and written in plain C.
- Validation is ours to write: no schema checks the files for free as with JSON. The cost is a few typed
  getters and the error messages.
- The sandbox must be kept tight. Adding a standard library to the state (for instance `os` for the time)
  breaks the determinism and needs a new record.
- Lua 5.4 is used rather than LuaJIT, which is stuck on the 5.1 language and less portable across the
  platforms of the CI.
- The existing JSON setup is converted to Lua tables once; JSON is not kept as a second format.
