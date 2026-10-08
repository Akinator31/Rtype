# How to draw a sprite

This guide adds an image to the client's assets, loads it, and shows it with an entity. It is for anyone
adding graphics to the client.

## Prerequisites

- The project builds: see [Build](../getting-started/build.md).
- The image is a **PNG** file. JPEG is not supported (see [Supported formats](../architecture/rendering.md#supported-formats)).
- Its licence allows using it in the project. Note its author and licence when adding it.

## Steps

1. **Add the file** to `apps/client/assets/`, in the folder of its kind, for instance
   `apps/client/assets/sprites/ship.png`. Nothing has to be declared in CMake: the whole folder is copied
   next to the executable at every build.

2. **Load it once**, after the `ResourceManager` is created and before the game loop. Loading returns a
   handle; keep it to give it to the entities.

    ```cpp
    const TextureId shipTexture = resources.loadTexture("sprites/ship.png");   // relative to assets/
    ```

3. **Create an entity that shows it.** Everything the client draws is an entity
   ([ADR 0007](../decisions/0007-client-in-the-ecs.md)): it needs a `Position`, a `Drawable` and a `Sprite`.

    ```cpp
    const entity_t ship = ecs.createEntity();
    ecs.addComponent<SceneMember>(ship, game);                    // destroyed with the game scene
    ecs.addComponent<Position>(ship, 200.0f, 540.0f);             // centre of the sprite, in game units
    ecs.addComponent<Drawable>(ship, Layer::World, 10);           // world layer, drawn over lower orders
    ecs.addComponent<Sprite>(ship, shipTexture, Rectangle{0.0f, 0.0f, 32.0f, 32.0f}, 96.0f, 96.0f);
    ```

    - The `Position` is the **centre** of the sprite, in game units (1920x1080), not in window pixels.
    - The `Rectangle` is the part of the texture to show, in texture pixels: here the first 32x32 frame of a
      sprite sheet. The last two numbers are the size on screen, in game units: here three times bigger.
    - The `Drawable` sets the layer (`World` for the game, `Interface` for the HUD and menus) and the order:
      a higher order is drawn on top.

4. **Change it** through its components, from a [system](write-a-system.md) or the game code:

    ```cpp
    ecs.getComponent<Position>(ship).y -= 10.0f;                 // move it
    ecs.getComponent<Sprite>(ship).source.x = 32.0f * frame;      // show another frame of the sprite sheet
    ```

    A `Scale` or an `Opacity` component changes its size or transparency, and a `Tween` animates them (see
    [Animations](../architecture/scenes-and-interface.md#animations)).

5. **Build and run** the client:

    ```sh
    cmake --build build
    ./build/bin/r-type_client
    ```

## Result

The `SceneRenderer` draws the sprite every frame, at the position of the entity. It stays at the same place
relative to the game when the window is resized, and disappears when its scene closes.

If the client stops with `r-type_client: Cannot load texture ...`, the path is wrong, the file is missing from
`build/bin/assets/` (build again), or the format is not supported.

!!! note "Drawing without an entity"

    The `Renderer` can also draw directly (`renderer.drawTexture()` between `beginFrame()` and `endFrame()`).
    Keep it for debugging tools: what is drawn this way is not part of a scene, nor of a layer, and is not
    drawn on the VR board.
