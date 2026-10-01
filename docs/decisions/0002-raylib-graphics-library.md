# 0002 — raylib for the client

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-01 |

## Context

The client needs windowing, 2D rendering, input, and audio on Linux and Windows. Full game engines (Unity,
Unreal, Godot) are forbidden: the library must stay low-level enough for the engine to be our own.

## Options

| | raylib | SFML | SDL |
|---|---|---|---|
| Scope | Window, 2D/3D rendering, input, audio | Window, 2D rendering, input, audio, network | Window, input, audio; 2D renderer or GPU API |
| API | C, small and simple | C++, object-oriented | C, lower level |
| Build | CMake, no external dependency | CMake, a few external dependencies | CMake, no external dependency |
| 3D / shaders | Yes | Limited | Through the GPU API |

## Decision

Use **raylib 6.0**. It covers everything the client needs with a small API, builds from source with CMake
and no further dependency on both platforms, and keeps 3D and shaders available for later features. Version
6.0 is the current release; 5.5 was tried first but its CMake configuration triggers deprecation warnings
with CMake 4.

## Consequences

- raylib is a C library: rendering is wrapped behind our own engine interfaces so that game code does not
  depend on it directly.
- Requires CMake ≥ 3.25.
