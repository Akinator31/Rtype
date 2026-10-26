# 0020 — Engine owned asset worker

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-22 |
| **Supersedes** | [0017](0017-asset-loading-threads.md) |

## Context

ADR 0017 allowed resource managers to create private workers and made the engine's shared worker lazy. That left the number and lifetime of client asset threads dependent on which objects were constructed. The proposed [thread architecture](../architecture/threads.md) keeps game logic, rendering, audio API calls and network polling on the main thread, with one worker for file reading and CPU preparation.

## Decision

`Engine` owns a `ThreadManager` with an explicit assets worker member. The worker starts when the manager is constructed. `Engine::getThreads().getAssetsWorker()` supplies the same worker to the client resource and audio managers. `ResourceManager`, `AudioPlayer` and `AsyncResourceRequests` require that worker explicitly and cannot create private workers.

`ThreadManager::isMainThread()` compares the caller with the thread that constructed the manager. `stopAll()` rejects new submissions, cancels work still queued, waits for the current task and joins the worker. It is idempotent and is also called by the manager destructor. A cancelled request reports `BackgroundLoaderCancelled` through its future. The client calls `stopAll()` before releasing its resources.

The worker prepares asset data only. The resource owner still calls `finishPendingLoads()` on the main thread to create GPU and audio resources. ECS systems, scenes, events and `network::EventLoop::poll()` remain on the main thread. Adding another engine thread requires an explicit member and shutdown path in `ThreadManager`.

## Consequences

- Client asset preparation has one known worker and an explicit shutdown point.
- Callers must obtain a worker from the engine before constructing resource managers.
- Scene asset manifests and loading screens described in the thread proposal are separate work.
