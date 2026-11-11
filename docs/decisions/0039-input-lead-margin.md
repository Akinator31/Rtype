# 0039 — The lead the inputs aim at grows with the late ones

| | |
|---|---|
| **Status** | Accepted; amends the target lead of [0028](0028-input-stamps-follow-the-lead.md) |
| **Date** | 2026-11-11 |

## Context

The client keeps the smallest lead of its inputs of the last 0.5 s between 2 and 3 ticks
([ADR 0028](0028-input-stamps-follow-the-lead.md)): a tick of margin over just in time. On a distant server,
a datagram is held back now and then, longer than the others, more rarely than every 0.5 s: the window has
forgotten it by the next one, and that one is late again. The game applies the previous input at its tick; the
prediction applied the right one, and the next snapshot corrects it. The players saw small rollbacks of their
ship, more as the server was further.

## Options

| | Kept? | Why |
|---|---|---|
| A. A higher fixed target (4) | No | Two ticks of input latency for everybody, for the networks that do not need them |
| B. A longer window (3 s) | No | The pace follows a smallest lead that lags behind: it overshoots, then slows down, again and again |
| C. A margin above the target that grows at each late input and comes back down when it was not needed | **Yes** | Only the network that needs it pays it; the pace keeps its short window |

## Decision

**C.** Each time inputs are too late, after the first report, `InputClock` raises its target by a tick, up to
4 ticks above 2. After 15 s without inputs too late, it lowers it by a tick, unless a lead of those 15 s was 1:
a tick less would have lost that one. The jump of the late inputs goes to the new target.

## Consequences

- A network that hiccups every few seconds loses inputs a few times, then none: a datagram 70 ms late every
  2 s lost 57 inputs a minute with the fixed target, none with the margin (`tests/client_input_clock.cpp`).
- Each tick of margin is a tick of input latency, which only a network that needs it pays, and gives back once
  it is calm.
- Nothing changes in the protocol nor on the server.
