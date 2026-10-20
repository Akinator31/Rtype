# 0017 — Asset preparation on a background thread

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-19 |

## Context

The main menu loads several large models. Synchronous file reads and decoding delay the first rendered frame. Raylib creates textures, model buffers and font atlases using the window's graphics context; these operations must remain on the render thread. Audio objects must also be created and used on their owning thread.

## Decision

The engine owns one lazily created FIFO worker queue for asset preparation. The client passes it to the render and audio resource managers; direct users of either manager get a private queue by default. The worker reads the file and, where raylib offers a CPU-only operation, decodes it. `ResourceManager::finishPendingLoads()` and `AudioPlayer::finishPendingLoads()` are called by their owning threads. They finalize at most one ready request per asset type on each call. A `std::shared_future<Id>` reports completion or an exception. The caller polls with `wait_for(0)` and may draw a placeholder or loading screen until it is ready; it must not block its own thread with `get()` while a request is pending.

| Asset | Worker | Owning thread |
|---|---|---|
| Texture | Read file and decode image in CPU memory | Upload texture and create mipmaps |
| Font | Read file, decode glyphs and build their atlas in CPU memory | Upload atlas texture |
| Model | Read model bytes | Parse with raylib and upload meshes, materials and textures |
| Sound | Read file and decode wave in CPU memory | Create audio buffers and voices |
| Music | Read file | Create the streaming decoder on the audio owner thread |

Raylib 6.0 does not expose a CPU-only model loader or a separate model upload operation. Consequently, model parsing still takes time on the render thread after the background file read. This is a remaining limitation of this implementation, visible on unusually large models. No OpenGL call runs on a worker.

Requests for the same path and options share a future. The underlying `ResourceCache` serializes inserts and lookups and stores resources at stable addresses, so overlapping requests cannot create duplicate handles. A failed request reports its exception and is removed from the pending map, allowing a retry. Existing synchronous methods remain available and return the same cached handle when an asynchronous request has completed.

The client's startup requests its fonts, starfield and menu models before building the menu. It draws a loading screen using the built-in font until the requests are ready, then builds scenes with the existing synchronous APIs, which find the assets in the cache. Tests exercise worker/owner separation, deduplication, retries, the engine's shared worker, concurrent cache access and the audio path without a device.

## Consequences

- The loading screen responds while files are read and textures or sounds are decoded.
- Finalization must be pumped each frame on the resource owner's thread.
- The manager must be destroyed after its outstanding requests or the requests receive a broken-promise error.
- Resources remain owned by the manager and are freed before the window or audio device closes.
