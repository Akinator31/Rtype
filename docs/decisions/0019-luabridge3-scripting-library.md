# 0019 — LuaBridge3 and a scripting library for Lua behaviors

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-21 |

## Context

[ADR 0018](0018-lua-game-config.md) chose Lua 5.4 to describe enemies, weapons and projectiles, read through the
plain Lua C API behind a small wrapper in `libs/game`. It left one question open: when scripts call into the
simulation, the binding is to be reconsidered.

That time has come. Some behaviors are code, not data: the movement pattern of an enemy, the phases of a boss,
the timeline of a wave. A script needs to read and change the game (the position of its entity, its velocity,
firing its weapon, a random number from the game's seed), and the simulation needs to call a script function at
each tick and read back what it returns.

With the plain C API, each exposed type is a userdata with a metatable, an `__index` function, a check of every
argument on the stack and a push of every result. That is a few dozen lines per type and per function, all
written by hand and easy to get wrong (stack balance, type checks).

The requirements of ADR 0018 still apply: no raylib (the server runs the scripts), deterministic, sandboxed,
one more CPM package at most per need, the same build on Linux and Windows. In addition:

- **Little binding code.** Exposing a class or a function takes one line, not a hand-written metatable.
- **Calls both ways.** C++ calls Lua functions and reads typed results; Lua calls C++ functions and methods.
- **Errors with a name.** A failing script reports its file and its message, and never crashes the server.
- **Contained compile time.** The binding headers stay out of the headers of the game.
- **Maintained.** The library follows the Lua releases.

## Options

The runtime stays Lua 5.4 (ADR 0018): LuaJIT is stuck on the 5.1 language and hard to build with CPM on
Windows, and Luau is another language, typed and sandboxed but not supported by most bindings and not the
format chosen for the configuration files.

| | A. Plain C API + our wrapper | B. sol2 (v3) | C. LuaBridge3 |
|---|---|---|---|
| Exposing a class | A metatable and `__index` written by hand | `lua.new_usertype<T>("T", ...)` | `beginClass<T>("T").addFunction(...)` |
| Calling a Lua function | `lua_getfield`, pushes, `lua_pcall`, pops | `sol::protected_function` | `luabridge::LuaRef`, called like a function |
| Lua versions | All | 5.1 to 5.4, LuaJIT | 5.1 to 5.4, LuaJIT, Luau, Ravi |
| Dependency | None besides Lua | Header-only, C++17 | Header-only, C++17 |
| Compile time | None | Heavy: deep templates in every translation unit that includes it | Lighter |
| Code in our repository | Large and error-prone | Small | Small |
| Maintenance | Ours | Rare releases | Active |
| Errors | By hand around `lua_pcall` | Exceptions or `protected_function_result` | Exceptions (`LuaException`) or result checks |

Option A is the most explicit, but the binding code grows with every type the scripts see, and that code is
where the bugs would be. sol2 is the most complete and the best documented, at the price of a heavy compile
time and few releases. LuaBridge3 covers what we need with a simpler API, compiles faster, and is maintained.

## Decision

**C: LuaBridge3, in a new library `libs/scripting`, on the Lua 5.4 of ADR 0018.**

This replaces the sections *The binding* and *Where it lives* of ADR 0018. Its file format, sandbox and
validation are unchanged.

### `libs/scripting`

`rtype::scripting` owns what every use of Lua shares, without any game notion (like the ECS, it stays generic):

- the build of Lua: the Lua 5.4 sources are fetched by CPM (`DOWNLOAD_ONLY`) and compiled as a static C
  library, without `lua.c` and `luac.c`; LuaBridge3 is fetched by CPM, header-only. Both are included as
  `SYSTEM` so that their warnings do not reach `rtype_options`;
- a `State` that owns the `lua_State`, opens only the libraries of the sandbox of ADR 0018 (base, `table`,
  `string`, `math` without `math.random`), installs the instruction count hook, and loads a file into its own
  environment table, so that two scripts do not share their globals;
- the conversion of a Lua error into a C++ error that names the file.

It depends on Lua and LuaBridge3 only: no raylib, no ECS, no engine. LuaBridge3 is part of its public
interface, for the libraries that write bindings.

### Bindings in `libs/game`

`libs/game` links `rtype::scripting` **PRIVATE**. It writes the bindings of what the scripts may see, in its
`.cpp` files only: no header of the game includes Lua or LuaBridge3, and the simulation's systems keep working on
C++ types.

The scripts see a small API made for them (their entity's position and velocity, firing, the tick, a random
number), not the ECS: the component caches are not exposed. The random number comes from
`GameContext::randomBetween`, seeded by the rules, so a game still replays the same. The exact API is
documented in `docs/architecture/game.md` when it is written.

### States and lifetime

- The configuration files keep a throwaway state each, as in ADR 0018: they are read once into C++
  definitions.
- The behavior scripts run in one `State` owned by the `Simulation`, created when the game starts and destroyed
  with it. The scripts run wherever the simulation runs: the server instances and the client offline. An
  online client never runs a script, it shows the snapshots ([ADR 0016](0016-replication.md)).

### Errors

A script that fails to load rejects the configuration, as in ADR 0018: the server does not start. A script that
fails during the game (an error or the instruction limit) is logged with its file and message, and that
behavior is stopped for the rest of the game; the server keeps running.

## Consequences

- Exposing a function or a type to the scripts is one line in a binding file, instead of a hand-written
  metatable.
- One more header-only dependency, LuaBridge3, beside Lua; both build from source with CPM on every platform.
- The wrapper planned by ADR 0018 lives in `libs/scripting`, not in `libs/game`, and the configuration loader
  uses it like the behaviors do.
- LuaBridge3 hides its templates behind a plain API; our code does not add metaprogramming. Compile time stays
  limited to the binding files, which are the only ones that include it.
- The API of the scripts is a contract: renaming a bound function breaks the scripts. It is kept small and
  documented.
- Every call into Lua costs a little more than a C++ call. With a few dozen enemies at a fixed tick this is
  negligible; if a profile says otherwise, the per-tick calls are the place to look, not the runtime.
- Revisit this record if LuaBridge3 stops following the Lua releases, or if the scripts need a typed language
  (Teal or Luau).
