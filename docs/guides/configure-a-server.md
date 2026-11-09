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
    game_port = 4242                              # UDP: every game, the same number as the lobby
    max_games = 57
    key = "/data/server.key"
    database_file = "/run/secrets/database_url"   # or: database = "sqlite:/data/rtype.db"

    [web]
    listen = "127.0.0.1:8080"                     # the default; "0.0.0.0:8080" in a container, behind the proxy
    public_url = "https://admin.rtype.example.org"  # the panel from outside
    trusted_proxies = ["172.16.0.0/12"]           # whose X-Forwarded-For is believed
    panel_dir = "/opt/r-type/admin"               # the default: admin/ next to the server

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
    social = false            # true: the players get their Discord token, for the voice of Discord

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
| | `game_port` | 4242 | UDP port of every game, as `--game-port`; 0 lets the system choose. The same number as `port` is fine: one is TCP, the other UDP |
| | `max_games` | 57 | Most games at once, from 1 to 1000, as `--max-games` |
| | `key` | `server.key` | Private key of the server, as `--key` |
| | `database` | `sqlite:rtype.db` | Where the master keeps its data, as `--database`; a relative SQLite file is next to the settings file |
| | `database_file` | | A file holding the address of the database, as `--database-file`. Not with `database` |
| `[web]` | `listen` | `127.0.0.1:8080` | Where the administration panel listens: `<ip>:<port>`, `[<ipv6>]:<port>`. Without accounts, a loopback address only (`127.0.0.1`, `[::1]`): nobody signs in to the panel without them. Port 0 takes a free port |
| | `public_url` | | Where the panel is reached from outside, `https://...`. The players sign in with Discord without it ([ADR 0026](../decisions/0026-discord-social-sdk.md)); the administrators need it to sign in to the panel with Discord, which sends them back to `<public_url>/auth/discord/callback`. In `https://`, every cookie of the panel is `Secure`. The live events of the panel accept a page of its origin, beside the address the browser asked |
| | `trusted_proxies` | `[]` | The proxies whose `X-Forwarded-For` is believed: addresses, or `<ip>/<prefix>` |
| | `panel_dir` | `admin/` next to the server | The folder of the built panel. Given and missing, the server does not start; by default and missing, the panel says it is not built |
| `[accounts]` | `enabled` | `true` | Accounts with an email and a password. Needs `[mail]` |
| | `guests` | `true` | Whether players can still play without an account |
| `[mail]` | `host` | | The mail server the codes are sent through. Required |
| | `security` | `"starttls"` | `"starttls"`, `"tls"` from the start, or `"none"`: in the clear, for a relay on this machine or a test server such as Mailpit, and then without `username` |
| | `port` | 587; 465 with `"tls"`, 25 with `"none"` | Port of the mail server |
| | `username`, `password_file` | | The account on the mail server, and the file of its password; both, or neither |
| | `from` | | The sender of the mails, `R-Type <rtype@example.org>` or `rtype@example.org`: its address is also the sender SMTP announces. Required |
| `[discord]` | `client_id` | | The id of the Discord application, its digits. Required |
| | `client_secret_file` | | The file of its secret. Required |
| | `social` | `false` | The players who sign in with Discord get their Discord token, which the Social SDK of the game needs for the voice and the friends of Discord ([ADR 0034](../decisions/0034-discord-token-in-the-client.md)): the server asks `identify openid sdk.social_layer` instead of `identify` ([Set up accounts](set-up-accounts.md#the-social-features-of-discord)) |
| `[admin]` | `admins` | `[]` | The administrators, each an email or `discord:<id>`. Never a username, which anyone could take by registering it first. Administrators of the panel whatever their role, once their account exists, its email verified; given the role `admin` at every start. The panel can neither demote, ban nor delete them |

The sections go together:

- `[accounts]` needs `[mail]`, to send the codes, and `[mail]` needs `[accounts]`. Accounts with `enabled =
  false` keep their `[mail]` without using it.
- `[discord]` needs `[accounts]`: Discord signs in to accounts.

The server serves its panel with `[web]` ([Run a server](run-a-server.md#steps), step 4). With accounts
enabled, it checks its mail server at every start, `--check` included, without sending anything
([Set up accounts](set-up-accounts.md)), then lets its players create accounts and sign in. With `[discord]`,
it checks its Discord application the same way, and the players sign in with Discord too
([Set up accounts](set-up-accounts.md#sign-in-with-discord)). With accounts, the administrators sign in to the
panel with their account, those of `[admin]` first, and the panel may listen beyond the loopback, behind a proxy
([Administer a server](administer-a-server.md#from-anywhere-with-an-account)). Without accounts, `[admin]` is
checked, and not used.

## Result

The server reads its settings at start, `--check` included, and stops with code 1 when they are wrong:

| Message | Fix |
|---|---|
| `Cannot read the settings file server.toml` | The path is wrong, or the server's user cannot read it |
| `server.toml:3:1: unknown key 'prot' in [server]` | A typo: unknown keys and sections are refused, so that none goes unnoticed |
| `server.toml:2:8: [server] port must be a number` | A value of the wrong type: `4242`, not `"4242"` |
| `[mail] password_file: cannot read /run/secrets/mail_password` | The secret file is missing, or the server's user cannot read it |
| `[accounts] needs a [mail] section, to send the codes` | Add `[mail]`, or remove `[accounts]` |
| `[mail] from must hold the address of the sender` | Write the sender `Name <address>`, or the address alone |
| `[mail] security = "none" would send the password in the clear` | Use `"starttls"` or `"tls"` with an account on the mail server |
| `Cannot use the mail server mail.example.org:587: Login denied` | The mail server refused the account: see [Set up accounts](set-up-accounts.md#result) for this message and the others |
| `[web] listen must be a loopback address (127.0.0.1, [::1]) without accounts` | Keep the panel on this machine, or enable the accounts |
| `Cannot serve the panel on 127.0.0.1:8080: Address already in use` | Another program, or another server, uses that port: choose another `listen` |
| `The folder of the panel /opt/r-type/admin does not exist` | Give the folder of the built panel in `panel_dir`, or remove the key |
| `[admin] admins: "pavel" is neither an email nor discord:<id>` | Name the administrator by their email or their Discord id |

Every option of the command line is in [Server](../architecture/server.md#master).
