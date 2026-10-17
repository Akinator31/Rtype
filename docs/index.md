---
title: R-Type
---

# R-Type

**A networked remake of the classic horizontal shoot'em up, built on our own C++ game engine.**

Up to four players connect to an authoritative server and fight the Bydos together. The project is split
into a graphical client, a multithreaded server, and the engine they both rely on.

!!! info "Project status"

    Early skeleton: the client opens a window and the server starts. The pages below describe the
    target design and are filled in as the engine takes shape.

<div class="grid cards" markdown>

-   :lucide-rocket: **Getting started**

    ---

    Build the client and the server on Linux or Windows, and run them.

    [:octicons-arrow-right-24: Build and run](getting-started/index.md)

-   :lucide-layers: **Architecture**

    ---

    How the engine is organised: rendering, networking and game logic as decoupled layers.

    [:octicons-arrow-right-24: Architecture](architecture/index.md)

-   :lucide-radio-tower: **Network protocol**

    ---

    The binary UDP protocol between clients and server, precise enough to write your own client.

    [:octicons-arrow-right-24: Protocol specification](protocol/index.md)

-   :lucide-scale: **Technical decisions**

    ---

    Why we chose each library and tool, compared with the alternatives.

    [:octicons-arrow-right-24: Decision records](decisions/index.md)

-   :lucide-accessibility: **Accessibility**

    ---

    What the game and its documentation offer to players with disabilities.

    [:octicons-arrow-right-24: Accessibility](accessibility.md)

-   :lucide-git-pull-request: **Contributing**

    ---

    Git workflow, coding conventions, and how to add a page to this documentation.

    [:octicons-arrow-right-24: Contributing](contributing/index.md)

</div>

## At a glance

| | |
|---|---|
| **Binaries** | `r-type_server`, `r-type_client`, `r-type_keygen` (server keys) |
| **Language** | C++20 |
| **Build** | CMake ≥ 3.25, dependencies fetched by [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) |
| **Platforms** | Linux (GCC, Clang), Windows (MSVC) |
| **Client library** | [raylib](https://www.raylib.com/) 6.0 |
| **Source code** | [github.com/Akinator31/Rtype](https://github.com/Akinator31/Rtype) |
