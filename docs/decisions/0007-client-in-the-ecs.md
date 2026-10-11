# 0007 — The whole client in the ECS, with scenes and a hierarchy

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-07 |
| **Supersedes** | [0005](0005-world-in-ecs-interface-in-ui-layer.md) |

## Context

[ADR 0005](0005-world-in-ecs-interface-in-ui-layer.md) put the game world in the ECS and the interface (menus,
HUD) in a separate library of screens and widgets. It was decided while the ECS did not exist, and it relied
on three reasons: the interface could not wait for the ECS, an interface needs parent/child relations, and
interface needs could weigh on an ECS shared with the server.

Since then:

- The ECS is done ([ADR 0006](0006-ecs-component-storage.md)). It is generic: any type can be a component,
  so components of the interface can live in the client only, unseen by the server.
- Writing the interface library showed that most of its code would only serve the interface: drawing texts,
  positions, and above all **animations**. A button bouncing when it gets the focus, an enemy flashing when
  hit and a bonus appearing are the same thing, and would be written twice.
- The team decided that the HUD is drawn by the ECS.

The questions were where the interface lives, how screens and game phases (home, game, pause) are organised,
and how entities attached to others (a dropdown and its options, the Force and the ship) are handled.

## Options

### Where the interface lives

| | A. Separate interface library (ADR 0005) | B. Interface made of entities |
|---|---|---|
| Code shared with the game | Little: the library has its own drawing and no animations | Drawing, positions, hierarchy, animations |
| Animations | Written in each widget | One `Tween` component and system, for the interface and the game |
| Separation from the world (VR panel) | Natural: two separate parts | A `Layer` component: world and interface are drawn in two passes |
| Input priority over the game | Returned by the library | The scene at the top receives the input (`Scenes`) |

### Scenes

| | One ECS, entities tagged by scene | One ECS per scene | No scenes, a global state |
|---|---|---|---|
| Entities of several scenes at once (transition, star-field shared by the menus and the game, pause over the game) | Natural | Two worlds to keep in sync | Natural |
| Cleaning a closed scene | One generic function destroying its entities | Free: the whole ECS is destroyed | By hand, error-prone |
| Systems | Ask which scene receives the input or updates | Registered per scene | One `switch` growing with the game |

### Parent / child relations

Without a hierarchy, every attached entity (dropdown options, Force, turrets of a boss, health bar above a
ship) must be moved, hidden and destroyed by hand. With one, it follows its parent. Entity ids are reused by the
ECS, so a reference kept by a child could end up designating another entity: references need a generation
counter.

## Decision

**B, in one ECS with tagged scenes, and a hierarchy.**

- **Everything the client draws is an entity**: the world, the HUD and the menus. A `Drawable` component gives
  its `Layer` (`World` or `Interface`) and its order. The `SceneRenderer` draws the world layer, then the
  interface layer: the interface stays on top, and the two passes stay separate for VR.
- **Scenes** (`engine::Scenes`, in `libs/engine`): every entity of a scene has a `SceneMember`; closing a scene destroys them all.
  Entities without it are persistent. Scenes form a stack: only the scene at the top receives the input, a
  scene can keep updating when covered (the network game under the pause menu), and an overlay keeps the scenes
  below visible.
- **Hierarchy** (`ecs::Parent`, `LocalPosition`, `Hidden`): a child follows its parent, is hidden with it and is
  destroyed with it. References between entities are `EntityRef`, with a generation.
- **The ECS gives the elapsed time** to every system, and runs **deferred actions** after its systems, so that a
  button can open a menu without changing the world while the systems go through it.
- **The interface library is removed.** Its behaviour (focus, navigation with the keyboard, a gamepad or the
  mouse, text fields, values read through functions) is kept by components and systems of the client library
  `libs/engine`.

## Consequences

- Animations, drawing and positions are written once, for the game and the interface.
- The ECS gains features used by the server too (hierarchy, entity references, elapsed time), and its systems
  now take the elapsed time: `update(float elapsed)`.
- The order of the systems matters and is documented: scenes first, then the interface (which reads the input
  before the game), then the game, then what turns the state into positions and sizes.
- Every system working on entities of a scene must ask `Scenes` whether the entity receives the input or
  updates. Persistent entities are never forgotten in a closed scene, but must not hold references to entities of
  a scene without checking them with `Ecs::isAlive()`.
- Menus are built with `MenuBuilder`, which only creates entities: anything can be added afterwards, an icon or
  an animation.
