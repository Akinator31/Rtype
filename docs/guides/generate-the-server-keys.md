# How to generate the server keys

This guide makes the key pair the server proves its identity with, and puts each key where it belongs. It is
for anyone who runs a server: the official one, or another.

The client only talks to a server that has the **private key** matching a **public key** it trusts: another
server, or an attacker between them, is refused
([ADR 0014](../decisions/0014-encryption.md#tcp-tls-13-server-key-pinned),
[TLS](../architecture/networking.md#tls)). Players never handle a key file: the client trusts the keys of the
official servers, built into it, and asks the player at the first connection to any other server
([ADR 0020](../decisions/0020-server-key-distribution.md)).

!!! tip "For development, no key is needed"

    A Debug server without `server.key` uses the development key pair committed in `keys/development/`, which
    Debug clients trust. Release builds refuse it: a release server does not start with it, and a release
    client never connects to a server that presents it.

## Prerequisites

- The project builds: see [Build](../getting-started/build.md). The tool, `r-type_keygen`, is built with the
  server and packaged with it.

## Steps

1. **Generate the keys on the machine of the server**, in the directory of your choice (created if missing).
   The private key never has to travel:

    === "Linux"

        ```sh
        ./build/bin/r-type_keygen --out keys
        ```

    === "Windows"

        ```powershell
        .\build\bin\Release\r-type_keygen.exe --out keys
        ```

    ```
    Private key: keys/server.key  (keep it on the server only, never share it)
    Public key:  keys/server.pub  (public: its fingerprint is what the clients trust)
    Fingerprint: 5b61c7a9ce1a44bfc64fec2013a4a09b6b4d5a8db4129eff890a1462c7ea1f1c
    ```

    The tool does not replace keys that already exist: see [Replacing the keys](#replacing-the-keys).

2. **Keep `server.key` on the server**, which loads it with its `--key` option
   ([Run a server](run-a-server.md)). On Linux, the file is only readable by the user who made it (`rw-------`): run the server as
   that user. On Windows, the file gets the access rights of its directory, so put it in a directory only the
   server's account can read.

    Never commit it, never send it, never put it in an environment variable or a Docker image: anyone who has
    it can pretend to be the server. Git ignores `*.key` files, except the development key.

3. **Tell the clients its fingerprint**, which depends on the server:

    === "The official server"

        Make a **backup key pair** too, in another directory, and keep its private key **offline** (an
        encrypted USB key, a password manager), with a copy of `server.key`. It is never installed on the
        server.

        ```sh
        ./build/bin/r-type_keygen --out backup-keys
        ```

        Then write the address of the server and both fingerprints in `apps/client/src/Online/OfficialServers.hpp`,
        and commit it: the next release of the client trusts both keys, without asking the players.

        ```cpp
        inline constexpr std::array<OfficialServer, 1> OfficialServers{{
            {.name = "Official server", .host = "rtype.example.org", .port = 4242,
             .keys = {"<fingerprint of keys/server.key>", "<fingerprint of backup-keys/server.key>"}},
        }};
        ```

    === "Another server"

        Nothing to install on the clients: at their first connection, they show the fingerprint of the server
        and ask the player to trust it. Give the fingerprint to the players by another way (a message, a
        website), so that they can check that it is the same.

4. **Check a fingerprint** at any time:

    ```sh
    r-type_keygen --fingerprint keys/server.key   # or server.pub, which gives the same fingerprint
    ```

    Without the tool, OpenSSL gives the same value:

    ```sh
    openssl pkey -pubin -in server.pub -outform DER | sha256sum
    ```

## Result

The server has `server.key`, the clients trust its fingerprint. When a client connects, the server proves in
the TLS handshake that it has the private key, and the client checks that its public key is one it trusts.
Otherwise, the client refuses the server before sending it anything, with `NetworkError::UntrustedServer` and
the fingerprint it presented in the logs. A player who trusted another key for this server sees a warning
naming both.

## Replacing the keys

Replace the keys only if they must change.

```sh
r-type_keygen --out keys --force
```

- **The official server** switches to its backup key: install the backup `server.key` on the server, and the
  clients already out keep working. Then make a new backup key pair and release a client that trusts the new
  pair of keys.
- **Another server**: the players who trusted the old key are warned that it changed, and must forget it
  (**Forget the server key**, in the Online menu) before trusting the new one. Tell them beforehand.

**If `server.key` leaked** (committed, sent, copied to a machine you do not trust), replace it at once:
whoever has the old private key can pretend to be the server to the clients that still trust it. Removing
the file from git is not enough, since it stays in the history.

## Reference

```
r-type_keygen [--out <directory>] [--force]
r-type_keygen --fingerprint <file>
```

| Option | Effect |
|---|---|
| `-o`, `--out <directory>` | Where to write `server.key` and `server.pub`, created if missing. The current directory by default. |
| `-f`, `--force` | Replace the keys already there. Without it, nothing is written if either file exists. |
| `--fingerprint <file>` | Show the fingerprint of `server.key` or `server.pub`, and write nothing. |
| `-h`, `--help` | Show the usage. |

| Exit code | Meaning |
|---|---|
| `0` | Done |
| `1` | The keys could not be written (they exist, or the directory cannot be written) or read |
| `2` | Invalid arguments |

Errors go to the logs, on the error output; set `RTYPE_LOG_LEVEL=debug` for more detail
([Logging](../architecture/logging.md)). The keys are ECDSA P-256 keys in PEM, which OpenSSL and other tools
read.
