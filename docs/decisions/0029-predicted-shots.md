# 0029 — The shots of the player are predicted, then matched by their age

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-11-07 |

## Context

Online, the ship of the player is predicted ([ADR 0016](0016-replication.md)): drawn at the tick of its newest
input, ahead of the game. Its shots were drawn with the rest of the world, through the interpolation, 100 ms
in the past on top of the round trip. On the screen of the player, a shot appeared a round trip and 100 ms
after Fire, far from the nose of the ship, and trailed behind it; on the screen of the others, everything being
in the same past, it looked right.

Moving the shots on to the tick of the ship (drawing them in its time) puts their trail back in line, but they
still appear that late, already that far ahead.

## Options

| | Kept? | Why |
|---|---|---|
| A. Draw the shots in the time of the ship only | No | Their trail is right, they still appear a round trip late |
| B. Draw a shot when Fire is pressed, from the fire interval alone | No | Wrong as soon as the magazine empties or the cooldown is not where the client thinks: a shot too many, or one missing |
| C. Predict the weapon with the steps of the game, reconciled from its state in the snapshots; match each shot of the game by the tick it was fired at | **Yes** | Exact when nothing is lost; a wrong guess is corrected by the next snapshot |
| D. As C, matched by order or by position | No | Fragile: a lost or destroyed shot shifts every match after it |

## Decision

**C.**

- How a weapon steps, where the shot of a player starts and how a shot moves are in one place,
  `WeaponFiring.hpp` (`stepWeapon()`, `playerShot()`, `moveShot()`), used by the systems of the game and by the
  prediction of the client, as `moveShip()` is for the ship. A test checks them against the game, to the bit,
  for every weapon of `config/weapons/`.
- Each `SNAPSHOT` gives the client the state of the weapon of its ship (cooldown, ammo, reload: 6 bytes), and
  each shot its **age** in the byte of the health, which a shot does not have: fired at `tick − age`. The game
  protocol is at version 2 already ([ADR 0028](0028-input-stamps-follow-the-lead.md)): no new version.
- The client replays its inputs from the state of the weapon at the tick of each snapshot, as it does for the
  ship, and draws each shot fired at once, under an id of its own. The shot of the game fired at the same tick
  takes its place, under that id: one entity from the press to the end, one sound.
- A predicted shot the prediction no longer gives, or that no snapshot brought 6 ticks after its tick, is no
  longer drawn.

## Consequences

- A shot appears on the frame the input is sent, at the nose of the ship drawn, and goes on without a jump when
  the game's comes, wherever the drawn world is.
- The client must have the same definitions as the server (`config/`): with another weapon, the shots are
  predicted wrong, then dropped, and those of the game drawn as they come, late.
- Bydos are drawn in the past: a shot that hits within a round trip of the ship flies through its target for
  that long, until its destruction comes. **Amended by [ADR 0038](0038-shots-glide-to-the-time-of-the-world.md)**:
  the time of a shot glides from the ship to the drawn tick as it flies, so that it meets the Bydos in their
  time.
- A part of a snapshot holds the same 69 entities with four players; 70 instead of 71 without players.
