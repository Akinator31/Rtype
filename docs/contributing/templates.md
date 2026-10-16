# Page templates

Copy a template into a new file, fill it in, and [add the page to the navigation](documentation.md#adding-a-page).

## Architecture decision record

For `docs/decisions/NNNN-short-title.md`, with the next free number.

```markdown
# NNNN — Decision title

| | |
|---|---|
| **Status** | Proposed / Accepted / Superseded by NNNN |
| **Date** | YYYY-MM-DD |

## Context

What problem are we solving? Which constraints apply (subject requirements, platforms, team)?

## Options

| | Option A | Option B | Option C |
|---|---|---|---|
| Criterion 1 | | | |
| Criterion 2 | | | |

## Decision

What we chose and the main reasons.

## Consequences

What becomes easier or harder, what has to be done next, when to revisit this decision.
```

## How-to guide

For a task-oriented page: one goal, numbered steps.

```markdown
# How to <reach a goal>

What this guide achieves and who it is for.

## Prerequisites

- What must be installed or done first.

## Steps

1. First step.
2. Second step.

## Result

How to check that it worked.
```

## Protocol message

For a new message in the protocol specification: in [Lobby (TCP)](../protocol/lobby.md#5-messages),
[Game (UDP)](../protocol/game.md#8-messages) or [Master and instances](../protocol/instances.md#4-messages).
Take the next free type in the range of its direction, and add it to the right subsection. Changing an
existing message increases the [protocol version](../protocol/index.md#4-versioning).

```markdown
### MESSAGE_NAME (`0xNN`)

| | |
|---|---|
| **Direction** | Client → Game / Game → Client / Client → Master / ... |
| **Reliability** | Reliable / Unreliable (UDP messages only) |
| **Sent when** | Which event triggers it, and how often. |

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | | | |

**Receiver behaviour:** what the receiver does, and which values make the message invalid.
```
