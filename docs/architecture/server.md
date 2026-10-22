# Server

`r-type_server` is one binary with two modes ([ADR 0013](../decisions/0013-one-process-per-game.md)): the
**master**, started by whoever hosts the server, runs the lobby; each **instance**, started by the master with
`--instance`, runs one game. This page describes how the program starts, what it is given, and how it stops.
The lobby and the game come in the next steps of the "Server & networking" work.

To run a server, see [Run a server](../guides/run-a-server.md).

## Code layout

```
apps/server/src/
  main.cpp                 reads the environment, then calls run()
  Server.cpp               run(): the options, then the master or an instance
  Common/                  what both modes share
    GameConfig.cpp         the configuration of the game: where it is, loaded and logged
    InstanceMessages.cpp   the messages between the master and its instances
    Options.cpp            the command line of both modes, the exit codes
    Lifecycle.cpp          runUntilStopped(): the event loop and the stop signals
    Secret.cpp             the secret the master gives to its instances
  Master/                  the master: the lobby, and the instances it starts
    Master.cpp             runMaster()
    Lobby.cpp              the lobby: the TLS connections of the clients and their requests
    Games.cpp              the games: one instance each, started, connected back, watched
    Process.cpp            Process: starts a program and stops it, on Linux and Windows
  Instance/                an instance: one game
    Instance.cpp           runInstance(): the life of the instance, with or without a master
    MasterLink.cpp         the connection to the master
    GameHost.cpp           the network side of the game: UDP, sessions, players, game messages
    GameThread.cpp         the simulation thread, at 60 ticks per second
```

Everything but `main.cpp` is the library `r-type_server_core`, so that the tests can call it
(`tests/server_options.cpp`, `tests/server_lifecycle.cpp`, `tests/process.cpp`, `tests/server_instance.cpp`,
`tests/server_lobby.cpp`, `tests/server_games.cpp`). Includes are written from `src/`, like in the client:
`#include "Common/Options.hpp"`. The next steps add their files to the folder of their mode: the lobby and the
supervision of the instances in `Master/`, the game in `Instance/`, what both sides read in `Common/`.

## Starting

```mermaid
flowchart TB
    Main["main()"] --> Parse["parseOptions()"]
    Parse -- "invalid" --> Exit2["exit 2"]
    Parse -- "--help, --version" --> Show["shows it, exit 0"]
    Parse -- "master" --> Master["runMaster()<br/>loads the key and the configuration, opens the port"]
    Parse -- "--instance" --> Instance["runInstance()<br/>loads the configuration, reads the secret"]
    Master -- "fails" --> Exit1["exit 1"]
    Instance -- "fails" --> Exit1
    Master --> Run["runUntilStopped()"]
    Instance --> Run
```

*The options are checked first; then each mode does everything that can fail before it starts running, so that
a server that cannot work stops at once with the reason.*

### Master

| Option | Default | Meaning |
|---|---|---|
| `-p`, `--port <port>` | 4242 | TCP port of the lobby; 0 lets the system choose |
| `--bind <ip>` | `0.0.0.0` | Address to listen on: every interface by default, `127.0.0.1` for this machine only |
| `--game-ports <first>-<last>` | 4243–4299 | UDP ports of the games, one per game: their number is the most games at once |
| `-k`, `--key <file>` | `server.key` | Private key of the server ([Generate the server keys](../guides/generate-the-server-keys.md)) |
| `--check` | | Loads the key and the game configuration and opens the port, then exits: checks a configuration without running it |

The master loads its key (`ServerIdentity::load()`), checks the configuration of the game and opens its port
before anything else, and logs them:

```
INFO  master   Game configuration /opt/r-type/config: 2 projectiles, 2 weapons, 1 ships, 2 enemies, 2 waves
INFO  master   Lobby on TCP 0.0.0.0:4242, games on UDP 4243-4299 (57 at most)
INFO  master   Server key 4026de0635ae89391bff98b7b9ddc0689fa8529f9804e4c4721eadce94d92dff
INFO  master   Master started: Ctrl+C to stop
```

The fingerprint is the one the clients pin: compare it with `r-type_keygen --fingerprint server.pub` when
something looks wrong.

### Instance

The master starts each instance with these arguments ([instances protocol](../protocol/instances.md#1-starting-an-instance)),
never by hand:

| Option | Meaning |
|---|---|
| `--instance` | The instance mode |
| `--id <id>` | Id of the game, from 1 |
| `--udp-port <port>` | UDP port of the game |
| `--master-port <port>` | Port where the master listens for its instances, on `127.0.0.1` |
| `--config <dir>` | The configuration the master checked, as an absolute path |

It then writes the **secret** on the instance's standard input: 64 hexadecimal digits and a line feed. The
instance reads it before anything else (`readSecret()`), and stops with code 1 without it. The secret proves to
the master that the process connecting is the one it started; it is never an argument, since every user of the
machine can read the arguments of a process (`ps`).

**Standalone mode.** `r-type_server --instance --standalone --udp-port 4242` runs a game without a master, with
public test keys, for development, the tests, the measures and the fuzzing. It only exists in **debug builds**
(`RTYPE_STANDALONE_INSTANCE`): a released server refuses it. See [below](#standalone-mode).

### Common options

| Option | Meaning |
|---|---|
| `--config <dir>` | The configuration of the game, the Lua files of the enemies and weapons ([ADR 0018](../decisions/0018-lua-game-config.md), [guide](../guides/configure-enemies-and-weapons.md)); `config/` next to the executable by default |
| `--log-level <level>` | `trace`, `debug`, `info`, `warning` or `error`; wins over `RTYPE_LOG_LEVEL` ([Logging](logging.md)) |
| `-h`, `--help` | Shows the usage |
| `--version` | Shows the version, the one of the project |

**The configuration of the game** is read by `readGameConfig()` (`Common/GameConfig.hpp`), with
`loadGameConfig()` of `libs/game`. The master reads it only to check it, the instance reads it again and gives
it to the `GameThread`, which makes its `Simulation` with it. With an error, neither starts (exit 1), as
ADR 0018 says, and each error is logged with its file and its field:

```
ERROR master   Configuration: enemies/bydo_scout.lua: health: expected an integer from 1 to 255
ERROR master   The game configuration in /opt/r-type/config is not valid: fix it, or give another one with --config
```

The options of one mode are refused in the other (`--key` with `--instance`, `--id` without it), as are unknown
options: a typo never goes unnoticed.

## The lobby

The master accepts the clients on its TCP port, over TLS with its key, and answers their requests
([lobby protocol](../protocol/lobby.md)). `Lobby` (`Master/Lobby.hpp`) runs on the thread of the loop; the
messages are those of `libs/lobby` (`rtype/lobby/Messages.hpp`), which the client uses too.

```mermaid
stateDiagram-v2
    [*] --> Handshake: TCP accepted, within the limits
    Handshake --> Unwelcomed: TLS secured
    Unwelcomed --> Lobby: HELLO / WELCOME
    Lobby --> InGame: CREATE_GAME or JOIN_GAME / GAME_JOINED
    InGame --> Lobby: LEAVE_GAME / LEFT_GAME, or LEFT_GAME (game over, crash, timeout)
    Handshake --> [*]: 10 s
    Unwelcomed --> [*]: no HELLO in 10 s
    Lobby --> [*]: closed, or 30 s without a frame
    InGame --> [*]: closed, or 30 s without a frame
```

*Each connection is timed in each state; the client keeps it open with PING.*

| Request | Before `HELLO` | In the lobby | In a game |
|---|---|---|---|
| `HELLO` | `WELCOME` with a new player id; `INVALID_NAME`; `UNSUPPORTED_VERSION`, then closed | `ALREADY_WELCOMED` | `ALREADY_WELCOMED` |
| `PING` | `PONG` | `PONG` | `PONG` |
| `LIST_GAMES` | `HELLO_REQUIRED` | `GAME_LIST` | `GAME_LIST` |
| `CREATE_GAME` | `HELLO_REQUIRED` | `GAME_JOINED` once the game is ready; `INVALID_NAME`, `SERVER_FULL`, `GAME_START_FAILED` | `ALREADY_IN_GAME` |
| `JOIN_GAME` | `HELLO_REQUIRED` | `GAME_JOINED`; `GAME_NOT_FOUND`, `GAME_FULL` | `ALREADY_IN_GAME` |
| `LEAVE_GAME` | `HELLO_REQUIRED` | `NOT_IN_GAME` | `LEFT_GAME (REQUESTED)` |
| Any other type | `UNKNOWN_MESSAGE` | `UNKNOWN_MESSAGE` | `UNKNOWN_MESSAGE` |
| A malformed payload | `MALFORMED`, then closed | `MALFORMED`, then closed | `MALFORMED`, then closed |

- **One reply per request, in order**: the requests of a connection are handled one after the other, so a
  client can send several without waiting. `CREATE_GAME` is answered once its game is ready, up to 10 seconds
  later: the requests sent meanwhile wait for its answer (32 at most, then the client is disconnected).
- **The state comes first**: a second `HELLO` is `ALREADY_WELCOMED` whatever it holds, a `LIST_GAMES` before
  `HELLO` is `HELLO_REQUIRED` whatever its payload.
- **A player id** is given by `HELLO`: never 0, never given twice by a master. Names do not have to be unique.
- **Leaving without asking**: a game over, a crash or a client that timed out send `LEFT_GAME` with their
  reason, and the player is back in the lobby. A client that closed its UDP session itself (`QUIT`) is back in
  the lobby without a message: it knows. A lobby connection that closes takes its player out of its game.

**Limits** (`LobbyLimits`, the defaults of the [protocol](../protocol/index.md)):

| Limit | Default | When reached |
|---|---|---|
| Connections | 256 | The next ones are closed at once, before any TLS work |
| Connections per IP address | 8 | Same |
| TLS handshake | 10 s | Closed |
| `HELLO` after the handshake | 10 s | Closed: `PING` does not count |
| Silence of a welcomed client | 30 s | Closed: any frame counts, `PING` is sent after 10 s of silence |

On Ctrl+C or `SIGTERM`, the lobby stops accepting and closes every connection with a TLS `close_notify`, then
the loop stops 100 ms later. The logs show the players:

```
INFO  lobby    Pavel (1) joined the lobby from 192.168.1.20:51234
INFO  lobby    Pavel (1) left the lobby
```

## The games

`Games` (`Master/Games.hpp`) runs the games of the master, one instance each
([ADR 0013](../decisions/0013-one-process-per-game.md), [instances protocol](../protocol/instances.md)).

```mermaid
sequenceDiagram
    participant C as Client
    participant M as Master
    participant I as Instance
    C->>M: CREATE_GAME (name, max players)
    Note over M: takes a free UDP port, draws a secret
    M->>I: starts r-type_server --instance, secret on its standard input
    I->>M: INSTANCE_HELLO (id, secret), on 127.0.0.1
    M->>I: CONFIGURE
    I->>M: READY (UDP port)
    M->>I: ADD_PLAYER (player, session, keys)
    M->>C: GAME_JOINED (game, UDP port, session, keys)
    C->>I: CONNECT, then the game
```

*The creator is answered once the instance is ready; a player who joins later is added at once.*

- **Its own executable**: the master starts `currentExecutable()` with `--instance --id --udp-port
  --master-port`, and the `--log-level` it runs with: the instances log like it.
- **The instances connect back** to a listener of the master on `127.0.0.1`, on a port of the system. An
  instance has 10 seconds to present itself; an unknown id closes its connection, a wrong secret (compared in
  constant time, `sameSecret()`) also kills the instance it pretends to be.
- **A session per player**: an id unique in its game, and two keys drawn by the master, given to the instance
  (`ADD_PLAYER`) before the player (`GAME_JOINED`), so that the game knows the session when the client
  connects.
- **One UDP port per game**, from `--game-ports`: none free is `SERVER_FULL`. A port is given back once the
  process has exited.

| What happens | The players | The instance |
|---|---|---|
| The last player leaves, or its lobby connection closes | — | `SHUTDOWN (EMPTY)` |
| No `INSTANCE_HELLO` or no `READY` within 10 s, or the program cannot start | `ERROR (GAME_START_FAILED)` | Killed |
| `GAME_OVER` | `LEFT_GAME (GAME_OVER)` | Exits on its own |
| It exits, or closes its connection, without `GAME_OVER` | `LEFT_GAME (GAME_CRASHED)` | Killed if still there |
| `PLAYER_DISCONNECTED (TIMED_OUT)` | `LEFT_GAME (TIMED_OUT)` | — |
| The master stops | Their UDP sessions get `DISCONNECT (SHUTDOWN)` | `SHUTDOWN (MASTER_STOPPING)` |

The processes are checked every 50 ms (`exitCode()` never blocks). An instance that does not exit within 5
seconds of `SHUTDOWN` or `GAME_OVER` is killed. On Ctrl+C, the master closes its lobby, shuts every game down,
and stops once every instance has exited:

```
INFO  games    Pavel creates game 1 "Bydo hunters" for 4 players: instance 48213 on UDP 4243
INFO  games    game 1 "Bydo hunters" ready on UDP 4243
INFO  games    Pavel (1) joins game 1 "Bydo hunters" (1/4)
```

## An instance at work

### With its master

```mermaid
sequenceDiagram
    participant M as Master
    participant I as Instance
    participant C as Client
    Note over I: reads the secret
    I->>M: INSTANCE_HELLO (id, secret), on 127.0.0.1
    M->>I: CONFIGURE (name, max players)
    Note over I: opens its UDP port, starts the game thread
    I->>M: READY (UDP port)
    M->>I: ADD_PLAYER (player, session, keys, name)
    C->>I: CONNECT
    I->>C: ACCEPTED (slot, 60, tick), PLAYER_JOINED
    I->>M: PLAYER_CONNECTED
    loop every tick
        C->>I: INPUT
        I->>C: SNAPSHOT (every 2 ticks), ENTITIES_DESTROYED
    end
    M->>I: SHUTDOWN
    I->>C: DISCONNECT (SHUTDOWN)
    Note over I: exits with 0
```

*The instance presents itself, is configured, opens its port, then runs the game for the players the master
gives it.*

`MasterLink` holds the connection to the master ([instances protocol](../protocol/instances.md)). The master and
its instances come from the same build, so anything outside the protocol is a failure: no `CONFIGURE` within 10
seconds, a player before `CONFIGURE`, `CONFIGURE` twice, a frame that does not decode. A failure, like the
master closing the connection, stops the game with exit code 1: an instance never runs without its master.

### Two threads

```mermaid
flowchart LR
    subgraph Net["network thread (main): the event loop"]
        Link["MasterLink<br/>TCP to the master"]
        Host["GameHost<br/>UDP, SessionHost, players"]
    end
    subgraph Sim["game thread"]
        Game["GameThread<br/>Simulation, 60 ticks/s"]
    end
    Link --> Host
    Host -- "commands: add, remove, input" --> Game
    Game -- "loop.post(): each tick, the snapshot" --> Host
```

*They share nothing but the commands, under a mutex, and the results, handed through `loop.post()`
([ADR 0013](../decisions/0013-one-process-per-game.md)).*

- **`GameThread`** owns the `Simulation` of `libs/game` and steps it every 1/60 s, whatever the network does.
  Behind by more than 250 ms (a machine that slept, a debugger), it goes on from now instead of running fast.
  The commands of the network thread apply at the start of the next tick. The inputs of each player wait in an
  `InputBuffer` for the tick they are stamped with ([below](#inputs)), and the result of each tick goes back with
  the tick of the last input applied for each player.
- **`GameHost`** is the game seen from the network ([game protocol](../protocol/game.md)):
  - a player added gets the lowest free slot, and its ship when its client connects;
  - on `CONNECT`, the client receives `ACCEPTED`, then `PLAYER_JOINED` for each player connected, itself
    included; the others receive its `PLAYER_JOINED`;
  - the inputs of each `INPUT` go to the game, which applies each one at its tick;
  - every 2 ticks, each client receives the snapshot in parts, with its last input; every tick, the entities
    destroyed. They leave as soon as the tick is published, not at the next update of the sessions: up to a
    tick of delay less, and no jitter from it;
  - the game over goes to every client (`GAME_OVER`, with the last snapshot), then to the instance with the
    score of each player; nothing is published after it;
  - a session that closes takes the ship away; the others receive `PLAYER_LEFT`, and the master
    `PLAYER_DISCONNECTED`, unless it removed the player itself (`REMOVE_PLAYER`).
- Its `SessionHost` is updated at 60 Hz by a `Timer` of the loop, not by the game thread: the heartbeats and
  resends go on even if the game stalls.

### Inputs

A client sends, once per tick, an `INPUT` with its last 8 inputs, each stamped with the tick at which the game
should apply it ([game protocol, 9.2](../protocol/game.md#92-inputs)). The game thread keeps them in one
`InputBuffer` per player:

| What happens | What the game does |
|---|---|
| An input for a tick to come, at most 60 ahead | Kept until its tick, then applied |
| An input for a tick already simulated, or more than 60 ahead | Ignored |
| The same tick received again (the next `INPUT` carries it too) | The first one is kept |
| A tick without its input: its datagram was lost or late | The last input is applied again: the movement goes on |
| 30 ticks without any input | An empty input: the ship of a player who lost the connection stops |

The 8 inputs of each `INPUT` are what makes a lost datagram harmless: the next one brings the missing ticks again.
The client stamps its inputs a little ahead of the game, from the round-trip time it measures
([The client online](client-online.md)), so that they arrive before their tick.

### Stopping an instance

| What happens | The clients | Exit code |
|---|---|---|
| The game is over | `GAME_OVER`, then `DISCONNECT (GAME_OVER)` once acknowledged, 2 s at most; the master gets the scores | 0 |
| `SHUTDOWN` from the master | `DISCONNECT (SHUTDOWN)` | 0 |
| Ctrl+C, `SIGTERM` (or `Process::stop()` from the master) | `DISCONNECT (SHUTDOWN)` | 0 |
| The master closes the connection, or breaks the protocol | `DISCONNECT (SHUTDOWN)` | 1 |
| The UDP port cannot be opened | — | 1 |
| A player added to a full game, or twice | `DISCONNECT (SHUTDOWN)` | 1 |
| The simulation throws | `DISCONNECT (SHUTDOWN)` | 1 |

In each case the game thread stops first, then the sessions and the link close, and the loop stops 100 ms later,
once the last datagrams are sent.

### Standalone mode

Without a master, nobody gives the clients a session and its keys: in standalone mode they are fixed, in
`rtype/game/Standalone.hpp`, so that a client in development can join.

| Slot | Session | Client key | Server key |
|---|---|---|---|
| 0 to 3 | `standalone::sessionId(slot)`: 1 to 4 | `standalone::clientKey(slot)`: every byte `0x10 + slot` | `standalone::serverKey(slot)`: every byte `0x20 + slot` |

The four sessions wait for their clients; a session that closes, or never connects within 10 seconds, is given
again, so that a client can always join. These keys are public: a released server refuses the standalone mode.

```sh
./build/bin/r-type_server --instance --standalone --udp-port 4242 --log-level debug
```

## Stopping

Both modes end in `runUntilStopped(loop, shutdown)`, which runs the event loop on the main thread and handles
the **stop signals**: Ctrl+C (`SIGINT`), `SIGTERM` (sent by `kill`, systemd or `docker stop`), and on Windows
Ctrl+Break and the closing of the console.

```mermaid
stateDiagram-v2
    [*] --> Running
    Running --> Stopping: first signal: shutdown()
    Stopping --> Stopped: shutdown() is done, loop.stop()
    Stopping --> Forced: second signal, or 10 s
    Running --> Stopped: the work ends (game over)
    Stopped --> [*]: exit 0
    Forced --> [*]: exit 1
```

*The first signal starts a proper stop; a second one, or the grace period, cuts it.*

1. **The first signal** calls `shutdown`, which ends the work properly and calls `loop.stop()` once done. An
   instance sends `DISCONNECT (SHUTDOWN)` to its players. The master closes the lobby, which disconnects its
   clients; with the games, it will also send `SHUTDOWN` to its instances and wait for them.
2. **A second signal** stops at once: the person who pressed Ctrl+C twice does not want to wait.
3. **After 10 seconds** (`GracePeriod`) without the loop stopped, the server stops at once too: a stop that
   hangs never blocks a restart.

```
INFO  server   SIGINT received: stopping (again to stop at once)
INFO  master   Master stopped
```

Under the hood, `StopSignals` (`libs/network`) receives the signals on the loop's thread, and a `Timer` counts
the grace period ([Networking](networking.md#timers-and-stop-signals)). A signal handler of the C library could
only set a flag: here the shutdown is ordinary code, which may close sockets and log.

## Processes

The master starts its instances with `Process` (`Master/Process.hpp`), which wraps
[reproc](https://github.com/DaanDeMeyer/reproc) ([ADR 0013](../decisions/0013-one-process-per-game.md)). It is
the only part of the server that knows the processes of the system, on Linux and Windows.

```cpp
const std::vector<std::string> arguments = {"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000"};
rtype::server::Process instance(rtype::server::currentExecutable(), arguments,
                                {.input = formatSecret(secret) + "\n", .stopGrace = std::chrono::seconds(5)});

instance.exitCode(); // its exit code once it has exited, nothing while it runs: never waits
instance.stop();     // asks it to stop, waits stopGrace, kills it if it still runs
```

- **Its own executable.** `currentExecutable()` asks the system for the path of the program running
  (`/proc/self/exe` on Linux, `GetModuleFileNameW()` on Windows): `argv[0]` can be relative or a name found in
  the `PATH`, and the instance must be the same build as its master.
- **The arguments arrive unchanged**: there is no shell, so a space, a quote or a `;` is just a character.
  reproc does the quoting of the command line of Windows, where a backslash before a quote is a trap. An
  **empty argument is refused** (`std::invalid_argument`): reproc does not quote it, so Windows would lose it
  and shift the others.
- **The secret goes through the standard input**, written before the program starts (`input`), which is then
  closed. Nothing can be written later: writing to an instance that has already died would raise `SIGPIPE`
  and stop the master.
- **Its output is the master's**: the logs of the instances go to the same terminal, or the same journal.
- **Stopping** comes in two steps. `terminate()` asks: `SIGTERM` on Linux, Ctrl+Break on Windows, which
  `runUntilStopped()` of the instance receives like Ctrl+C, so that it can tell its players. `kill()` stops at
  once. `stop()` does both: it asks, waits `stopGrace`, then kills. On Windows, Ctrl+Break needs a console:
  without one, `stop()` kills at once. A program that has only just started does not receive it yet either:
  `stop()` then kills it after `stopGrace`.
- **No program left behind**: destroying a `Process` that still runs calls `stop()`, even when an exception
  goes through the master.

| Exit code | Meaning |
|---|---|
| `0`, `1`, `2` | The [exit codes](#exit-codes) of `r-type_server` |
| `Process::Terminated` (143) | Stopped by `terminate()` without handling it: 128 + `SIGTERM`, as a shell shows it |
| `Process::Killed` (137) | Stopped by `kill()`: 128 + `SIGKILL`, the same value on Windows |

`exitCode()` never blocks, so the master can check its instances from a `Timer` of its event loop; `wait()` and
`stop()` block, for the tests and the end of the master. The supervision of the instances itself comes with the
lobby.

## Exit codes

| Code | When |
|---|---|
| `0` | Stopped properly: after a stop signal, after `--help`, `--version` or `--check`, or at the end of its game for an instance |
| `1` | Could not start (key missing or invalid, port in use, no secret), stopped on an unexpected error, or stop forced |
| `2` | Invalid arguments |

The master reads the exit code of its instances: anything but 0 is a crash, which it reports to the players
([instances protocol](../protocol/instances.md#3-life-and-death-of-an-instance)).

## Tests

- `tests/server_options.cpp` (tag `[server]`): the defaults, every option of both modes, every invalid
  combination, `--help` and `--version` first, the standalone mode refused in release, the exit codes of
  `run()`, a master without its key, an instance without a secret, and the secret itself.
- `tests/server_lifecycle.cpp` (tag `[server]`): `SIGINT` and `SIGTERM`, sent to the test process itself, call
  the shutdown once; a second signal and the grace period force the stop; a server whose work ends stops
  properly.
- `tests/process.cpp` (tag `[process]`): real programs started and stopped. A helper,
  `tests/helpers/process_child.cpp`, does what its arguments say: exits with a code, checks its standard input
  and its end, writes its arguments to a file, waits, or ignores the requests to stop. Before a request to stop,
  the tests wait for the file it creates once ready, since Windows drops a Ctrl+Break sent too early. The tests check the
  exit codes, arguments with spaces, quotes and backslashes, an empty argument refused, the input then its end, a program that does not
  exist, `terminate()`, `kill()`, `stop()` killing a program that ignores it, the destructor and the moves.
  They also start `r-type_server --instance` itself: with its secret and a master that never answers, it runs
  until asked to stop, then exits with 0; without it, it exits with 1.
- `tests/server_replication.cpp` (tag `[replication]`): the `InputBuffer`: an input applied at its tick, the
  last one repeated then emptied after a silence, the ticks of a lost datagram filled by the next one, inputs
  too late, too far ahead or received twice ignored.
- `tests/server_instance.cpp` (tag `[instance]`): the messages between the master and its instances (round
  trips, bytes, every malformed case, random bytes), the game thread (its rate, each input applied at its
  tick, whatever their order, nothing after `stop()`), the game over of `GameHost` (sent, reported with the
  scores, acknowledged, nothing after), every failure of `MasterLink`, and whole instances run by `runInstance()` on
  a thread of the test, with a fake master over TCP and real clients over UDP: two players who join, see each
  other and move, one removed by the master, one who quits, then `SHUTDOWN`; the master going away; a full
  game; a UDP port taken; and the standalone mode with its public keys, stopped by Ctrl+C.
- `tests/lobby_messages.cpp` (tag `[lobby]`): the `HELLO` frame of the specification, the bytes of the other
  messages, round trips, the names checked apart, every malformed case, and random bytes.
- `tests/server_lobby.cpp` (tag `[lobby]`): a `Lobby` on the loopback and real TLS clients pinning its key.
  `HELLO`, `LIST_GAMES` and `PING`; requests sent without waiting answered in order; the answers of each
  state; invalid names, another version, malformed requests; the limits per address and in all, a place
  freed taken again; the handshake, `HELLO` and silence timeouts, `PING` keeping a client; and closing the
  lobby.
- `tests/server_games.cpp` (tag `[games]`): real instances of `r-type_server` started by `Games`, real lobby
  clients over TLS and game clients over UDP. A game created, listed, joined, full, left, and shut down with
  its last player; the requests after `CREATE_GAME` answered in order; `SERVER_FULL`; an instance that cannot
  start, exits at once or never gets ready, and its port given back; an impostor with an unknown id or a wrong
  secret; a crash reported as `GAME_CRASHED`; a silent client reported as `TIMED_OUT`; the master stopping its
  games; and the real binary run as a master, which starts the instance of a game created by a client.
- The smoke tests of CTest run `r-type_server --help`, refuse an unknown argument, and run `--check` with a key
  made by `r-type_keygen` for the occasion.

## Limits and next steps

- **The limits of the lobby are not options yet**: `LobbyLimits` has the defaults of the protocol; options can
  come when a host needs others.
- **The waves are sent, not shown**: the instance sends `WAVE_STARTED` at the start of each wave, but the client
  does not decode it yet (no HUD).
- **The client draws the latest snapshot**: drawing in the past between two snapshots (interpolation) and
  predicting the ship of the player come on the side of the client, and make the game online as smooth as
  offline.
- **No configuration file**: everything is on the command line, which suits a service (systemd, Docker). A file
  can come if the options multiply.
