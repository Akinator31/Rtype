# 0011 — Asio as the networking library

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-15 |

## Context

The client and the server talk over the network: UDP for the game, TCP for the lobby
([ADR 0012](0012-tcp-lobby-udp-game.md)). Something has to open the sockets, send and receive bytes, and tell
the program when data arrives.

Requirements:

- **Linux and Windows**, with the same code. Windows is built by the CI with MSVC, and cross-play between both
  is a goal of the subject.
- **Never wait for a client.** The server keeps simulating the game frame after frame whatever the clients
  do, so receiving and sending must not block the simulation.
- **Encapsulated.** The subject allows Asio or the system sockets, but system sockets must be wrapped in our
  own abstractions. Like raylib in the client ([ADR 0002](0002-raylib-graphics-library.md)), the library
  must not appear in the public headers: the rest of the code only sees our classes.
- **Fetched by CPM** ([ADR 0001](0001-cpm-package-manager.md)), nothing installed on the system.
- **The interesting part stays ours.** Reliability over UDP, sessions, encryption of the datagrams and
  replication of the world are the topics of the *Advanced Networking* track: the library moves bytes, our
  code decides what they mean.

## Options

| | A. System sockets | B. Asio (standalone) | C. ENet | D. GameNetworkingSockets |
|---|---|---|---|---|
| What it is | BSD sockets on Linux, Winsock on Windows | C++ library for sockets, timers and asynchronous operations | C library: reliable and unreliable channels over UDP | Valve's library: reliability, encryption, fragmentation over UDP |
| Linux and Windows | Two APIs, differences to handle by hand (`WSAStartup`, error codes, `SOCKET` vs `int`, `closesocket`) | Same code on both | Yes | Yes |
| Waiting for data | `poll`/`select`, or `epoll` / IOCP to write ourselves | Event loop (`io_context`) with handlers, on `epoll` and IOCP | Its own polling function | Its own polling function |
| TCP | Yes | Yes | No | No |
| Timers (heartbeat, resend) | To write | Included (`steady_timer`) | Internal only | Internal only |
| What is left for us | Everything | The protocol: reliability, sessions, encryption, replication | Sessions and replication: reliability is done | Replication only |
| Dependency | None | Header-only, no dependency (no Boost) | Small C library | Large: protobuf, plus OpenSSL or libsodium |
| Through CPM | — | Easy: headers only | Easy | Hard |

- **A** works, but the Linux/Windows differences and an efficient event loop are a lot of code that the game
  does not need to own, and that would have to be wrapped anyway.
- **C** and **D** do the reliability (and, for D, the encryption) themselves. They would save time, but they
  remove the very work the networking track evaluates, and neither handles the TCP side.

## Decision

**B: standalone Asio, fetched by CPM, wrapped in a new library `libs/network` (`rtype::network`).**

- Asio is a **private** dependency of `libs/network`. Its public headers expose our own types (`Address`,
  `UdpSocket`, `TcpListener`, `TcpConnection`), never `asio::` types. The client and the server link
  `rtype::network`, never Asio.
- The standalone version is used (`ASIO_STANDALONE`), not Boost.Asio: no Boost in the project.
- Asio's TLS support (`asio::ssl`) is **not** used: it requires OpenSSL. The encryption uses Mbed TLS behind
  our own wrapper ([ADR 0014](0014-encryption.md)), with Asio only moving the encrypted bytes.
- `libs/network` knows nothing about R-Type: sockets, serialization, framing, reliability, sessions and
  encryption are generic. The R-Type messages live in a separate library shared by the client and the server.
- Each program decides which thread runs the Asio event loop. The server never runs the game simulation on
  that thread ([ADR 0013](0013-one-process-per-game.md)).
- Messages go through the logging library (`rtype::log::Logger`, subsystem `network`), like the rest of the
  code.

## Consequences

- One code base for Linux and Windows. Asio provides the event loop, the timers and the error handling.
- Asio's asynchronous style (handlers called by `io_context::run()`) has a learning cost. The wrapper keeps
  it inside `libs/network` and offers a simpler interface to the rest of the code.
- Reliability, sessions and encryption of the datagrams are ours to write and test. The work is split into
  the issues of the "Server & networking" epic.
- If the wrapper is respected, Asio could be replaced by system sockets later without touching the client
  or the server.
