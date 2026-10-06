# How to draw a sprite

This guide adds an image to the client's assets, loads it and draws it on screen. It is for anyone adding
graphics to the client.

## Prerequisites

- The project builds: see [Build](../getting-started/build.md).
- The image is a **PNG** file. JPEG is not supported (see [Supported formats](../architecture/rendering.md#supported-formats)).
- Its licence allows using it in the project. Note its author and licence when adding it.

## Steps

1. **Add the file** to `apps/client/assets/`, in the folder of its kind, for instance
   `apps/client/assets/sprites/ship.png`. Nothing has to be declared in CMake: the whole folder is copied
   next to the executable at every build.

2. **Load it once**, after the `ResourceManager` is created and before the game loop. Loading returns a
   handle; keep it to draw the texture later.

    ```cpp
    const TextureId ship = resources.loadTexture("sprites/ship.png");   // relative to assets/
    ```

3. **Draw it** between `beginFrame()` and `endFrame()`. Positions are in game units (1920x1080), not in
   window pixels.

    ```cpp
    renderer.beginFrame();
    renderer.drawTexture(ship, {100.0f, 540.0f});
    renderer.endFrame();
    ```

4. **To draw one frame of a sprite sheet**, give the part of the texture to draw (in texture pixels) and
   where to draw it (in game units). The destination can be bigger than the source to scale the sprite up,
   and an angle rotates it around its centre:

    ```cpp
    const Rectangle frame{32.0f * frameIndex, 0.0f, 32.0f, 32.0f};   // 32x32 frames side by side
    renderer.drawTexture(ship, frame, {100.0f, 540.0f, 96.0f, 96.0f}, 0.0f);
    ```

5. **Build and run** the client:

    ```sh
    cmake --build build
    ./build/bin/r-type_client
    ```

## Result

The sprite is drawn at the given position, and stays at the same place relative to the game when the window
is resized.

If the client stops with `r-type_client: Cannot load texture ...`, the path is wrong, the file is missing from
`build/bin/assets/` (build again), or the format is not supported.
