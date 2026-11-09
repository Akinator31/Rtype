# Architecture

!!! warning "Draft"

    This page describes the **target** architecture. It is updated as each subsystem is implemented.

R-Type follows a client/server model with an **authoritative server**: clients only send player inputs and
render what the server tells them; every gameplay decision (spawns, collisions, deaths) is taken by the
server.

## Overview

```mermaid
flowchart LR
    subgraph Client["r-type_client"]
        direction TB
        CInput[Input] --> CNet[Networking]
        CNet --> CWorld[Local world state]
        CWorld --> CRender[Rendering & audio]
    end

    subgraph Server["r-type_server"]
        direction TB
        SNet[Networking] --> SLogic[Game logic]
        SLogic --> SWorld[Authoritative world state]
    end

    Client -- "player inputs (UDP)" --> Server
    Server -- "world updates (UDP)" --> Client
```

*Clients send their inputs to the server over UDP; the server simulates the game and sends world updates
back to every client.*

### Server processes

The server hosts several games at once. The `r-type_server` you start is the **master**: it runs the lobby
and starts **one process per game** ([ADR 0013](../decisions/0013-one-process-per-game.md)). The client talks
to the master over TCP until it joins a game, then to the game over UDP
([ADR 0012](../decisions/0012-tcp-lobby-udp-game.md)). Both are encrypted
([ADR 0014](../decisions/0014-encryption.md)).

```mermaid
flowchart LR
    Client["r-type_client"]
    subgraph Host["Server host"]
        Master["Master<br/>r-type_server<br/>lobby"]
        G1["Game 1<br/>r-type_server --instance"]
        G2["Game 2<br/>r-type_server --instance"]
        Master -- "starts, local TCP" --> G1
        Master -- "starts, local TCP" --> G2
    end
    Client -- "TLS over TCP 4242: lobby" --> Master
    Client -- "encrypted UDP: inputs and world" --> G1
```

*The client joins a game through the master, then exchanges the game traffic directly with that game's
process.*

How the server starts, what it is given and how it stops: [Server](server.md). How the master serves its
administration panel over HTTP: [Web server](web.md). What an administrator sees and does:
[Administration](administration.md), and the web page it does it from: [The panel](panel.md). What the master
keeps of the accounts of the players: [Accounts](accounts.md).

New to networking? Start with [Networking concepts](networking-concepts.md).

## Layers

The engine is split into decoupled subsystems, shared by the client and the server where it makes sense.

| Layer | Responsibility | Used by |
|---|---|---|
| **Core** | Entities, components, systems (`libs/ecs`), physics (`libs/physics`), logging (`libs/log`), timing | Client, server |
| **Networking** | Sockets (Asio), binary serialization, reliability over UDP, encryption, the [protocol](../protocol/index.md) | Client, server |
| **Game logic** | Players, Bydos, missiles, waves, collisions (`libs/game`, [ADR 0015](../decisions/0015-one-game-scene-local-or-remote-session.md), [Game](game.md)) | Server, client offline |
| **Rendering** | Window, textures, drawing, built on raylib | Client |
| **Interface** | Menus, options and HUD, made of entities like the world (`libs/engine`) | Client |
| **Audio** | Music, sound effects and the voices of the players, built on raylib (`libs/audio`) | Client |
| **Voice** | The voice chat of an online game, through the game or Discord (`libs/voice`, [Voice](voice.md)) | Client |
| **Input** | Keyboard / gamepad mapping to player actions | Client |

## World and interface

Everything the client draws is an entity of the ECS, as decided in
[ADR 0007](../decisions/0007-client-in-the-ecs.md): the world (ships, Bydos, missiles, star-field), the HUD and
the menus. Each drawn entity belongs to a **layer**, and the layers are drawn one after the other:

```mermaid
flowchart TB
    subgraph Frame["One frame of the client"]
        direction TB
        Update["ecs.update(elapsed)<br/>scenes, interface, game, layout"]
        World["1. World layer<br/>ships, Bydos, star-field"]
        UI["2. Interface layer<br/>HUD and menus"]
    end
    Update --> World --> UI --> Renderer["Renderer<br/>(the only one drawing)"]
```

*Each frame updates the ECS, then draws the world layer and the interface layer on top of it.*

- Only the `Renderer` draws: the systems never call raylib.
- The interface layer is always drawn after the world. In VR, the world layer is drawn on the board and the
  interface layer on a floating panel.
- Phases of the game (home screen, game, pause menu) are **scenes**: groups of entities opened and closed
  together. See [Scenes and interface](scenes-and-interface.md).

## Libraries

The programs are built on twelve libraries:

| Library | Role | Page |
|---|---|---|
| `libs/render` | Window, textures and fonts, drawing at a fixed logical resolution. Wraps raylib. | [Rendering](rendering.md) |
| `libs/input` | Keyboard and gamepads turned into game actions, with rebindable controls. Wraps raylib. | [Input](input.md) |
| `libs/ecs` | Entities, components and systems, with a hierarchy. Shared with the server, so it never depends on raylib. | [ECS](ecs.md) |
| `libs/log` | Messages with a level and a subsystem name, coloured in a terminal. Shared with the server. | [Logging](logging.md) |
| `libs/network` | Addresses, UDP and TCP sockets, the event loop that runs them, TCP framing, TLS and the binary serialization. Wraps Asio and Mbed TLS. Shared with the server. | [Networking](networking.md) |
| `libs/physics` | Movement and collisions as components and systems on the ECS. Knows no game: each game names its collision layers and reacts to the collisions. Shared with the server. | — |
| `libs/game` | The rules of the R-Type: a `Simulation` that takes the inputs of the players and steps the world one tick at a time, and the loader of the Lua configuration of its enemies and weapons. Shared by the server and the client offline; no raylib, no network. | [Game](game.md) |
| `libs/scripting` | Lua 5.4 and LuaBridge3, built from source, and a sandboxed `State` that runs a file in its own environment. Knows no game. Shared with the server. | [Game](game.md#configuration) |
| `libs/lobby` | The messages of the lobby protocol, between a client and the master: hello, list, create and join games, the messages of the administrators. Shared by the client and the server. | [Server](server.md#the-lobby) |
| `libs/engine` | Client systems on the ECS: scenes, drawing, animations, menus as entities, navigation. | [Scenes and interface](scenes-and-interface.md) |
| `libs/audio` | Sound effects, streamed music and the voices of the other players, with separate volumes, and what the output plays, for the echo cancellation. Wraps raylib. | [Audio](audio.md) |
| `libs/database` | The data a program keeps from one run to the next, in SQLite or PostgreSQL: opening by address, migrations, transactions. Wraps SOCI and SQLite. For the server. | [Database](database.md) |
| `libs/discord` | Discord in the client: an interface per feature (the sign-in, the presence, then the connection), and their implementations through the Discord Social SDK, kept in `libs/discord/sdk/`, on one shared `Session`. Wraps the SDK. For the client. | [The client online](client-online.md#discord) |
| `libs/voice` | The voice chat of an online game: the interface the client knows, the rule of what carries each voice, the game or Discord, the devices as interfaces (`rtype::voice`, header only), the codec of the voice (`rtype::voice_codec`), the echo cancellation, noise suppression and gain control of the microphone (`rtype::voice_processing`), and the voice of the game with its jitter buffer and the microphone (`rtype::voice_client`). Wraps libopus, webrtc-audio-processing, and the miniaudio of raylib for the microphone. For the client. | [Voice](voice.md) |
| `libs/core` | Building blocks shared by the other libraries: resource handles and caches. Header-only, no dependency. | — |
| `libs/shaders` | GLSL shaders (colour filters, lighting of the 3D models), one file per stage in `glsl/`, embedded at build time as C++ strings and compiled by `render`. Header-only, no dependency. | [Rendering](rendering.md) |

Their public headers contain no raylib type: raylib is a private dependency of `render`, `input`, `audio` and `voice_client`,
and the client code only uses our own classes. The server only links the libraries that do not depend on
raylib: the ECS, the physics, the game, the scripting, the lobby, the log, the core and the network libraries. Likewise, Asio and Mbed TLS are
private dependencies of `network`, and reproc, which starts the instances of the server, toml++, which reads
its settings file, Crow, which serves its panel ([Web server](web.md)), and Argon2, which hashes the passwords of
the accounts ([Accounts](accounts.md#passwords)), private dependencies of `r-type_server`. libcurl, which sends
the mails of the accounts ([Accounts](accounts.md#the-mails)), asks Discord who a player is, and is the HTTP
client of the tests, comes through CPM too, with OpenSSL from the system on Linux. The Discord Social SDK is a
private dependency of `rtype::discord_sdk`, linked by `r-type_client` alone
([ADR 0026](../decisions/0026-discord-social-sdk.md)). libopus, which encodes the voice, is a private dependency
of `rtype::voice_codec` ([Voice](voice.md#the-codec)); webrtc-audio-processing and abseil, which take the echo
and the noise out of the microphone, private dependencies of `rtype::voice_processing`
([Voice](voice.md#the-sound-processing)); the microphone is captured by the miniaudio raylib
compiles, a private dependency of `rtype::voice_client` ([Voice](voice.md#the-devices)).

## Repository layout

```
apps/       executables, one subdirectory per program
  client/   graphical client (r-type_client)
    assets/ sprites, fonts and sounds, copied next to the executable
  server/   game server (r-type_server)
  minigame/ Orbit Breaker, a brick breaker made with the engine alone (orbit_breaker)
config/     the Lua configuration of the game: enemies, weapons... (client and server)
libs/       libraries shared by the programs, one subdirectory per library
  audio/    sound effects, music and the voices played (client)
  core/     handles and resource caches (all)
  database/ SQLite and PostgreSQL through SOCI, migrations (server)
  ecs/      entity component system, hierarchy (client and server)
  engine/   client systems: drawing, animations, menus (client)
  game/     rules of the R-Type, the Simulation (server and client offline)
  input/    keyboard and gamepad input (client)
  lobby/    messages between a client and the master (client and server)
  log/      logging (client and server)
  network/  addresses, sockets and event loop (client and server)
  render/   window, resources and drawing (client)
  scripting/ Lua and its sandbox (client and server)
  shaders/  GLSL shaders, embedded in the binaries at build time (client)
tests/      unit and smoke tests (Catch2)
cmake/      CMake helpers (CPM bootstrap)
docs/       this documentation (Zensical)
doxygen/    API reference configuration
scripts/    developer and CI scripts
```

Each library keeps its public headers apart from its implementation, so that the libraries it depends on
(raylib, for instance) stay out of its interface:

```
libs/<name>/
  CMakeLists.txt
  include/rtype/<name>/   public headers, included as <rtype/<name>/Header.hpp>
  src/                    implementation
```

The client's assets live in `apps/client/assets/`. The build copies them next to the executable
(`build/bin/assets/`) and the client package ships them in the same place, so the client loads them from
`rtype::render::applicationDirectory() / "assets"` and finds them wherever it is started from. Paths given
to the `ResourceManager` are relative to this directory, for instance `"sprites/player.png"`.

The configuration of the game lives in `config/` at the root, since the client and the server read the same
files. The build copies it next to both programs (`build/bin/config/`, removed and copied again at each build)
and both packages ship it; see [Configure enemies and weapons](../guides/configure-enemies-and-weapons.md).
