# 0032 — A voice chat of our own in the game session, and Discord beside it in the same game

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-11-08 |

## Context

The players of an online game have no way to talk to each other but outside the game. A voice chat needs a
microphone captured and encoded, the voice carried with little delay and tolerant of losses, a buffer that
evens out the network, and playback; without a headset, the echo of the speakers cancelled.

What the game already has:

- **A transport fit for voice**: the UDP session of a game ([Game protocol](../protocol/game.md)), with
  unreliable messages, datagrams of at most 1200 bytes, ChaCha20-Poly1305, a replay window, and the relay of the
  master that forwards by session without decrypting ([ADR 0027](0027-one-udp-port-relay.md)).
- **A microphone without a new dependency**: raylib 6.0 compiles miniaudio 0.11.24 into itself, capture
  included (`MA_NO_DEVICE_IO` and `MA_NO_CAPTURE` are not defined in `raudio.c`).
- **The Discord Social SDK** in the client ([ADR 0026](0026-discord-social-sdk.md)), whose lobbies have voice
  calls; but they need a Discord token in the client (not given today), only players with Discord have it, and
  Discord limits the communication features of an application until it approves it for production.

The players with Discord will want its voice, and the others must not be left out: **a game where two
players are on Discord and two are not must let the four hear each other**.

## Options

| | Kept? | Why |
|---|---|---|
| A. Ours: Opus in the game session, forwarded by the game without decoding | **Yes** | Every player, guests included; no external service; the sound of each player apart, for the spatial voice; moderated by the server that runs the game |
| B. The voice of the Discord Social SDK only | No | Only the players with Discord; needs a token in the client and the approval of Discord first; a kicked player keeps the voice until the master manages the lobbies |
| C. A middleware (Vivox, Agora) | No | One more proprietary SDK and an account per host, for a transport the game has |
| D. Mumble or another program beside the game | No | Another program to install and run; no link with the game, its players or its moderation |
| E. WebRTC (libwebrtc, or libdatachannel) | No | Its own transport, encryption and signalling, for what the game session does; libwebrtc is a heavy build |
| F. A and B, one backend per game | No | A game with players on both is split in two: the case asked for |
| G. A and B, **one backend per player**, in the same game | **Yes** | The four hear each other; the players on Discord keep it between them |

## Decision

**A, then G: the voice of the game, and Discord as a second backend, chosen per player in the same game.**

1. **One interface for the client**, `rtype::voice::VoiceChannel` (`libs/voice`): join and leave a game, the
   other players by their slot, mute, talk, volume, mute for me, position. The client, its HUD and its settings
   know it only, whatever carries each voice.
2. **Each player has a backend**, `rtype::voice::Backend`: `None`, `Game` or `Discord`. The client tells its own
   to the game (`VOICE_STATE`), the game tells every client the backend of each player (`VOICE_ROSTER`).
3. **The game carries a voice unless both players are on Discord** (`carriedByGame()`): Discord already
   carries it between them, and the game would double it. A player on `None` neither talks nor hears.
4. **Opus**, 48 kHz mono, frames of 20 ms, about 24 kbit/s, in-band FEC: unreliable messages, never resent.
   **The game never decodes**: it checks sizes and rates, and forwards.
5. **A player on Discord bridges its own voice** to the game for the players on `Game`: the microphone opened
   once, by Discord, its samples taken before Discord processes them, then processed and encoded by us. The
   voices received from Discord are played by the same mixer as those of the game.

The wire format of the messages is
written in the [Game protocol](../protocol/game.md) by the step that adds them.

## Consequences

- Every player of an online game can talk, whatever its account, and a game can mix the players on Discord and
  on the voice of the game.
- The client takes a new dependency, libopus (BSD), and later speexdsp and RNNoise (BSD) for the echo and the
  noise. The server takes none: it only forwards.
- The voice is never stored nor logged, by the game or the client; a replay has no voice. The game sees the
  frames it forwards (the encryption is client to game, not player to player), the relay does not.
- At most 50 frames per second per talking player, carried in the datagrams of the inputs when they fit: the
  rate limit of the relay (200 datagrams per second) is not approached. About 90 kbit/s down per client when
  the three others talk.
- A player muted by an administrator is silenced by the game for those on `Game`, and by the clients of those on
  Discord until the master manages the Discord lobbies.
- The voice of Discord and the voice of the game have different delays: accepted.
