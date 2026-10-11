# 0010 — A custom kinematic physics engine

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-11 |

## Context

The engine needs physics: entities that move, and the contacts between them (a missile hits a Bydo, a ship
picks up a power-up, a ship runs into the walls of a level). The physics belongs to the engine, not to the
R-Type: another game made with it must be able to use it with its own collision layers and its own reactions.

Requirements:

- **Kinematic only.** A shoot'em up has no mass, rotation, friction, bounce or stacking: entities move at the
  speed they are given, and only the contacts matter, plus blocking against static obstacles.
- **In the ECS.** Movement and collisions are components and systems, like the rest of the engine
  ([ADR 0007](0007-client-in-the-ecs.md)), with no second world to keep in sync.
- **Without the engine library.** The server simulates the game and has no window: the physics depends on
  `libs/ecs` only, never on raylib nor on `libs/engine`, where the scenes and the events live
  ([ADR 0008](0008-events-switch-systems.md), [ADR 0009](0009-named-scenes-in-the-engine.md)).
- **Independent of the frame rate.** The same inputs give the same results at 30 or 144 frames per second,
  so that the server and the clients agree, the client can predict and be corrected, and tests are reproducible.
- **A few hundred colliders** per update on the server (players, Bydos, missiles), within a fraction of a frame.
- **3D assets are planned.** The collision shapes are set by hand, independently of what is drawn: never the
  mesh of a model.

## Options

| | A. Box2D | B. Jolt Physics | C. Custom kinematic physics |
|---|---|---|---|
| Dimensions | 2D | 3D | Ours: 3D geometry, used in 2D with `z = 0` |
| What it brings | Rigid bodies, joints, continuous collisions | Rigid bodies, characters, constraints, multithreading | Movement, overlaps, layers, blocking |
| Used features | Overlaps and filtering: a small part | Same, an even smaller part | All of them |
| Data | Its own `b2World`: positions copied from and to the components every step | Its own `PhysicsSystem`, same copies | The ECS components themselves |
| Creating and destroying entities | Mirrored in its world (bodies to create and destroy) | Same | Nothing to do |
| Determinism | Same binary and platform | Cross-platform when built for it | Same binary and platform; ours to control |
| Dependency | One more CPM package, in the server too | A large one, in the server too | None |
| Code to write | The synchronization with the ECS | Same | Integration, intersection tests, broad phase, tunneling: a few hundred lines |

A and B solve problems the engine does not have (rigid body dynamics), and their main cost, a second world to
keep in sync with the ECS, is paid on every entity. Their strength, stable stacking and joints, is not used.

## Decision

**C: a custom, kinematic physics library, `libs/physics` (`rtype::physics`), depending on `rtype::ecs` only.**

### What the engine provides, and what the game provides

The physics provides **components**, **systems** and pure functions, and knows no game:

| Provided by the physics | Provided by the game |
|---|---|
| `Velocity`, `Acceleration`, `MaxSpeed` | The values |
| `Collider` (shape, offset, `trigger`), `Static`, `FastMoving` | The shapes and sizes, set by hand |
| `CollisionLayer`, `CollisionMask`, as 32-bit masks, and `canCollide()` | The names of the bits (`Player`, `Enemy`, …) |
| `ConfinedToBounds`, `DestroyOutOfBounds` | The zones |
| `MovementSystem`, `BoundsSystem`, `CollisionSystem` | Its own systems that react to the contacts (damage, pick-ups, score) |

### Shapes and geometry

- **Axis-aligned boxes first**, spheres next, behind a `std::variant` in `Collider`. No rotated boxes, no
  polygons, no meshes.
- **The geometry is written in 3D** (`Vec3`, `Aabb`, `Sphere`, `Ray`), as pure functions independent of the
  ECS. The game is played on a plane: `Position` stays `{x, y}` in the ECS, and the systems use `z = 0`. The
  same functions serve later for anything in 3D (picking with a ray, a 3D view) without a rewrite.

### Fixed time step

The physics runs in **fixed steps** (1/60 s by default, set by the game), whatever the frame rate. A single
system registered in the ECS, `PhysicsStep`, keeps an accumulator and runs the physics systems once per step,
in this order:

```cpp
void PhysicsStep::update(float elapsed)
{
    _accumulator += elapsed;
    for (int steps = 0; _accumulator >= _step && steps < _maxSteps; ++steps) {
        _movement.update(_step);  // Velocity and Acceleration → Position (LocalPosition for children)
        _bounds.update(_step);    // ConfinedToBounds, DestroyOutOfBounds
        _transform.update(_step); // Position of the children, from their parent
        _collisions.update(_step); // contacts, then blocking against Static colliders
    }
    _accumulator = std::min(_accumulator, _step); // late beyond _maxSteps: slow down instead of freezing
}
```

- The game's systems that write velocities (input, AI) are registered **before** `PhysicsStep`, those that read
  contacts **after** it. They run once per frame, the physics zero, one or several times.
- The cap on steps per frame avoids the "spiral of death": a machine too slow to keep up slows the game down
  instead of freezing it.
- The remaining fraction of a step (`alpha`) is exposed, so that the drawing can interpolate between two steps.
- Each physics system also derives from `rtype::ecs::System` and can be registered alone, for a test or a
  game that does not want a fixed step.

### Contacts as components

As decided in [ADR 0008](0008-events-switch-systems.md), what a system needs is in components. The
`CollisionSystem` gives each entity in contact a **`Contacts` component**: the list of the entities it touches,
with the normal and the depth of each contact. The game's systems go through the entities that have it (a
damage system goes through the missiles that have `Contacts`).

`PhysicsStep` clears `Contacts` at the start of each of its updates, once per frame, then fills it across all
the steps of the frame: a contact found in any step is seen once by the game's systems, which run once per frame,
and a frame without any step leaves no old contact behind. Entities are never destroyed while a system goes
through them: destruction goes through `ecs.defer()`.

### Scenes

The physics does not know the scenes, which live in the engine. `PhysicsStep` takes an optional filter,
`std::function<bool(entity_t)>`: the engine gives it `scenes.updates(entity)`, so that the entities of a covered
scene (the game under the pause menu) do not move. Without a filter, every entity is simulated, which is what
the server and the tests do.

### Collisions in a step

1. **Broad phase**: a uniform grid, cells about the size of a typical collider; a naive version first, kept in
   the tests to check that the grid finds the same pairs.
2. **Filtering** with `canCollide()`: both layers must accept each other.
3. **Narrow phase** with the geometry functions, which also give the normal and the depth.
4. **Contacts** added to both entities.
5. **Resolution** for two non-trigger colliders when one of them is `Static`: the moving entity is pushed out
   along the axis of least penetration, and its velocity along that axis is cancelled. Triggers are never
   resolved.
6. **Fast entities** (`FastMoving`) are tested over the volume swept during the step (swept AABB), so that a
   fast missile does not go through a thin Bydo between two steps. Other entities do not pay for it.

## Consequences

- Movement and collisions are ordinary components and systems: no second world, nothing to synchronize when an
  entity is created or destroyed, and the server links the physics without raylib.
- The game decides everything that is specific to it (layers, shapes, reactions), so the physics can be reused
  by another game made with the engine.
- The simulation does not depend on the frame rate, which the network prediction and the tests need.
- The results are identical for the same binary on the same platform. Between Linux (GCC) and Windows (MSVC),
  floating-point results may differ slightly: the server is authoritative and corrects the clients, so this is
  acceptable, but exact cross-platform replays would need fixed-point numbers.
- Everything is ours to write and test: intersection tests, grid, swept collisions. The work is split into the
  tasks of the "Physics Engine" issue.
- No rigid body dynamics. If a game needs mass, rotation, joints or stacking, a library such as Jolt Physics
  would be considered then, in a new record that supersedes this one.
