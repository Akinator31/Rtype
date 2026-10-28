# How to use an external database

This guide gives a server a PostgreSQL database hosted elsewhere, in place of the SQLite file it uses by
default, without its password ever on the command line. It is for whoever hosts a server; several servers can
share one database this way.

Without it, a server keeps its data in `rtype.db`, in its current directory, and needs nothing else
([ADR 0023](../decisions/0023-server-database.md)).

## Prerequisites

- A server built with PostgreSQL: its configure said `Database engines: SQLite, PostgreSQL`
  ([Building with PostgreSQL](../architecture/database.md#building-with-postgresql)). On Linux, it needs libpq to
  run: `sudo apt install libpq5` on Debian and Ubuntu, `postgresql-libs` on Arch.
- A PostgreSQL server the machine of the R-Type server can reach, version 9.5 or newer.
- The right to create a user and a database on it, or someone who has it.

## Steps

1. **Make a user and a database for R-Type**, on the PostgreSQL server, as its administrator:

    ```sql
    CREATE USER rtype WITH PASSWORD 'a long random password';
    CREATE DATABASE rtype OWNER rtype;
    ```

    The server creates its tables itself, at its first start: the user only needs to own its database.

2. **Write the address of the database in a file only the server's user can read**, on one line:

    ```
    postgresql://rtype:a%20long%20random%20password@db.example.org:5432/rtype?sslmode=verify-full
    ```

    ```sh
    chmod 600 database.txt
    ```

    | Part | Meaning |
    |---|---|
    | `rtype:...@` | The user and its password. Characters such as `@`, `:`, `/` or a space are written `%40`, `%3A`, `%2F`, `%20` |
    | `db.example.org:5432` | The PostgreSQL server and its port |
    | `/rtype` | The database |
    | `sslmode=verify-full` | Encrypts the connection and checks the certificate of the server. Over the Internet, always; `disable` only on a network you trust |

    In Docker, the file is a secret, mounted in `/run/secrets/`. The password can also stay out of the
    address, in the `~/.pgpass` file of the server's user, which libpq reads.

3. **Check that the server reaches it**:

    ```sh
    ./build/bin/r-type_server --database-file database.txt --check
    ```

    ```
    INFO  master   Database postgresql://rtype:***@db.example.org:5432/rtype?sslmode=verify-full, schema 0
    INFO  master   Check passed: the server can start
    ```

    The logs never show the password. The check creates the tables of the server, as a start would.

4. **Start the server** with the same option:

    ```sh
    ./build/bin/r-type_server --database-file database.txt
    ```

## Result

The server keeps its data in PostgreSQL. If the connection is lost while it runs, it connects again on its
own, waiting longer between each attempt, up to 30 seconds; the players keep playing meanwhile
([The database thread](../architecture/database.md#the-database-thread)). A server that cannot reach its
database at start does not start, and says why:

| Message | Fix |
|---|---|
| `this program is built without PostgreSQL` | Build it with libpq ([Building with PostgreSQL](../architecture/database.md#building-with-postgresql)) |
| `Connection refused`, `timeout expired` | The address or the port is wrong, or a firewall blocks it. The server waits 10 seconds, or the `connect_timeout` of the address |
| `password authentication failed for user "rtype"` | The user or the password is wrong, or a character of the password is not written as `%..` |
| `database "rtype" does not exist` | Create it (step 1), or fix the end of the address |
| `... does not hold the address of a database on its first line` | The file is empty, or its first line is not a `postgresql://` address |
| `... has migration N (...), which this program does not know` | A newer server changed this database: run that version, or a server of the same version |

Every option is listed by `r-type_server --help` and in [Server](../architecture/server.md#the-database).
