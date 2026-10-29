# How to configure a server

This guide gives a server its settings in one file, in place of a long command line: its ports, its key, its
database, and the settings of the administration panel and the accounts. It is for whoever hosts a server. To
start one and open its ports, see [Run a server](run-a-server.md).

Without a settings file, a server runs with its defaults and the options of its command line, and needs nothing
else.

## Prerequisites

- A server that runs: see [Run a server](run-a-server.md).
- A folder the server's user can read, for the settings file and the files of its secrets.

## Steps

1. **Write the settings file**, `server.toml`, in [TOML](https://toml.io). Every section and every key is
   optional; here is the whole file:

    ```toml
    # server.toml
    [server]
    port = 4242
    game_ports = "4243-4299"
    key = "/data/server.key"
    database_file = "/run/secrets/database_url"   # or: database = "sqlite:/data/rtype.db"

    [web]
    listen = "127.0.0.1:8080"                     # the default; "0.0.0.0:8080" in a container, behind the proxy
    public_url = "https://admin.rtype.example.org"  # needed by Discord
    trusted_proxies = ["172.16.0.0/12"]           # whose X-Forwarded-For is believed

    [accounts]
    enabled = true
    guests = true

    [mail]
    host = "mail.example.org"
    port = 587
    security = "starttls"                         # or "tls" (port 465)
    username = "rtype@example.org"
    password_file = "/run/secrets/mail_password"
    from = "R-Type <rtype@example.org>"

    [discord]
    client_id = "123456789012345678"
    client_secret_file = "/run/secrets/discord_secret"

    [admin]
    admins = ["pavel@example.org", "discord:123456789012345678"]   # always admins
    ```

    Relative paths are relative to the folder of the settings file, not to the current directory: `key =
    "server.key"` is next to `server.toml`. On Windows, a path that starts with `/` or `\` is kept as written,
    on the current drive, as on the command line.

2. **Put each secret in a file of its own**, only readable by the server's user. A secret is the first line of
   its file, the spaces at its end cut; it is never logged, and never written in the settings file itself:

    ```sh
    printf '%s\n' 'the password of the mail account' > /run/secrets/mail_password
    chmod 600 /run/secrets/mail_password
    ```

    On Linux and macOS, the server warns about a secret file every user of the machine can read. In Docker, the
    secrets are mounted in `/run/secrets/`.

3. **Check the settings**:

    ```sh
    ./build/bin/r-type_server --settings server.toml --check
    ```

    ```
    INFO  server   Settings server.toml
    INFO  master   Database sqlite:/data/rtype.db, schema 0
    INFO  master   Check passed: the server can start
    ```

    A file with a mistake is refused, with its line and its column, and the server does not start:

    ```
    ERROR server   server.toml:3:1: unknown key 'prot' in [server]
    ```

4. **Start the server with it**, adding options on the command line if needed: they win over the file.

    ```sh
    ./build/bin/r-type_server --settings server.toml --port 5000
    ```

## The keys

| Section | Key | Default | Meaning |
|---|---|---|---|
| `[server]` | `port` | 4242 | TCP port of the lobby, as `--port`; 0 lets the system choose |
| | `game_ports` | `"4243-4299"` | UDP ports of the games, as `--game-ports` |
| | `key` | `server.key` | Private key of the server, as `--key` |
| | `database` | `sqlite:rtype.db` | Where the master keeps its data, as `--database`; a relative SQLite file is next to the settings file |
| | `database_file` | | A file holding the address of the database, as `--database-file`. Not with `database` |
| `[web]` | `listen` | `127.0.0.1:8080` | Where the administration panel listens: `<ip>:<port>`, `[<ipv6>]:<port>`. Without accounts, a loopback address only (`127.0.0.1`, `[::1]`): nobody signs in to the panel without them. Port 0 takes a free port |
| | `public_url` | | Where the panel is reached from outside, `https://...`; needed by Discord |
| | `trusted_proxies` | `[]` | The proxies whose `X-Forwarded-For` is believed: addresses, or `<ip>/<prefix>` |
| `[accounts]` | `enabled` | `true` | Accounts with an email and a password. Needs `[mail]` |
| | `guests` | `true` | Whether players can still play without an account |
| `[mail]` | `host` | | The mail server the codes are sent through. Required |
| | `security` | `"starttls"` | `"starttls"`, or `"tls"` from the start |
| | `port` | 587, or 465 with `"tls"` | Port of the mail server |
| | `username`, `password_file` | | The account on the mail server, and the file of its password; both, or neither |
| | `from` | | The sender of the mails, `R-Type <rtype@example.org>`. Required |
| `[discord]` | `client_id` | | The id of the Discord application. Required |
| | `client_secret_file` | | The file of its secret. Required |
| `[admin]` | `admins` | `[]` | The administrators, each an email or `discord:<id>`. Never a username, which anyone could take by registering it first. The panel can neither demote nor ban them |

The sections go together:

- `[accounts]` needs `[mail]`, to send the codes, and `[mail]` needs `[accounts]`. Accounts with `enabled =
  false` keep their `[mail]` without using it.
- `[discord]` needs `[accounts]`, and `public_url` in `[web]`, where Discord sends the players back.

The panel, the accounts, the mail and Discord come in the next versions of the server: until then, their
sections are checked, and not used yet.

## Result

The server reads its settings at start, `--check` included, and stops with code 1 when they are wrong:

| Message | Fix |
|---|---|
| `Cannot read the settings file server.toml` | The path is wrong, or the server's user cannot read it |
| `server.toml:3:1: unknown key 'prot' in [server]` | A typo: unknown keys and sections are refused, so that none goes unnoticed |
| `server.toml:2:8: [server] port must be a number` | A value of the wrong type: `4242`, not `"4242"` |
| `[mail] password_file: cannot read /run/secrets/mail_password` | The secret file is missing, or the server's user cannot read it |
| `[accounts] needs a [mail] section, to send the codes` | Add `[mail]`, or remove `[accounts]` |
| `[web] listen must be a loopback address (127.0.0.1, [::1]) without accounts` | Keep the panel on this machine, or enable the accounts |
| `[admin] admins: "pavel" is neither an email nor discord:<id>` | Name the administrator by their email or their Discord id |

Every option of the command line is in [Server](../architecture/server.md#master).
