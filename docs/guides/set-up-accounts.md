# How to set up accounts

This guide prepares a server for the accounts of its players: the mail server their codes are sent through. It
is for whoever hosts a server. The accounts themselves, and Discord, come in the next versions of the server
([#259](https://github.com/Akinator31/Rtype/issues/259)); this part gives the server its mail, which it already
checks at every start.

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
