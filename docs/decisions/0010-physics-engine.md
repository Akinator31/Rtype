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

`CollisionLayer` and `CollisionMask` each hold a 32-bit mask. The game assigns meanings to the bits.
`canCollide(a, b)` accepts a pair only when `a`'s mask includes at least one of `b`'s layer bits
and `b`'s mask includes at least one of `a`'s layer bits. A missing layer or mask is treated as
all bits set, so an entity with neither component accepts every layer. An explicit zero mask or
layer matches nothing.

### Shapes and geometry

- **Axis-aligned boxes first**, spheres next, behind a `std::variant` in `Collider`. No rotated boxes, no
  polygons, no meshes.
- **The geometry is written in 3D** (`Vec3`, `Aabb`, `Sphere`, `Ray`), as pure functions independent of the
  ECS. The game is played on a plane: `Position` stays `{x, y}` in the ECS, and the systems use `z = 0`. The
  same functions serve later for anything in 3D (picking with a ray, a 3D view) without a rewrite.

`Aabb::halfSize` contains the distances from its center to its faces. A shape stored in a `Collider` has
its world center at `{Position.x, Position.y, 0} + Collider::offset + shape.center`.
Intersections and point containment include touching boundaries, including zero-size shapes. A ray
uses `origin + direction * t` for `t >= 0`; its direction need not be normalized, and a ray starting
inside a shape hits at `t = 0`. Penetration returns a normal pointing from the first shape toward the
second and a nonnegative depth; no overlap returns no result. For coplanar AABBs of zero `z` thickness,
penetration chooses an `x` or `y` axis so the result can resolve a 2D collision.

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

### Collision events

`CollisionSystem` exposes the frame's `Collision` events through `collisions()`. Each event contains two
`EntityRef` values, a normal pointing from the first entity toward the second, and a penetration depth.
Game systems registered after physics can read the list to apply damage, pickups, or scoring.

`PhysicsStep` clears the list at the start of every frame and collects contacts across its fixed steps.
The same pair appears once per frame, including when it touches across several steps. A frame without a
physics step has an empty list. A standalone `CollisionSystem` clears its list on each `update()` call.
Game systems must use `ecs.defer()` to destroy an entity after reading the events; `EntityRef` remains
recognizable as stale if an entity is later destroyed or its id reused.

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
4. **Collision events** added to the system's list.
5. **Resolution** for two non-trigger colliders when one of them is `Static`: the moving entity is pushed out
   along the axis of least penetration, and its velocity along that axis is cancelled. Triggers are never
   resolved. Two mobile colliders and two static colliders produce events without physical separation.
   `MovementSystem` does not move an entity marked `Static`, even if it has a `Velocity`. When a mobile child
   is pushed, its `LocalPosition` and world `Position` are both updated.
6. **Fast AABB entities** (`FastMoving`) expand their grid bounds over the displacement of the step. If the
   shapes do not overlap at the end of the step, a swept AABB test checks their relative movement and emits
   a contact with zero penetration at the first hit. A fast solid is stopped at the first static obstacle;
   a trigger only emits an event. Unmarked entities do not run swept narrow-phase tests. The sweep uses
   `Velocity * elapsed` for the preceding position, so teleports and position corrections need a separate
   collision check. Spheres continue to use overlap tests only.

### Zone bounds

`BoundsSystem` runs after movement and before the hierarchy transform. `ConfinedToBounds` keeps a whole
collider inside its rectangle and cancels velocity directed out through a blocked edge. An entity without
a collider is treated as a point. If a collider is too large to fit on one axis, its center is placed at
the zone midpoint on that axis and its velocity on that axis is cancelled. A `Static` entity is not moved
by confinement.

`DestroyOutOfBounds` removes an entity through `ecs.defer()` once its entire collider lies beyond the
rectangle expanded by `margin`. Touching the expanded border is still inside. A child uses its current
world position from its parent's position and its `LocalPosition`; confinement adjusts its local position
so the following `TransformSystem` preserves the result. Zones must have finite ordered limits and the
destruction margin must be finite and nonnegative.

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
