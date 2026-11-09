# 0031 — The destructions, unreliable, sent again with the snapshots

| | |
|---|---|
| **Status** | Accepted; amends the "Events" of [0016](0016-replication.md) |
| **Date** | 2026-11-07 |

## Context

[ADR 0016](0016-replication.md) sends each destruction as a reliable message, so that a lost part of a snapshot
never looks like a destruction. A reliable message is delivered in order: one lost holds back every reliable
message behind it until it is sent again, after `max(1.5 × RTT, 100 ms)`.

Since the interpolation draws the world 50 ms in the past ([ADR 0030](0030-measured-delays.md)), a destruction
has about 50 ms to arrive before the drawn tick reaches it. One lost meant a Bydo or a shot drawn on after its
end, then its explosion late, and the destructions after it late too: the shots that leave the game area are
destroyed many times per second.

## Options

| | Kept? | Why |
|---|---|---|
| A. Reliable, as before | No | A loss costs a resend and holds back what follows |
| B. Unreliable, once | No | A loss leaves an entity drawn for good |
| C. Unreliable, sent again with the snapshots for a while | **Yes** | A loss costs a snapshot interval, as for the inputs ([ADR 0016](0016-replication.md)) |
| D. Each part says the range of ids it covers: an entity missing from it is gone | No | Loses the cause, hence the explosions, unless the destructions are sent besides |

## Decision

**C.** `ENTITIES_DESTROYED` is unreliable, sent at its tick, then again with each snapshot of the next 30 ticks
(`RecentDestructions` in the instance). The message does not change; the client already ignored a destruction
of an entity it does not have. As a last resort, an entity no snapshot had for 60 ticks is removed, without an
effect (`RemoteWorld::forgetStale()`): every copy of its destruction was lost.

## Consequences

- A lost destruction arrives with the next snapshot, 33 ms later, instead of after a resend; none holds back
  another.
- The destructions cost more bandwidth: each one 15 times, 5 bytes each, packed with the snapshots. A burst of
  100 destructions is about 500 bytes per snapshot for half a second.
- `WAVE_STARTED` and `GAME_OVER` stay reliable: they are rare, and the next snapshot does not carry them.
