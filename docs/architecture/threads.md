# Threads

!!! note "Status: partially implemented"
[ADR 0022](../decisions/0022-engine-thread-manager.md) records the worker ownership and shutdown design
and supersedes ADR 0017. Scene asset manifests and automatic loading remain proposed work. The render and
logic threads of #117 and #118 were not merged.

## Why change

Today each feature starts its own thread wherever it needs one:

- the logic thread of #118 is a `std::thread` written in `apps/client/src/main.cpp`, with its own stop flag,
  its own error handling and its own join;
- the asset worker is created lazily by `AssetLoadingService`, and every `ResourceManager`, `AudioPlayer` or
  `AsyncResourceRequests` built without a loader silently starts **another** `BackgroundLoader`;
- to make concurrent access safe, mutexes were added inside `Ecs::defer` and the engine events (#118), so the
  ECS stops being single-owner and pure.

Nobody knows the full list of threads, the order they stop in, or which thread may touch what.

The new design puts a **`ThreadManager` inside the `Engine`**: the only place that creates threads, names
them and stops them. **In the game, it runs a single worker: asset loading.** Everything else stays on the
main thread.

## The threads

| Thread     | Owns (only this thread touches it)                                                                                                               | Work                                                                                     |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------- |
| **Main**   | window, raylib (graphics, input, audio), `Renderer`, `ResourceManager`, `AudioPlayer`, `Ecs`, scenes, events, game systems, `network::EventLoop` | the frame loop: input, `network.poll()`, `engine.update()`, finishing the loads, drawing |
| **Assets** | nothing persistent                                                                                                                               | reads asset files and decodes them in CPU memory, one request at a time                  |

Why nothing else gets a thread:

- **Rendering and logic**: the logic costs a fraction of a millisecond per frame for a few hundred entities.
  Splitting it from the rendering would mean copying every frame for the renderer, a mailbox for the input,
  cross-thread text measurements and one more frame of latency, for almost no gain. The `SceneRenderer`
  keeps reading the ECS directly.
- **Audio**: raylib (miniaudio) already mixes on its own internal thread.
- **Network**: the sockets never block, `EventLoop::poll()` handles what is ready and returns at once, and the
  sessions are written to run on the game's thread without locks ([Client online](client-online.md)). A
  network thread would only copy each message into a queue that Main reads at the start of the frame, which
  is what `poll()` already does. Timeouts (5 s) are far above a slow frame.

The **only work that blocks for long is reading and decoding files**: it is the only one moved off Main.

raylib is not thread safe and its graphics context belongs to the thread that opened the window: every raylib
call that touches the GPU, the window or the audio device happens on Main. The Assets thread only calls
CPU-only functions (`LoadImage`, `LoadFileData`, `LoadWave`…).

## Overview

```mermaid
flowchart LR
    subgraph Engine
        TM["ThreadManager<br/>creates · names · stops"]
    end

    subgraph Main["Main thread"]
        direction TB
        Game["Ecs · scenes · game<br/>network.poll()"]
        Owners["ResourceManager<br/>AudioPlayer"]
        Game -- "loadTextureAsync()<br/>loadSoundAsync()…" --> Owners
        Owners -- "shared_future ready" --> Game
    end

    S["Assets worker<br/>read file · decode"]

    TM -. owns .-> S
    Owners -- "prepare (file path)" --> S
    S -- "prepared data" --> Owners
```

## Rules

1. **Only the `ThreadManager` creates threads.** `ResourceManager`, `AudioPlayer` and `AsyncResourceRequests`
   receive the engine's assets worker; they no longer create one by default.
2. **One owner per object.** The worker never sees the ECS, a resource handle or a raylib object that needs
   the GPU or the audio device. No mutex protects game state: the locks added in #118 go away.
3. **The worker only exchanges values**: a file path and options in, decoded data (an `Image`, a `Wave`, file
   bytes) out.
4. **Main never blocks on the worker.** It checks a request with `wait_for(0)`, never `get()` while it is
   pending, and draws a placeholder or a loading screen meanwhile.

## Loading an asset

The prepare/finish split of #116 is kept: **prepare** on the worker, **finish** on Main. Main finishes at most
one ready request per asset type each frame (`finishPendingLoads()`), so a burst of loads does not make one
frame stall.

```mermaid
sequenceDiagram
    participant G as Game (Main)
    participant R as ResourceManager (Main)
    participant S as Assets worker

    G->>R: loadTextureAsync("ship.png")
    R->>S: submit prepare: read file, decode image
    R-->>G: shared future of TextureId
    Note over G: keeps running, draws a placeholder
    S->>S: LoadImage (CPU only)
    S-->>R: prepared image
    G->>R: next frame: finishPendingLoads()
    R->>R: upload to GPU
    R-->>G: the future is ready
```

| Asset   | Assets worker                    | Main thread                                            |
| ------- | -------------------------------- | ------------------------------------------------------ |
| Texture | read, decode image               | upload, mipmaps                                        |
| Font    | read, decode glyphs, build atlas | upload atlas                                           |
| Model   | read bytes                       | parse and upload (raylib has no CPU-only model loader) |
| Sound   | read, decode wave                | create the audio buffer                                |
| Music   | read                             | create the stream                                      |

Requests for the same path and options share their future; a failed request reports its exception through
it and can be retried (ADR 0017).

## One frame

```mermaid
sequenceDiagram
    autonumber
    participant M as Main
    participant S as Assets worker

    S-->>M: prepared data, at any time
    M->>M: poll window and devices
    M->>M: network.poll()
    M->>M: engine.update(elapsed)
    M->>M: finishPendingLoads() (render and audio)
    M->>M: SceneRenderer draws World then Interface, endFrame (vsync)
```

## Scene loading (dynamic)

The assets worker is what makes loading the next scene in the background possible. Each scene declares the
assets it needs, and `switchScene()` / `pushScene()` load them by themselves: there is no separate call to
load a scene ahead.

```cpp
engine.createScene("level2", {
    .assets = {
        {AssetType::Model, RTYPE_MODEL_BOSS},
        {AssetType::Sound, RTYPE_SOUND_LASER},
    },
}, buildLevel2);

engine.switchScene("level2"); // requests the missing assets, shows "loading" until they are ready, then opens
```

```mermaid
stateDiagram-v2
    [*] --> Declared: createScene
    Declared --> Loading: switchScene / pushScene
    Loading --> Failed: a request threw
    Loading --> Open: every future ready
    Open --> Declared: closed, assets released
    Failed --> Loading: retry
```

- `switchScene()` / `pushScene()` request every asset of the scene that is not loaded yet. If none is
  missing, the scene opens at the next update, as today.
- Otherwise the engine pushes the `"loading"` scene (an ordinary named scene chosen by the client) and checks
  the futures at each update with `wait_for(0)`. When all are ready, it pops it and applies the switch.
- Each asset counts the open scenes using it. At zero, it is freed. A memory budget with LRU eviction can come
  later.

## API sketch

Explicit members, no generic registry: adding a thread later means adding a member.

```cpp
namespace rtype::engine
{
/// Creates and stops every thread of the engine. For now: the assets worker.
class ThreadManager
{
    std::thread::id _main = std::this_thread::get_id();
    std::shared_ptr<core::BackgroundLoader> _assets = std::make_shared<core::BackgroundLoader>();

public:
    ThreadManager() = default;
    ThreadManager(const ThreadManager &) = delete;
    ThreadManager &operator=(const ThreadManager &) = delete;
    ~ThreadManager();                       // stopAll()

    /// The worker that reads and decodes asset files. Given to ResourceManager and AudioPlayer.
    [[nodiscard]] std::shared_ptr<core::BackgroundLoader> getAssetsWorker() const;
    /// For asserts: assert(engine.getThreads().isMainThread()).
    [[nodiscard]] bool isMainThread() const;
    /// Drops the requests not started, waits for the running one and joins. Idempotent.
    void stopAll();
};
}
```

```cpp
// apps/client/src/main.cpp
rtype::engine::Engine engine;
ResourceManager resources(window, assetsDirectory, engine.getThreads().getAssetsWorker());
AudioPlayer audio(device, assetsDirectory, engine.getThreads().getAssetsWorker());
// ... frame loop ...
engine.getThreads().stopAll(); // before the resources, the audio device and the window are freed
```

`BackgroundLoader` stays the worker itself (FIFO queue, `submit()` returning a future); it gains the ability to
drop pending requests on stop, which then report a cancellation through their future.

## Start and stop

```mermaid
flowchart TB
    subgraph starting["Start"]
        direction TB
        s1["1. Engine created: ThreadManager starts the assets worker"] --> s2["2. Window, audio device"]
        s2 --> s3["3. ResourceManager and AudioPlayer get the worker"]
        s3 --> s4["4. Startup assets requested, loading screen until ready"]
        s4 --> s5["5. Frame loop"]
    end
    subgraph stopping["Stop"]
        direction TB
        t1["1. Main leaves the frame loop"] --> t2["2. stopAll(): drop pending requests, join the worker"]
        t2 --> t3["3. Free resources, close audio device and window"]
    end
    starting ~~~ stopping
```

The worker stops before anything is freed, so no prepared data arrives for a manager that is gone.

## What changes in the current code

| Kept                                                                         | Changed                                                               | Dropped                                                                                                |
| ---------------------------------------------------------------------------- | --------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| `BackgroundLoader`, `AsyncResourceRequests`, the prepare/finish split (#116) | `AssetLoadingService` becomes `ThreadManager` (`engine.getThreads()`) | the default private `BackgroundLoader` of `ResourceManager`, `AudioPlayer` and `AsyncResourceRequests` |
| `finishPendingLoads()` pumped by Main                                        | `BackgroundLoader` drops pending requests on stop                     | #117 (render thread: `RenderCommands`, `InputMailbox`, `RenderQueries`)                                |
| `network.poll()` on Main                                                     |                                                                       | #118 (logic thread, mutexes in `Ecs` and engine events)                                                |

## Migration plan

Each step is one PR that builds without warnings and keeps the tests green.

1. **engine**: `ThreadManager` replaces `AssetLoadingService`; `BackgroundLoader` can stop with pending
   requests; tests (one worker, stop with pending requests, `isMainThread()`).
2. **render, audio, core**: the loader becomes a required argument; the client and the tests pass the
   engine's worker; explicit `stopAll()` at shutdown.
3. **Scenes**: asset manifests loaded by `switchScene()` / `pushScene()`, `"loading"` scene, reference
   counting.
4. **ADR** recording this design, superseding 0017. PR #221 (#117, #118) is closed without merging.

## Server

The server does not link the engine and keeps its own threads ([Server](server.md)): the master runs its
network loop, its database thread ([Database](database.md#the-database-thread)) and the two threads of its web
server ([Web server](web.md#responses-from-the-loop)), each instance its network loop and its game thread.

The web threads never touch the lobby, the games or the administration: a request that needs them posts its
work to the loop, which replies, and the reply is sent back by the web thread of the connection. Neither waits
for the other. The live events go the other way: the loop hands each frame of a WebSocket to the web thread of
its connection, under the lock that keeps the connection alive meanwhile
([Web server](web.md#the-websocket)). The administration records its actions through the database thread, and
gets their ids back on the loop ([Administration](administration.md)).
