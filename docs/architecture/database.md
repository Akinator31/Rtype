# Database

`libs/database` (`rtype::database`) keeps the data a program needs from one run to the next, in SQLite or in
PostgreSQL, through [SOCI](https://soci.sourceforge.net) ([ADR 0023](../decisions/0023-server-database.md)). It
holds no table of the game: it opens a database, applies the migrations a program gives it, and gives the SOCI
session to run queries. The server is its user; the settings of the client are another system
([Settings](settings.md)).

| | |
|---|---|
| Engines | SQLite 3.54.1, built from its single C file, on every platform. PostgreSQL: not built yet. |
| Dependencies | SOCI 4.1.4 (public: its session is in the API), fmt 12.2.0 (SOCI's), SQLite: all through CPM, with a hash |
| Threads | A `Database` is used by one thread at a time |

## Opening a database

A database is given by its address, as the user writes it:

| Address | Engine |
|---|---|
| `sqlite:rtype.db`, `sqlite:/data/rtype.db` | A SQLite file, created with its directories if missing. A file name may contain spaces. |
| `postgresql://rtype@db.example.org:5432/rtype?sslmode=verify-full`, `postgres://...` | A PostgreSQL server, in the format libpq reads. Refused for now: this build has no PostgreSQL. |

```cpp
#include "rtype/database/Database.hpp"

using rtype::database::Address;
using rtype::database::Database;

const Address address = Address::parse(text);   // std::invalid_argument: not an address
Database database = Database::open(address);    // rtype::database::Error: cannot be opened
logger.info("Database {}", address.toString()); // without its password, if it has one
```

`Database::open()` throws `rtype::database::Error`, whose message is for the user, if the database cannot be
opened: a directory that cannot be created, a file that is not a SQLite database (it is left as it is), an
engine this program is built without. A SQLite database waits 5 seconds for a lock held by another connection,
and checks foreign keys, as PostgreSQL does.

## Migrations

The schema of a program changes with it: each change is a **migration**, numbered from 1, that the program
gives in order. The database keeps those it applied in the table `schema_migrations` (version, description,
date), and `migrate()` applies the others:

```cpp
const std::vector<rtype::database::Migration> Migrations{
    {1, "create the players table",
     {"CREATE TABLE players (id BIGINT PRIMARY KEY, name TEXT NOT NULL UNIQUE, created_at BIGINT NOT NULL)"}},
    {2, "create the games table",
     {"CREATE TABLE games (id BIGINT PRIMARY KEY, player_id BIGINT NOT NULL REFERENCES players (id))",
      "CREATE INDEX games_by_player ON games (player_id)"}},
};

const int applied = database.migrate(Migrations); // 0 when the database is up to date
```

- **Each migration runs in a transaction**, with the row that records it: a migration that fails leaves
  nothing of itself, and the database stays at the previous version.
- **One statement per string**: SQLite runs only the first statement of a string.
- **A migration is never changed once merged**: a database that applied it would not apply it again. A change
  is a new migration.
- **What the database has must match the program**, or `migrate()` throws `Error`: a migration the program does
  not know (the database was used by a newer program), or known by another description (two branches each
  added their own migration under the same number).
- Migrations numbered otherwise than 1, 2, 3..., or without a description or statements, throw
  `std::invalid_argument`: a bug of the program, found by its tests.

`schemaVersion()` gives the version of the database: its last migration, 0 for a new database.

## Queries

The queries go through the SOCI session, with their values **bound**, never pasted into the SQL: a value cannot
change the query, whatever it contains.

```cpp
soci::session &sql = database.session();

// Writing
sql << "INSERT INTO players (id, name, created_at) VALUES (:id, :name, :created_at)",
    soci::use(id), soci::use(name), soci::use(createdAt);

// Reading one row: a column that can be NULL needs an indicator
long long best = 0;
soci::indicator found = soci::i_ok;
sql << "SELECT MAX(score) FROM games WHERE player_id = :id", soci::use(id), soci::into(best, found);

// Reading rows
const soci::rowset<soci::row> rows = (sql.prepare << "SELECT id, name FROM players ORDER BY name");
for (const soci::row &row : rows) {
    show(row.get<long long>(0), row.get<std::string>(1));
}
```

Several changes that go together go in a transaction: all of them are kept, or none.

```cpp
database.transaction([&] {
    sql << "INSERT INTO games ...";
    sql << "UPDATE players ...";
}); // committed; an exception rolls it back and goes on
```

A query that fails throws `soci::soci_error`.

## Portable SQL

SOCI runs the SQL it is given: it does not translate it. Write the SQL both engines understand:

| For | Write | Not |
|---|---|---|
| Ids | `BIGINT PRIMARY KEY`, the new one read with `INSERT ... RETURNING id` | `SERIAL` (PostgreSQL), `AUTOINCREMENT` (SQLite) |
| Insert or update | `INSERT ... ON CONFLICT (id) DO UPDATE SET x = excluded.x` | `INSERT OR REPLACE` (SQLite), `MERGE` |
| Dates | Unix milliseconds in a `BIGINT`, read as `long long` | `TIMESTAMP`, `datetime()`, `NOW()` |
| Booleans | `INTEGER`, 0 or 1 | `BOOLEAN` |
| Text | `TEXT` | `VARCHAR(n)`: SQLite ignores the length |
| Integers in C++ | `long long` for a `BIGINT` | `int`, which a large value overflows |
| Order | `ORDER BY`, whenever the order matters | Relying on the order rows come in |

The tests of the queries run on every engine available ([Tests](#tests)): what one engine refuses fails there.

## Tests

`tests/database.cpp` (tag `[database]`) runs on SQLite, in a directory of its own per test:

- addresses: the forms accepted, those refused, a password never shown;
- opening: a file created with its directories, a name with spaces, a file that is not a database refused and
  left as it is, PostgreSQL refused by this build without showing the password;
- migrations: applied in order and once, a new one applied alone, one that fails leaving nothing, a newer
  database refused, a migration known by another description refused, a numbering error;
- transactions: committed, or rolled back by an exception;
- portable SQL: `RETURNING`, `ON CONFLICT`, 64-bit dates, foreign keys checked, a `NULL` read with an indicator.
