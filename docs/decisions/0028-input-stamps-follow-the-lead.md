# 0028 — The stamps of the inputs follow the lead the game reports

| | |
|---|---|
| **Status** | Accepted; amends the "Inputs" of [0016](0016-replication.md) |
| **Date** | 2026-11-07 |

## Context

[ADR 0016](0016-replication.md) stamps each input with the tick at which the game should apply it. The client
estimated that tick from the latest snapshot and the round-trip time, plus 2 ticks of margin, and never let a
stamp go back: `stamp = max(previous + 1, estimate)`.

Online, a player who did not move for a while, then moved, saw the ship come back to where it was, as if the
game did not listen. That is what happened:

- a stamp could only go up. Every excess stayed for good: a peak of the RTT, a client clock a little fast (a
  sum of frame times 1% too long is 0.6 tick per second), the game skipping the ticks it missed after a stall
  (`GameThread::MaxLag`);
- the game ignores the inputs more than 60 ticks ahead, without telling. After some time (about 100 s for a
  clock 1% fast), every input was ignored, and the ship stopped in the game;
- standing still, the prediction and the game agree, so nothing shows. Moving, the prediction moves the ship,
  the snapshots bring it back.

Below 60 ticks, the excess was input latency for nothing: the shots left later and later after the press.

The client cannot know how early its inputs arrive: only the game sees it.

## Options

| | Kept? | Why |
|---|---|---|
| A. A better estimate (RTT without the delay of the acknowledgements, a filter) | No | Still open loop: a drift of the clocks or ticks skipped by the game are never seen |
| B. Let the estimate also go down, jumping back | No | Inputs stamped twice for the same tick: the game keeps the first one, the prediction refuses the second |
| C. The game reports how early the inputs arrive; the client changes the pace of its inputs, jumps only forward | **Yes** | Closed loop on what matters; the stamps stay consecutive |
| D. Synchronize the clocks (NTP-like) | No | Gives the offset, not the jitter nor the ticks the game skips; more messages |

## Decision

**C: each `SNAPSHOT` carries the smallest lead of the client's `INPUT` messages since the previous snapshot,
and the client paces its inputs on it.**

- The **lead** of an `INPUT` is its tick minus the last tick simulated when it arrives, measured even when it
  is ignored. 1 is just in time; 0 or less is lost. A signed byte after "Last input" in `SNAPSHOT`, −128 when no
  `INPUT` came: the game protocol goes to version 2.
- The client keeps the smallest lead of the last 0.5 s between 2 and 3 by sending its inputs a little faster
  (at most 10%) or slower (at least half as fast) than once per tick. The stamps stay consecutive: no input
  lost, none twice, and the stamps too far ahead come back down.
- A lead of 0 or less jumps the next stamp forward at once, by what is missing to reach 2. The ticks skipped
  carry the previous input, as the game and the prediction do, and the leads of the next RTT + 2 ticks, those of
  the inputs sent before the jump, are ignored.
- The estimate of ADR 0016 still stamps the first inputs, until the first lead.

## Consequences

- The inputs arrive about 2 ticks before their tick, whatever the drift of the clocks, the RTT or a stall of the
  game: a constant input latency of RTT/2 + 2 ticks, instead of one that grows.
- The game says nothing more than one byte per part; it keeps no history per client. The jitter is absorbed by
  taking the smallest lead over half a second: the latest datagram sets the margin.
- Slowing down samples the controls a little less often for a moment (at worst 30 times per second, while far
  ahead), never lost.
- `client::InputClock` is pure, tested alone and against the `InputBuffer` of the server
  (`tests/client_input_clock.cpp`): clocks 1% fast or slow for 5 minutes, stamps 50 ticks ahead, a game that
  stalls 300 ms, a client that starts late, a network that jitters.
- An older client cannot play on a newer server, nor the reverse: the version of the datagrams differs.
