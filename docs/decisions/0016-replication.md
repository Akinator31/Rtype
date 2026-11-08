# 0016 — Replication of the game world

| | |
|---|---|
| **Status** | Proposed; its "Inputs" amended by [0028](0028-input-stamps-follow-the-lead.md), its delay of 100 ms by [0030](0030-measured-delays.md), its reliable destructions by [0031](0031-destructions-sent-again.md) |
| **Date** | 2026-10-17 |

## Context

The game instance runs the `Simulation` of `libs/game` at 60 ticks per second and is authoritative: clients
send what their players do, never positions or results ([ADR 0015](0015-one-game-scene-local-or-remote-session.md),
[ADR 0013](0013-one-process-per-game.md)). The protocol already defines how to exchange messages over UDP,
encrypted, reliable or not ([game protocol](../protocol/game.md)). What travels inside is left open: ADR 0015
notes that "full or delta snapshots, and how often they are sent, are the next decisions".

Two flows must be decided:

- **Inputs**, from each client to its game: what the player does at each tick.
- **The world**, from the game to each client: the entities (ships, Bydos, missiles), the players' scores and
  lives, and what happens (an explosion, a new wave, the end of the game).

Requirements:

- **Losses and reordering are normal.** A lost datagram must cost at most a little smoothness, never a wrong
  world on the client: an entity that stays on screen after being destroyed, an action never received.
- **One datagram is at most 1200 bytes** ([game protocol](../protocol/game.md#2-datagram-layout)), of which
  about 1150 for the messages. A game holds tens to a few hundred entities: missiles are many and short-lived.
- **Latency matters more than bandwidth.** Waiting for a resend (an RTT or more) is visible; a few tens of
  kilobytes per second per player are not a problem for a server or a home connection.
- **Simple to implement and to test**, in the client and in the server, by a team new to networking. Another
  client must be writable from the specification alone.
- **Ready for later improvements**: interpolation now, client-side prediction of the local ship later,
  compression if measurements show the need.

## Options

### The world

| | A. Full snapshot every tick | B. Full snapshots at a lower rate, explicit destructions | C. Delta snapshots | D. Reliable events only |
|---|---|---|---|---|
| Idea | The state of every entity, 60 times per second | The state of every entity every 2 ticks (30 per second), split in parts if needed; destructions as reliable messages | Only what changed since the last snapshot the client acknowledged (the Quake 3 model) | "Spawned here", "moved there", reliable |
| A lost datagram | Nothing: the next one replaces it | Nothing: the next one replaces it | Nothing, the base is the last acknowledged one | Everything waits for the resend |
| Size per player | ~16 bytes × entities × 60 / s | Half of A | Much smaller when few things change; but in a shoot'em up almost everything moves | Small, but many messages |
| Entities beyond one datagram | Several datagrams per tick | Several parts, each one usable alone | Same, plus the base tracking | — |
| Destruction | An entity missing from the snapshot | A reliable message: a lost part does not look like a destruction | Explicit in the delta | Explicit |
| Complexity | Low | Low | High: a history of snapshots per client, acknowledgements, a base per client | Medium, and laggy |
| Smoothness on the client | Interpolation between snapshots | Interpolation, 100 ms behind | Same as B | Jerky on losses |

### Inputs

| | A. The current state, unreliable | B. Tick-stamped, unreliable, sent again in the next datagrams | C. Reliable |
|---|---|---|---|
| Idea | "These buttons are held now" | "At tick T the buttons were…, at T−1…": the last 8 ticks in every datagram | Each input as a reliable message |
| A lost datagram | The input of that moment is lost; a short tap may never arrive | Recovered by the next datagram: 8 datagrams in a row must be lost to lose an input | Waits for the resend: the ship reacts an RTT late |
| Which tick an input belongs to | Unknown: the server applies it when it arrives | Known: the server applies it at the right tick, and can tell the client which input it applied last (needed for prediction) | Known |
| Size | 1 byte | 1 byte per tick: 8 bytes | 1 byte, plus the reliability |

## Decision

**World: B. Full snapshots every 2 ticks (30 per second), unreliable, split in independent parts when they do
not fit in one datagram; entities destroyed with a reliable message. Inputs: B. One byte of buttons per tick,
stamped with its tick, the last 8 ticks in every datagram.**

### Snapshots

- A **snapshot** holds the tick, the players' scores and lives, and the state of each entity: id, kind,
  owner, position, velocity, health and flags, 16 bytes per entity in fixed point. A datagram carries up to
  69 entities; beyond, the snapshot is split in up to 16 **parts**, each with the tick and the players.
- Each entity state is complete: a part can be applied alone, a lost part only delays the update of its
  entities. The client creates an entity the first time it appears in a snapshot.
- An entity is **destroyed** only by `ENTITIES_DESTROYED`, reliable, with the cause (killed, or removed when it
  leaves the game area). Missing from a snapshot means nothing, since its part may be lost. The game never
  reuses an entity id, so a late snapshot cannot be mistaken for a new entity.
- The client keeps the last snapshots and draws the world **100 ms in the past**, interpolating between the two
  snapshots around that time: three snapshots, so one lost snapshot goes unnoticed.

### Inputs

- An input is one byte per tick: up, down, left, right, fire. The client samples the virtual gamepad once per
  tick and sends `INPUT` with the last 8 inputs.
- The game applies, at each tick, the input of that tick if it has it, otherwise the last one it has. Inputs of
  ticks already simulated are ignored; inputs too far ahead too.
- Each snapshot tells the client the tick of its last input that the game applied: unused for now, it is what
  prediction needs later. **Amended by [ADR 0028](0028-input-stamps-follow-the-lead.md)**: each snapshot also
  tells how early the inputs arrive, and the client paces its inputs on it.

### Events

Only what cannot be read from the next snapshot is a reliable message: a destruction (to draw an explosion and
remove the entity), a new wave (for the HUD), and the end of the game. Scores and lives are in every snapshot:
a lost snapshot is replaced, and the client never shows an old score after a newer one. **Amended by
[ADR 0031](0031-destructions-sent-again.md)**: the destructions are unreliable, sent again with the snapshots of
the next 30 ticks.

## Consequences

- The client and the server only build or read whole snapshots: no history per client, no base to agree on. A
  replication bug can be reproduced offline, since `LocalSession` produces the same `Snapshot`
  ([ADR 0015](0015-one-game-scene-local-or-remote-session.md)).
- Bandwidth, for 100 entities: 2 datagrams per snapshot (about 1.8 KB), 30 snapshots per second, so about
  55 KB/s per client from the server, and about 4 KB/s from each client. Four players: about 220 KB/s for an
  instance.
- The world is drawn 100 ms late, on top of the network latency. The local ship can be predicted later, from
  the inputs not applied yet, without changing the messages.
- A game cannot hold more than 16 × 69 = 1104 entities in a snapshot: far beyond what a level needs. The
  game must keep its entities in the area that positions can encode (−4096 to 4096 units), and destroy those
  that leave it.
- Delta compression is the next step if a level ever needs much more bandwidth: it changes the `SNAPSHOT`
  message only, and increases the protocol version.
- Adding an entity kind or an event does not change the version: older clients skip what they do not know.
