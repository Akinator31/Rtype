# How to set up accounts

This guide gives the players of a server their accounts: the mail server their codes are sent through, what the
players can then do, and what the server keeps of them. It is for whoever hosts a server. Discord comes in a next
version of the server ([#259](https://github.com/Akinator31/Rtype/issues/259)).

A server without `[accounts]` needs none of this: its players play as guests.

## Prerequisites

- A server and its settings file ([Configure a server](configure-a-server.md)).
- An account on a mail server that sends for the address the mails come from: the SMTP of the mail host of your
  domain, or a sending service. The server sends a few mails per player: a code when they register, when they
  forget their password.
- The port of that mail server open from the machine of the server, outgoing: 587 (STARTTLS) or 465 (TLS). Many
  hosts block 25.

## Steps

1. **Write the password of the mail account in a file only the server's user can read**, on one line:

    ```sh
    printf '%s\n' 'the password of the mail account' > /run/secrets/mail_password
    chmod 600 /run/secrets/mail_password
    ```

2. **Add the accounts and the mail server to the settings**:

    ```toml
    [accounts]
    enabled = true
    guests = true

    [mail]
    host = "mail.example.org"
    port = 587
    security = "starttls"                    # or "tls" on port 465
    username = "rtype@example.org"
    password_file = "/run/secrets/mail_password"
    from = "R-Type <rtype@example.org>"
    ```

    | Key | Meaning |
    |---|---|
    | `host`, `port` | The mail server, 587 by default |
    | `security` | `"starttls"`: a connection in the clear made secure with STARTTLS, required; a server that does not offer it is refused. `"tls"`: TLS from the start, port 465. The certificate of the mail server is checked against those of the system |
    | `username`, `password_file` | The account on the mail server, and the file of its password |
    | `from` | The sender the players see. Its address is also the one given to the mail server: it must be allowed to send for it |

    The other keys are in [Configure a server](configure-a-server.md#the-keys).

3. **Check the mail server**, without sending anything:

    ```sh
    ./build/bin/r-type_server --settings server.toml --check
    ```

    ```
    INFO  master   Mail server mail.example.org:587, mails from R-Type <rtype@example.org>
    INFO  master   Check passed: the server can start
    ```

    The server connects, starts TLS, signs in with the account, and leaves: no mail is sent. It does the same at
    every start.

### To try without a real mail server

[Mailpit](https://mailpit.axllent.org) keeps the mails it receives and shows them in a browser, which is enough
to see the mails of the server while developing:

```sh
docker run --rm -p 127.0.0.1:1025:1025 -p 127.0.0.1:8025:8025 axllent/mailpit
```

```toml
[mail]
host = "127.0.0.1"
port = 1025
security = "none"
from = "R-Type <rtype@example.org>"
```

`security = "none"` sends in the clear, and takes no `username`: a password would travel unencrypted. The server
warns about it at start. Keep it for a mail server on the same machine, or for tests.

## Result

A server with accounts reaches its mail server at every start, or does not start, and says why:

| Message | Fix |
|---|---|
| `Cannot use the mail server mail.example.org:587: Login denied` | The username or the password is wrong, or the account may not send through SMTP |
| `Couldn't connect to server`, `Timeout was reached` | The host or the port is wrong, or a firewall blocks the port: many hosts block 25, some 465 |
| `SSL connect error`, `SSL peer certificate or SSH remote key was not OK` | The mail server has no valid certificate for its name: give the name of its certificate as `host` |
| `Requested SSL level failed` | The mail server does not offer STARTTLS: use `"tls"` on its port 465 |
| `Couldn't resolve host name` | The host is wrong, or the machine has no DNS |
| `The mails go to 127.0.0.1 in the clear` | A warning: `security = "none"` |

!!! warning "In a container"

    A mail server down at start stops the server: in Docker, with `restart: unless-stopped`, the container then
    restarts again and again until the mail server answers. The logs of the container say why, at each start.

While it runs, a mail the server refuses or cannot be reached for is tried again after 30 seconds, 1 minute, then
2 minutes, then given up, and the logs say so. One address receives at most 5 mails an hour, so that nobody can
flood the mailbox of another by typing their email. The logs show each mail by its subject and a masked address,
`p…@example.org`, never its code. How the server sends them is in [Accounts](../architecture/accounts.md#the-mails).

## What the players can do

Once the server starts with `[accounts]`, its players can, from the Online menu of the game
([Play online](play-online.md)):

- **create an account** with a username, an email and a password: a code of 6 digits reaches the email, valid
  15 minutes, which they type in the game. An account not verified within 24 hours is deleted;
- **sign in** with their username or their email, and **stay signed in** on that server for 30 days without
  playing, each game pushing it back;
- **get a new password** with a code sent to their email, which signs them out everywhere else;
- **sign out**, and **delete their account** with its password.

`guests = true` lets players play without an account too. On a server with accounts, a guest carries `~` before
its name, in the game and in the panel, and cannot take the username of an account: whoever sees `Pavel` knows
it is the account. With `guests = false`, a player must sign in before creating or joining a game.

The server limits the tries, so that nobody guesses a password or a code, nor floods a mailbox:

| Limit | Value |
|---|---|
| Requests of the accounts from one address | 10 a minute |
| Wrong passwords of one account in a row | 10, then 15 minutes of pause |
| Wrong codes | 5 per code, 20 per account per day; the holder gets a mail on the 20th |
| A new code for the same account | Once a minute |
| Mails to one address | 5 an hour |

The players see what the server answers as messages of the game; the logs of the server say who signed in, by
account id, never a password, a code, a session or an email.

## The data kept, and for how long

What the server keeps of the players, in its database ([Accounts](../architecture/accounts.md#the-tables)), so
that a host can tell them, as the GDPR asks:

| Data | Kept | Until |
|---|---|---|
| Username, email | As typed | The account is deleted by its player, or within 24 hours if the email is never verified |
| Password | Its Argon2id hash only, never the password | The account is deleted, or the password changed |
| Dates: created, email verified, last seen | | The account is deleted |
| Ban and its reason | | The ban is lifted, or the account deleted |
| Sessions | The SHA-256 of their token, when they were made and last used | 30 days without use, sign-out, a new password, a ban, or the account deleted |
| Codes sent by email | Their hash | Used, replaced, 15 minutes, or the account deleted |
| Wrong codes of the day | A count | The next day (UTC) |
| Actions of the administrators about a player | Its name, its address, its account id ([Administration](../architecture/administration.md)) | Kept; when the account is deleted, its name becomes `deleted`, its address and id are removed |
| Mails | Nothing: sent through the mail server, which keeps what its host keeps | |
| Limits on tries, the address of a connection | In memory only | A restart of the server |

A player deletes their account from the game, with its password: the account, its sessions and its codes are
deleted at once. The guests leave nothing in the database. The logs hold the usernames, account ids and IP
addresses of the players, for as long as the host keeps the logs.
