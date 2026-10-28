# 0023 — SOCI over SQLite and PostgreSQL for the server data

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-28 |

## Context

The server will keep data from one run to the next. What it keeps is not known yet (results, scores,
players...): each feature will bring its own tables. What is known is how it runs:

- **On its own**, on a classmate's machine, a LAN, or the host of the team: the data stays next to the server,
  with nothing to install nor to run beside it.
- **With a database hosted elsewhere**, a managed PostgreSQL for example: given its address, the server uses
  it, and several servers can share it.
- **One code**: the queries, the schema and its evolution are written once, whatever runs them.
- The client and the server build on Linux and Windows, with their dependencies fetched by CPM
  ([ADR 0001](0001-cpm-package-manager.md)).
- Only the master touches the data: the instances report to it ([ADR 0013](0013-one-process-per-game.md)), so
  a game that crashes cannot damage it. The master runs one event loop, which a query to a remote database must
  not block.
- No secret on the command line, which `ps` shows to anyone, nor in an environment variable
  ([ADR 0020](0020-server-key-distribution.md)): a password to the database is no exception.

## Options

### Where the data lives

| | A. PostgreSQL only | B. PostgreSQL only, started by the server | C. SQLite only | D. SQLite locally, PostgreSQL remotely | E. libSQL |
|---|---|---|---|---|---|
| Locally | A PostgreSQL to install and run beside the server | The server runs `initdb`, starts `postgres` and stops it | One file | One file | One file |
| Remotely | Yes | Yes | No: a database file on a network share gets corrupted | Yes | Its own server, `sqld`, or its cloud |
| On the host | PostgreSQL | The PostgreSQL binaries, a data directory tied to their major version | Nothing | Nothing locally; libpq for a remote database | Nothing locally |
| Build | libpq | libpq | One C file | One C file, libpq for PostgreSQL | A Rust library: the Rust toolchain in the build, Windows included |
| Maturity | Decades | Decades, but supervising it is ours to write | Decades | Decades | Young, being rewritten by its authors |

### How the code reaches it

| | Two engines, by hand (sqlite3 and libpq C APIs) | SOCI | sqlpp11 | ODBC |
|---|---|---|---|---|
| Code to write | Every query twice, or our own abstraction over both | Once: plain SQL, values bound to it | Once: queries as C++ expressions | Once: plain SQL |
| Engines | Ours to keep equal | SQLite, PostgreSQL, MySQL, Oracle... behind one `session` | SQLite, PostgreSQL, MySQL | Any with a driver |
| Build | The two C libraries | CMake, fetched by CPM; SQLite from its single C file | Header-heavy templates | A driver manager and a driver installed on every host |
| Maturity | — | Since 2004, version 4.1.4 (2026) | Active, its successor sqlpp23 under way | Decades |

SOCI does not translate SQL: the queries must be written in the SQL both engines understand.

## Decision

**D, through SOCI: SQLite by default, PostgreSQL given its address, in a library of its own, `libs/database`.**
It holds no table of the game: it opens a database, runs the migrations of the program, and runs queries on a
thread of its own.

- **One address chooses the engine**: `sqlite:<file>` (by default `sqlite:rtype.db`, in the current directory,
  next to `server.key`), or `postgresql://user@host:5432/rtype?sslmode=verify-full`, in the format libpq reads.
  An address with a password is read from a file (`--database-file`, a Docker secret), or the password comes
  from libpq's own `~/.pgpass`.
- **The code writes SQL once**, through the `soci::session` the library gives, in the SQL both engines share:
  `BIGINT` ids created with `INSERT ... RETURNING id`, `INSERT ... ON CONFLICT ... DO UPDATE`, dates as Unix
  milliseconds in a `BIGINT`, booleans as `INTEGER` 0 or 1, `TEXT` strings. SOCI is not wrapped: it already is
  the layer over the engines.
- **Migrations**: the program gives the list of its schema changes, numbered. The database keeps its version in
  a table, and the missing changes are applied when it opens, each in a transaction. A database newer than the
  program is refused, rather than misused.
- **A thread of its own**: the master gives it tasks, which run there, and gets their results back on its event
  loop. A connection lost is opened again, with a growing delay.
- **Dependencies**: SOCI and the single C file of SQLite through CPM, with a hash. PostgreSQL needs libpq from
  the system: it is built when found (`RTYPE_WITH_POSTGRESQL`), and a server built without it refuses a
  `postgresql://` address, saying why.
- **A library**: it is used by the server only, but it wraps SOCI, SQLite and libpq, which nothing else must
  see.

## Consequences

- A server runs alone with a file, or with any PostgreSQL, without a line of its code changing. Several servers
  can share one PostgreSQL.
- Every query is tested on both engines: SQLite always, PostgreSQL when a test database is given, which the CI
  does with a PostgreSQL container. A query that only one engine understands fails there.
- Two engines still behave differently where SQL leaves it open: types checked loosely by SQLite, the order of
  rows without `ORDER BY`, text comparisons. The rules above, and the tests on both, are what keeps them equal.
- A Windows server without libpq installed only has SQLite. The Linux release and the Docker image have both.
- The settings of the client are not concerned: they stay a `key=value` file
  ([Settings](../architecture/settings.md)).
- Next: the library and its SQLite engine, its thread, PostgreSQL and the CI, then the options of the server.
  The first tables come with the first feature that keeps data.
