# Administration

Everything an administrator sees of a server and does to it ([ADR 0025](../decisions/0025-server-administration.md)):
the games, the players of the lobby, live, and the actions on them, each one recorded. It lives in the master,
on its event loop, independent of what shows it: the web server serves it as the
[administration API](../protocol/admin-api.md), and the panel is its first client ([The panel](panel.md)).

| | |
|---|---|
| Code | `Master/Administration.{hpp,cpp}` (what it shows and does), `Master/AdminModel.hpp` (its view of the games and the lobby, and their interfaces), `Master/AddressBans.{hpp,cpp}` (the bans in memory), `Master/AdminStore.{hpp,cpp}` (its tables), `Web/AdminJson.{hpp,cpp}` (its JSON) |
| Thread | The event loop of the master; its records on the database thread |
| Tables | `admin_actions`, `address_bans` (migrations 1 and 2 of `ServerMigrations`) |
| Tests | `tests/server_admin.cpp`, tag `[admin]`; `tests/client_online.cpp` for what the client shows |

## The pieces

```mermaid
flowchart LR
    Web["WebServer<br/>Crow threads"] -- "loop.post()" --> Admin["Administration"]
    Admin -- "AdminGames" --> Games
    Admin -- "AdminLobby" --> Lobby
    Games -- "AdminObserver" --> Admin
    Lobby -- "AdminObserver" --> Admin
    Lobby -- "find()" --> Bans["AddressBans"]
    Admin -- "add()" --> Bans
    Admin -- "run()" --> DB["DatabaseThread<br/>admin_actions, address_bans"]
    Admin -- "events" --> Web
```

*Administration reads and acts through two small interfaces, and hears what happens through a third.*

- **`AdminGames`** (implemented by `Games`): the running games with their players and their last status;
  `kick(player)` takes a player out of its game, `end(game)` ends a game.
- **`AdminLobby`** (implemented by `Lobby`): the welcomed players, with their address and their game;
  `disconnect(player, code)` sends `ERROR (KICKED)` or `ERROR (BANNED)` then closes the connection;
  `sendMessage(player, text)` sends `SERVER_MESSAGE`.
- **`AdminObserver`** (implemented by `Administration`): `Games` and `Lobby` call it as things happen, with an
  id only: a player joined, changed game or left; a game started, changed or ended, with why. `Administration`
  reads the rest through the two interfaces, already up to date.

The tests give `Administration` fakes of the first two, and call the third themselves: no instance, no socket.

`runMaster()` puts it together once the games and the lobby exist, and takes the observers away before they
are destroyed:

```cpp
AddressBans bans(loadAddressBans(database, now));           // before the database thread takes it
Lobby lobby(loop, std::move(listener), identity, games);
lobby.setBans(&bans);
Administration administration(loop, games, lobby, bans, &databaseThread, settings);
games.setObserver(&administration);
lobby.setObserver(&administration);
web.setAdministration(&administration);
```

## What it shows

`server()`, `games()`, `game(id)`, `players()` and `bans()` read the state of the master when they are called;
`actions()` reads the record on the database thread, its answer coming back on the loop. A game is shown once
its instance said `READY`, until it ends: `Games::adminGames()` leaves out the games that start or end.

**Live events**: `subscribe()` calls its subscriber at once with a snapshot (the server, the games, the
players), then with each event, in order, on the loop. The web server subscribes each WebSocket of
`/api/events`.

| Event | Sent when | By |
|---|---|---|
| `PlayerJoined` | `HELLO` welcomed | `Lobby::hello()` |
| `PlayerUpdated` | `GAME_JOINED`, `LEFT_GAME` | `Lobby` |
| `PlayerLeft` | its connection closed, whatever the reason | `Lobby::forget()` |
| `GameStarted` | `READY` | `Games::ready()` |
| `GameUpdated` | a player added or removed, a `GAME_STATUS` (every second) | `Games` |
| `GameEnded` | a running game leaves that state: empty, over, crashed, ended by an administrator, master stopping | `Games` |
| `ActionRecorded` | an action is in `admin_actions`, with its id | `Administration` |

## What it does

| Action | Through | The player sees |
|---|---|---|
| `kick(player, reason, actor)` | `AdminGames::kick()`: `REMOVE_PLAYER (KICKED)` to its instance; then `AdminLobby::disconnect(KICKED)` | `LEFT_GAME (KICKED)` if in a game, then `ERROR (KICKED)`, and the connection closes |
| `endGame(game, reason, actor)` | `AdminGames::end()`: `SHUTDOWN (ENDED_BY_ADMIN)` to its instance | `LEFT_GAME (ENDED_BY_ADMIN)`, back in the lobby; its UDP session ends with `DISCONNECT (SHUTDOWN)` |
| `sendMessage(game or everyone, text, actor)` | `AdminLobby::sendMessage()` for each player of the game, or of the lobby | `SERVER_MESSAGE`, shown in a toast |
| `ban(address, duration, reason, actor)` | `AddressBans::add()`, saved in `address_bans`; each player connected from it removed with `BANNED` | `ERROR (BANNED)`; refused at `HELLO` until it expires |

In the game, a player removed by an administrator leaves as one who quit: its session gets
`DISCONNECT (QUIT)`, and the others `PLAYER_LEFT (QUIT)`. The [game protocol](../protocol/game.md) does not
change; the lobby connection says why.

**Every action is recorded** in `admin_actions`, with its time, its actor (`local` for the local access token,
until the accounts of step 12), what it was done to, and its details as a JSON object
([the record](../protocol/admin-api.md#3-reading)). The action is done first, at once;
its record goes to the database thread, and comes back as an `ActionRecorded` event once written. A record that
fails is logged, and the action stays done. The logs say each action too:

```
INFO  admin    Bob (2) removed by local, because "cheating"
INFO  admin    Game 1 "Duo" ended by local, because "maintenance"
INFO  admin    Message from local to everyone (3 players): Restart in 5 minutes
INFO  admin    198.51.100.4 banned by local for 3600 s, because "flood": 1 players removed
```

## Bans

A ban refuses an IP address at `HELLO` until a date: `ERROR (BANNED)`, then the connection closes. The bans of
addresses are temporary, 7 days at most (`MaxBanDuration`): an address is shared by a household or a school,
and changes. The accounts will bring bans of accounts.

| Where | What |
|---|---|
| `AddressBans` | In memory, on the loop: what the lobby asks at each `HELLO`, by `find(address)`. An expired ban is not found, even before it is purged |
| `address_bans` | The table, one row per address (`address` its primary key, `created_at`, `expires_at`, `reason`, `actor`). Each ban is saved as it is added, on the database thread |
| At start | `loadAddressBans()` deletes the expired rows, then reads the others, before the database thread takes the database: a ban survives a restart |
| Every minute | The expired bans are forgotten in memory, and deleted from the table |

An address is compared without its port, and an IPv4 address seen by a listener on `[::]`
(`::ffff:203.0.113.9`) as the IPv4 address it holds (`banKey()`). The bans go by the address of the TCP
connection to the lobby: the lobby has no proxy in front of it, unlike the panel.

## Tests

`tests/server_admin.cpp` (tag `[admin]`):

- **`Administration` alone**, with fakes of the games and the lobby: the snapshot then the events in order;
  each action, what it asked of the games and the lobby, and its record read back, newest first and page by
  page, on SQLite and PostgreSQL; a ban capped at 7 days.
- **The bans**: expiry, the port and the IPv6 writing of an address, the table on both engines with its purge.
- **End to end**, with real lobby clients, real instances and the web server on a free port: the WebSocket's
  snapshot then its events, every route; a player removed, seen by the client and by its instance; a game
  ended; a message to a game and to everyone; an address banned, refused at `HELLO`, after a restart too, and
  welcome again once expired; no session, no CSRF token or another session's, wrong JSON, and a WebSocket opened
  without a session, without an origin, or from another one.
