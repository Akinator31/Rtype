# How to write a system

This guide adds a component and a system to the ECS: a `Velocity` that moves entities every frame. It is for
anyone writing gameplay, on the client or on the server.

## Prerequisites

- The project builds: see [Build](../getting-started/build.md).
- You know the vocabulary: an **entity** is only an id, a **component** is plain data attached to an entity, a
  **system** runs the logic on the entities that have the components it needs. How components are stored is
  explained in [ADR 0006](../decisions/0006-ecs-component-storage.md).

## Steps

1. **Write the component**: a plain struct, with default values. No logic, no pointers to other entities
   (use an `EntityRef`, see below).

    ```cpp
    struct Velocity {
        float x = 0.0f;   // game units per second
        float y = 0.0f;
    };
    ```

    Put components used by the client **and** the server (positions, velocities, health) where both can
    include them; never in `libs/engine`, which depends on raylib and is not linked by the server.

2. **Write the system**: derive from `rtype::ecs::System`, get the caches of the components it uses with
   `use<T>()` in the constructor, and keep the references.

    ```cpp
    class MovementSystem : public rtype::ecs::System
    {
        rtype::engine::Scenes &_scenes;
        rtype::ecs::ComponentCache<rtype::ecs::Position> &_positions;
        rtype::ecs::ComponentCache<Velocity> &_velocities;

    public:
        MovementSystem(rtype::ecs::ComponentCaches &caches, rtype::engine::Scenes &scenes)
            : _scenes(scenes), _positions(use<rtype::ecs::Position>(caches)), _velocities(use<Velocity>(caches))
        {
        }

        void update(float elapsed) override
        {
            for (rtype::ecs::entity_t entity = 0; entity < _velocities.size(); ++entity) {
                if (!_velocities.has(entity) || !_positions.has(entity) || !_scenes.updates(entity)) {
                    continue;
                }
                rtype::ecs::Position &position = _positions.get(entity);
                const Velocity &velocity = _velocities.get(entity);
                position.x += velocity.x * elapsed;
                position.y += velocity.y * elapsed;
            }
        }
    };
    ```

    - Go through the ids of the **rarest** component (here `Velocity`), and check the others with `has()`.
    - **Multiply by `elapsed`**: speeds are per second, so the game runs at the same speed at 30 or 144
      frames per second. Never read a clock in a system.
    - **Ask the scenes** whether the entity updates (`_scenes.updates(entity)`): the entities of a menu
      covered by another one must not move. A system that reads the player's input checks
      `_scenes.receivesInput(entity)` instead.

3. **Register it**, at the right place in the order of the systems (see
   [The systems and their order](../architecture/scenes-and-interface.md#the-systems-and-their-order)). The
   arguments after the type are given to the constructor, after the caches:

    ```cpp
    ecs.registerSystem<MovementSystem>(scenes);
    ```

    Game systems go after the `InterfaceSystem` and before the `TransformSystem`, so that children follow
    entities moved in this frame.

4. **Create entities** that have the components:

    ```cpp
    const entity_t missile = ecs.createEntity();
    ecs.addComponent<SceneMember>(missile, game);         // destroyed with the game scene
    ecs.addComponent<Position>(missile, 100.0f, 540.0f);
    ecs.addComponent<Velocity>(missile, 1200.0f, 0.0f);
    ```

5. **Test it**, without a window: create an `Engine`, register the system, run an update with a chosen elapsed
   time, and check the result. Add the test to `tests/` (see [Testing](../contributing/testing.md)).

    ```cpp
    TEST_CASE("A missile moves at its velocity", "[game]")
    {
        Engine engine;
        Ecs &ecs = engine.getEcs();
        Scenes &scenes = engine.getScenes();
        ecs.registerSystem<MovementSystem>(scenes);
        const SceneId game = scenes.open();
        const entity_t missile = ecs.createEntity();
        ecs.addComponent<SceneMember>(missile, game);
        ecs.addComponent<Position>(missile, 100.0f, 540.0f);
        ecs.addComponent<Velocity>(missile, 1200.0f, 0.0f);

        engine.update(0.5f);

        REQUIRE(ecs.getComponent<Position>(missile).x == 700.0f);
    }
    ```

## Rules to keep in mind

| Situation | Do |
|---|---|
| A component points to another entity (target, owner) | Store an `EntityRef` (`ecs.ref(entity)`) and check it with `ecs.isAlive(reference)` before using it: ids are reused. |
| An entity is attached to another one (the Force to the ship) | Use the [hierarchy](../architecture/scenes-and-interface.md#hierarchy): `attach()`, then move it with its `LocalPosition`. |
| The system creates or destroys entities, or adds components of a type it goes through | Wrap the change in `ecs.defer(...)`: it runs after the systems, when no cache is being read. Destroying is safe while iterating, creating is not. |
| The system needs data shared by every entity (score, wave number) | Give it to the constructor by reference, like the `Scenes` above, rather than a global. |

## Result

Entities with a `Position` and a `Velocity` move every frame, and stop moving when their scene is covered by
another one. The test runs in the CI with the other unit tests.
