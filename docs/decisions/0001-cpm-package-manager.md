# 0001 — CPM.cmake as package manager

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-01 |

## Context

The project must build on Linux and Windows (MSVC) without relying on system-wide libraries, and
third-party code must not be copied into the repository. A package manager has to fetch and build the
dependencies (raylib for now, more later). The CI must not download every dependency again on each run.

## Options

| | CPM.cmake | vcpkg | Conan |
|---|---|---|---|
| Extra tool to install | None (CMake script) | vcpkg checkout + bootstrap | Python + Conan client |
| Integration | `CPMAddPackage()` in CMakeLists | Toolchain file + manifest | Generated toolchain + profiles |
| Builds dependencies | From source, as part of the project | From source, cached as binaries | Prebuilt binaries when available |
| CI caching | Source cache directory | Binary cache | Binary cache / remote |
| Learning curve | Low | Medium | Medium to high |

## Decision

Use **CPM.cmake**. It only requires CMake, which every developer and CI runner already has, and dependencies
are declared next to the targets that use them. Downloads are pinned by version and verified with a SHA-256
hash, and the source cache (`CPM_SOURCE_CACHE`) is kept between CI runs.

## Consequences

- Dependencies are compiled from source on a clean build, which takes longer than downloading binaries.
  Acceptable for a small dependency set; to be reconsidered if heavy libraries are added.
- `cmake/CPM.cmake` is the official bootstrap script: it downloads a pinned CPM version on configure.
