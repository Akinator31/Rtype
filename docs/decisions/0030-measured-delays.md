# 0030 — A round-trip time without the delay of the acknowledgements, an interpolation delay measured

| | |
|---|---|
| **Status** | Accepted; amends the delay of 100 ms of [0016](0016-replication.md) |
| **Date** | 2026-11-07 |

## Context

Two delays of the client online were wrong for every network.

- **The round-trip time** was the time between sending a packet and receiving its acknowledgement. The game
  acknowledges an `INPUT`, unreliable, only with its next datagram: a snapshot, up to 2 ticks later. The ping
  shown was up to 33 ms too high, and so were the estimate of the first stamps, the time a jump of the stamps
  takes to show ([ADR 0028](0028-input-stamps-follow-the-lead.md)) and the resend delay.
- **The interpolation** drew the world 100 ms in the past, always: twice what a steady network needs, too little
  for one that jitters by more than 70 ms.

## Options

| | Kept? | Why |
|---|---|---|
| A. Acknowledge each `INPUT` at once, in a datagram of its own | No | 60 more datagrams per second per client, for a measure |
| B. A ping and its pong, timed by the game | No | A message more, and its own delays in the queues |
| C. Each body tells how long its `Ack` waited (`Ack delay`, as QUIC) | **Yes** | 2 bytes per datagram, the measure stays where it was |

| | Kept? | Why |
|---|---|---|
| D. A fixed delay, 100 ms | No | Too long on a good network, too short on a bad one |
| E. A delay set by the player | No | A setting nobody understands |
| F. The delay each world needed, the largest of the last seconds | **Yes** | Follows losses and jitter, comes back down by itself |

## Decision

**C and F.**

- The body has an `Ack delay`, a `u16` in tenths of a millisecond after the ack bits: the time since its `Ack`
  was received. A sample of the RTT is taken from the packet `Ack` only, minus that delay; the packets
  acknowledged by the bits, whose delay is not known, and a saturated delay give none. The game protocol is at
  version 2 in this branch already ([ADR 0028](0028-input-stamps-follow-the-lead.md)).
- Each world the interpolation receives tells the delay it needed not to be waited for: the ticks since the
  previous one, plus how much later than the earliest of the last 3 s it arrived, plus a tick. The delay is the
  largest of those 3 s, from 50 to 250 ms; 100 ms before 8 worlds came. The drawn tick follows it as it follows
  its target already, a little faster or slower, never backwards.

## Consequences

- On a steady network, the world is drawn 50 ms in the past instead of 100: the others, the Bydos and their
  shots are 50 ms more recent, and a hit looks closer to where it happens.
- A network that jitters or loses snapshots is drawn further back, enough that no world is waited for, and
  comes back to 50 ms 3 s after it is steady again.
- The ping shown is the network's, within a frame: the client receives its datagrams once per frame.
- A bad spike keeps the delay up for 3 s.
