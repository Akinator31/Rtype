# 0038 — The time of a shot of the player glides from its ship to the world

| | |
|---|---|
| **Status** | Accepted; amends the time of the shots of [0029](0029-predicted-shots.md) |
| **Date** | 2026-11-11 |

## Context

Online, the ship of the player is drawn at the tick of its newest input, ahead of the game; the Bydos at the
drawn tick, in the past ([ADR 0016](0016-replication.md), [ADR 0030](0030-measured-delays.md)). Its shots were
drawn in the time of the ship ([ADR 0029](0029-predicted-shots.md)), from the press to their end.

The gap between the two times is the lead of the inputs, a round trip and the delay of the interpolation: about
5 ticks on the same machine, 12 to 15 with a server 80 ms away. A shot in the time of the ship met each Bydo
that far in the future of the Bydo: one going down the screen at 200 units per second was drawn 40 to 50 units
from where the game had it. On a distant server, the players saw their shots go through Bydos without any
damage, and stop short of the ones they hit.

## Options

| | Kept? | Why |
|---|---|---|
| A. Draw the shots in the time of the ship (as before) | No | What the shot seems to hit is not what it hits, more so the further the server |
| B. Draw them in the time of the world | No | They appear a round trip and the delay of the interpolation after the press, behind the nose: what ADR 0029 fixed |
| C. Lag compensation: the game tests the hits of the shots of a player against the Bydos as its client drew them | No | The game keeps the past positions of every Bydo; the client tells its drawn tick in every `INPUT` (a new version of the protocol); the rules of the hits depend on each client. Too much for a game of cooperation |
| D. The time of a shot glides from the ship to the drawn tick as it flies | **Yes** | It leaves the nose at once, and meets the Bydos in their time; only the client changes |

## Decision

**D.** `ShotPrediction::timeOf(fired, drawn, ship)` gives the tick each shot of the player is drawn at: the
tick of the ship when it is fired, gliding to the drawn tick by a smoothstep over `max(15, 3 × gap)` ticks of
flight, then the drawn tick. A shot of the game too old to tell when it was fired (age 255) is drawn at the
drawn tick. Its end is drawn once its time, not the ship, is past its destruction.

## Consequences

- What a shot seems to hit is what it hits, as soon as it glided: within a third of a second of flight on the
  same machine, under a second with 250 ms of gap.
- While it glides, a shot seems a little slower, never less than half its speed; the faster weapons show it
  the most.
- A Bydo right in front of the nose is still met in the time of the ship: the shot flies through it until the
  destruction comes, as before.
- Its impact comes when the Bydo it hit flashes, both in the time of the world.
- `tests/client_shot_prediction.cpp` checks that the time goes forward at every frame, never slower than half,
  and that every predicted shot is drawn where the game has it at its time, for every weapon.
