# 0005 — World in the ECS, interface in a separate layer

| | |
|---|---|
| **Status** | Superseded by [0007](0007-client-in-the-ecs.md) |
| **Date** | 2026-10-06 |

## Context

The client draws two different kinds of things:

- **The game world**: ships, Bydos, missiles, obstacles, the star-field. Most of it comes from the server,
  which simulates the game with the ECS (`libs/ecs`); the client keeps a copy of these entities in its own
  ECS.
- **The interface**: menus (home, connection, waiting room, options, game over) and the in-game HUD (score,
  lives, players).

We had to decide whether the interface is also made of ECS entities, or lives outside of the ECS. Constraints:

- The ECS is shared with the server, which has no interface. It is developed by another team member and
  does not exist yet.
- Menus exist before any game world: the home and connection screens are shown before the client is
  connected to a server.
- Menus must be usable with a keyboard and a gamepad only, which requires a notion of focus (the selected
  button) and navigation between widgets. This is also needed for [accessibility](../accessibility.md) and
  for VR, where there is no mouse.
- VR is planned with a "board" view: the world is drawn on a board floating in front of the player, and the
  interface on a separate floating panel. World and interface must be drawable separately.

## Options

| | A. Everything in the ECS | B. World in the ECS, interface in a separate layer | C. Interface outside the ECS, including on entities |
|---|---|---|---|
| Single way of describing what is drawn | Yes | No: entities for the world, widgets for the interface | No |
| Can start before the ECS exists | No | Yes | Yes |
| Menus without a game world | Needs a "menu world" | Natural | Natural |
| Widget hierarchy, focus, navigation | The ECS must support parent/child relations | Handled by the interface layer | Handled by the interface layer |
| Health bars and names following entities | Natural | Natural: they are drawn by the ECS | The interface must track entities |
| Impact on the ECS shared with the server | Must support interface needs | None | None |
| World and interface drawn separately (VR) | Possible, with layers | Natural: two passes | Natural |

Engines use both approaches: Unity and Unreal keep their interface systems apart from their game objects,
while Bevy builds its interface with ECS entities.

## Decision

**Option B.** The game world is made of ECS entities; the interface is a light layer of screens and
widgets in the client, which does not know the ECS.

| Element | Where |
|---|---|
| Ships, Bydos, missiles, obstacles, star-field | ECS, drawn by the render system |
| Interface attached to an entity: health bar above a ship, player name, damage numbers | ECS, drawn by the render system, since it follows the entity and disappears with it |
| Menus, options, connection and game over screens | Interface layer |
| HUD fixed on screen: score, lives, list of players | Interface layer |

Four rules keep the two parts from mixing:

1. **Only the `Renderer` draws.** The ECS render system and the interface both draw through it; neither
   calls raylib.
2. **The order is fixed**: in each frame, the world is drawn first, then the interface on top of it.
3. **The interface does not read the ECS.** The game code gives it plain values (score, lives, player
   names). A change in the ECS does not change the interface.
4. **The ECS does not know the interface.**

## Consequences

- The interface can be built now, in parallel with the ECS.
- The ECS stays focused on the game: it does not need parent/child relations nor focus handling for
  widgets, and the server is not affected by interface needs.
- A frame of the client is made of two passes, world then interface. In VR, the world pass is drawn on the
  board and the interface pass on a floating panel.
- There are two ways of describing what is drawn: entities for the world, widgets for the interface.
  Developers must know where each element belongs; the table above is the reference.
- The interface layer must stay light: a few widgets (button, text, text field), a stack of screens, and
  keyboard and gamepad navigation first. It is not meant to become a general-purpose interface framework.
- To revisit if the ECS ends up supporting parent/child relations and the team wants a single model: the
  interface could then be moved to entities, screen by screen.
