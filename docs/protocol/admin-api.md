# Administration API (HTTP)

| | |
|---|---|
| **Status** | Implemented |
| **Transport** | HTTP from the web server of the master, behind the HTTPS of the host's proxy ([Web server](../architecture/web.md)) |
| **Format** | JSON, UTF-8 |
| **Who** | A session of the panel: the local access token of `admin-url.txt` traded for a cookie |

The API through which the panel, or any other tool, sees a server and acts on it
([ADR 0025](../decisions/0025-server-administration.md)). Every route is answered by the event loop of the
master: what it shows is the state of the master at that moment. How it works inside the master:
[Administration](../architecture/administration.md).

## 1. Conventions

- **Times** are Unix milliseconds (`1798000000123`), **durations** milliseconds, except the duration of a ban,
  given in seconds.
- **Ids** of players and games are those of the [lobby protocol](lobby.md): a player id comes from `WELCOME`, a
  game id from `GAME_LIST`. They are never 0, and never reused by a master until it restarts.
- **Addresses** are IP addresses without a port: `203.0.113.9`, `2001:db8::7`. An IPv4 address seen in IPv6
  (`::ffff:203.0.113.9`) is written as IPv4.
- **Errors** are `{"error": "<what went wrong, for a person>"}`, with the status:

| Status | When |
|---|---|
| `400` | A body or a query the route cannot take: not JSON, a field missing, of the wrong type or out of its range |
| `401` | No session, or one that expired: open the address in `admin-url.txt` again |
| `403` | A `POST` without the CSRF token of its session; a WebSocket opened by a page that is not the panel |
| `404` | No player, or no game, with this id; no ban of this address |
| `503` | The master is stopping, or its database cannot answer now |

A request without a session is refused before anything else is read: a `401` says nothing about the body.

## 2. Sessions

```
GET /?token=<64 hex digits>           →  303, Set-Cookie: rtype_session=…; HttpOnly; SameSite=Strict
GET /api/session                      →  200 {"csrfToken": "<64 hex digits>"}
```

Every route below needs the cookie. Every `POST` also needs the CSRF token of its session in a header, which a
page of another site cannot read, so cannot send:

```
POST /api/players/7/kick HTTP/1.1
Cookie: rtype_session=4f…
X-CSRF-Token: 9a…
Content-Type: application/json

{"reason": "cheating"}
```

A session lasts 12 hours, and ends with the master: a restart asks for the new address of `admin-url.txt`.

## 3. Reading

### `GET /api/server`

```json
{
  "version": "0.4.0",
  "startedAt": 1798000000123,
  "uptime": 3600000,
  "ports": {"lobby": 4242, "games": {"first": 4243, "last": 4299}, "panel": 8080},
  "players": 3,
  "games": 1,
  "bans": 0
}
```

`players` counts the players of the lobby, in a game or not; `games` the running games; `bans` the addresses
banned now.

### `GET /api/games`

The running games, sorted by id: their instance said `READY`, and they do not end. A game being created is not
shown until it is ready (10 seconds at most).

```json
[
  {
    "id": 3,
    "name": "Bydo hunters",
    "maxPlayers": 4,
    "port": 4243,
    "createdAt": 1798000000123,
    "tick": 5400,
    "wave": 2,
    "statusAge": 412,
    "stale": false,
    "players": [
      {"id": 7, "name": "Alice", "score": 1500, "alive": true, "latency": 35},
      {"id": 9, "name": "Bob", "score": null, "alive": null, "latency": null}
    ]
  }
]
```

| Field | Meaning |
|---|---|
| `tick`, `wave` | From the last `GAME_STATUS` of the instance ([instances, 4.1](instances.md#game_status-0x06)); `null` before the first |
| `statusAge` | Since that status, or since the game got ready |
| `stale` | No status for 3 seconds: the instance, or its game thread, is stuck |
| `players` | The players the master added, sorted by id. `score`, `alive` and `latency` (in milliseconds) come from the status: `null` until the client of the player connected, and `latency` until it was measured |

### `GET /api/games/{id}`

One game, as above. `404` if no running game has this id.

### `GET /api/players`

The players of the lobby, sorted by id.

```json
[
  {"id": 7, "name": "Alice", "address": "203.0.113.9", "gameId": 3, "joinedAt": 1798000000123},
  {"id": 8, "name": "Carol", "address": "198.51.100.4", "gameId": null, "joinedAt": 1798000004567}
]
```

`gameId` is the game the player is in, `null` in the lobby. Names do not have to be unique.

### `GET /api/actions`

Query: `?limit=100&before=<id>`, both optional. The record of the actions, newest first: `limit` of them (1 to 500, 100 by default), with an id below `before`
if given, for the next page.

```json
[
  {
    "id": 12,
    "at": 1798000000123,
    "actor": "local",
    "action": "kick",
    "target": "player:7",
    "details": {"name": "Alice", "address": "203.0.113.9", "gameId": 3, "reason": "cheating"}
  }
]
```

| `action` | `target` | `details` |
|---|---|---|
| `kick` | `player:<id>` | `name`, `address`, `account` (its id) if it was signed in, `gameId` if it was in a game, `reason` |
| `end_game` | `game:<id>` | `name`, `players` (their names), `accounts` (the ids of those signed in), `reason` |
| `message` | `game:<id>` or `everyone` | `text`, `recipients` |
| `ban` | `address:<ip>` | `duration` (seconds), `expiresAt`, `reason`, `removed` (the players connected from it) |
| `unban` | `address:<ip>` | `expiresAt` (when the ban would have ended), `bannedBy` (the actor of the ban), `reason` |

`actor` is `local` for the local access token; the accounts will add theirs, and `deleted` once its account is
deleted. A player who deletes its account is taken out of the actions done to it: its name becomes `deleted`, its
address and id are removed ([Accounts](../architecture/accounts.md#deleting-an-account)). A `reason` not given is
`""`.

### `GET /api/bans`

The addresses banned now, the first to expire first.

```json
[{"address": "198.51.100.4", "createdAt": 1798000000123, "expiresAt": 1798003600123, "reason": "flood", "actor": "local"}]
```

## 4. Acting

Each action is done at once, then recorded: it shows in `/api/actions`, and as an `action` event, once the
database has it.

### `POST /api/players/{id}/kick`

Body: `{"reason": "…"}`, optional, at most 200 bytes of text; an empty body is accepted.

The player is taken out of its game, its instance told (`REMOVE_PLAYER (KICKED)`), and the player too
(`LEFT_GAME (KICKED)`); then out of the server: `ERROR (KICKED)`, and its connection closes
([lobby](lobby.md#error-0x86)). It may connect again: to keep it out, ban its address.

`200 {"playerId": 7}`, or `404` if no player of the lobby has this id.

### `POST /api/games/{id}/end`

Body: `{"reason": "…"}`, optional.

The instance stops (`SHUTDOWN (ENDED_BY_ADMIN)`), and its players are back in the lobby
(`LEFT_GAME (ENDED_BY_ADMIN)`).

`200 {"gameId": 3}`, or `404` if no running game has this id.

### `POST /api/messages`

Body: `{"text": "…", "gameId": 3}`. `text` is 1 to 200 bytes of text without control characters, so on one
line; `gameId` is optional, or `null`: without it, the message goes to every player of the lobby.

Each player gets a `SERVER_MESSAGE` ([lobby](lobby.md#server_message-0x87)), which the client shows.

`200 {"recipients": 2}`, or `404` if no running game has this id.

### `POST /api/bans`

Body: `{"address": "198.51.100.4", "duration": 3600, "reason": "…"}`. `duration` in seconds, 1 to 604800 (7
days): addresses are shared and change, so a ban is temporary. `reason` is optional.

The lobby refuses the address at `HELLO` until the ban expires (`ERROR (BANNED)`), across restarts of the
master. The players connected from it are removed at once: out of their game, then `ERROR (BANNED)`. A ban of
an address already banned replaces it.

```json
{"ban": {"address": "198.51.100.4", "createdAt": 1798000000123, "expiresAt": 1798003600123, "reason": "flood", "actor": "local"}, "removed": 1}
```

### `POST /api/bans/lift`

Body: `{"address": "198.51.100.4", "reason": "…"}`. `reason` is optional.

The ban of the address ends before its time: the lobby welcomes the address again at once, and the next start
of the master does not read it again. The address is the one of the ban, in any writing (`::ffff:198.51.100.4`
lifts the ban of `198.51.100.4`).

`200 {"address": "198.51.100.4"}`, or `404` if no ban of this address runs: never banned, lifted already, or
expired.

## 5. Events

`GET /api/events` opens a WebSocket of the live events, as text frames of JSON. It needs the session cookie,
and the `Origin` of the page that opens it must be the panel's: the address the browser asked
(`http://<Host>` or `https://<Host>`), or `[web] public_url`. Otherwise the upgrade is refused, `401` or `403`,
before any event: `SameSite=Strict` does not stop a page of a sibling domain, and the events hold the
addresses of the players.

The first frame is a snapshot; the events follow, in the order they happen. What the panel sends is ignored.

```json
{"type": "snapshot", "server": {…}, "games": [{…}], "players": [{…}]}
```

| `type` | Fields | When |
|---|---|---|
| `player.joined` | `player` | A client said `HELLO` and was welcomed |
| `player.updated` | `player` | A player entered or left a game |
| `player.left` | `playerId` | A player left the lobby: its connection closed, or it was removed |
| `game.started` | `game` | A game is ready |
| `game.updated` | `game` | A player came or went, or its instance sent its status: every second for each game |
| `game.ended` | `gameId`, `reason` | A running game ended: `empty`, `over`, `crashed`, `ended_by_admin` or `master_stopping` |
| `action` | `action` | An action was recorded, as in `/api/actions` |

The objects are those of the routes: `server` of `/api/server`, `game` of `/api/games`, `player` of
`/api/players`, `action` of `/api/actions`.

```json
{"type": "player.joined", "player": {"id": 7, "name": "Alice", "address": "203.0.113.9", "gameId": null, "joinedAt": 1798000000123}}
{"type": "game.ended", "gameId": 3, "reason": "ended_by_admin"}
```

A panel that reconnects gets a new snapshot: events missed meanwhile are in it.

## 6. With curl

From the machine of the server, with the address of `admin-url.txt`:

```sh
url=$(cat /data/admin-url.txt)                     # http://127.0.0.1:8080/?token=…
base=${url%%/?token=*}
curl -s -c cookies -o /dev/null "$url"            # the session cookie
csrf=$(curl -s -b cookies "$base/api/session" | sed 's/.*"csrfToken":"\([0-9a-f]*\)".*/\1/')

curl -s -b cookies "$base/api/games"
curl -s -b cookies -X POST -H "X-CSRF-Token: $csrf" -d '{"reason":"cheating"}' "$base/api/players/7/kick"
curl -s -b cookies "$base/api/actions"
```
