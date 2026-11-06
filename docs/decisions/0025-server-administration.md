# 0025 — Administration through a web panel served by the master

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-28 |

## Context

Whoever hosts a server cannot see what happens on it, besides its logs, nor act on it: see the games and the
players in them, as they play; remove a player; end a game; tell every player something; ban an account.

- **The master runs as a service**, under systemd or in a container (the team's runs in Dokploy), with no
  terminal anyone looks at.
- **The panel must look good**, close to the art direction of the client.
- **A server without accounts** ([ADR 0024](0024-player-accounts.md)) still has someone who runs it, and who
  must be able to administer it from the machine.
- **A server with accounts** can be administered from anywhere, by accounts with the `admin` role.
- **The host provides HTTPS**: Dokploy routes a domain to a port of the container through its proxy, Traefik,
  with a Let's Encrypt certificate. The ports of the game do not go through it: the lobby has its own TLS, with
  the key the clients pin ([ADR 0021](0021-server-generated-key-trust-on-first-use.md)), and the games a range
  of UDP ports.
- **Players have no website**: the web server of the master serves its administrators, and the page Discord
  sends a player back to ([ADR 0024](0024-player-accounts.md)).

## Options

### Where the administration is shown

| | A. A text interface in the terminal of the master | B. A text tool beside it, `r-type_admin` | C. A web panel served by the master | D. A scene of the client |
|---|---|---|---|---|
| Works for a service or a container | No: no terminal | Yes, from a shell on the machine | Yes, from a browser | Yes, with an account |
| Without accounts | Yes | Yes | Yes, from the machine | No |
| Look | Text | Text | The best: any web component | The interface of the game |
| Work | Moderate | Moderate | The most: a web server, a web application, web security | Moderate |

### The web server in the master

| | Crow | Drogon | Boost.Beast | cpp-httplib |
|---|---|---|---|---|
| HTTP, routes, JSON | Yes | Yes | HTTP only, the rest by hand | Yes |
| WebSocket, for live updates | Yes | Yes | Yes | No |
| Built on | Asio, ours | Its own event loop, and an ORM besides SOCI | Boost.Asio: Boost in the build | Its own threads |
| Form | Headers, through CPM | A framework to build | Boost | One header |

## Decision

**C: a web panel, served by the master with Crow, on an administration API that the master owns.**

### One API, in the master

- The **administration API** lives in the master, on its event loop: the games, their players, live events
  (a player joins, a game ends, scores), and the actions (remove a player, end a game, send a message to a
  game or to everyone, ban or unban an account, change the role of an account). It does not depend on what
  shows it: the panel is its first client, a scene of the client could be another.
- **Every action is recorded** in the database, with who did it, when, and on what: an administrator can see
  what the others did.
- The live state of a game (wave, scores, players' latency) comes from its instance, which reports it to the
  master at a low rate: a message of the instances protocol.

### The web server

- **Crow 1.3.4**, header-only, through CPM, given the Asio the project already uses: one Asio in the binary. It
  runs on threads of its own and hands every request to the event loop of the master with `EventLoop::post()`,
  as the database thread does ([ADR 0023](0023-server-database.md)): the API is never touched from two threads.
- It serves the **panel** (static files), a **REST API** under `/api/` for the actions, a **WebSocket** for the
  live events, and the page Discord sends a player back to.
- **Plain HTTP only.** HTTPS is the host's: a reverse proxy in front of the master, Traefik in Dokploy, Caddy or
  nginx elsewhere. The master never handles a web certificate.
- **It listens on `127.0.0.1:8080` by default.** In a container, it listens on the network of the container,
  which only the proxy reaches. Behind a proxy, the address of a visitor is read from `X-Forwarded-For` only
  when the request comes from a proxy the configuration trusts.

### Who gets in

| | Without accounts | With accounts |
|---|---|---|
| Where the panel listens | The loopback only: the master refuses to start if asked to listen elsewhere | Anywhere, behind the proxy of the host |
| How | At start, the master writes the address of the panel with a secret token in a file only the server's user can read; the token opens a session. From the machine, or through an SSH tunnel | An account with the `admin` role, with its password or Discord. The token of the machine works too, on the loopback |
| The first administrator | — | Named in the configuration, or given the role from the panel opened with the token |

- **Sessions** are cookies `HttpOnly`, `Secure` behind HTTPS, `SameSite=Strict`, that expire; the actions also
  carry a token against cross-site requests; sign-in attempts are limited.
- The panel sends a strict **Content Security Policy**: it runs its own scripts only, and holds no secret.

### The panel

- A **React** application in **TypeScript**, with **Chakra UI** v3, built with **Vite**, in `web/admin/`. Its
  theme takes the colours, fonts and shapes of the interface of the client, so that both look like one game.
- The **CI** builds it with Node, and the server archive and its Docker image carry the result beside the
  server, as they carry `config/`. Building the game needs no Node: a server built without the panel says so
  on its page.
- Developed with the Vite development server, which forwards `/api/` to a master running locally.

### Deployment

A **Docker image** of the server comes with it, and a `docker-compose.yml` that Dokploy can use: the panel's
port behind the proxy and its domain; the lobby port (TCP) and the game ports (UDP, one since
[ADR 0027](0027-one-udp-port-relay.md)) published on the host directly; volumes for the key and the SQLite database; the secrets as files.

## Consequences

- A host administers its server from a browser, a host without accounts from its machine, and a host in
  Dokploy from its domain, with nothing to set up for HTTPS.
- The master gets a web server: a second way in, which takes care (sessions, cross-site requests, limits, the
  proxy) and tests of its own.
- The repository gets a second language and a second toolchain: TypeScript, Node, npm, for the panel only.
- The instances report the live state of their game to the master: a new message of the instances protocol.
- The administration API stays independent of the panel: a scene of the client, or a text tool, can be added
  later without changing the master.
- The plan of this work splits the API, the web server, the panel, the reports of the instances and the Docker
  image into steps that can each be merged on their own, after the accounts they depend on for remote access.
