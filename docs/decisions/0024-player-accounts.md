# 0024 — Player accounts, per server, all or nothing

| | |
|---|---|
| **Status** | Accepted; its sign-in with Discord superseded by [0026](0026-discord-social-sdk.md) |
| **Date** | 2026-10-28 |

## Context

A player is a name, typed at each connection: anyone can take anyone's, nothing follows a player from one
session to the next. The features to come need to know who plays: the administration of a server
([ADR 0025](0025-server-administration.md)), and later scores, statistics or progression kept in the database
([ADR 0023](0023-server-database.md)).

- **Anyone can run a server**, with its own database; the team runs the official one, and has a mail server for
  it. A classmate on a LAN has neither a mail server nor an OAuth application.
- **Players sign in with an email and a password, or with Discord**; Google can come later.
- **Players have no website**: everything that concerns them happens in the client. The web server of a master
  serves its administrators only.
- **A forgotten password must be recoverable**, which takes a mail server: an account that cannot be recovered
  is lost.
- **Guests keep playing** as today, with a name and no account.
- **No secret on the command line nor in an environment variable** ([ADR 0020](0020-server-key-distribution.md)).

## Options

### Where the accounts live

| | A. Per server, in its database | B. One central service of the team |
|---|---|---|
| An account works on | That server | Every server that trusts the service |
| A server needs | Its own mail server and OAuth application | Nothing but the service |
| Servers of others | Independent | Depend on a service of the team, which sees their players |
| Work | The accounts in the master | A service of its own to host, secure and keep up, and a trust between it and every server |

### When accounts are enabled

| | A. Each part works on its own | B. All or nothing, per sign-in method |
|---|---|---|
| A server without a mail server | Accounts that cannot verify an email nor recover a password | No accounts: guests only |
| A server with a mail server, without Discord | — | Email and password accounts, no Discord button |
| A broken configuration | Found by the first player it fails | Found at start: the server does not start |

### How a password is kept

| | SHA-256, salted | PBKDF2 (in TF-PSA-Crypto) | bcrypt | Argon2id |
|---|---|---|---|---|
| Cost of a guess for an attacker with the database | Billions per second on a GPU | Slowed by iterations, cheap on a GPU | Slowed, little memory | Slowed, and costly in memory, which GPUs lack |
| Recommendation (OWASP) | Never | Acceptable, with many iterations | Acceptable | First choice |

### How a player signs in with Discord

| | A. The client receives the answer of Discord on a local port | B. The web server of the master receives it |
|---|---|---|
| Redirect address | `http://127.0.0.1:<port>/...`, registered with its port in the Discord application: the port can be taken | `https://<public address of the master>/auth/discord/callback` |
| Needs | A small HTTP server in the client | The web server of the master reachable from the Internet, behind the proxy of the host |
| What the player sees | The browser, then the game | The browser, a page "signed in, back to the game", then the game |

## Decision

**Accounts per server (A), all or nothing per sign-in method (B), passwords hashed with Argon2id, Discord
answered by the web server of the master (B).**

### Enabled or not

- **Accounts are disabled by default**: the server works as today, with guests only, and an administration
  panel reachable from the machine only ([ADR 0025](0025-server-administration.md)).
- **Email and password accounts** need the database and a **mail server**. Enabled, the master checks the mail
  server at start (connection, TLS, authentication, nothing sent), as it checks its key and its database: a
  server whose mail server does not answer **does not start**, and says why. `--check` does the same.
- **Discord** is enabled only when its application is fully configured (client id, secret, public address of
  the web server), and checked at start in the same way. Google, later, the same.
- **Guests stay**: a server with accounts lets players in without one, unless its configuration forbids it. A
  guest cannot take the name of an account of the server, and is shown as a guest.

### Configuration

The settings of the master that do not fit a command line go in a **TOML file** given to it: the accounts, the
mail server, Discord, the web server. Secrets are never in it: it gives the path of a file per secret (the
password of the mail server, the secret of the Discord application), only readable by the server's user, a
Docker secret for instance. The file is read with toml++ (header-only, MIT, through CPM).

### Accounts and sessions

- An account has a **username**, unique on the server, shown in game; an **email**, verified, for a password
  account; a **password hash**, or none for a Discord-only account; its **Discord identity**, if linked; a
  **role**, `player` or `admin`; and can be **banned**.
- **Passwords** are hashed with **Argon2id**, from the reference implementation of the Password Hashing
  Competition (a few C files, built by CMake from CPM), with at least the parameters OWASP recommends (19 MiB,
  2 passes); the hash is kept in the standard `$argon2id$v=19$m=...` form, so that the parameters can grow
  without breaking the existing hashes. Hashing takes tens of milliseconds and that memory: it runs on a worker
  thread, never on the event loop, and at most a few at once.
- **Emails carry a code**, not a link: 6 digits, valid 15 minutes, a few attempts, typed in the client. It
  verifies the address of a new account, and resets a forgotten password.
- **A session** is a random token of 256 bits, given at sign-in and kept by the client in its settings for this
  server; the database keeps its SHA-256 only, so that a copy of the database opens no session. A session
  expires after 30 days without use, and every session of an account ends when its password changes, or when
  it is banned.
- **Sign-in attempts are limited**, per account and per address.

### Sign-in with Discord

1. The client asks the master to sign in with Discord, through the lobby.
2. The master answers with the address of the authorization page of Discord, with a state of its own, bound to
   that lobby connection; the client opens it in the browser.
3. Discord sends the browser back to the web server of the master, which exchanges the code for the identity
   of the player with the secret of the application (HTTPS, libcurl), and shows a page asking to go back to
   the game.
4. The master signs the lobby connection in: an existing account linked to that Discord identity, or a new
   account whose username the player chooses in the client.

### What the client learns

The welcome of the lobby tells what the server offers: guests allowed, password accounts, Discord. The client
shows only what is offered; a server without accounts looks like today. The new messages of the lobby
(register, sign in, verify, reset, sign out, sign in with Discord) raise the version of the lobby protocol.

### Dependencies

- **libcurl**, built through CPM, for the mail server (SMTP with TLS) and the requests to Discord (HTTPS). Its
  TLS is the system's: OpenSSL on Linux, Schannel on Windows. No release of curl found supports Mbed TLS 4,
  which the project uses for its own protocols.
- **Argon2** reference implementation and **toml++**, through CPM.

## Consequences

- A server of a classmate keeps working with nothing to set up; the official server gets accounts with its mail
  server and its Discord application.
- An account is valid on one server only: a player of two servers has two accounts. A central service stays
  possible later, in a new record, if the team wants accounts shared across servers.
- A server with accounts depends on its mail server: if it goes down while the server runs, codes cannot be
  sent until it is back, and players are told so; sign-in with a password or a session still works.
- Discord needs the web server of the master reachable from the Internet, which the host provides
  ([ADR 0025](0025-server-administration.md)).
- The lobby protocol, the client, the database (accounts, sessions, codes, Discord identities) and the
  configuration all change: the plan of this work splits it into steps that can each be merged on their own.
- Three dependencies more: libcurl, Argon2, toml++. On Linux, the server needs OpenSSL, which libpq already
  brings.
