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
  Accounts/                the accounts of the players (Accounts page)
    AccountRules.cpp       what a username, an email and a password must be
    AccountStore.cpp       their tables: accounts, account_sessions, account_codes, discord_identities
    PasswordHasher.cpp     Argon2id, on two threads of its own
    MailMessages.cpp       what the mails say, in English and French
    Mailer.cpp             the interface of the mailer, and a mail written for SMTP
    CurlMailer.cpp         the mail server, with libcurl, on a thread of its own; its check at start
    Discord.cpp            who a player is for Discord, with libcurl, on a thread of its own; its check at start
    AccountService.cpp     the requests of the accounts in the lobby, on the loop
    PanelAccounts.cpp      the administrators of the panel: their sign-in, their sessions, on the loop
  Common/                  what both modes share
    GameConfig.cpp         the configuration of the game: where it is, loaded and logged
    InstanceMessages.cpp   the messages between the master and its instances
    Options.cpp            the command line of both modes, the exit codes
    RelayFrame.cpp         the relay frame: a datagram of the game with the address of its client
    Lifecycle.cpp          runUntilStopped(): the event loop and the stop signals
    Secret.cpp             the secret the master gives to its instances
    Settings.cpp           the settings file of the master (--settings) and its secret files
  Master/                  the master: the lobby, and the instances it starts
    Master.cpp             runMaster()
    Lobby.cpp              the lobby: the TLS connections of the clients and their requests
    Games.cpp              the games: one instance each, started, connected back, watched
    Relay.cpp              the relay: every game on one UDP port, on a thread of its own
    Process.cpp            Process: starts a program and stops it, on Linux and Windows
    ServerKey.cpp          the key of the master: loaded, or generated at its first start
    ServerDatabase.cpp     the database of the master: its address, its migrations
    Administration.cpp     what an administrator sees and does (Administration page)
    AdminModel.hpp         its view of the games and the lobby, and the interfaces it acts through
    AddressBans.cpp        the bans of IP addresses, checked by the lobby at HELLO
    AdminStore.cpp         its tables: admin_actions, address_bans
  Web/                     the web server of the master, for its panel (Web server page)
    WebServer.cpp          Crow on two threads: the files of the panel, the sessions, the API, its WebSocket
    AdminJson.cpp          the JSON of the administration API
    Health.cpp             --health: asks /api/health of the master running on this machine
    LocalAccess.cpp        the local access token and admin-url.txt
    Visitor.cpp            the address of a visitor behind the trusted proxies
  Instance/                an instance: one game
    Instance.cpp           runInstance(): the life of the instance, with or without a master
    MasterLink.cpp         the connection to the master
    GameHost.cpp           the network side of the game: sessions, players, game messages
    GameTransport.hpp      how the datagrams of the game come and go, for GameHost
    DirectTransport.cpp    the transport of a game with a UDP port of its own
    RelayedTransport.cpp   the transport of a game behind the relay of the master, on the loopback
    GameThread.cpp         the simulation thread, at 60 ticks per second
```

Everything but `main.cpp` is the library `r-type_server_core`, so that the tests can call it
(`tests/server_options.cpp`, `tests/server_key.cpp`, `tests/server_database.cpp`, `tests/server_lifecycle.cpp`,
`tests/process.cpp`, `tests/server_instance.cpp`, `tests/server_lobby.cpp`, `tests/server_games.cpp`,
`tests/server_settings.cpp`, `tests/server_web.cpp`, `tests/server_admin.cpp`,
`tests/server_account_store.cpp`, `tests/relay_frame.cpp`, `tests/server_relay.cpp`). Includes are written from `src/`, like in the client:
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
| `--game-port <port>` | 4242 | UDP port of every game, served by the [relay](#the-relay) on the address of `--bind`; 0 lets the system choose. The same number as `--port` by default: one is TCP, the other UDP ([ADR 0027](../decisions/0027-one-udp-port-relay.md)) |
| `--max-games <n>` | 57 | Most games at once, from 1 to 1000: past it, creating one is `SERVER_FULL` |
| `-k`, `--key <file>` | `server.key` | Private key of the server, generated there if the file does not exist ([Manage the server key](../guides/generate-the-server-keys.md)) |
| `--fingerprint` | | Shows the fingerprint of the key, for the players to check, then exits. Never generates a key |
| `--database <address>` | `sqlite:rtype.db` | Where the master keeps its data: `sqlite:<file>`, or `postgresql://<user>@<host>:<port>/<database>` ([The database](#the-database)) |
| `--database-file <file>` | | Reads the address of the database from the first line of the file: for an address with a password, which the command line would show to every user of the machine |
| `--settings <file>` | | Reads the settings file of the master ([Configure a server](../guides/configure-a-server.md)): the options above in `[server]`, and the sections of the panel and the accounts. The command line wins over the file |
| `--check` | | Reads the settings file, loads the key, or generates it, the game configuration and the database, which it creates or brings up to date, opens the port of the lobby, the UDP port of the games and the one of the panel, and with accounts signs in to the mail server without sending anything, and with `[discord]` asks Discord a token of its application, then exits: checks a configuration without running it |
| `--health` | | Asks `/api/health` of the master running on this machine, at the `listen` address of `[web]` in the settings file (the loopback for `0.0.0.0`), shows `healthy` and exits with 0 when it answers within 5 seconds, else exits with 1. The health check of the [Docker image](../guides/deploy-with-docker.md), which has no curl |

The master loads its key, or generates it at its first start (`loadOrCreateKey()`, in `Master/ServerKey.hpp`,
[ADR 0021](../decisions/0021-server-generated-key-trust-on-first-use.md)), checks the configuration of the game,
opens its database, its ports (the lobby in TCP, the games in UDP) and the one of its panel ([Web server](web.md)), and with accounts checks its mail
server ([Accounts](accounts.md#at-start)), before anything else, and logs them. With accounts, it then logs
`Accounts on, guests allowed` (or `no guests`) before it starts, and gives the role `admin` to the accounts of
`[admin] admins`, warning of those that have none yet ([Web server](web.md#accounts)):

```
INFO  master   Game configuration /opt/r-type/config: 2 projectiles, 2 weapons, 1 ships, 2 enemies, 2 waves
INFO  master   Database sqlite:rtype.db, schema 0
INFO  master   Lobby on TCP 0.0.0.0:4242, games on UDP 0.0.0.0:4242 (57 at most)
INFO  master   Panel on HTTP 127.0.0.1:8080
INFO  master   The panel is not built: no index.html in /opt/r-type/admin
INFO  master   Server key 4026de0635ae89391bff98b7b9ddc0689fa8529f9804e4c4721eadce94d92dff
INFO  master   Panel at http://127.0.0.1:8080: open the address written in admin-url.txt
INFO  master   Master started: Ctrl+C to stop
```

A UDP port taken stops the master at start, `--check` included, as a TCP port taken does: `Cannot open UDP
0.0.0.0:4242 for the games: Address already in use`.

The options `--game-ports` and `--game-relay`, and the key `game_ports`, of the time when each game had a port of
its own, are refused with a message that names `--game-port` and `--max-games`.

`admin-url.txt`, beside the key, holds the address that opens the panel with the local access token, new at
each start, readable by its owner only ([Local access](web.md#local-access)).

The fingerprint is the one the players trust at their first connection. The key file:

- **does not exist**: a key is generated, written there only readable by its owner (`O_EXCL`, mode `0600` on
  Unix, the directories created if missing), with a warning, since it should only happen once per server;
- **exists**: it is loaded. If it cannot be read or holds no key, the master stops: it never replaces a key,
  which would make every player refuse the server.

### Instance

The master starts each instance with these arguments ([instances protocol](../protocol/instances.md#1-starting-an-instance)),
never by hand:

| Option | Meaning |
|---|---|
| `--instance` | The instance mode |
| `--id <id>` | Id of the game, from 1 |
| `--master-port <port>` | Port where the master listens for its instances, on `127.0.0.1` |
| `--config <dir>` | The configuration the master checked, as an absolute path |

No UDP port: the instance receives its datagrams from the [relay](#the-relay), whose port `CONFIGURE` gives.
`--udp-port <port>` gives one of its own, for a `CONFIGURE` without a relay (the tests) and the standalone mode.

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

**The settings file** of the master, given with `--settings`, is read by `loadSettings()` (`Common/Settings.hpp`),
with toml++, before anything else: `run()` reads the command line once to find the file, then again over what the
file gives, so that the command line wins. Its `[server]` section holds the options above; the other sections
(`[web]`, `[accounts]`, `[mail]`, `[discord]`, `[admin]`) are checked and kept in `MasterOptions::settings`, for
the panel and the accounts. A file with a mistake stops the master (exit 1), with its line and its column; the
secrets are in files of their own, read by `readSecretFile()` and never logged
([Configure a server](../guides/configure-a-server.md)):

```
ERROR server   server.toml:12:5: [admin] admins: "pavel" is neither an email nor discord:<id>; an administrator is never named by username
```

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

## The database

The master keeps its data in a database ([ADR 0023](../decisions/0023-server-database.md)), through
`libs/database` ([Database](database.md)). Only the master opens it: the instances report to it, so a game
that crashes cannot damage the data.

| Step | What happens |
|---|---|
| Its address | `--database`, by default `sqlite:rtype.db` in the current directory, like the key; or the first line of the file of `--database-file`, whose content is never logged, since it may hold a password. A password given on the command line is warned about, and never logged |
| At start | `openServerDatabase()` opens it, a SQLite file being created with its directories, and applies the migrations of the master it does not have yet (`ServerMigrations`, in `Master/ServerDatabase.hpp`). A database that cannot be opened, or that a newer server changed, stops the master before it opens its port |
| Running | A `DatabaseThread` owns it: the features that keep data give it their tasks, which run on its thread, and get their results back on the event loop. It is destroyed before the loop, after the tasks already given are done |

| Migration | Tables | Used by |
|---|---|---|
| 1 | `admin_actions`: the record of the actions of the administrators | [Administration](administration.md) |
| 2 | `address_bans`: the IP addresses banned for a time, read at start | [Administration](administration.md#bans) |
| 3 | `accounts`: the accounts of the players | [Accounts](accounts.md#the-tables) |
| 4 | `account_sessions`: their sessions, by the SHA-256 of their token | [Accounts](accounts.md#sessions) |
| 5 | `account_codes`: the codes sent by email, by their hash | [Accounts](accounts.md#codes) |

```
INFO  master   Database postgresql://rtype:***@db.example.org/rtype, schema 0
ERROR master   The database sqlite:rtype.db has migration 3 (create the scores table), which this program does not know: it was made by a newer one
```

To use a PostgreSQL server, see [Use an external database](../guides/use-an-external-database.md).

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
| `HELLO` | `WELCOME` with a new player id, the features, and the account of its token; `INVALID_NAME`; with accounts, `NAME_RESERVED` for the username of an account; `UNSUPPORTED_VERSION`, `BANNED`, or `SESSION_EXPIRED` for a session token refused, then closed | `ALREADY_WELCOMED` | `ALREADY_WELCOMED` |
| `PING` | `PONG` | `PONG` | `PONG` |
| `LIST_GAMES` | `HELLO_REQUIRED` | `GAME_LIST` | `GAME_LIST` |
| `CREATE_GAME` | `HELLO_REQUIRED` | `GAME_JOINED` once the game is ready; `INVALID_NAME`, `SERVER_FULL`, `GAME_START_FAILED`; `GUESTS_DISABLED` for a guest of a master without guests | `ALREADY_IN_GAME` |
| `JOIN_GAME` | `HELLO_REQUIRED` | `GAME_JOINED`; `GAME_NOT_FOUND`, `GAME_FULL`; `GUESTS_DISABLED` for a guest of a master without guests | `ALREADY_IN_GAME` |
| `LEAVE_GAME` | `HELLO_REQUIRED` | `NOT_IN_GAME` | `LEFT_GAME (REQUESTED)` |
| `REGISTER` … `DELETE_ACCOUNT` (the accounts) | `HELLO_REQUIRED` | With accounts, their replies ([Accounts](accounts.md#the-requests)); without, `ACCOUNTS_DISABLED`, the payload not read | `ALREADY_IN_GAME`: nobody changes identity in a game |
| Any other type | `UNKNOWN_MESSAGE` | `UNKNOWN_MESSAGE` | `UNKNOWN_MESSAGE` |
| A malformed payload | `MALFORMED`, then closed | `MALFORMED`, then closed | `MALFORMED`, then closed |

- **One reply per request, in order**: the requests of a connection are handled one after the other, so a
  client can send several without waiting. `CREATE_GAME` is answered once its game is ready, up to 10 seconds
  later, and on a server with accounts `HELLO` and the requests of the accounts once the database, the hasher
  or the mailer answered: the requests sent meanwhile wait for its answer (32 at most, then the client is
  disconnected).
- **The state comes first**: a second `HELLO` is `ALREADY_WELCOMED` whatever it holds, a `LIST_GAMES` before
  `HELLO` is `HELLO_REQUIRED` whatever its payload.
- **A player id** is given by `HELLO`: never 0, never given twice by a master. Names do not have to be unique.
- **The accounts** ([lobby protocol 3](../protocol/lobby.md#6-accounts)) are answered by the `AccountService`
  when the settings enable `[accounts]` ([Accounts](accounts.md#in-the-lobby)): `WELCOME` offers password
  accounts, and guests unless `accounts.guests = false`; a guest carries `~` before its name. Without accounts,
  `WELCOME` offers guests only, every request of the accounts gets `ACCOUNTS_DISABLED`, and a `HELLO` with a
  session token gets `SESSION_EXPIRED`, after which the client connects again without it. The locale of `HELLO`
  is kept with the connection, for the language of the [emails](accounts.md#the-mails).
- **Leaving without asking**: a game over, a crash, a client that timed out, or an administrator who removes
  the player or ends its game, send `LEFT_GAME` with their reason, and the player is back in the lobby. A client that closed its UDP session itself (`QUIT`) is back in
  the lobby without a message: it knows. A lobby connection that closes takes its player out of its game.

- **The administrators** ([Administration](administration.md)) see the players through `AdminLobby`: their
  name, address and game. They send them `SERVER_MESSAGE`, or remove them: `ERROR (KICKED)`, then the
  connection closes. An address banned (`setBans()`) is refused at `HELLO` with `ERROR (BANNED)`, and a player
  connected from it when the ban comes is removed with it.

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

- **Its own executable**: the master starts `currentExecutable()` with `--instance --id --master-port`, and
  the `--log-level` it runs with: the instances log like it.
- **The instances connect back** to a listener of the master on `127.0.0.1`, on a port of the system. An
  instance has 10 seconds to present itself; an unknown id closes its connection, a wrong secret (compared in
  constant time, `sameSecret()`) also kills the instance it pretends to be.
- **A session per player**: an id and two keys drawn by the master, given to the instance (`ADD_PLAYER`)
  before the player (`GAME_JOINED`), so that the game knows the session when the client connects. The id is
  random, never 0, and unique across the master, not only in its game: one id names one player of one game.
  It stays taken until the instance of its game exits, so an instance never sees the same id twice, even
  after its player left. An id already taken is drawn again (`takeSession()`); the tests force collisions
  with `setSessionDraw()`.
- **Every game behind the [relay](#the-relay)**, on one UDP port: the instance is given the relay in
  `CONFIGURE`, `READY` carries its port on the loopback, and `GAME_JOINED` the public port of the relay, the
  same for every game. `--max-games` games at once, those ending included until their process exits: one
  more is `SERVER_FULL`.

| What happens | The players | The instance |
|---|---|---|
| The last player leaves, or its lobby connection closes | — | `SHUTDOWN (EMPTY)` |
| No `INSTANCE_HELLO` or no `READY` within 10 s, or the program cannot start | `ERROR (GAME_START_FAILED)` | Killed |
| `GAME_OVER` | `LEFT_GAME (GAME_OVER)` | Exits on its own |
| It exits, or closes its connection, without `GAME_OVER` | `LEFT_GAME (GAME_CRASHED)` | Killed if still there |
| `PLAYER_DISCONNECTED (TIMED_OUT)` | `LEFT_GAME (TIMED_OUT)` | — |
| The master stops | Their UDP sessions get `DISCONNECT (SHUTDOWN)` | `SHUTDOWN (MASTER_STOPPING)` |
| An administrator removes a player | `LEFT_GAME (KICKED)`, then `ERROR (KICKED)` | `REMOVE_PLAYER (KICKED)` |
| An administrator ends the game | `LEFT_GAME (ENDED_BY_ADMIN)` | `SHUTDOWN (ENDED_BY_ADMIN)` |
| A malformed or unexpected message, `GAME_STATUS` included | `LEFT_GAME (GAME_CRASHED)` | Killed |

**The status of each game.** Every second, a running instance sends `GAME_STATUS`: its tick, its wave, and for
each connected player its score, whether it is alive, and its latency. `Games` keeps the last one and when it
came; `Games::status(id)` gives it with its age, for a running game only. A game without `GAME_STATUS` for 3
seconds (`GamesSettings::statusTimeout`) is **stale**: shown so, not stopped, since a process that dies is
already caught by its exit. The [administration](administration.md) reads it from there, through
`AdminGames`, and is told of each game that starts, changes or ends through its `AdminObserver`.

The processes are checked every 50 ms (`exitCode()` never blocks). An instance that does not exit within 5
seconds of `SHUTDOWN` or `GAME_OVER` is killed. On Ctrl+C, the master closes its lobby, shuts every game down,
and stops once every instance has exited:

```
INFO  games    Pavel creates game 1 "Bydo hunters" for 4 players: instance 48213
INFO  games    game 1 "Bydo hunters" ready
INFO  games    Pavel (1) joins game 1 "Bydo hunters" (1/4)
```

In `debug`, the port where the instance receives the frames of the relay: `game 1 "Bydo hunters" receives from
the relay on 127.0.0.1:40213`.

### The relay

`Relay` (`Master/Relay.hpp`) serves every game of the master on one public UDP port, given by `--game-port`
([ADR 0027](../decisions/0027-one-udp-port-relay.md)).
It owns the only public UDP socket, reads the session in the clear header of each datagram
([game protocol](../protocol/game.md), section 2), and forwards it to the instance of that game over the
loopback, in a [relay frame](../protocol/instances.md#5-the-relay-frame) with the address of its client. Each
instance still runs in its own process ([ADR 0013](../decisions/0013-one-process-per-game.md)).

```mermaid
flowchart LR
  C1["Client A"] -- "UDP 4242" --> R
  C2["Client B"] -- "UDP 4242" --> R
  subgraph Master
    R["Relay<br/>own thread, own EventLoop<br/>session → instance"]
  end
  R -- "127.0.0.1, relay frame" --> I1["Instance 1<br/>127.0.0.1:ephemeral"]
  R -- "127.0.0.1, relay frame" --> I2["Instance 2<br/>127.0.0.1:ephemeral"]
```

*The clients send to one port; the relay finds the game of each datagram by its session.*

**Inbound**, each datagram on the public port, in this order:

1. its header is invalid (`parseDatagramHeader()`: size, magic, version, session 0): dropped;
2. its address is banned (its copy of the [bans](administration.md#bans)): dropped;
3. its address sends more than 200 datagrams per second: dropped, with the same counter as the `SessionHost`
   of a game (`RateLimiter`, `libs/network`);
4. no game has its session: dropped, logged at `trace`;
5. otherwise, it goes in a frame, with the address of its client, to the instance of its session.

**Outbound**, each frame on its loopback port:

1. from an address that is not one of the instances: dropped;
2. a frame that does not decode, or whose datagram has no valid header: dropped;
3. the session of the datagram is not one of that instance: dropped, since an instance only ever speaks for
   its own players;
4. otherwise, the datagram goes from the public port to the address of the frame.

The client sees nothing of it: it sends to the address of the master on the port of `GAME_JOINED`, and its
answers come from there. The relay never holds the keys of a datagram, so the encryption stays end to end,
client to instance; the instance still sees the real address of each client, so its own limit per address
and its following of an address that changes work as with a port of its own.

**The table** `session → game`, and the loopback address of each instance, live on the thread of the relay.
`Games`, on the loop of the master, posts every change to it: the instance at its `READY`, a session at its
`ADD_PLAYER`, a session released when its player leaves, still routed 2 seconds (`Relay::SessionLinger`) for the
`DISCONNECT` of its instance to go out, and every session of a game removed when its instance exits. A session
released whose id the master gave since to another game, once the first game is gone, stays routed to the new
one. The bans are posted the same way: `AddressBans` calls its change handler at each ban added or lifted, and
the master posts a copy; an expired ban is never applied, since the relay checks its date.

**Its own thread**, with its own `EventLoop` ([Threads](threads.md#server)): it copies every datagram of every
game, about 480 per second for a game of 4 at 60 Hz, both ways, so it never waits on the lobby, its TLS or its
database. It shares nothing with the loop of the master but posted functions, and its counters (`counters()`:
datagrams forwarded each way, dropped for each reason), which are atomic.

**The cost**: two hops on the loopback per round trip. With 8 games of 4 players at 60 Hz, each datagram
echoed by its instance (`rtype_tests "[.relay-latency]"`, a Release build), the relay adds 40 to 70 µs to the
median round trip, against a tick of 16.7 ms.

**Its stop**: once every instance has exited, their last `DISCONNECT (SHUTDOWN)` forwarded, the master stops the
relay (`stop()`, after the work posted before), then its own loop.

## An instance at work

### With its master

```mermaid
sequenceDiagram
    participant M as Master
    participant I as Instance
    participant C as Client
    Note over I: reads the secret
    I->>M: INSTANCE_HELLO (id, secret), on 127.0.0.1
    M->>I: CONFIGURE (name, max players, relay port)
    Note over I: opens a UDP port on the loopback, behind the relay, starts the game thread
    I->>M: READY (UDP port)
    M->>I: ADD_PLAYER (player, session, keys, name)
    C->>I: CONNECT
    I->>C: ACCEPTED (slot, 60, tick), PLAYER_JOINED
    I->>M: PLAYER_CONNECTED
    loop every tick
        C->>I: INPUT
        I->>C: SNAPSHOT (every 2 ticks), ENTITIES_DESTROYED
    end
    loop every second
        I->>M: GAME_STATUS (tick, wave, scores, latencies)
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
        Host["GameHost<br/>transport, SessionHost, players"]
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
  the tick of the last input applied for each player, the smallest lead of its `INPUT` messages, and the state
  of the weapon of its ship (`Simulation::weaponOf()`).
- **`GameHost`** is the game seen from the network ([game protocol](../protocol/game.md)):
  - its datagrams come and go through a `GameTransport`, with the real address of each client, so its
    `SessionHost` limits and follows clients by their address whatever carries the datagrams. The instance
    builds it from `CONFIGURE`, before the host, and "Cannot open UDP port" comes from there:
    - relay port 0 (the standalone mode, the tests of an instance alone): `DirectTransport`, a `UdpSocket`
      bound to `--udp-port`, or a port of the system without it;
    - a relay port: `RelayedTransport`, a `UdpSocket` on `127.0.0.1` at a port the system chooses, given to the
      master in `READY`. It reads only the frames from `127.0.0.1:<relay port>`, hands the game the address of
      the client and the datagram of each one ([relay frame](../protocol/instances.md#5-the-relay-frame)), and
      sends every datagram back to the relay in a frame. The log says `Game 7 behind the relay on
      127.0.0.1:4242, receiving on 127.0.0.1:40213`. A master always gives one ([The relay](#the-relay));
  - a player added gets the lowest free slot, and its ship when its client connects;
  - on `CONNECT`, the client receives `ACCEPTED`, then `PLAYER_JOINED` for each player connected, itself
    included; the others receive its `PLAYER_JOINED`;
  - the inputs of each `INPUT` go to the game, which applies each one at its tick;
  - every 2 ticks, each client receives the snapshot in parts, with its last input, the smallest lead of its
    `INPUT` messages over those 2 ticks and the weapon of its ship; every tick, the entities
    destroyed. They leave as soon as the tick is published, not at the next update of the sessions: up to a
    tick of delay less, and no jitter from it;
  - the game over goes to every client (`GAME_OVER`, with the last snapshot), then to the instance with the
    score of each player; nothing is published after it;
  - a session that closes takes the ship away; the others receive `PLAYER_LEFT`, and the master
    `PLAYER_DISCONNECTED`, unless it removed the player itself (`REMOVE_PLAYER`);
  - `status()` builds the `GAME_STATUS` sent every second by a `Timer` of the instance: the tick, the wave
    and the players of the last result published, and the round-trip time each session measures from the
    acknowledgements. The game thread copies nothing more for it: its numbers already come with each result,
    and the network thread keeps the last, so no lock is held during a tick.
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

Each `INPUT` received also gives its **lead**, ignored or not: its tick minus the last tick simulated (1 just in
time, 0 or less too late, above 60 too early). `InputBuffer::takeLead()` gives the smallest since the previous
tick, `GameHost` keeps the smallest until the next snapshot, and the snapshot tells it to the client, −128 if no
`INPUT` came. The client paces its inputs on it so that they arrive about 2 ticks early
([The client online](client-online.md#input-clock), [ADR 0028](../decisions/0028-input-stamps-follow-the-lead.md)):
nothing on the side of the game corrects a client, it only reports.

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
   instance sends `DISCONNECT (SHUTDOWN)` to its players. The master stops its web server, closes the lobby, which disconnects its
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
const std::vector<std::string> arguments = {"--instance", "--id", "7", "--master-port", "5000"};
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
| `0` | Stopped properly: after a stop signal, after `--help`, `--version` or `--check`, at the end of its game for an instance; with `--health`, the master is healthy |
| `1` | Could not start (settings file refused, key missing or invalid, port of the lobby or of the panel in use, panel off the loopback without accounts, no secret), stopped on an unexpected error, or stop forced; with `--health`, the master is not healthy or cannot be reached |
| `2` | Invalid arguments |

The master reads the exit code of its instances: anything but 0 is a crash, which it reports to the players
([instances protocol](../protocol/instances.md#3-life-and-death-of-an-instance)).

## Tests

- `tests/server_options.cpp` (tag `[server]`): the defaults, every option of both modes, every invalid
  combination, `--help` and `--version` first, the standalone mode refused in release, the exit codes of
  `run()`, a master without its key, an instance without a secret, and the secret itself.
- `tests/server_settings.cpp` (tags `[server]`, `[settings]`): every key of the settings file and their
  defaults, an unknown key or section, a wrong type, a TOML error, each with its line and column; invalid
  values, the rules between the sections (accounts without mail, Discord without accounts, the
  panel off the loopback without accounts), an administrator named by username; relative paths from the folder
  of the file; the secret files, missing, empty, readable by everyone, never logged; the sender of the mails and
  its address, mail in the clear without a password; the command line winning; and `--settings --check`, with
  the fake mail server.
- `tests/server_mail.cpp` (tags `[server]`, `[mail]`): the mails, their format, and `CurlMailer` against the
  fake mail server of `tests/helpers/SmtpServer.hpp` ([Accounts](accounts.md#tests)).
- `tests/server_key.cpp` (tag `[server]`): the key generated at the first start, then loaded, only readable by
  its owner, never replaced, and `--fingerprint`.
- `tests/server_web.cpp` (tags `[server]`, `[web]`): a real `WebServer` with its loop on a thread, asked with
  libcurl (`tests/helpers/Http.hpp`). `/api/health` answered after the loop has worked, the others meanwhile;
  a request waiting for the loop answered `503` at the stop; the files of the panel and their types,
  `index.html` for its pages, the page "not built", no path out of its folder, pages and files asked one after
  the other on a connection kept alive, as a browser does; the headers of security on every
  response; the token traded for a session cookie, a wrong token refused, neither in the logs at any level;
  `X-Forwarded-For` from the right and only from a trusted proxy, in `visitorAddress()` and end to end; a port
  taken. Real masters: `admin-url.txt` (`0600`) whose address opens the page "not built" with a session, a port
  taken, a missing `panel_dir`, and a panel off the loopback refused without accounts, allowed with them and
  their mail server. Every test that runs a
  master gives its panel port 0, with `testing::webOnAnyPort()`. `--health`: the answers of `/api/health`
  read (healthy, another status, not HTTP, cut), a master asked at the address of its settings file, one whose
  loop is stuck past the timeout, one gone, and a panel on port 0, which it cannot find.
- `tests/helpers/panel_players.cpp`, `rtype_panel_players`: not a test, the players of a game for the end-to-end
  tests of the panel ([The panel](panel.md#tests)), which run a real master.
- `tests/server_admin.cpp` (tags `[server]`, `[admin]`): the administration, alone with fakes of the games and
  the lobby, then end to end over HTTP and the WebSocket with real clients and instances
  ([Administration](administration.md#tests)).
- `tests/server_account_store.cpp` (tags `[server]`, `[accounts]`): the rules of the accounts, the hasher of
  the passwords, and their tables on every engine ([Accounts](accounts.md#tests)).
- `tests/server_accounts.cpp` (tags `[server]`, `[accounts]`, `[database]`): the accounts in a real lobby, with a
  mailer of the tests that keeps the codes, on every engine: every flow and its errors, the order of the replies,
  the limits, the purge, the deletion of an account ([Accounts](accounts.md#tests)).
- `tests/server_database.cpp` (tag `[server]`): `sqlite:rtype.db` by default, `--database`, `--database-file`
  with a Windows line end, the invalid combinations; a database created at start, then opened again; a file
  without an address, empty or missing, stopping the master without its content logged; a password on the
  command line warned about and never logged; a file that is not a database, and a database of a newer server,
  stopping it; and the master started on every engine of the tests.
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
  too late, too far ahead or received twice ignored, and the lead of each `INPUT` measured, even ignored.
- `tests/server_instance.cpp` (tag `[instance]`): the messages between the master and its instances (round
  trips, bytes, every malformed case, random bytes), the game thread (its rate, each input applied at its
  tick, whatever their order, the lead of the inputs, nothing after `stop()`), the game over of `GameHost` (sent, reported with the
  scores, acknowledged, nothing after), a `GameHost` on a fake transport (a client far away connected, every
  datagram sent back to its address, the transport closed with the game), a `RelayedTransport` (only the frames
  of the relay read, malformed ones dropped, every datagram sent in a frame to the relay), every failure of
  `MasterLink`, and whole instances run by `runInstance()` on
  a thread of the test, with a fake master over TCP and real clients over UDP: two players who join, see each
  other and move, one removed by the master, one who quits, then `SHUTDOWN`; the master going away; a full
  game; a UDP port taken; a player removed by an administrator; a game behind a fake relay on the loopback
  (two clients on one public port, each answered at its own address, a third one whose frames come from
  another port never heard, `DISCONNECT` through the relay); and the standalone mode with its public keys,
  stopped by Ctrl+C.
- `tests/relay_frame.cpp` (tag `[relay]`): the relay frame: round trips in IPv4 and IPv6, its bytes, the
  largest datagram and one byte more, every malformed case, and random bytes.
- `tests/server_relay.cpp` (tag `[relay]`): the `Relay` alone, with fake clients and instances on the loopback:
  each datagram to the instance of its session with the address of its client, and back from the public
  port; every drop of each direction (not a datagram of the game, an unknown session, a frame from a port that
  is not an instance, one that does not decode, an instance that sends for a player of another game); a banned
  address until its ban is lifted, an expired ban; the rate per address (`RateLimiter`); a session released
  still routed for 2 seconds, then gone; a game gone with its sessions; an id given again to another game; the
  stop. The change handler of `AddressBans`. A measure, hidden from the runs (`[.relay-latency]`): the latency
  the relay adds with 8 games of 4 players at 60 Hz.
- `tests/lobby_messages.cpp` (tag `[lobby]`): the `HELLO` frame of the specification, the bytes of the other
  messages, round trips, the names checked apart, every malformed case, and random bytes.
- `tests/server_lobby.cpp` (tag `[lobby]`): a `Lobby` on the loopback and real TLS clients pinning its key.
  `HELLO`, `LIST_GAMES` and `PING`; requests sent without waiting answered in order; the answers of each
  state; invalid names, another version, malformed requests; the limits per address and in all, a place
  freed taken again; the handshake, `HELLO` and silence timeouts, `PING` keeping a client; and closing the
  lobby.
- `tests/server_games.cpp` (tag `[games]`): real instances of `r-type_server` started by `Games`, real lobby
  clients over TLS and game clients over UDP. A game created, listed, joined, full, left, and shut down with
  its last player; the requests after `CREATE_GAME` answered in order; session ids unique across two games,
  0 and collisions drawn again, an id kept until its instance exits; `SERVER_FULL` past `max_games`; an
  instance that cannot start, exits at once or never gets ready, and its place given back; an impostor with an unknown id or a wrong
  secret; a crash reported as `GAME_CRASHED`; a silent client reported as `TIMED_OUT`; the master stopping its
  games; every game behind a real relay: two games of two players on one UDP port, each player seeing only
  its game, a player who leaves still receiving its `DISCONNECT`, and a crashed instance whose sessions are
  gone; and the real binary run as a master, its lobby and its games on the same number in TCP and UDP, which
  starts the instance of a game created by a client.
- `tests/server_key.cpp` (tag `[server]`): a key generated where none exists, then the same one loaded; only
  its owner can read it; a file that is not a key, or cannot be written, stops the master and is never
  replaced; `--fingerprint` shows the key and never makes one.
- The smoke tests of CTest run `r-type_server --help`, refuse an unknown argument, and run `--check`, which
  makes its key in the build directory the first time.
- `docker/smoke-test.sh`, in the *Docker* workflow: the image started with a test settings file, `--health`
  and the health check of the image, a user that is not root, the key, the database and `admin-url.txt` in
  `/data`, two players of `rtype_panel_players` in a game over the published ports, and `SIGTERM` stopping it
  with code 0 ([Deploy with Docker](../guides/deploy-with-docker.md)).
- `docker/smoke-test-accounts.sh`, after it: the image started with `docker/server-accounts.toml`, the settings
  of the official server, its Discord secret in `/run/secrets`. `rtype_fake_services`
  (`tests/helpers/fake_services.cpp`) runs a fake Discord over HTTPS, under the network alias `discord.com`,
  and a fake mail server; the image is given their certificate to trust, so the master reaches Discord at the
  address it always uses. The test checks that the master checked both at start, that a player signs in to its
  account and plays with a guest, that the panel offers the password and Discord, refuses a player and lets an
  administrator of `[admin] admins` in with a `Secure` cookie, and the stop on `SIGTERM`.

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
