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

## Layers

The engine is split into decoupled subsystems, shared by the client and the server where it makes sense.

| Layer | Responsibility | Used by |
|---|---|---|
| **Core** | Entities, components, systems (`libs/ecs`), timing, logging | Client, server |
| **Networking** | Sockets, binary serialization, the [protocol](../protocol/index.md) | Client, server |
| **Game logic** | Players, Bydos, missiles, waves, collisions | Server |
| **Rendering** | Sprites, star-field, UI, built on raylib | Client |
| **Audio** | Music and sound effects | Client |
| **Input** | Keyboard / gamepad mapping to player actions | Client |

## Client libraries

The client is built on three libraries. Each one has its own page:

| Library | Role | Page |
|---|---|---|
| `libs/render` | Window, textures and fonts, drawing at a fixed logical resolution. Wraps raylib. | [Rendering](rendering.md) |
| `libs/input` | Keyboard and gamepads turned into game actions, with rebindable controls. Wraps raylib. | [Input](input.md) |
| `libs/ecs` | Entities, components and systems. Shared with the server, so it never depends on raylib. | — |

Their public headers contain no raylib type: raylib is a private dependency of `render` and `input`, and the
client code only uses our own classes. The server links none of them except the ECS.

## Repository layout

```
apps/       executables, one subdirectory per program
  client/   graphical client (r-type_client)
    assets/ sprites, fonts and sounds, copied next to the executable
  server/   game server (r-type_server)
libs/       libraries shared by the programs, one subdirectory per library
  ecs/      entity component system (client and server)
  input/    keyboard and gamepad input (client)
  render/   window, resources and drawing (client)
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
