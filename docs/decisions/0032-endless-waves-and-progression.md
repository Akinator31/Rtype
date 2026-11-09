# 0032 — Endless waves online, and the experience of the accounts

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-11-08 |

## Context

Online, a game was the story of the client: wave n a wave of difficulty n, up to the one before the final boss,
then a victory. A game lasted five waves at most, and nothing was kept of it: the stats of the client stay on its
machine, and the accounts had nothing to show of the games they played.

The players asked for something to play for online: games that last as long as they survive, and a progression
kept by their account.

## Options

| | Kept? | Why |
|---|---|---|
| A. Online like offline, five waves then a victory | No | Short, and nothing to earn |
| B. Waves drawn at random without end, the experience computed by the instance and kept by the master | **Yes** | The instance knows what happened, the master has the accounts and the database |
| C. Waves without end, the experience computed by the master from the scores | No | The score does not tell the waves cleared, nor the streaks |
| D. The experience counted by the client, sent to the master | No | A client can lie |

## Decision

**B.**

- **The waves.** `Rules::endless`, set by the instances only: each wave is drawn at random among all of them,
  whatever their difficulty, from the seed of the rules, and they follow each other until every player is out
  of lives. No victory. The final boss, the waves of the last difficulty, stays offline with its rescue.
- **The experience.** A player clears a wave when its ship is alive as the wave ends; its streak counts the
  waves it cleared in a row. A wave cleared gives `xpPerWave`, a Bydo shot down its `xp`, both times the
  multiplier of the streak: 100% up to a streak of 1, then `streakBonus` more per wave, at most `streakCap`
  (`config/progression.lua`, `Progression`). The multipliers are integers in percent: every machine computes
  the same experience. The simulation counts it in `PlayerSlot`, and the snapshots carry it to the clients.
- **The accounts.** The instance reports what each player connected at the end did in `GAME_OVER`; the master
  adds it to the account of each player signed in (`account_progress`, migration 8) and tells its client
  (`PROGRESS`). A guest, or a player who left before the end, earns nothing.
- **The levels** are not kept: `levelOf()` finds them from the experience, `round(500 × n^1.5)` from level `n`
  to the next, the same function in the master and the client. The curve can change without a migration.

## Consequences

- The game protocol goes to version 3 (`SNAPSHOT` carries 14 more bytes per player), the instances to version
  5, the lobby to version 5: a client and a server of different versions do not play together, as before.
- A part of a snapshot holds 65 entities with four players instead of 69.
- An instance that crashes, or a database that cannot answer, loses the experience of the game: it is logged.
- The experience depends on the files of the server: a server with another `progression.lua` gives another
  experience to the same accounts.
