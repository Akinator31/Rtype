# 0027 — One UDP port for every game, through a relay in the master

| | |
|---|---|
| **Status** | Accepted; amends the "Ports" of [0013](0013-one-process-per-game.md) |
| **Date** | 2026-11-06 |

## Context

[ADR 0013](0013-one-process-per-game.md) gave each game a UDP port of its own, from a range: 57 ports by
default, the same numbers inside and outside Docker, since the lobby gives the players the port of their game.
Hosting a server meant opening and publishing that whole range for a program that only needs one port, and
each port was one more firewall rule, one more line in the compose file, one more port forward on a home
router, and one more thing to get wrong ("the players reach the lobby, then no game").

Every datagram of a game already starts with a header in the clear that names its session
([Game protocol](../protocol/game.md), section 2), and the master draws the sessions: it can tell which game a
datagram is for without decrypting it.

## Options

| | Kept? | Why |
|---|---|---|
| A. One port per game, from a range (ADR 0013) | No | The range to open, publish and forward; the most games is the size of the range |
| B. `SO_REUSEPORT` with an eBPF program that steers by session | No | Linux only; the server also runs on Windows |
| C. Every game as a thread of the master, on its socket | No | Gives up the crash isolation of ADR 0013 |
| D. A Unix datagram socket between the master and its instances | No | Not on Windows, where AF_UNIX is stream only |
| E. A relay in the master on one UDP port, the instances behind it on the loopback | **Yes** | One port everywhere, the processes stay apart |
| F. As E, on a UDP number other than the lobby's | No | Two numbers to explain instead of one; TCP and UDP 4242 never conflict |

## Decision

**E: the master owns the only public UDP socket, on 4242 by default, the number of the lobby, and relays each
datagram to the instance of its session over the loopback.**

- Options `--game-port <port>` (default 4242; 0: a port of the system) and `--max-games <n>` (default 57), in
  the settings file `game_port` and `max_games`. `--game-ports`, `--game-relay` and `game_ports` are gone, and
  refused with a message that names their replacements.
- The relay runs on a thread of its own, with its own event loop: it reads the session in the clear header,
  drops what is malformed, banned, over the rate of its address or for an unknown session, and wraps the rest
  in a relay frame (the address of the client, then the datagram) to the instance. It checks that an instance
  only answers for its own sessions ([Server](../architecture/server.md#the-relay)).
- An instance started by a master gets no UDP port: `CONFIGURE` gives it the port of the relay on `127.0.0.1`,
  and it answers `READY` with the port it receives the frames on. The standalone mode keeps its own port,
  `--udp-port`.
- The encryption stays end to end, client to instance: the relay never holds the keys of what it forwards.
- `GAME_JOINED` still carries a UDP port, now the one of the relay for every game: the client does not change.

## Consequences

- A host opens **4242/tcp and 4242/udp**, and nothing else; Docker publishes `4242:4242/udp` once.
- The most games at once is a setting, `max_games`, no longer the size of a range: past it, `SERVER_FULL`.
- The master sees every datagram before a game does: unknown sessions, banned addresses and floods are dropped
  at the door.
- One more hop on the loopback, about 10 to 50 µs, against a tick of 16.7 ms; and one more thread in the master,
  which copies every datagram of every game.
- A master that stops must stop its relay after its instances, so that their last `DISCONNECT` goes out.
- Instances protocol 4 ([Instances](../protocol/instances.md)): the relay port in `CONFIGURE`, the loopback port
  in `READY`.
